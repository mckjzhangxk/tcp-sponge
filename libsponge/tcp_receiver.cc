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
    

    if (hdr.syn)
    {
        if(_isn.has_value()){
            return false;
        }
        _isn=hdr.seqno;
        _last_accept_abs_index=0;
    }
    if(!_isn.has_value())
        return false;
    
    
    
    uint64_t abs_index=unwrap(hdr.seqno, *_isn, _last_accept_abs_index);
    uint16_t last_abs_index=abs_index+seg.length_in_sequence_space();
    if (abs_index==last_abs_index)
    {
       last_abs_index++;
    }

    uint16_t stream_index=abs_index?abs_index-1:0;

    auto& payload=seg.payload();
    _reassembler.push_substring(payload.copy(),stream_index,hdr.fin);

    
    
    uint16_t start_window_index=_last_accept_abs_index+1;
    uint16_t last_windex_index=start_window_index+window_size();

    if (start_window_index==last_windex_index)
    {
        last_windex_index++;
    }
    
    
    _last_accept_abs_index=_reassembler.stream_out().bytes_written()-1+1;


    return contain(abs_index,last_abs_index,start_window_index,last_windex_index);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    
    optional<WrappingInt32> r;

    if(_isn.has_value()){
        r=wrap(_last_accept_abs_index+1,*_isn);
    }
    return r; 
}

size_t TCPReceiver::window_size() const { 
    return _capacity-_reassembler.stream_out().buffer_size(); 
}
