#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _sws(1)
    ,_next_seqno(0)
    , _ackno(0)
    , _timer(retx_timeout)
    ,_syn_send(false)
    ,_fin_send(false)
    ,_outstanding_segs()
    {}

size_t TCPSender::bytes_in_flight() const {
    if(_syn_send)
        return _next_seqno-_ackno ;
    else
        return 0;
 }

void TCPSender::fill_window() {


    if(!_syn_send){
        _syn_send= true;
        TCPSegment seg;
        seg.header().syn=true;
        seg.header().seqno=wrap(0,_isn);
        _transmit(seg);
        return ;
    }

    size_t remain=_sws-bytes_in_flight();

    while (remain>0&&_stream.buffer_size()>0)
    {
        TCPSegment seg;
        seg.header().seqno=wrap(_next_seqno,_isn);

        int n=min(remain,TCPConfig::MAX_PAYLOAD_SIZE);
        
        std::string sendData=_stream.read(n);
    
        seg.payload()=Buffer(std::move(sendData));

        _transmit(seg);


        remain-=sendData.size();
    }

    if(remain>0&&_stream.eof()&&!_fin_send){//fin只会发送一次
        _fin_send=true;

        TCPSegment seg;
        seg.header().seqno=wrap(_next_seqno,_isn);
        seg.header().fin=true;
        _transmit(seg);
        
    }

    
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    
    uint64_t old_ackno=_ackno;
    uint64_t new_ackno= unwrap(ackno,_isn,_next_seqno);
    
    if(new_ackno<=old_ackno)
        return true;
    if(new_ackno>_next_seqno)
        return false;
    
    _ackno=new_ackno;
    _sws=window_size;
    
    uint64_t num_of_ack=new_ackno-old_ackno;
    _outstanding_segs.remove_prefix(num_of_ack);

    _timer.stop();

    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(_timer.expire(ms_since_last_tick)){
        _timer.stop();
        _timer.double_mul();

        TCPSegment seg;
        seg.header().seqno=wrap(_ackno,_isn);
        auto& payload=_outstanding_segs.buffers().front();

        if(_ackno==0)
            seg.header().syn=true;
        if(_fin_send&&(_ackno+1==_next_seqno)){
            seg.header().fin=true;
        }else{
            seg.payload()=payload;
        }

        _transmit(seg,false);
        _timer.start();
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { 
    uint32_t x=_timer.get_muls(); 
    int c=-1;
    while (x)
    {
       x>>=1;
       c++;
    }
    
    return c;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno=wrap(_next_seqno,_isn);
     _transmit(seg,false);
}
