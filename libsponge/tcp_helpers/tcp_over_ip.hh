#ifndef SPONGE_LIBSPONGE_TCP_OVER_IP_HH
#define SPONGE_LIBSPONGE_TCP_OVER_IP_HH

#include "buffer.hh"
#include "fd_adapter.hh"
#include "ipv4_datagram.hh"
#include "tcp_segment.hh"

#include <optional>
//   IPV4Adapter 提供以下功能： 两种数据结构的转换: InternetDatagram  <=====>  TCPSegment
//   
//   1.对于接收
//    InternetDatagram ---->  [unwrap_tcp_in_ip] ---> TCPSegment
// 
//   2.对于发送
//       TCPSegment    ---->   [wrap_tcp_in_ip]  ---> InternetDatagram
// 
//! \brief A converter from TCP segments to serialized IPv4 datagrams
class TCPOverIPv4Adapter : public FdAdapterBase {
  public:
    //把一个ip数据包 转换成 一个TCP数据包(如果 src/dst ip,port校验完成) ，相当于接收
    std::optional<TCPSegment> unwrap_tcp_in_ip(const InternetDatagram &ip_dgram);
    //把一个TCP数据包，转换成 一个Ip数据包 ，相当于发送
    InternetDatagram wrap_tcp_in_ip(TCPSegment &seg);
};

#endif  // SPONGE_LIBSPONGE_TCP_OVER_IP_HH
