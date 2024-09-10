#include "tcp_state.hh"

using namespace std;

//返回receiver的字符串状态
// error:底层 bytestreqam有error
// FIN: 收到FIN 数据包
// LISTEN: 没有ackno（没收到isn）
// SYN:数据传输开始


bool TCPState::operator==(const TCPState &other) const {
    return _active == other._active and _linger_after_streams_finish == other._linger_after_streams_finish and
           _sender == other._sender and _receiver == other._receiver;
}

bool TCPState::operator!=(const TCPState &other) const { return not operator==(other); }

string TCPState::name() const {
    return "sender=`" + _sender + "`, receiver=`" + _receiver + "`, active=" + to_string(_active) +
           ", linger_after_streams_finish=" + to_string(_linger_after_streams_finish);
}

TCPState::TCPState(const TCPState::State state) {
    switch (state) {
        case TCPState::State::LISTEN:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::CLOSED;
            break;
        case TCPState::State::SYN_RCVD:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::SYN_SENT:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::ESTABLISHED:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            break;
        case TCPState::State::CLOSE_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            _linger_after_streams_finish = false;// 不用wait-time,因为随后发出的FIN会携带对本次收到FIN的确认
            break;
        case TCPState::State::LAST_ACK:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            _linger_after_streams_finish = false;// 不用wait-time,因为随后发出的FIN会携带对本次收到FIN的确认
            break;
        case TCPState::State::CLOSING:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_1:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_2:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::TIME_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::RESET:
            _receiver = TCPReceiverStateSummary::ERROR;
            _sender = TCPSenderStateSummary::ERROR;
            _linger_after_streams_finish = false;//RST直接关闭TCP，而且不用TIME-WAIT
            _active = false;
            break;
        case TCPState::State::CLOSED:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            _linger_after_streams_finish = false;
            _active = false;
            break;
    }
}

TCPState::TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger)
    : _sender(state_summary(sender))
    , _receiver(state_summary(receiver))
    , _active(active)
    , _linger_after_streams_finish(active ? linger : false) {}


string TCPState::state_summary(const TCPReceiver &receiver) {
    if (receiver.stream_out().error()) {
        return TCPReceiverStateSummary::ERROR;
    } else if (not receiver.ackno().has_value()) {//没有收到syn, 表示LISTEN
        return TCPReceiverStateSummary::LISTEN;
    } else if (receiver.stream_out().input_ended()) {//已经被终止，说明收到FIN,是fin_recv
        return TCPReceiverStateSummary::FIN_RECV;
    } else {
        return TCPReceiverStateSummary::SYN_RECV;//收到syn,没收到fin,所以是syn_recv
    }
}

string TCPState::state_summary(const TCPSender &sender) {
    if (sender.stream_in().error()) {
        return TCPSenderStateSummary::ERROR;
    } else if (sender.next_seqno_absolute() == 0) {//next_seq=0,说明没有发生syn,所以是 closed
        return TCPSenderStateSummary::CLOSED;

    } else if (sender.next_seqno_absolute() == sender.bytes_in_flight()) {//next_seq>0,_ackno=0，等价于 next_seq=next_seq-_ackno
        return TCPSenderStateSummary::SYN_SENT;

    } else if (not sender.stream_in().eof()) {//是不是多余？
        return TCPSenderStateSummary::SYN_ACKED;

    } else if (sender.next_seqno_absolute() < sender.stream_in().bytes_written() + 2) {//+2（SYN+FIN），这说明没有发送FIN
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.bytes_in_flight()) {//发送了FIN,但是还有 【没有被确认】的字节
        return TCPSenderStateSummary::FIN_SENT;
    } else {
        return TCPSenderStateSummary::FIN_ACKED;
    }
}
