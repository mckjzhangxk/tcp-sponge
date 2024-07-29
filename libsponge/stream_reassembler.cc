#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), 
                                                            _capacity(capacity),
                                                            buffer(capacity),
                                                            buffer_valid(capacity),
                                                            accept_index(0) {

    _last_index=0xffffffffffffffff;

}

void StreamReassembler::_pop(size_t n) {
    for (size_t k = 0; k < n; k++){
                buffer.pop_front();
                buffer_valid.pop_front();
    }
    buffer.resize(_capacity);
    buffer_valid.resize(_capacity);
}

void StreamReassembler::_push(const string &data, const uint64_t index){
        if (index>=_capacity)
            return;
        
        size_t sz=index+data.size();
        if (sz>_capacity)
            sz=_capacity;
        
        
        for (size_t k = 0; k <sz; k++)
        {
           buffer[index+k]=data[k];
           buffer_valid[index+k]=1;
        }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if (data.size()==0)
    {
       return;
    }
    if(eof){
        _last_index=index+data.size();
    }
    if (index<=accept_index)//表示可接受的逻辑
    {
    
        if (accept_index>index+data.size()-1)//表示data已经被接受了
        {
            return;
        }
        size_t i=accept_index-index; //i 是accept_index 对应 data的索引
        size_t n=_output.write( data.substr(i));
        _pop(n);
        accept_index+=n;

        if(n<data.size()){
            _push(data.substr(n),0);
            return;
        }
        if (accept_index==_last_index)
        {
            _output.end_input();
        }
        
        size_t len=0;//计算最多有多少个 un assem的准备好了，可以被写出了
        for (size_t k = 0; k < buffer_valid.size(); k++){
            if (buffer_valid[k])
                len++;
            else
                break;
        }
        if (len)
        {
            n=_output.write(string().assign(buffer.begin(),buffer.begin()+len));    
            _pop(n);
            accept_index+=n;
                    if (accept_index==_last_index)
            {
                _output.end_input();
            }
        }
        
    }else{
        size_t i=index-accept_index;//data  缓存在i 位置
       _push(data,i);
    }
    
    
}

size_t StreamReassembler::unassembled_bytes() const { 
    
    size_t r=0;
    for (size_t i =0; i < buffer_valid.size(); i++)
    {
        if (buffer_valid[i])
        {
            r++;
        }
        
    }
    
    return r; 
}

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }
