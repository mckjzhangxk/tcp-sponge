#include "tcp_state.hh"

using namespace std;
//返回receiver的字符串状态
// error:底层 bytestreqam有error
// FIN: 收到FIN 数据包
// LISTEN: 没有ackno（没收到isn）
// SYN:数据传输开始
string TCPState::state_summary(const TCPReceiver &receiver) {
    if (receiver.stream_out().error()) {
        return TCPReceiverStateSummary::ERROR;
    } else if (not receiver.ackno().has_value()) {
        return TCPReceiverStateSummary::LISTEN;
    } else if (receiver.stream_out().input_ended()) {
        return TCPReceiverStateSummary::FIN_RECV;
    } else {
        return TCPReceiverStateSummary::SYN_RECV;
    }
}

string TCPState::state_summary(const TCPSender &sender) {
    if (sender.stream_in().error()) {
        return TCPSenderStateSummary::ERROR;
    } else if (sender.next_seqno_absolute() == 0) {
        return TCPSenderStateSummary::CLOSED;
    } else if (sender.next_seqno_absolute() == sender.bytes_in_flight()) {
        return TCPSenderStateSummary::SYN_SENT;
    } else if (not sender.stream_in().eof()) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.next_seqno_absolute() < sender.stream_in().bytes_written() + 2) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.bytes_in_flight()) {
        return TCPSenderStateSummary::FIN_SENT;
    } else {
        return TCPSenderStateSummary::FIN_ACKED;
    }
}
