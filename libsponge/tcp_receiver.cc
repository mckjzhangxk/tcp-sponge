#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//                              ____________
//                             |            |
//  syn,(S,F),payload   --->   |  Receiver  | -->  ackno,window
//                             |____________|
//                             |_reassembler|
//                             |____________|
//                                   |
//                                   |=====> stream_out
// Receiver 的成员组成

//  _reassembler：
//      1.未组装的字节队列:长度是 C，第一个字节序号是ackno(绝对)， 从第一个字节 一共允许接受window size个字节
//                      <--------------C--------------->
//                      |XXXXXXX-----------------------|
//                              <---------W------------>
//                              ^
//                              |
//                           [ackno]


//      2.内存字节流，保存以及完成组装的字节,长度是C，有BufferSize个没有被 上层消费的字节留在 ByteStream中
//                      <--------------C--------------->
//                      |XXXXXXXXXXXXXXXXXXXXXXX-------|
//                      <-----------sz--------->

// A.Receriver 对外 提供ackno,window_size
// B.Receriver的输入方法 segment_received(TCPSegment &seg)，提取<syn,fin,seq,payload>, 进而改变Receriver 内部_reassembler 的状态

using namespace std;
////类型定义成uint16 造成bug
inline bool TCPReceiver::contain(uint64_t xlow,uint64_t xhigh,uint64_t ylow,uint64_t yhigh){
    return !(ylow>=xhigh ||yhigh<= xlow);
}

//         <---------------W------------------>
// window: [..................................]
//          ^
//          |
//        [ackno]


//      <----Len Seq Space--------->
// seg: [..........................]
//      ^
//      |
//      [seq]

// 特殊情况：
// 1. Len Seq Space=0： 视作 1
// 2. W=0： 视作 1
// 3. isn为空： 返回false,除非seg.syn=true 才继续后面的逻辑
bool TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& hdr=seg.header();


    //窗口区 所对应的 【绝对索引】
    // payload_size + SYN(1) + FIN(1)
    ////类型定义成uint16 造成bug
    uint64_t start_window_index=_reassembler.stream_out().bytes_written()+_isn.has_value()+_reassembler.stream_out().input_ended();
    //类型定义成uint16 造成bug
    uint64_t end_windex_index=start_window_index+window_size();
    if (start_window_index==end_windex_index)
        end_windex_index++;

    if (!_isn.has_value()&&hdr.syn)
        _isn=hdr.seqno;

    if(!_isn.has_value())//这里是否应该抛出异常？？？而不是返回false
        return false;

    //数据payload 所对应的 【绝对索引】
    uint64_t start_abs_index=unwrap(hdr.seqno, *_isn, start_window_index);

    //类型定义成uint16 造成bug
    uint64_t end_abs_index=start_abs_index+seg.length_in_sequence_space();
    if (start_abs_index==end_abs_index)
        end_abs_index++;

     int ret=contain(start_window_index,end_windex_index,start_abs_index,end_abs_index);

     if(ret){
         //类型定义成uint16 造成bug
        uint64_t stream_index=start_abs_index?start_abs_index-1:0;
        auto& payload=seg.payload();
        _reassembler.push_substring(payload.copy(),stream_index,hdr.fin);
    }


    return ret;
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    
    std::optional<WrappingInt32> r;

    if(_isn.has_value()){
        r=wrap(_reassembler.stream_out().bytes_written()+1+_reassembler.stream_out().input_ended(),*_isn);
    }
    return r;
}

size_t TCPReceiver::window_size() const { 
    return _capacity-_reassembler.stream_out().buffer_size(); 
}
