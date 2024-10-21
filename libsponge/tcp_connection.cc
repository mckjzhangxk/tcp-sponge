#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

// 对于 RST的处理逻辑:
// A.直接跳过 sender.ack_received()逻辑,因为 RST就是要中断,没必要再有确认逻辑了
// B.对于Recerver:
//     LISTEN状态: 直接 _unclean_shutdown
//     SYN_RECV,FIN_RECV状态: 要验证 数据包的Seq是否正确.正确 调用_unclean_shutdown
void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active)
        return;

    const TCPHeader &hdr = seg.header();
    const Buffer &payload = seg.payload();

//    _time_since_last_segment_received = 0;

    bool need_reply_ack = false;  // 表示需要恢复确认包


    bool _sender_closed=_sender.next_seqno_absolute()==0;
    //对于rst数据包,不用再考虑关于对sender的确认了(ackno,sws)
    if(!hdr.rst){
        bool _sender_synsent= !_sender_closed&&(_sender.bytes_in_flight()==_sender.next_seqno_absolute());

        if (hdr.ack) {
            //_sender处于 CLOSED,SYN_SENT需要被严格的校验后 确认
            // 此数据包 是否与本tcpconnect 属于 【统一会话】
            if (_sender_closed) {  // CLOSED状态不应该被确认
                _unclean_shutdown();
                return;
            } else if (_sender_synsent) { //SYN_SEND 状态要保证[正确的确认],否则就不是【同一会话】
                // 1.对syn 只能有 syn-ack 或者ack,不能携带payload
                // 2.只发送syn出去，收到的还是错误的ackno,说明 不属于【同一次会话】

                if (payload.size() > 0||_sender.next_seqno() != hdr.ackno)
                    return;

            }
            // 【同一会话】的处理逻辑
            // SYN_SENT,SYN_ACKED,FIN_SEND,FIN_ACKED 种状态下的逻辑

            if(!_sender.ack_received(hdr.ackno, hdr.win)){
                // 非有效确认包,需要重传
                need_reply_ack = true;
            }
        } else{
            //收到的数据包没有确认 满足以下条件之一 是允许的

            // 1.Sender 处于CLosd 状态
            // 2.Sender 处于SYN_SENT状态
            if(!_sender_closed&&!_sender_synsent){
                return;
            }
        }
    }


    // 对于receiver,只考虑hdr.seqno()
    if (!need_reply_ack) {  // 说明sender认为 segment是有效的
        if(!hdr.rst){
            _time_since_last_segment_received=0;
            if (_receiver.segment_received(seg)){
                if (hdr.syn&& _sender_closed){
                    connect();
                    return;
                }

                if (seg.length_in_sequence_space() > 0) {  // 正确接收后的"负载"需要被回复

                    need_reply_ack = true;
                }
            } else{
                need_reply_ack = true;
            }
        }else{
            auto ackopt=_receiver.ackno();

            if (!ackopt.has_value()) { //LISTEN状态下，还不知道remote的 isn,所以任何的rst都会触发_unclean_shutdown(false)
                _unclean_shutdown(false);
            } else if(*ackopt==hdr.seqno){//SYN_RECV,FIN_RECV 只要验证 发送的序列号正确,就可以unclean_shutdown了
                _unclean_shutdown(false);
            }
        }
    }

    // 此时，seq,ack,win都已经更新好了,可以回复确认了
    if (need_reply_ack)
        _push_ack_segment();

    _clean_shutdown();
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active)
        return;

    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {  // 如果重试次数过多，发送rst
        fprintf(stderr, "too many retry\n");
        _unclean_shutdown();
    } else {  // 因为有可能涉及到重传，所以调用以下发送方法
        _push_segments_out();
        _clean_shutdown();
    }
}

void TCPConnection::connect() {  // 发送SYN
    _sender.fill_window();
    _push_segments_out();
}

size_t TCPConnection::write(const string &data) {  // 正常发送数据
    auto &bs = _sender.stream_in();
    size_t n = bs.write(data);
    _sender.fill_window();
    _push_segments_out();
    return n;
}

void TCPConnection::end_input_stream() {  // 发送FIN
    _sender.stream_in().end_input();
    _sender.fill_window();
    _push_segments_out();
}

// 销毁对象的时候，如果还是获得状态，调用_unclean_shutdown(true)，立即终止，并且通知对方
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

void TCPConnection::_push_segments_out() {
    while (!_sender.segments_out().empty()) {
        auto &seg = _sender.segments_out().front();  // seq,(S,F),payloud以及被设置好了
        _sender.segments_out().pop();

        TCPHeader &hdr = seg.header();
        auto ackopt = _receiver.ackno();

        if (ackopt.has_value()) {
            hdr.ack = true;
            hdr.ackno = ackopt.value();

            size_t sws = _receiver.window_size();
            hdr.win = sws > 0xffff ? 0xffff : sws;

        } else {
            hdr.ack = false;
            hdr.ackno = WrappingInt32(0);
            hdr.win = 1;
        }

        if (_sender.stream_in().error() || _receiver.stream_out().error()) {  // 如果本TCP已经发送过rst,那么 携带RST
            hdr.rst = true;
        }

        size_t  sz=seg.payload().size();

        if(sz!=seg.payloadSize){
            fprintf(stderr,"payload size EORROR[%ld] -->[%ld]\n",seg.payloadSize,sz);
            throw  "_push_segments_out layload size error";
        }

        _segments_out.push(seg);
    }
}
// 针对 【本次会话】 的异常终止 与是否回复rst
void TCPConnection::_unclean_shutdown(bool send_rst) {
    fprintf(stderr, "=========UUUUUUCCclean_shutdown,%s===========\n", state().name().c_str());
    _active = false;
    _linger_after_streams_finish = false;

    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    if (send_rst) {  // 发生给对方一个数据包
        _push_ack_segment();
    }
}

// 本方法目的是修改_active=false
void TCPConnection::_clean_shutdown() {
    if (!_active)
        return;
    // Preq1,收到 Fin(remote)
    bool preq1 = _receiver.stream_out().input_ended();
    // Preq2, 发出 Fin(local),这里注意，EOF并不能说明一定发送了FIN，调用_sender.fillwindow后才能说明一定发送了FIN
    bool preq2 = _sender.stream_in().eof() && (2 + _sender.stream_in().bytes_written()) == _sender.next_seqno_absolute();

    // Preq3,说明收到了 FIN-ACK（local）
    bool preq3 = preq2 && (_sender.bytes_in_flight() == 0);

    if (preq1 && !preq2) {
        // 先收到FIN的时候，执行本行
        _linger_after_streams_finish = false;
    } else if (preq1 && preq3) {
        if (_linger_after_streams_finish == false)
            _active = false;
        else {
            //            fprintf(stderr,"timewait %ld\n",time_since_last_segment_received());
            if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
                _active = false;
            }
        }
    }
}


// 回复确认包，如果发送队列有segment,不需要单独发生确认包，否则可以回复一个空的
// 对于Receriver: _push_segments_out()会填写此刻的 ackno,sws
// 对于Sender: _sender.segments_out()的seg的 seq都是小于 next_seq的合法;序列
void TCPConnection::_push_ack_segment() {
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    _push_segments_out();
}

