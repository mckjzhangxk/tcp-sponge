#include "tcp_sponge_socket.hh"

#include "network_interface.hh"
#include "parser.hh"
#include "tun.hh"
#include "util.hh"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

using namespace std;

static constexpr size_t TCP_TICK_MS = 10;

//condition 返回true,函数会一直循环下去，触发 _eventloop.wait_next_event返回EXIT

//_eventloop.wait_next_event返回EXIT: 注册的规则都被删除，或者某个fd发生错误。

//   每次执行 _eventloop.wait_next_event() 后，都会吧 执行的时间戳 通过tcp.tick(),
//   adapter.tick() 通知给 底层组件

// 退出条件 
//  1. condition（）返回false
//  2.local_fd,adapter [IN],[OUT]事件都不可能 被触发(比如都关闭)
//! \param[in] condition is a function returning true if loop should continue
template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::_tcp_loop(const function<bool()> &condition) {
    auto base_time = timestamp_ms();
    while (condition()) {
        auto ret = _eventloop.wait_next_event(TCP_TICK_MS);
        if (ret == EventLoop::Result::Exit or _abort) {
            break;
        }

        if (_tcp.value().active()) {
            const auto next_time = timestamp_ms();//相对于启动的时间的 ms
            _tcp.value().tick(next_time - base_time);//事件从监听到完成的时间
            _datagram_adapter.tick(next_time - base_time); //对于 Ethernet Adapter需要本方法，arp解析 ip是否超时？

            base_time = next_time;
        }
    }
}

//! \param[in] data_socket_pair is a pair of connected AF_UNIX SOCK_STREAM sockets
//! \param[in] datagram_interface is the interface for reading and writing datagrams
template <typename AdaptT>
TCPSpongeSocket<AdaptT>::TCPSpongeSocket(pair<FileDescriptor, FileDescriptor> data_socket_pair,
                                         AdaptT &&datagram_interface)
    : LocalStreamSocket(move(data_socket_pair.first))
    , _thread_data(move(data_socket_pair.second))
    , _datagram_adapter(move(datagram_interface)) {
    _thread_data.set_blocking(false);
}

//  =============================================通信流程的设置 =================================================
// 
// 输出：write(string)：
//                        tcp.active()=T   &&
//                   outbound_stream.remaining_outbound_capacity()>0 &&
//                     _outbound_shutdown=F                                              ________                          segments_out.size()>0
// _thread_data[IN]==================1=========================> outbound_stream   ----> |      | ----> segments_out  ======================2======================> adapter[OUT]
//                                                                                       | TCP  |
//                                                                                       | Conn |
// 输入：string read(n)                                                                   |      |
//                                                                                       |      |                                inbound_stream.size()>0 ||
//                        tcp.active()=T                                                 |      |                          _inbound_shutdown=F && inbound_stream异常 
//      adapter[IN]==================3=========================>segment_received   ----> |______| ----> inbound_stream ======================4=======================> _thread_data[OUT]
// 
//  
// 1. local 输出到 tcp.outbound
//      开放条件： _outbound_shutdown==false && tcp.active()==true && outbound_stream.remaining_outbound_capacity()>0
//      条件说明：
//               _outbound_shutdown==false：表示本地有没有关闭输出==>_thread_data.eof()
//                     tcp.active()==true: 存活才可能输出到远端
//              outbound_stream.remaining_outbound_capacity()>0:  TCP的sender容量不满的时候才可以搬运数据


// 2. tcp.outbound 输出到 adapter
//     开放条件：  segments_out.size()>0
//     条件说明：
//              segments_out()是所有需要发送到remote的TCPSegment,这里如果不为空，说明tcp.byte_in_flight>0,自然tcp.active()是存活的了。

// 3. adapter[IN]= 输出到  inbound_stream
//     开放条件：   tcp.active()==true
//     条件说明：
//              经过 tcp.segment_received()方法后 会进入inbound_stream,这个过程中(处于组装或者就绪状态)，如果inbound_stream的容量不足也没关系，不能被接受的字节会被忽略掉，
//              所以只需要tcp.active()==true这个条件

// 4.inbound_stream 输出到  local
//      开放条件：  inbound_stream.buffer_size()>0 ||  //有数据给上层
//                _inbound_shutdown=false && [ _tcp->inbound_stream().eof() || _tcp->inbound_stream().error() ] // inbound_stream异常 ==>_tcp->inbound_stream().eof() || _tcp->inbound_stream().error()

//     条件说明： 
//              数据要从inbound_stream 搬运到  local, 如果有数据可以搬运，当然应该触发。另一个情况是 这里还没有捕捉到【输入流没被关闭】，但是 inbound_stream已经没有可能再往外输出数据了，所以 
//              需要触发一次这个方法，把 _inbound_shutdown设置成true.
//      

// TCPConnection的I/O方法
// 
//  1.size_t write(string)              // 写入outbound_stream
//  1.1 remaining_outbound_capacity()  // outbound_stream的容量

//  2.void segment_received(TCPSegment);
//  3.queue<TCPSegment>  segments_out()

//  4.ByteStream inbound_stream()
//  4.1 string peek_output(n)     //读取n个字节
//  4.2 void pop_output(n)        //移除n个字节

// Adapter的I/O方法
// 
// std::optional<TCPSegment> read()；
// void write(TCPSegment &seg) ；


//  =============================================Socket内部状态的变换 =================================================

// [A]输出关闭:

//                             1. FIN ,_outbound_shutdown=T,outbound.end_input()
//    _thread_data[IN]   ----------------------------------------------------------->      Remote
//             
//                             2. ACK, _fully_acked=T        
//              Local    <-----------------------------------------------------------      adapter[IN] 


// [B]输入被关闭：

//                             3. FIN, _inbound_shutdown=T，SHUT_WR
//  _thread_data[OUT]    <-----------------------------------------------------------      Remote
//                             4. ACK ， TIMEWAIT
//              Local    ----------------------------------------------------------->      adapter[OUT] 

//  1. local关闭输出：
//   触发条件:_thread_data.eof()
//   触发动作：
//      _outbound_shutdown=true
//      _tcp.end_input_stream()   //起名问题

// 2. 确认  local关闭输出：
//   触发条件：  _thread.eof() && tcp.byte_in_flight()==0
//   触发动作：
//      _fully_acked=true


// 3.Remote关闭输出：
//   触发条件:  _tcp->inbound_stream().eof()==true ||  _tcp->inbound_stream().error()
//   触发动作：
//      _inbound_shutdown=true
//     _thread_data.shutdown(SHUT_WR);
//   说明：
//    1.adapter[IN]事件响应中，如果收到FIN，tcp.segment_received()内部就会调用 inbound_stream.end_input(),从而
//    这个 tcp->inbound_stream().input_ended()返回true

//    2.adapter[IN]事件响应中，如果收到RST，tcp.segment_received()内部就会调用 inbound_stream.set_error(),从而
//     这个 tcp->inbound_stream().error()()返回true

//    3.这里 认为的【关闭inbound】 表示收到 FIN后并且 inbound数据全部被上游取走， 才是_inbound_shutdown=true,而不是
//    只收到FIN就认为【关闭inbound】

// 4.确认  Remote关闭输出：
//   我们无法确认这个ACK是否被对方收到，所以进入到 TIMEWAIT的状态
// 

//建立 tcp的eventloop,实现 _datagram_adapter  <----> _thread_data的数据 通信
// 1.当_datagram_adapter有数据可读的时候，读取一个有效的segment,把这个segment交给 _tcp的receiver， 这相当于有数据来自于网络，先缓存到_receriver中进行组装
// 2.当_thread_data有数据可读的时候，读取数据payload,把数据 交给_tcp.sender, 这相当于app 调用wrire,把数据先缓存到sender中
// 3.当 _thread_data可写的时候，把_tcp 的receiver组装好的数据写入_thread_data，这样app 可以read
// 4._datagram_adapter可写的时候，把 _tcp中需要输出的数据写入_datagram_adapter,这样完成app write
//                  
//                            
// eg:    read(fd,buf,n)
//    _datagram_adapter(IN)-----------> tcp.receiver(tcp.segment_received(seg))----------->_thread_data(OUT)-->read(fd,buf,n)
//    

// eg:    write(fd,buf,n)
//    write(fd,buf,n)->_thread_data(IN)-----------> tcp.sender(tcp.write())  ----------->_datagram_adapter(OUT)
//    


template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::_initialize_TCP(const TCPConfig &config) {
    _tcp.emplace(config);//初始化 TCPConnection
    // Set up the event loop

    // There are four possible events to handle:
    //
    // 1) Incoming datagram received (needs to be given to
    //    TCPConnection::segment_received method)
    //
    // 2) Outbound bytes received from local application via a write()
    //    call (needs to be read from the local stream socket and
    //    given to TCPConnection::data_written method)
    //
    // 3) Incoming bytes reassembled by the TCPConnection
    //    (needs to be read from the inbound_stream and written
    //    to the local stream socket back to the application)
    //
    // 4) Outbound segment generated by TCP (needs to be
    //    given to underlying datagram socket)

    //从 _datagram_adapter读取数据seg， 调用tcp->segment_received(seg)
    // rule 1: read from filtered packet stream and dump into TCPConnection

    // 输入1
    _eventloop.add_rule(_datagram_adapter,
                        Direction::In,
                        [&] {
                            auto seg = _datagram_adapter.read();
                            if (seg) {
                                _tcp->segment_received(move(seg.value()));
                            }

                            // debugging output:
                            if (_thread_data.eof() and _tcp.value().bytes_in_flight() == 0 and not _fully_acked) {
                                cerr << "DEBUG: Outbound stream to "
                                     << _datagram_adapter.config().destination.to_string()
                                     << " has been fully acknowledged.\n";
                                _fully_acked = true;
                            }
                        },
                        [&] { return _tcp->active(); });
    //对于_thread_data（应用程序的数据输入），如果有数据
    // 读取最多_tcp可发送的 字节，  然后调用 _tcp->write.
    //触发条件： _tcp是活动连接，并且 _tcp.sender还可以写数据，并且_outbound_shutdown=false
    // rule 2: read from pipe into outbound buffer
    
    // 输出1
    _eventloop.add_rule(
        _thread_data,
        Direction::In,
        [&] {
            const auto data = _thread_data.read(_tcp->remaining_outbound_capacity());//读取 _tcp sender内部最多容纳的字节数量
            const auto len = data.size();
            const auto amount_written = _tcp->write(move(data));
            if (amount_written != len) {
                throw runtime_error("TCPConnection::write() accepted less than advertised length");
            }

            if (_thread_data.eof()) {//没有输入了
                _tcp->end_input_stream();//相当于sender被关闭
                _outbound_shutdown = true;

                // debugging output:
                cerr << "DEBUG: Outbound stream to " << _datagram_adapter.config().destination.to_string()
                     << " finished (" << _tcp.value().bytes_in_flight() << " byte"
                     << (_tcp.value().bytes_in_flight() == 1 ? "" : "s") << " still in flight).\n";
            }
        },
        [&] { return (_tcp->active()) and (not _outbound_shutdown) and (_tcp->remaining_outbound_capacity() > 0); },
        [&] {
            _tcp->end_input_stream();
            _outbound_shutdown = true;
        });

    // rule 3: read from inbound buffer into pipe
    // _thread_data可写后， 把_tcp->receiver中组装好的数据 写入到_thread_data中。
    // 触发条件：_tcp->receiver有数据组装完毕，  或者_tcp->receiver不能再接受数据(eof()或者error) 但_inbound_shutdown=false
    
    //输入2
    _eventloop.add_rule(
        _thread_data,
        Direction::Out,
        [&] {
            ByteStream &inbound = _tcp->inbound_stream();
            // Write from the inbound_stream into
            // the pipe, handling the possibility of a partial
            // write (i.e., only pop what was actually written).
            const size_t amount_to_write = min(size_t(65536), inbound.buffer_size());
            const std::string buffer = inbound.peek_output(amount_to_write);
            const auto bytes_written = _thread_data.write(move(buffer), false);
            inbound.pop_output(bytes_written);//部分写入，移除已经写入的
            //fprintf(stderr,"poped %ld\n",bytes_written);
            if (inbound.eof() or inbound.error()) {
                _thread_data.shutdown(SHUT_WR);
                _inbound_shutdown = true;

                // debugging output:
                cerr << "DEBUG: Inbound stream from " << _datagram_adapter.config().destination.to_string()
                     << " finished " << (inbound.error() ? "with an error/reset.\n" : "cleanly.\n");
                if (_tcp.value().state() == TCPState::State::TIME_WAIT) {
                    cerr << "DEBUG: Waiting for lingering segments (e.g. retransmissions of FIN) from peer... "<<_datagram_adapter.config().destination.to_string()<<" \n";
                }
            }
        },
        [&] {
            return (not _tcp->inbound_stream().buffer_empty()) or
                   ((_tcp->inbound_stream().eof() or _tcp->inbound_stream().error()) and not _inbound_shutdown); //没有关闭，  但是 底层的 inbound EOF或者Error
        });
    //当_datagram_adapter可写的时候，
    // 不断遍历_tcp的数据segment_out,写入到 _datagram_adapter中
    // 触发 条件是  _tcp->segments_out().empty()==false
    // rule 4: read outbound segments from TCPConnection and send as datagrams
    _eventloop.add_rule(_datagram_adapter,
                        Direction::Out,
                        [&] {
                            while (not _tcp->segments_out().empty()) {
                                _datagram_adapter.write(_tcp->segments_out().front());
                                _tcp->segments_out().pop();
                            }
                        },
                        [&] { return not _tcp->segments_out().empty(); });
}
//调用socketpair()，创建一对相互连接的套接字,type是SOCK_STREAM
//! \brief Call [socketpair](\ref man2::socketpair) and return connected Unix-domain sockets of specified type
//! \param[in] type is the type of AF_UNIX sockets to create (e.g., SOCK_SEQPACKET)
//! \returns a std::pair of connected sockets
static inline pair<FileDescriptor, FileDescriptor> socket_pair_helper(const int type) {
    int fds[2];
    SystemCall("socketpair", ::socketpair(AF_UNIX, type, 0, static_cast<int *>(fds)));
    return {FileDescriptor(fds[0]), FileDescriptor(fds[1])};
}

//! \param[in] datagram_interface is the underlying interface (e.g. to UDP, IP, or Ethernet)
template <typename AdaptT>
TCPSpongeSocket<AdaptT>::TCPSpongeSocket(AdaptT &&datagram_interface)
    : TCPSpongeSocket(socket_pair_helper(SOCK_STREAM), move(datagram_interface)) {}

//当_tcp_thread还在执行的时候，设置abort=true,等待线程的退出
template <typename AdaptT>
TCPSpongeSocket<AdaptT>::~TCPSpongeSocket() {
    try {
        if (_tcp_thread.joinable()) {
            cerr << "Warning: unclean shutdown of TCPSpongeSocket\n";
            // force the other side to exit
            _abort.store(true);
            _tcp_thread.join();
        }
    } catch (const exception &e) {
        cerr << "Exception destructing TCPSpongeSocket: " << e.what() << endl;
    }
}

template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::wait_until_closed() {
    shutdown(SHUT_RDWR);
    if (_tcp_thread.joinable()) {
        cerr << "DEBUG: Waiting for clean shutdown... ";
        _tcp_thread.join();
        cerr << "done.\n";
    }
}
// A. 设置好  _datagram_adapter(网络) 与 _thread_data(应用)之间的 eventloop
// B. _tcp->connect(),发送第一个sned
// C. _tcp_loop([&] { return _tcp->state() == TCPState::State::SYN_SENT; }),知道状态发生变化
// D.启动_tcp_thread
//! \param[in] c_tcp is the TCPConfig for the TCPConnection,  关于 TCPConfig的参数（sender,receriver容量，rt_timeout）
//! \param[in] c_ad is the FdAdapterConfig for the FdAdapter,  adapter中的配置(关于 源和目标地址)
template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::connect(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad) {
    if (_tcp) {
        throw runtime_error("connect() with TCPConnection already initialized");
    }

    _initialize_TCP(c_tcp);

    _datagram_adapter.config_mut() = c_ad;  //设置好目标地址

    cerr << "DEBUG: Connecting to " << c_ad.destination.to_string() << "... ";
    _tcp->connect(); //_tcp.segments_out() 多了一个seg， 从而触发adapter. write(TCPSegment &seg) 

    const TCPState expected_state = TCPState::State::SYN_SENT;

    //TCPState::State 可以通过构造函数 转换成 TCPState
    if (_tcp->state() != expected_state) { 
        throw runtime_error("After TCPConnection::connect(), state was " + _tcp->state().name() + " but expected " +
                            expected_state.name());
    }
    //loop的 条件是  tcp一直处于 syn,send的状态，否则退出loop
    //等待三次握手的完成
    _tcp_loop([&] { return _tcp->state() == TCPState::State::SYN_SENT; });
    cerr << "done.\n";

    _tcp_thread = thread(&TCPSpongeSocket::_tcp_main, this);
}

//! \param[in] c_tcp is the TCPConfig for the TCPConnection， TCPConnection sender,receriver的容量，rt_timeout
//! \param[in] c_ad is the FdAdapterConfig for the FdAdapter , adpter的地址信息
template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::listen_and_accept(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad) {
    if (_tcp) {
        throw runtime_error("listen_and_accept() with TCPConnection already initialized");
    }

    _initialize_TCP(c_tcp);

    _datagram_adapter.config_mut() = c_ad;
    _datagram_adapter.set_listening(true);//当收到第一个syn,设置好 c_ad的 目标ip,port,源ip


    cerr << "DEBUG: Listening for incoming connection... ";
    //等待三次握手的完成，等到 ESTABLISHED状态
    _tcp_loop([&] {
        const auto s = _tcp->state();
        return (s == TCPState::State::LISTEN or s == TCPState::State::SYN_RCVD or s == TCPState::State::SYN_SENT);
    });
    cerr << "new connection from " << _datagram_adapter.config().destination.to_string() << ".\n";

    _tcp_thread = thread(&TCPSpongeSocket::_tcp_main, this);
}

//一个tcpconnection在完成三次握手后 的时间处理。
// 调用_tcp_loop，直到 tcpconnection的状态变成【不活动】才退出
template <typename AdaptT>
void TCPSpongeSocket<AdaptT>::_tcp_main() {
    try {
        if (not _tcp.has_value()) {
            throw runtime_error("no TCP");
        }
        _tcp_loop([] { return true; }); //等待 localstream,获得tunfd关闭
        shutdown(SHUT_RDWR);
        if (not _tcp.value().active()) {//tcp不活动的时候，打印执行完成， 退出状态
            cerr << "DEBUG: TCP connection finished "
                 << (_tcp.value().state() == TCPState::State::RESET ? "uncleanly" : "cleanly.\n");
        }
        _tcp.reset();//清除本tcp connect
    } catch (const exception &e) {
        cerr << "Exception in TCPConnection runner thread: " << e.what() << "\n";
        throw e;
    }
}

//! Specialization of TCPSpongeSocket for TCPOverUDPSocketAdapter
template class TCPSpongeSocket<TCPOverUDPSocketAdapter>;

//! Specialization of TCPSpongeSocket for TCPOverIPv4OverTunFdAdapter
template class TCPSpongeSocket<TCPOverIPv4OverTunFdAdapter>;

//! Specialization of TCPSpongeSocket for TCPOverIPv4OverEthernetAdapter
template class TCPSpongeSocket<TCPOverIPv4OverEthernetAdapter>;

//! Specialization of TCPSpongeSocket for LossyTCPOverUDPSocketAdapter
template class TCPSpongeSocket<LossyTCPOverUDPSocketAdapter>;

//! Specialization of TCPSpongeSocket for LossyTCPOverIPv4OverTunFdAdapter
template class TCPSpongeSocket<LossyTCPOverIPv4OverTunFdAdapter>;

CS144TCPSocket::CS144TCPSocket() : TCPOverIPv4SpongeSocket(TCPOverIPv4OverTunFdAdapter(TunFD("tun144"))) {}

void CS144TCPSocket::connect(const Address &address) {
    TCPConfig tcp_config;
    tcp_config.rt_timeout = 100;

    FdAdapterConfig multiplexer_config;
    multiplexer_config.source = {"169.254.144.9", to_string(uint16_t(random_device()()))};
    multiplexer_config.destination = address;

    TCPOverIPv4SpongeSocket::connect(tcp_config, multiplexer_config);
}

static const string LOCAL_TAP_IP_ADDRESS = "169.254.10.9";
static const string LOCAL_TAP_NEXT_HOP_ADDRESS = "169.254.10.1";

EthernetAddress random_private_ethernet_address() {
    EthernetAddress addr;
    for (auto &byte : addr) {
        byte = random_device()();  // use a random local Ethernet address
    }
    addr.at(0) |= 0x02;  // "10" in last two binary digits marks a private Ethernet address
    addr.at(0) &= 0xfe;

    return addr;
}

FullStackSocket::FullStackSocket()
    : TCPOverIPv4OverEthernetSpongeSocket(TCPOverIPv4OverEthernetAdapter(TapFD("tap10"),
                                                                         random_private_ethernet_address(),
                                                                         Address(LOCAL_TAP_IP_ADDRESS, "0"),
                                                                         Address(LOCAL_TAP_NEXT_HOP_ADDRESS, "0"))) {}

void FullStackSocket::connect(const Address &address) {
    TCPConfig tcp_config;
    tcp_config.rt_timeout = 100;

    FdAdapterConfig multiplexer_config;
    multiplexer_config.source = {LOCAL_TAP_IP_ADDRESS, to_string(uint16_t(random_device()()))};
    multiplexer_config.destination = address;

    TCPOverIPv4OverEthernetSpongeSocket::connect(tcp_config, multiplexer_config);
}
