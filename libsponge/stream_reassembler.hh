#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    //! Segment is the substring of byte stream.
    struct segment {
        size_t index;  //!< The begin index of the segment
        string data;   //!< The data of the segment
        bool operator<(const segment &s) const { return index < s.index; }
    };

    ByteStream _output;                //!< The reassembled in-order byte stream
    size_t _capacity;                  //!< The maximum number of bytes
    bool _eof = false;                 //!< Whether the last byte of `data` will be the last byte in the entire stream
    size_t _first_unread = 0;          //!< The first unread bytes index
    size_t _first_unassembled = 0;     //!< The first unassembled bytes index
    size_t _first_unacceptable;        //!< The first unacceptable bytes index
    std::set<segment> _segments = {};  //!< The stored segments

    //! Add segment to the StreamReassembler
    void _add_segment(segment &s, const bool eof);

    //! Merge two segments.
    static void _merge_segments(segment &s, const segment &other);

    //! Whether two segments overlapping.
    static bool _is_overlapping(const segment &a, const segment &b);

    //! Stitch output until the first unassembled index.
    void _stitch_output();

    //! Write data string of segment into output.
    void _stitch_segment(const segment &s);

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
