#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t x= static_cast<uint32_t>(n)+isn.raw_value();
    return WrappingInt32(x);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {

    uint64_t C32=(1ul<<32);
    uint64_t C31=(1ul<<31);
    
    uint32_t nr=n.raw_value();
    uint32_t ir=isn.raw_value();
    
    uint32_t offset=nr>=ir?nr-ir:nr+ (C32-ir);

    
    // checkpoint= a+k*(2**32)
    uint64_t checkpoint_a=checkpoint&0xffffffff;
    // uint64_t checkpoint_k=checkpoint>>32;

     
    int64_t diff=checkpoint_a-offset;
    int64_t r =checkpoint-diff;

    if(diff> C31){
        r+=C32;
    }else if( -diff>=C31 && r>C32){
        r-=-C32;
    }
    return r;
}


// uint64_t unwrap1(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    
//     WrappingInt32 n1=wrap(checkpoint,isn);
//     int64_t diff=n-n1,diff1,r;
//     uint64_t pos_diff=0;
//     if (diff<0)
//     {
//         diff1=diff+(1ul<<32);
//         pos_diff=diff1;
//     }else if(diff>0){
//         diff1=diff-(1ul<<32);
//         pos_diff=diff;
//     }else{
//         diff1=0;
//     }
    
//     if(abs(diff)>abs(diff1))
//         r=diff1;
//     else
//         r=diff;

//     if(r>=0){
//     	return checkpoint+diff;

//     }else if(checkpoint>=static_cast<uint64_t>(-r) ){
//         return checkpoint-static_cast<uint64_t>(-r);
//     } else{
//         return checkpoint+pos_diff;
//     }
// }
