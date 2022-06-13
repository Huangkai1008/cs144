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

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { DUMMY_CODE(ackno, window_size); }

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

void TCPSender::send_empty_segment() {}

Timer::Timer(const unsigned int _initial_retransmission_timeout)
    : _retransmission_timeout(_initial_retransmission_timeout) {}

void Timer::start() {
    this->_time_elapsed = 0;
    this->_is_timer_running = true;
}

bool Timer::is_running() const { return this->_is_timer_running; }

bool Timer::is_expired() const { return this->_time_elapsed >= this->_retransmission_timeout; }

void Timer::tick(const size_t ms_since_last_tick) const { this->_time_elapsed + ms_since_last_tick; }

unsigned int Timer::rto() const { return this->_retransmission_timeout; }

void Timer::set_rto(const unsigned int _rto) { this->_retransmission_timeout = _rto; }
