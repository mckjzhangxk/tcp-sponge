#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;
//                              _____________
//                             |             |
//  ackno,window        --->   |  Sender     | -->  syn,(S,F),payload
//                             |_____________|
//                            {recerver_window}
//                             | _byte_stream|
//                             |_____________|
//                                   |
//                                   |<===== stream_in

// recerver_window :
//       <----------------_sws---------------->
//       <---byte in flight--------><--fill--->       <=== _byte_stream
//       [AAAAABBBBBBCCCCCCCCDDDDDDD----------]
//        ^                         ^
//        |                         |
//       [ackno]                [next_seqno]

//_outstanding_segs: 按顺序存放以及发出但是没有被确认的segments(全部的byte in flight的字节)

//     | AAAAA    |   <-- timer(ackno)
//     | BBBBBB   |
//     | CCCCCCCC |
//     | DDDDDDD  |

// 1.当timer超时后，AAAA这个segment会被重传，并且double rto。
// 2. 当 ack_received(ackno,window_size)执行后，新的ackno产生

// recerver_window :
//       <----------------_sws--------------->
//       <-in flight-><---fill from bs------->
//       [BBBBBBCCCDDD-----------------------]
//        ^           ^
//        |           |
//       [ackno] [next_seqno]

// _outstanding_segs：
//     | BBBBBB   |   <-- timer(ackno)
//     | CCCCCCCC |
//     | DDDDDDD  |


//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _stream(capacity)
    , _sws(1)
    , _ackno(0)
    , _next_seqno(0)
    , _timer(retx_timeout)
    , _syn_sended(false)
    , _fin_sended(false)
    , _outstanding_segs() {}

size_t TCPSender::bytes_in_flight() const { return _next_seqno - _ackno; }

// recerver_window :
//       <----------------_sws---------------->
//       <---byte in flight--------><--fill--->       <=== _byte_stream
//       [AAAAABBBBBBCCCCCCCCDDDDDDD----------]
//        ^                         ^
//        |                         |
//       [ackno]                [next_seqno]

// 把bytestream中的数据 尽可能多的搬运到 发送队列中
void TCPSender::fill_window() {
    int sws = _sws ? _sws : 1;             // zero window probing
    int remain = sws - bytes_in_flight();  // 可以发生的最多字节数量，包括特殊字符syn,fin

    if (_fin_sended)
        return;
    // SYN 必须第一个发出
    if (!_syn_sended && remain > 0) {
        TCPSegment seg;
        seg.header().seqno = wrap(0, _isn);
        seg.header().syn = true;
        remain--;
        _transmit(seg);
        _next_seqno += seg.length_in_sequence_space();

        _syn_sended = true;
    }

    while (remain > 0 && !_fin_sended) {
        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);

        size_t n =min(static_cast<size_t>(remain), TCPConfig::MAX_PAYLOAD_SIZE);  // 可以发送的最多字节数量，不包括特殊字符

        std::string sendData = _stream.read(n);  // 从发送流中取出最多n个字节
        if (sendData.size() > 0)
            seg.payload() = Buffer(std::move(sendData));

        if (n > seg.length_in_sequence_space() && _stream.eof()) {  // 如果还有剩余空间，并且发送流结束
            _fin_sended = true;
            seg.header().fin = true;
        }
        if (seg.length_in_sequence_space() > 0) {
            _transmit(seg);
            _next_seqno += seg.length_in_sequence_space();
        } else
            break;
        remain -= seg.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
// 获得ackno,win_size后， 确认并更新 发送窗口, 有可能发送新的数据
//返回 是否三一个有效的确认号
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t old_ackno = _ackno;
    uint64_t new_ackno = unwrap(ackno, _isn, old_ackno);

    if (new_ackno > _next_seqno)  // 无效的确认，不可能确认号是 还没发送的字节索引
        return false;

    if (new_ackno > old_ackno) {
        _ackno = new_ackno;
    }
    _sws = window_size;

    // num_of_ack 表示 被确认的字节数量
    uint64_t num_of_ack = _ackno - old_ackno;
    bool stoped= false;
    if (num_of_ack > 0) {

        while (!_outstanding_segs.empty()) {
            TCPSegment &seg = _outstanding_segs.front();
            TCPHeader &hdr = seg.header();

            uint64_t seq = unwrap(hdr.seqno, _isn, _ackno);
            if (seq + seg.length_in_sequence_space() <= _ackno) {
                _outstanding_segs.pop();
                _timer.stop();
                stoped= true;
            } else
                break;
        }
    }
    // 有了确认，窗口发生了滑动，可以发送更多的数据了！！！
    fill_window();

    if (stoped && _outstanding_segs.size() > 0)  // 去掉有错？
        _timer.start(_ackno);
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.expire(ms_since_last_tick)) {  // 当发现 超时的时候，重传_outstanding_segs的tcpsegment

        // if(_sws>0){ 接受窗口>0的时候才重传？
        // 错误，比如对端sender没有需要发送的数据，即使receriver的window恢复成>0,也不再会触发对方通知我方的ack，我方还一直以为对方的window=0

        //
        // 1.触发超时都会 重传数据包，
        // 2.只有对方的sws>0的时候，才会增加重试的计数
        if (_sws > 0) //不是由于对端window=0造成的，才会增加计数。
            _timer.double_mul();  // 从0开始计数

        TCPSegment& seg=_outstanding_segs.front();
        _transmit(seg, false);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    uint32_t x = _timer.get_muls();
    int c = -1;
    while (x) {
        x >>= 1;
        c++;
    }

    return c;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _transmit(seg, false);
}