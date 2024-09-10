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
                                                            _accept_index(0),
                                                            _eof_index(-1),
                                                            _blocks(std::list<Block>{}),
                                                            _block_sz(0){
                                       
}

// data 由于以下原因 不具备 写入 ByteStream的条件
// A. ByteStream 容量满了(index==_accept_index)
// B. index与 ByteStream 中的seq不连续(index>_accept_index)
// 所以 本方法是把这些 无法进入ByteStream 缓存到 StreamReassembler中
void StreamReassembler::_push(const string &data, uint64_t start_index){
        if(data.size()==0)
            return;
        const char * _data_pointer=data.data();
        size_t data_sz=data.size();

        if(start_index<_accept_index){
            if (_accept_index>=start_index+data_sz)//表示全部被接收完成，无需放在reassembler中了
            {
                return;
            }
            _data_pointer+=(_accept_index-start_index);
            data_sz-=(_accept_index-start_index);
            start_index=_accept_index;

        }
        uint64_t capacity_end_index=_accept_index+_capacity;

        if (start_index>=capacity_end_index){//reassembler 容量不足，无法放置多余字节
            return;
        }
        
        uint64_t end_index=  min(start_index+data_sz,capacity_end_index);
        
        
        
        std::string_view dv={_data_pointer,end_index-start_index};//存放的数据

        std::list<Block>::iterator it ;

        // x=<start_index,end_index,dv> 是本次需要缓存的内容

        // 本次循环的目的是找到一个"可以合并"的单元it，把 x合并到it中
        // "可以合并"是指 start_index 在 [it.start_index,it.end_index]区间范围内
        //  合并过程： 1. dv[it.end_index-index] 追加到 it.data后
        //           2.  it.end_index=end_index

        //如果上面的 匹配对所有的_block都不成立，是由于以下之一的原因造成的
        //1.index 小于 _block中 【最小 起始索引】,此时x转换成一个BLOCK，插入到 _block的最前头
        //2. index 大于  _block中【最大 终止索引】，插入到 _block的最后头

        //A._block是一个 有序，不连续的 "块组",下面的 for,位置有序的 特点，
        // 临时破坏 块直接的 "不连续" 
        for (it = _blocks.begin(); it != _blocks.end(); ++it) {
            if(  it->start_index<=start_index ){

                    if(start_index<=it->end_index){
                        if(end_index> it->end_index){
                            _block_sz+=end_index-it->end_index;

                            uint64_t subIndex=it->end_index-start_index;

                            it->data.append(dv.substr(subIndex));
                            it->end_index=end_index;
                        }
    
                        break;
                    }
                    

            }else if (start_index<it->start_index){
                struct Block b{start_index,end_index,std::string{dv}};
                _blocks.insert(it,b);
                it--;
                _block_sz+=b.data.size();
                break;
            }
            
        }
        
        //B.同和把front与后面的it 进行合并，重新恢复不连续的特性
        if (it!=_blocks.end())//此时，it.startindex<(it+1).start,但是it.endindex可能 大于(it+1).start
        {
            //经过之前的匹配，it是经过<start_index,end_index,dv>合并后的元素
            // it后面还可能存在 联系"满足与it 合并"的元素
            //"满足与it 合并"是指：  (it+1).start_index   <=it.end_index
            
            //合并的策略是， 把it+1 的<start_index,end_index,data> 合并入it,从_block中删除 it+1, 
            //如此反复，指定 "合并条件不成立"

            //合并的方法 it+1 <start_index,end_index,data>

            //1. 把  (it+1).data[it.end_index-(it+1).start_index] 追加到it.data
            //2. it.end_index =(it+1).end_index

            std::list<Block>::iterator front=it;
            
            while(++it != _blocks.end()){
                if( front->end_index >= it->start_index ){
                     _block_sz-=(front->data.size()+it->data.size());
                    uint64_t subIndex=front->end_index-it->start_index;

                    if(subIndex<it->data.size()){//必须的判断，有可能front的数据 完全覆盖it的数据，不需要做多余的合并
                        front->data.append(it->data.substr(subIndex));
                        front->end_index=it->end_index;
                    }


                    _block_sz+=front->data.size();

                    _blocks.erase(it);
                    it=front;
           
                }else{
                    break;
                }    
            }   

        }else{
            struct Block b{start_index,end_index,std::string{dv}};
            _blocks.emplace_back(b);
            _block_sz+=b.data.size();
        }
        
}


std::optional<StreamReassembler::Block> StreamReassembler::_pop(){
    std::optional<Block> r;

    if (_blocks.size()>0){
        Block& b=_blocks.front();
        
        
        if(b.start_index>_accept_index){
                return r;
        }
        r=b;
        _blocks.pop_front();
        _block_sz-=r->data.size();

                            
        if(b.end_index>_eof_index){// 之前加入的block中的字节如果索引 大于_eof_index??
            r->end_index=_eof_index;
            r->data=r->data.substr(0,_eof_index-_accept_index);
        }


        return r;
        
    }
    
    return r;
 }
bool StreamReassembler::_accept_bytes(){
     _accept_index=_output.bytes_written();

    if (_accept_index==_eof_index){
            _output.end_input();
            return true;
    }else{
        return false;
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


void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    if(eof){
        _eof_index=index+data.size();
    }

    if (data.size()==0&&_accept_bytes()){
        return;
    }

     _push(data,index);
    auto r=_pop();
    if(r.has_value()){
        size_t n=_output.write(r->data);
        if (_accept_bytes())
            return;
        if(n<r->data.size()){
            _push(r->data.substr(n),_accept_index);
        }
    }    
}  
        

size_t StreamReassembler::unassembled_bytes() const { 
    return _block_sz; 
}

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }
