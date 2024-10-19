#ifndef SPONGE_LIBSPONGE_SOCKET_HH
#define SPONGE_LIBSPONGE_SOCKET_HH

#include "address.hh"
#include "file_descriptor.hh"

#include <cstdint>
#include <functional>
#include <string>
#include <sys/socket.h>

// Socket是FileDescriptor， 在此基础之上，有了 一对Address.
// 而且还有了 connect,bind,shutdown,set_reuseaddr,setsockopt 方法
//             _________            ________________
//  domain -->|         |  (fd）    |                | --- read
//  type   -->| syscall | ------>   | FileDescriptor | --- write
//            |_________|           |________________| --- close,eof()
//                                  |                |
//                                  |                | --- connect
//                                  |   SOCKET       | --- bind
//                                  |                | --- local_address,peer_address
//                                  |                | --- shutdown
//                                  |________________| --- set_reuseaddr,setsockopt
//! \brief Base class for network sockets (TCP, UDP, etc.)
//! \details Socket is generally used via a subclass. See TCPSocket and UDPSocket for usage examples.
class Socket : public FileDescriptor {
  private:
    //通过调用 function(fd_num(),sockaddr *, socklen_t *),获得 sockaddr地址，转换成Address 
    //! Get the local or peer address the socket is connected to
    Address get_address(const std::string &name_of_function,
                        const std::function<int(int, sockaddr *, socklen_t *)> &function) const;

  protected:
    //调用socket 创建一个sock_fd
    //! Construct via [socket(2)](\ref man2::socket)
    Socket(const int domain, const int type);
    //验证 fd 是否 是 domain,type
    //! Construct from a file descriptor.
    Socket(FileDescriptor &&fd, const int domain, const int type);

    //! Wrapper around [setsockopt(2)](\ref man2::setsockopt)
    template <typename option_type>
    void setsockopt(const int level, const int option, const option_type &option_value);

  public:
    //同系统调用bind
    //! Bind a socket to a specified address with [bind(2)](\ref man2::bind), usually for listen/accept
    void bind(const Address &address);

    //同系统调用connect
    //! Connect a socket to a specified peer address with [connect(2)](\ref man2::connect)
    void connect(const Address &address);

    //底层的shutdown方法， 更具how来更新 读写次数
    //! Shut down a socket via [shutdown(2)](\ref man2::shutdown)
    void shutdown(const int how);

    // 同 getsockname(fd_num(),..)系统调用
    //! Get local address of socket with [getsockname(2)](\ref man2::getsockname)
    Address local_address() const;
    // 同 getsockname(fd_num(),..)系统调用
    //! Get peer address of socket with [getpeername(2)](\ref man2::getpeername)
    Address peer_address() const;
    //底层的reuse
    //! Allow local address to be reused sooner via [SO_REUSEADDR](\ref man7::socket)
    void set_reuseaddr();
};

//! A wrapper around [UDP sockets](\ref man7::udp)
class UDPSocket : public Socket {
  protected:
    //根据fd创建 UDPSocket，验证fd是否是 <AF_INET, SOCK_DGRAM>
    //! \brief Construct from FileDescriptor (used by TCPOverUDPSocketAdapter)
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit UDPSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_INET, SOCK_DGRAM) {}

  public:
    //! Default: construct an unbound, unconnected UDP socket
    UDPSocket() : Socket(AF_INET, SOCK_DGRAM) {}

    //地址与数据
    //! Returned by UDPSocket::recv; carries received data and information about the sender
    struct received_datagram {
        Address source_address;  //!< Address from which this datagram was received
        std::string payload;     //!< UDP datagram payload
    };

    //! Receive a datagram and the Address of its sender
    received_datagram recv(const size_t mtu = 65536);
    
    //读取数据到datagram.payload中，地址到datagram.source_address中
    // payload的大小不会超过mtu
    //! Receive a datagram and the Address of its sender (caller can allocate storage)
    void recv(received_datagram &datagram, const size_t mtu = 65536);

    //发生数据payload,到对端的地址
    // 如果发送的字节不等于payload.size()，抛出异常
    //! Send a datagram to specified Address
    void sendto(const Address &destination, const BufferViewList &payload);

    //发送数据， 对方的地址应该绑定在fd上了
    // 如果发送的字节不等于payload.size()，抛出异常
    //! Send datagram to the socket's connected address (must call connect() first)
    void send(const BufferViewList &payload);
};

//! \class UDPSocket
//! Functions in this class are essentially wrappers over their POSIX eponyms.
//!
//! Example:
//!
//! \include socket_example_1.cc

// TCPSocket 是 Socket，在此基础之上，多了listen,accept方法
// 针对 Socket构造函数的两个 参数domain,type,分别设置成AF_INET, SOCK_STREAM
//! A wrapper around [TCP sockets](\ref man7::tcp)
class TCPSocket : public Socket {
  private:
    //! \brief Construct from FileDescriptor (used by accept())
    //! \param[in] fd is the FileDescriptor from which to construct
    explicit TCPSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_INET, SOCK_STREAM) {}

  public:
    //! Default: construct an unbound, unconnected TCP socket
    TCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

    //! Mark a socket as listening for incoming connections
    void listen(const int backlog = 16);

    //! Accept a new incoming connection
    TCPSocket accept();
};

//! \class TCPSocket
//! Functions in this class are essentially wrappers over their POSIX eponyms.
//!
//! Example:
//!
//! \include socket_example_2.cc

// LocalStreamSocket是unix socket
//! A wrapper around [Unix-domain stream sockets](\ref man7::unix)
// 针对 Socket构造函数的两个 参数domain,type,分别设置成AF_UNIX, SOCK_STREAM
class LocalStreamSocket : public Socket {
  public:
    //! Construct from a file descriptor
    explicit LocalStreamSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_UNIX, SOCK_STREAM) {}
};

//! \class LocalStreamSocket
//! Example:
//!
//! \include socket_example_3.cc

#endif  // SPONGE_LIBSPONGE_SOCKET_HH
