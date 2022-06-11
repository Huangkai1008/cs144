#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _first_unacceptable(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _first_unread = _output.bytes_read();
    _first_unacceptable = _first_unread + _capacity;
    segment s = {index, data};
    _add_segment(s, eof);
    _stitch_output();
    if (empty() && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t unassembled_bytes = 0;
    for (const auto &_segment : _segments) {
        unassembled_bytes += _segment.data.size();
    }
    return unassembled_bytes;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

void StreamReassembler::_add_segment(StreamReassembler::segment &s, const bool eof) {
    //!< If the segment is out of assemble range, ignore it.
    if (s.index >= _first_unacceptable || s.index + s.data.size() < _first_unassembled) {
        return;
    }

    bool seg_eof = eof;
    //!< If the segment is overflow, cut the bytes until the last acceptable index.
    if (s.index + s.data.size() > _first_unacceptable) {
        s.data = s.data.substr(0, _first_unacceptable - s.index);
        seg_eof = false;
    }

    //!< If the segment has parts unassembled, cut the bytes start from first unassembled index.
    if (s.index < _first_unassembled) {
        s.data = s.data.substr(_first_unassembled - s.index);
        s.index = _first_unassembled;
    }

    for (auto it = _segments.begin(); it != _segments.end();) {
        if (_is_overlapping(s, *it)) {
            _merge_segments(s, *it);
            _segments.erase(it++);
        } else {
            it++;
        }
    }
    _segments.insert(s);

    // if EOF was received before, it should remain valid
    _eof = _eof || seg_eof;
}

void StreamReassembler::_merge_segments(StreamReassembler::segment &s, const StreamReassembler::segment &other) {
    segment left{}, right{};
    if (s.index < other.index) {
        left = s;
        right = other;
    } else {
        right = s;
        left = other;
    }

    if (left.index + left.data.size() >= right.index + right.data.size()) {
        s = left;
    } else {
        s.index = left.index;
        s.data = left.data + right.data.substr(left.index + left.data.size() - right.index);
    }
}

bool StreamReassembler::_is_overlapping(const StreamReassembler::segment &a, const StreamReassembler::segment &b) {
    return max(a.index, b.index) < min(a.index + a.data.size(), b.index + b.data.size());
}
void StreamReassembler::_stitch_output() {
    while (!_segments.empty() && _segments.begin()->index == _first_unassembled) {
        _stitch_segment(*_segments.begin());
        _segments.erase(_segments.begin());
    }
}
void StreamReassembler::_stitch_segment(const StreamReassembler::segment &s) {
    _output.write(s.data);
    // Same as `_first_unassembled = _output.bytes_written();`
    _first_unassembled += s.data.size();
}
