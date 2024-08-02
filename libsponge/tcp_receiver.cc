#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::contain(uint16_t xlow,uint16_t xhigh,uint16_t ylow,uint16_t yhigh){
    return !(ylow>=xhigh ||yhigh<= xlow);
}
bool TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& hdr=seg.header();


    uint16_t start_window_index=0;
    // payload_size + SYN(1) + FIN(1)
    start_window_index=_reassembler.stream_out().bytes_written()+_isn.has_value()+_reassembler.stream_out().input_ended();

    uint16_t end_windex_index=start_window_index+window_size();
    if (start_window_index==end_windex_index)
    {
        end_windex_index++;
    }

    if (hdr.syn)
        _isn=hdr.seqno;

    if(!_isn.has_value())
        return false;

    uint64_t start_abs_index=unwrap(hdr.seqno, *_isn, start_window_index);
    uint16_t end_abs_index=start_abs_index+seg.length_in_sequence_space();
    if (start_abs_index==end_abs_index)
    {
        end_abs_index++;
    }



    uint16_t stream_index=start_abs_index?start_abs_index-1:0;

    auto& payload=seg.payload();
    _reassembler.push_substring(payload.copy(),stream_index,hdr.fin);


    return contain(start_window_index,end_windex_index,start_abs_index,end_abs_index);
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
