#ifndef SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
#define SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH

#include "fd_adapter.hh"
#include "file_descriptor.hh"
#include "lossy_fd_adapter.hh"
#include "tcp_segment.hh"

#include <optional>
#include <utility>

//FileDescriptor提供read,write方法用于获得数据包
// FdAdapterBase
//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter : public FdAdapterBase, public FileDescriptor {
  public:
    //! Construct from a TunFD sliced into a FileDescriptor
    explicit TCPOverIPv4OverTunFdAdapter(FileDescriptor &&fd) : FileDescriptor(std::move(fd)) {}

    //! Attempts to read and parse an IPv4 datagram containing a TCP segment related to the current connection
    //  读 Ip,TCP,校验数据包的有消息
    // 如果当前是listen状态， 读取数据包的 源/目的 ip， 源端口， 记录到config()中，把状态变成非listen
    // 如果乘法，返回解析好的TCPSegment
    std::optional<TCPSegment> read();

    //在seg的基础之上，封装好ip数据包，序列化完成后使用 FileDescriptor发送出去
    //! Creates an IPv4 datagram from a TCP segment and writes it to the TUN device
    void write(TCPSegment &seg);
};

//! Typedef for TCPOverIPv4OverTunFdAdapter
using LossyTCPOverIPv4OverTunFdAdapter = LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;

#endif  // SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
