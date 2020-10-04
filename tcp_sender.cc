#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// passes automated checks run by `make check_lab3`.



//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) { 
    _current_retransmission_timeout = _initial_retransmission_timeout;
    }

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t bytes_in_flight  = _next_seqno;
    if(_ack_seen)
        bytes_in_flight =  (_next_seqno > _max_seqno_acked)?(_next_seqno - _max_seqno_acked):0;
    return bytes_in_flight;
 }


void TCPSender::fill_window() {
    if(!_is_syn_sent)
    {
        TCPSegment syn_seg;
        syn_seg.header().seqno = next_seqno();
        syn_seg.header().syn = true;
        _segments_out.push(syn_seg);
        _next_seqno++;
        _is_syn_sent = true;
        _retransmission_queue.push(syn_seg);
        if(!_timer_on)
        {
            _timer_expiry = _time_alive + _current_retransmission_timeout;
            _timer_on = true;
        }
    }
    while(!_stream.buffer_empty() && (bytes_in_flight() < _window_size  || !_window_size))
    {
        TCPSegment tseg;
        tseg.header().seqno = next_seqno();
        //Zero window probing
        size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, min(_stream.buffer_size(), max(static_cast<uint64_t>(1),_window_size) - bytes_in_flight()));
        if(payload_size==0)
            break;
        tseg.payload() = Buffer(_stream.read(payload_size));
        _checkpoint = _stream.bytes_read();
        _next_seqno+=payload_size;
        if(_stream.eof() && bytes_in_flight()< _window_size && !_is_fin_sent)
        {
            tseg.header().fin=true;
            _next_seqno++;
            _is_fin_sent = true;
            
        }
        _segments_out.push(tseg);
        _retransmission_queue.push(tseg);
        if(!_timer_on)
        {
            _timer_expiry = _time_alive + _current_retransmission_timeout;
            _timer_on = true;
        }
        if(_window_size==0)//Zero window probing
            break;
    }
    if(!_is_fin_sent && _stream.eof() && (bytes_in_flight()<_window_size || bytes_in_flight()==0))
    {
        TCPSegment fin_seg;
        fin_seg.header().seqno = next_seqno();
        fin_seg.header().fin = true;
        _segments_out.push(fin_seg);
        _next_seqno++;
        _retransmission_queue.push(fin_seg);
        _is_fin_sent = true;
        if(!_timer_on)
        {
            _timer_expiry = _time_alive + _current_retransmission_timeout;
            _timer_on = true;
        }
    }
    _max_seqno = max(_max_seqno, _next_seqno - 1);
        

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = static_cast<size_t>(window_size);
    uint64_t abs_ackno = unwrap(ackno, _isn, _checkpoint);
    if(abs_ackno<=_max_seqno_acked)
        return true;
    uint64_t queue_front_seqno = (!_retransmission_queue.empty()) ? unwrap(_retransmission_queue.front().header().seqno, _isn, _checkpoint) : 0;
    if(abs_ackno<=_max_seqno+1)
    {
        _ack_seen = true;
        _max_seqno_acked = max(_max_seqno_acked, abs_ackno);
        while(!_retransmission_queue.empty() && queue_front_seqno < abs_ackno)
        {
            _retransmission_queue.pop();
            if(!_retransmission_queue.empty())
                queue_front_seqno = unwrap(_retransmission_queue.front().header().seqno, _isn, _checkpoint);
            else
                break;
        }
        _current_retransmission_timeout = _initial_retransmission_timeout;
        if(!_retransmission_queue.empty())
        {
            _timer_on = true;
            _timer_expiry = _time_alive + _current_retransmission_timeout;
        }
        else
            _timer_on = false;
        _consecutive_retransmissions = 0;
        return true;
    }
    return false;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _time_alive += ms_since_last_tick ;
    if(_retransmission_queue.empty())
        return;
    if(_time_alive < _timer_expiry || !_timer_on)
        return;
    while(!_retransmission_queue.empty())
    {
        _segments_out.push(_retransmission_queue.front());
        if(_window_size!=0)
        {
            _current_retransmission_timeout*=2;
            _consecutive_retransmissions++;
        }
        _timer_expiry = _time_alive + _current_retransmission_timeout;
        break;
        _retransmission_queue.pop();
    }
    if(_retransmission_queue.empty())
        _timer_on = false;
    
 }


unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment empty;
    empty.header().seqno = next_seqno();
    _segments_out.push(empty);
}
