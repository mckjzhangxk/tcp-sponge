#include "stream_reassembler.hh"

#include <iostream>

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
                                                            accept_index(0),
                                                            _last_index(-1) {


    for (size_t i = 0; i < _capacity; i++)
    {
        buffer_valid[i]=0;
    }
    
                                       
}

void StreamReassembler::_pop(size_t n) {
    for (size_t k = 0; k < n; k++){
                buffer.pop_front();
                buffer_valid.pop_front();
    }
    buffer.resize(_capacity,0);
    buffer_valid.resize(_capacity,0);
}

void StreamReassembler::_push(const string &data, const uint64_t index){
        if (index>=_capacity){
            std::cout<<"ccc"<<std::endl;
            return;
        }


        size_t lastIndex=index+data.size();
        if (lastIndex>_capacity){
            std::cout<<"ddd"<<std::endl;
            lastIndex=_capacity;
        }

        
        
        for (size_t k = index; k <lastIndex; k++)
        {
           buffer[k]=data[k-index];
           buffer_valid[k]=1;
        }
}

// List of steps that executed successfully:
// 	Initialized
// 	Action:      substring submitted with data "b", index `1`, eof `0`
// 	Expectation: bytes in stream = ""
// 	Action:      substring submitted with data "ab", index `0`, eof `0`
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

// Failure message:
// 	The reassembler was expected to have `2` total bytes assembled, but there were `3`

// List of steps that executed successfully:
// 	Initialized
// 	Action:      substring submitted with data "b", index `1`, eof `0`
// 	Action:      substring submitted with data "a", index `0`, eof `0`
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if(eof){
        _last_index=index+data.size();
    }

    if (data.size()==0)
    {
        if (accept_index==_last_index)
        {
            _output.end_input();
        }
      return;
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


        if (accept_index==_last_index)
        {
            _output.end_input();
            return;
        }

        if(n<data.size()){
            _push(data.substr(n),0);
            return;
        }

        
        size_t len=0;//计算最多有多少个 un assem的准备好了，可以被写出了
        for (size_t k = 0; k < _capacity; k++){
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
        size_t o=unassembled_bytes();

        size_t i=index-accept_index;//data  缓存在i 位置
       _push(data,i);
       size_t n=unassembled_bytes();
       std::cout<<n-o<<" vs "<<data.size()<<std::endl;
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
