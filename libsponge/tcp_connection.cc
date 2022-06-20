#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active) {
        return;
    }

    // Reset the timer.
    _time_since_last_segment_received = 0;

    const TCPHeader &header = seg.header();

    // If sender is `CLOSED` and receiver is `LISTEN`, the connection's state is `LISTEN`.
    // As the receiver side, it should only accept `SYN` segment.
    if (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0) {
        if (!header.syn) {
            return;
        }

        // Accept the SYN segment.
        _receiver.segment_received(seg);

        // Try to send SYN segment, use for opening TCP at the same time.
        connect();
        return;
    }

    // If sender is `SYN_SENT` and receiver is `LISTEN`, the connection's state is `SYN_SENT`.
    // As the receiver side, it should accept `SYN&ACK` segment.
    if (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() > 0 &&
        _sender.next_seqno_absolute() == bytes_in_flight()) {
        // The `SYN&ACK` segment can't carry any payloads.
        if (seg.payload().size()) {
            return;
        }

        // The `SYN&ACK` segment must has `SYN` and `ACK` flag.
        if (!header.ack || !header.syn) {
            return;
        }

        _receiver.segment_received(seg);
        _sender.send_empty_segment();
        return;
    }

    // If the segment's `RST` flag is set, sets both the inbound and outbound streams to the error
    // state and kills the connection permanently.
    if (header.rst) {
        unclean_shutdown();
        return;
    }

    _receiver.segment_received(seg);
    _sender.ack_received(header.ackno, header.win);

    // If the incoming segment occupied any sequence numbers, the TCPConnection makes
    // sure that at least one segment is sent in reply, to reflect an update in the ackno and
    // window size.
    if (_sender.segments_out().empty() && seg.length_in_sequence_space() > 0) {
        _sender.send_empty_segment();
    }

    // Extra special case:  responding to a “keep-alive” segment.
    if (_receiver.ackno().has_value() and seg.length_in_sequence_space() == 0 and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    // Send segments.
    send_segments();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);
    return {};
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

void TCPConnection::end_input_stream() {}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send an RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _segments_out.front();
        _sender.segments_out().pop();
        // Before sending the segment, the TCPConnection will ask the TCPReceiver for the fields
        // it’s responsible for on outgoing segments: ackno and window size. If there is an ackno,
        // it will set the ack flag and the fields in the TCPSegment.
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            // "Send the biggest value you can. You might find the std::numeric limits class helpful."
            seg.header().win = min(_receiver.window_size(), numeric_limits<size_t>::max());
        }
        _segments_out.emplace(seg);
    }
}
void TCPConnection::unclean_shutdown() {
    // The outbound and inbound ByteStreams should both be in the error state,
    // and active() can return false immediately.
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}
