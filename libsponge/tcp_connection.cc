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


    bool need_reply_empty=false;//表示需要恢复确认包

    const TCPState& st=state();
    const auto& _sender_state=st.state_summary(_sender);
    

    //对于sender,只考虑ackno
    if(hdr.ack){

        //_sender处于 CLOSED,SYN_SENT需要被严格的校验后 确认
        // 此数据包 是否与本tcpconnect 属于 【统一会话】
        if(_sender_state==TCPSenderStateSummary::CLOSED){//我没有syn不需要被确认
            _unclean_shutdown(true);
            return;
        }else if (_sender_state==TCPSenderStateSummary::SYN_SENT){
            //对syn 只能有 syn-ack 或者ack,不能携带payload
            if(payload.size()>0)
                return;
            //只发送syn出去，收到的还是错误的ackno,说明 不属于【同一次会话】
            if(_sender.next_seqno()!=hdr.ackno){
                //如果remote 还没有关闭，发生rst
                //为了让remote能正确 确认发生的rst数据包,发送的segment
                // seqnum: remote期待收到的序号(hdr.ackno)
                // acknum: local对remote信息的确认(hdr.seqno+seg.length_in_sequence_space
                if(!hdr.rst)
                    _unclean_shutdown(hdr.ackno,hdr.seqno+seg.length_in_sequence_space());
                return;
            }
        }
        //【同一会话】的处理逻辑
        
        //返回false,说明对方给我们的【无效的ackno】
        if(!_sender.ack_received(hdr.ackno,hdr.win)){
            //需要重传空包,让对方明确我方的seq
            // 注意这里任务，这个TCP Segment的 hdr.ackno是无效的，
            // 所以发送TCP Segment所携带的信息(rst) 直接忽略
            need_reply_empty=true;
        } else if(hdr.rst){
            _unclean_shutdown(false);
            return;
        }
    } else{
        if (_sender_state==TCPSenderStateSummary::SYN_SENT&&hdr.rst){
            return ;
        }
    }

    const auto& _receiver_state=st.state_summary(_receiver);
    //对于receiver,只考虑hdr.seqno()
    if(!need_reply_empty){//说明sender认为 segment是有效的

        //LISTEN状态下，还不知道remote的 isn,所以任何的rst都会触发_unclean_shutdown(false)
        if(_receiver_state==TCPReceiverStateSummary::LISTEN){
            if(hdr.rst){
                 _unclean_shutdown(false);
                return;
            }
        }

        if( _receiver.segment_received(seg)){
            if(hdr.rst){
                _unclean_shutdown(false);
                return;
            }
             if(seg.length_in_sequence_space()>0){//正确接收后的"负载"需要被回复
                if(hdr.syn&&_sender_state==TCPSenderStateSummary::CLOSED){
                    connect();
                    return;
                }
                need_reply_empty=true;
             }
        }else{//数据包没有被_receiver【确认可被接受】,或者说就是【seqno不在接受窗口内】
            if(!hdr.rst){
                need_reply_empty=true;
            }
           
        }

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

         TCPHeader& hdr=seg.header();
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
//针对 【本次会话】 的异常终止 与是否回复rst
void TCPConnection::_unclean_shutdown(bool send_rst){
            _active=false;
            _linger_after_streams_finish=false;
            
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();

            if(send_rst){//发生给对方一个数据包
               _push_ack_segment();
            }

}

void TCPConnection::_unclean_shutdown(WrappingInt32 seqno,WrappingInt32 ackno){
    _unclean_shutdown(false);


    TCPSegment seg;
    TCPHeader& hdr=seg.header();

    hdr.seqno=seqno;
    hdr.rst= true;
    hdr.ack=true;
    hdr.ackno=ackno;
    _segments_out.push(seg);
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
        else if(time_since_last_segment_received()>=10*_cfg.rt_timeout){
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



bool TCPConnection::_fin_sent() {
      return _sender.stream_in().eof() && (2+_sender.stream_in().bytes_written()) == _sender.next_seqno_absolute();
}


