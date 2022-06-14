#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    TCPSegment seg;

    //! If SYN flag has not sent, sent the SYN segment.
    //! The SYN segment doesn't carry any payload.
    //! The sender's state turn to `SYN_SENT`.
    if (!_syn_sent) {
        seg.header().syn = true;
        send_segment(seg);

        _syn_sent = true;
        return;
    }

    //    if (!_segments_outstanding.empty() && _segments_outstanding.front().header().syn)
    //        return;
    //
    //    if (!_stream.buffer_size() && !_stream.eof())
    //        return;
    //    if (_fin_sent)
    //        return;

    //! If SYN flag has sent but nothing acknowledged, wait util the ack.
    if (next_seqno_absolute() == bytes_in_flight()) {
        return;
    }

    //! Assume the receiver’s window size as one byte
    //! before sender has gotten an ACK from the receiver
    //! `zero window probing`
    uint64_t window_size = _receiver_window_size == 0 ? 1 : _receiver_window_size;
    uint64_t remain;
    while ((remain = window_size - (_next_seqno - _last_ackno))) {
        uint64_t payload_size = min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
        seg.payload() = _stream.read(payload_size);
        //! Set fin flag.
        if (_stream.eof() && seg.length_in_sequence_space() < window_size) {
            seg.header().fin = true;
        }
        if (seg.length_in_sequence_space() == 0)
            return;
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!_is_ack_valid(absolute_ackno)) {
        return;
    }

    _receiver_window_size = window_size;
    _last_ackno = absolute_ackno;

    _timer.start();

    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() > absolute_ackno) {
            break;
        }
        _bytes_in_flight -= seg.length_in_sequence_space();
        _segments_outstanding.pop();
    }

    fill_window();

    if (_segments_out.empty()) {
        _timer.stop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    //! If the timer not running, skip the tick.
    if (!_timer.is_running()) {
        return;
    }

    _timer.tick(ms_since_last_tick);

    //! If the timer is expired and exist outstanding segments,
    //! retransmit the earliest (lowest sequence number) segment
    if (_timer.is_expired() && !_segments_outstanding.empty()) {
        _segments_out.push(_segments_outstanding.front());

        //! If the window size is nonzero:
        //! 1. keep track of the number of consecutive retransmissions
        //! 2. double the value of RTO
        if (_receiver_window_size) {
            _consecutive_retransmissions += 1;
            _timer.set_rto(_timer.rto() << 1);
        }

        //! reset the transmission timer and start it
        _timer.start();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = next_seqno();

    //! If the timer is not running, start the timer.
    if (!_timer.is_running()) {
        _timer.start();
    }

    _segments_out.push(seg);
    _segments_outstanding.push(seg);

    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

bool TCPSender::_is_ack_valid(uint64_t absolute_ackno) {
    //! Ignore the ack number greater than next seq number or less than smallest unacked seq number
    return absolute_ackno <= _next_seqno &&
           absolute_ackno >= unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno);
}

Timer::Timer(const unsigned int _initial_retransmission_timeout)
    : _retransmission_timeout(_initial_retransmission_timeout) {}

void Timer::start() {
    this->_time_elapsed = 0;
    this->_is_timer_running = true;
}

void Timer::stop() { this->_is_timer_running = false; }

bool Timer::is_running() const { return this->_is_timer_running; }

bool Timer::is_expired() const { return this->_time_elapsed >= this->_retransmission_timeout; }

void Timer::tick(const size_t ms_since_last_tick) { this->_time_elapsed = this->_time_elapsed + ms_since_last_tick; }

unsigned int Timer::rto() const { return this->_retransmission_timeout; }

void Timer::set_rto(const unsigned int _rto) { this->_retransmission_timeout = _rto; }
