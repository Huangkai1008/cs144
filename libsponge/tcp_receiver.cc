#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    WrappingInt32 seqno = header.seqno;

    //! If the receiver still in `SYN_RECV` state, refuse any new SYN segment.
    //! If the receiver not in `SYN_RECV` state, refuse any non-SYN segment.
    if ((_syn_received && header.syn) || (!_syn_received && !header.syn)) {
        return;
    }

    //! If the the receiver in `LISTEN` state, get the SYN segment.
    if (!_syn_received && header.syn) {
        _isn = header.seqno;
        _syn_received = true;
        seqno = seqno + 1;
    }

    if (_syn_received && header.fin) {
        _fin_received = true;
    }

    //! Use the index of the last reassembled byte as the checkpoint.
    size_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t absolute_seqno = unwrap(seqno, _isn, checkpoint);
    _reassembler.push_substring(seg.payload().copy(), absolute_seqno - 1, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_received) {
        return nullopt;
    }

    //! If outgoing ack is corresponding to FIN.
    //! In the situation, FIN is received and all segments are assembled, add another one.
    bool fin_finished = _fin_received && _reassembler.empty();
    return wrap(_reassembler.first_unassembled() + 1 + fin_finished, _isn);
}

size_t TCPReceiver::window_size() const {
    //! Equal to `first_unacceptable - first_unassembled`
    return _capacity - _reassembler.stream_out().buffer_size();
}
