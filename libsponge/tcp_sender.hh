#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The retransmission timer.
class Timer {
  private:
    //! Whether the timer is running
    bool _is_timer_running{false};

    //! The time elapsed
    unsigned int _time_elapsed{0};

    //! Current value of RTO
    unsigned int _retransmission_timeout;

  public:
    //! Initialize a Timer
    explicit Timer(unsigned int _initial_retransmission_timeout);

    //! Start timer
    void start();

    //! Stop timer
    void stop();

    //! Whether the timer running
    bool is_running() const;

    //! Whether the timer expired
    bool is_expired() const;

    //! \brief Notifies the Timer of the passage of time
    void tick(const size_t ms_since_last_tick);

    //! Get current RTO
    unsigned int rto() const;

    //! Set current RTO
    void set_rto(const unsigned int _rto);
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    //! last acknowledge number
    uint64_t _last_ackno{0};

    //! the number of consecutive retransmissions
    unsigned int _consecutive_retransmissions{0};

    //! the size of receiver window.
    uint16_t _receiver_window_size{0};

    //! the tcp sender timer
    Timer _timer;

    //! outstanding segments that the TCPSender may resend
    std::queue<TCPSegment> _segments_outstanding{};

    //! Whether the SYN flag sent
    //! If SYN sent, the sender's state convert `CLOSED` to `SYN_SENT`
    bool _syn_sent = false;

    //! Whether the FIN flag sent.
    bool _fin_sent = false;

    //! How many sequence numbers are occupied by segments sent but not yet acknowledged.
    uint64_t _bytes_in_flight = 0;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    //! Send segment
    void send_segment(TCPSegment &seg);

    //! Whether the ack valid
    bool _is_ack_valid(uint64_t absolute_ackno) const;
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
