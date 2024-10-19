#ifndef SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
#define SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH

#include "ethernet_header.hh"
#include "network_interface.hh"
#include "tun.hh"

#include <optional>
#include <unordered_map>
#include <utility>

//                                        _____________________
//                 (InternetDatagram)     |                    |
//  _tun.read()  ---------------------->  | unwrap_tcp_in_ip() | ------>  TCPSegment read() 
//                                        |____________________|

//                          _____________________
//     |                    |                    |   (InternetDatagram)
//  write(TCPSegment) -->   | wrap_tcp_in_ip()   | --------------------->   _tun.write()
//                          |____________________|

// 在 具备 TCPSegment与 InternetDatagram转换能力的基础之上， 加入了 tun设备
// 从而借助 tun设备的read,write 实现Internet Layer 层的数据传输。
// 
//   如果看作接口， 提供对一个TCPSegment的读和写，将IP层隐藏起来了
//     void write(TCPSegment &seg) ；
//     std::optional<TCPSegment> read()；

// TCPOverIPv4Adapter
//! \brief A FD adapter for IPv4 datagrams read from and written to a TUN device
class TCPOverIPv4OverTunFdAdapter : public TCPOverIPv4Adapter {
  private:
    TunFD _tun;

  public:
    //! Construct from a TunFD
    explicit TCPOverIPv4OverTunFdAdapter(TunFD &&tun) : _tun(std::move(tun)) {}

    //! Attempts to read and parse an IPv4 datagram containing a TCP segment related to the current connection
    std::optional<TCPSegment> read() {
        InternetDatagram ip_dgram;
        if (ip_dgram.parse(_tun.read()) != ParseResult::NoError) {
            return {};
        }
        return unwrap_tcp_in_ip(ip_dgram);
    }

    //! Creates an IPv4 datagram from a TCP segment and writes it to the TUN device
    void write(TCPSegment &seg) { _tun.write(wrap_tcp_in_ip(seg).serialize()); }

    //! Access the underlying TUN device
    operator TunFD &() { return _tun; }

    //! Access the underlying TUN device
    operator const TunFD &() const { return _tun; }
};

//! Typedef for TCPOverIPv4OverTunFdAdapter
using LossyTCPOverIPv4OverTunFdAdapter = LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;



// TCPSegment read():
//                               _______________                      ————————————————————
//              EthernetFrame    |             |  InternetDatagram   |                    |
//_tap.read() ---------------->  |   NIC       |-------------------> | TCPOverIPv4Adapter |---->  TCPSegment
//                               |_____________|         |            |____________________|
//                                  recv_frame           |
//                                                       |   ArpMessage
//                                                       |___________________________________________________
//                                                                                                           |
//                                                                                                           |  
//                                                                                                           | 触发
// //  write(TCPSegment &seg);                                                                               |
// //                                                                                                        |
//                ____________________                       ______________                             _____________
//                |                   |   InternetDatagram  |              |  EthernetFrame(Ipv4|Arp)   |           |   delay
// TCPSegment --> | TCPOverIPv4Adapter|  -----------------> |   NIC        |--------------------------> | NIC queue |--------->  _tap.write()
//                |___________________|                     |______________|                            |___________|
// 
//                                                    send_datagram(dgram, next_hop) 
// 
// 
// 在 具备A. TCPSegment与 InternetDatagram转换(TCPOverIPv4Adapter)
//       B  InternetDatagram与 EthernetFrame互相转换(NIC)
//  的基础之上 ， 加入了 tap设备 ,从而借助 tap设备的read,write 实现Link Layer 层的数据传输。
// 
//   如果看作接口， 提供对一个TCPSegment的读和写，将IP层隐藏起来了
//     void write(TCPSegment &seg) ；
//     std::optional<TCPSegment> read()；

//! \brief A FD adapter for IPv4 datagrams read from and written to a TAP device
class TCPOverIPv4OverEthernetAdapter : public TCPOverIPv4Adapter {
  private:
    TapFD _tap;  //!< Raw Ethernet connection，_tap上读取的是以太网的数据
    //由 local mac addr 和ip addr组成
    NetworkInterface _interface;  //!< NIC abstraction

    Address _next_hop;  //!< IP address of the next hop

    void send_pending();  //!< Sends any pending Ethernet frames,调用_tap.write( _interface.frame_out()[i] );

  public:
    //! Construct from a TapFD
    explicit TCPOverIPv4OverEthernetAdapter(TapFD &&tap,
                                            const EthernetAddress &eth_address,
                                            const Address &ip_address,
                                            const Address &next_hop);
    //! Attempts to read and parse an Ethernet frame containing an IPv4 datagram that contains a TCP segment
    std::optional<TCPSegment> read();

    //! Sends a TCP segment (in an IPv4 datagram, in an Ethernet frame).
    void write(TCPSegment &seg);

    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! Access the underlying raw Ethernet connection
    operator TapFD &() { return _tap; }

    //! Access the underlying raw Ethernet connection
    operator const TapFD &() const { return _tap; }
};

#endif  // SPONGE_LIBSPONGE_TUNFD_ADAPTER_HH
