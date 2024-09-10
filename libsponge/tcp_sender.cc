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
//把bytestream中的数据 尽可能多的搬运到 发送队列中
void TCPSender::fill_window() {
    int sws=_sws?_sws:1;
    int remain=sws-bytes_in_flight();//可以发生的最多字节数量，包括特殊字符syn,fin

    while (remain>0&&!_fin_send)
    {
        TCPSegment seg;
        seg.header().seqno=wrap(_next_seqno,_isn);

        int n=min((size_t)remain-!_syn_send,TCPConfig::MAX_PAYLOAD_SIZE);//可以发送的最多字节数量，不包括特殊字符
        
        if(!_syn_send){
            _syn_send= true;
            seg.header().syn=true;
        }
        
        std::string sendData=_stream.read(n);//从发送流中取出最多n个字节
        seg.payload()=Buffer(std::move(sendData));

        if (n>seg.length_in_sequence_space()&&_stream.eof()){//如果还有剩余空间，并且发送流结束
            _fin_send=true;
            seg.header().fin=true;
        }
        if(seg.length_in_sequence_space()>0)//
            _transmit(seg);
        else
            break ;
        remain-=seg.length_in_sequence_space();
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
// 获得ackno,win_size后， 确认并更新 发送窗口, 有可能发送新的数据
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    
    uint64_t old_ackno=_ackno;
    uint16_t old_win=_sws;
    uint64_t new_ackno= unwrap(ackno,_isn,_next_seqno);
    
    if(new_ackno<old_ackno){//过期的确认，直接返回
        return true;        
    }else if(new_ackno==old_ackno){//没有接受的新的数据，只是更新了 窗口数值
         _sws=window_size;
          if(_sws<=old_win) //没有新的确认，窗口也减小了，不能发送更多的数据(fill_window)，所以直接返回
	        return true;
    }
    
    if(new_ackno>_next_seqno)//无效的确认，不可能确认号是 还没发送的字节索引
        return false;
    
    _ackno=new_ackno;
    _sws=window_size;
    
    //num_of_ack 表示 被确认的字节数量
    uint64_t num_of_ack=new_ackno-old_ackno;
    if(num_of_ack>0){
        _timer.stop();
        if(old_ackno==0){//对syn的确认
            num_of_ack--;
        } 
        if(_fin_send&& (new_ackno==_next_seqno) ){//对fin的确认
            num_of_ack--;
        }
        _outstanding_segs.remove_prefix(num_of_ack);
    }
    //有了确认，窗口发生了滑动，可以发送更多的数据了！！！
    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(_timer.expire(ms_since_last_tick)){

        _timer.double_mul();//从0开始计数
        
        //接受窗口>0的时候才重传？
        if(_sws>0){//当发现 超时的时候，重传_outstanding_segs的tcpsegment
                TCPSegment seg;
                seg.header().seqno=wrap(_ackno,_isn);
            
                auto& payload=_outstanding_segs.size()>0?_outstanding_segs.buffers().front():Buffer();
                seg.payload()=payload;
                    
                
                if(_ackno==0){
                    seg.header().syn=true;        
                }

                if(_fin_send&&( _ackno+seg.length_in_sequence_space()==(_next_seqno-1) )){// _next_seqno-1是FIN的索引(当_fin_send=true时)
                    seg.header().fin=true;
                }            
                 _transmit(seg,false);
        }
           
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


//本方法的bug在于，不会重传syn,fin,因为syn,fin的payload长度一般是0
void TCPSender::_transmit(TCPSegment & seg,bool need_retry){
    _segments_out.push(seg);
          
    if(need_retry){
        _next_seqno+=seg.length_in_sequence_space();
        if(seg.payload().size()>0){
            _outstanding_segs.append(seg.payload());
        }

        _timer.start();
    }
}