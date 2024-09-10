#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {  
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight();}

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
     if (!_active)
        return;

    const TCPHeader &hdr=seg.header();
    const Buffer &payload=seg.payload();

     _time_since_last_segment_received=0;

    
     //  sender处于SYN_SENT，只应该收到syn或者syn-ack,不应该接受有负载的数据包
    if (_in_syn_sent() && seg.header().ack && seg.payload().size() > 0) {
        return;
    }

    if(hdr.rst){//收到对方的rst,只需要修改TCPConnect的状态，不需要回复RST
        _unclean_shutdown(false);
        return;
    }
    bool need_reply_empty=false;//表示需要恢复确认包
    
    //对于sender
    if(hdr.ack){
        if(!_sender.ack_received(hdr.ackno,hdr.win)){//说明对方给我们的确认号是错误的，再次发送一个空ack,让对方明确我方的seq
            need_reply_empty=true;
        }
    }
    //对于receiver
    if( !_receiver.segment_received(seg)){//返回false,说明seg不在receiver的接收窗口内,再次发送一个空ack,给对方纠正错误(ack,win)
           need_reply_empty=true;
    }else if(seg.length_in_sequence_space()>0){//正确接收后的"负载"需要被回复
        if(seg.header().syn&&_sender.next_seqno_absolute()==0){
            connect();
            return;
        }
        need_reply_empty=true;
    }
    
    //此时，seq,ack,win都已经更新好了,可以回复确认了
    if(need_reply_empty)
        _push_ack_segment();

    _clean_shutdown();
 }

bool TCPConnection::active() const { 
    return _active; 
}

size_t TCPConnection::write(const string &data) {
    auto & bs=_sender.stream_in();
    size_t n=bs.write(data);
    _sender.fill_window();
    _push_segments_out();
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (!_active)
        return;

    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received+=ms_since_last_tick;

    if(_sender.consecutive_retransmissions()>TCPConfig::MAX_RETX_ATTEMPTS){//如果重试次数过多，发送rst
        _unclean_shutdown();
        return;
    }else{//因为有可能涉及到重传，所以调用以下发送方法
        _push_segments_out();
        _clean_shutdown();
    }
 }

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _push_segments_out();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _push_segments_out();
}

//销毁对象的时候，如果还是获得状态，调用_unclean_shutdown(true)，立即终止，并且通知对方
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void  TCPConnection::_push_segments_out(){
    while (!_sender.segments_out().empty()) {
         auto& seg = _sender.segments_out().front();
         _sender.segments_out().pop();

         TCPHeader hdr=seg.header();
         auto ackopt=_receiver.ackno();
         hdr.win=1;
         if (ackopt.has_value())
         {
           hdr.ack=true;
           size_t sws=_receiver.window_size();
           if(sws>std::numeric_limits<uint16_t>().max()){
                sws=std::numeric_limits<uint16_t>().max();
           }

           hdr.win=sws;
           hdr.ackno=ackopt.value();
         }
         if(_sender.stream_in().error()||_receiver.stream_out().error()){//如果本TCP已经发送过rst,那么 携带RST
            hdr.rst=true;
         }     
         _segments_out.push(seg);
    }
}
void TCPConnection::_unclean_shutdown(bool send_rst){
            _active=false;
            _linger_after_streams_finish=false;
            
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();

            if(send_rst){//发生给对方一个数据包
                _push_ack_segment();
            }

}

//本方法目的是修改_active=false
void TCPConnection::_clean_shutdown(){
    if(!_active)
        return;
    //Preq1,收到 Fin(remote)
    bool preq1=_receiver.stream_out().input_ended();
    //Preq2, 发出 Fin(local),这里注意，EOF并不能说明一定发送了FIN，调用_sender.fillwindow后才能说明一定发送了FIN
    bool preq2=_fin_sent();

    //Preq3,说明收到了 FIN-ACK（local）
    bool preq3=preq2&&(_sender.bytes_in_flight()==0);

    if (preq1&& !preq2)
    {
        //先收到FIN的时候，执行本行
        _linger_after_streams_finish=false;
    }else if (preq1&&preq3){
        if(_linger_after_streams_finish==false)
                _active=false;
        else if(time_since_last_segment_received()>10*_cfg.rt_timeout){
                _active=false;
        }
    }
}
//回复确认包，如果发送队列有segment,不需要单独发生确认包，否则可以回复一个空的
void TCPConnection::_push_ack_segment(){
    if(_sender.segments_out().empty())
        _sender.send_empty_segment();
    _push_segments_out();

}

bool TCPConnection::_in_syn_sent() {
      return _sender.next_seqno_absolute() > 0 && _sender.bytes_in_flight() == _sender.next_seqno_absolute();
}


bool TCPConnection::_fin_sent() {
      return _sender.stream_in().eof() && (2+_sender.stream_in().bytes_written()) == _sender.next_seqno_absolute();
}