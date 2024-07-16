#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):m_head(0),m_tail(0),m_closed(false) {
     m_buf_size=capacity+1;
     m_buf=new char[m_buf_size];
}

size_t ByteStream::write(const string &data) {
    if (m_closed||_error)
    {
        set_error();
        return 0;
    }
    
    size_t n=data.size();
    size_t r=remaining_capacity();
    n=n>r?r:n;

    for (size_t i = 0; i < n; i++)
    {
        m_buf[m_head++]=data[i];
        if (m_head>=m_buf_size)
        {
            m_head=0;
        }
        
    }

    m_num_of_w+=n;
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (_error)
    {
        return "";
    }
    
    string s;

    size_t r=buffer_size();
    size_t n=len>r?r:len;

    s.reserve(n);

    int t=m_tail;
    for (size_t i = 0; i < n; i++)
    {
        s[i]=m_buf[t++];
        if (t>=m_buf_size)
        {
            t=0;
        }
        
    }
    
    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    if (_error)
    {
        return;
    }
    

    size_t r=buffer_size();
    size_t n=len>r?r:len;
    
    m_tail+=n;
    if (m_tail>=m_buf_size){
            m_tail-=m_buf_size;
     }
    m_num_of_r+=n;
}

void ByteStream::end_input() {
    m_closed=true;
}

bool ByteStream::input_ended() const { 
    return m_closed;
}

size_t ByteStream::buffer_size() const { 
    int r=m_head-m_tail;
    if (r<0)
    {
        r+=m_buf_size;
    }
    
    return 0;
 }

bool ByteStream::buffer_empty() const { 
    return m_head==m_tail;
 }

bool ByteStream::eof() const { return input_ended()&&buffer_empty(); }

size_t ByteStream::bytes_written() const { return m_num_of_w; }

size_t ByteStream::bytes_read() const { return m_num_of_r; }

size_t ByteStream::remaining_capacity() const { 

    return m_buf_size-1-buffer_size(); 
}
