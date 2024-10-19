#include "tcp_over_ip.hh"

#include "ipv4_datagram.hh"
#include "ipv4_header.hh"
#include "parser.hh"

#include <arpa/inet.h>
#include <stdexcept>
#include <unistd.h>
#include <utility>

using namespace std;

//! \details This function attempts to parse a TCP segment from
//! the IP datagram's payload.
//!
//! If this succeeds, it then checks that the received segment is related to the
//! current connection. When a TCP connection has been established, this means
//! checking that the source and destination ports in the TCP header are correct.
//!
//! If the TCP connection is listening (i.e., TCPOverIPv4OverTunFdAdapter::_listen is `true`)
//! and the TCP segment read from the wire includes a SYN, this function clears the
//! `_listen` flag and records the source and destination addresses and port numbers
//! from the TCP header; it uses this information to filter future reads.
//! \returns a std::optional<TCPSegment> that is empty if the segment was invalid or unrelated


            //   __________________________                     __________________________
            //  |   _listen=true        |                      |   _listen=false          |
            //  |    src_ip=0           |      Tcp.syn         |    src_ip=${myip}        |
            //  |    src_port=80        |     ------>          |    src_port=80           |
            //  |    dst_ip=0           |                      |    dst_ip=${peer_ip}     |
            //  |    dst_port=0         |                      |    dst_port=${peer_port} |
            //  |_______________________|                      |__________________________|
            // 
            //   FdAdapterConfig(Listen)                        FdAdapterConfig(ESTABLISH)

            // A.当Adapter 处于 Listen，收到【syn数据包】后，并且完成了校验(src_port==tcp.dst_port), 更新
            //  其他字段，把_listen 变成false, Adapter 处于ESTABLISH

            // B. 当Adapter 处于 ESTABLISH, 需要完成全部 字段的校验
            //  ip.proto==PROTO_TCP
            //  tcp.dst_ip==src_ip
            //  tcp.dst_port=src_port
            //  tcp.src_ip==dst_ip
            //  tcp.src_port=dst_port


optional<TCPSegment> TCPOverIPv4Adapter::unwrap_tcp_in_ip(const InternetDatagram &ip_dgram) {
    // is the IPv4 datagram for us?
    // Note: it's valid to bind to address "0" (INADDR_ANY) and reply from actual address contacted
    // 非listen下， 校验 是否去向我方ip
    if (not listening() and (ip_dgram.header().dst != config().source.ipv4_numeric())) {
        return {};
    }
    // 非listen下，验证ip地址 是否来自对方
    // is the IPv4 datagram from our peer?
    if (not listening() and (ip_dgram.header().src != config().destination.ipv4_numeric())) {
        return {};
    }
    //协议的验证
    // does the IPv4 datagram claim that its payload is a TCP segment?
    if (ip_dgram.header().proto != IPv4Header::PROTO_TCP) {
        return {};
    }

    // is the payload a valid TCP segment?
    // 把Ip的payload解析成 TCPSegment
    TCPSegment tcp_seg;
    if (ParseResult::NoError != tcp_seg.parse(ip_dgram.payload(), ip_dgram.header().pseudo_cksum())) {
        return {};
    }
    //验证去向的端口是否正确
    // is the TCP segment for us?
    if (tcp_seg.header().dport != config().source.port()) {
        return {};
    }

    // should we target this source addr/port (and use its destination addr as our source) in reply?
    if (listening()) {
        //记录  对方的 ip,port
        //记录  本端的 ip
        if (tcp_seg.header().syn and not tcp_seg.header().rst) {// 数据包是SYN， 而且没有设置RST
            // 回复的时候，源Ip来自 IP数据包, 源Port 不变
            config_mutable().source = {inet_ntoa({htobe32(ip_dgram.header().dst)}), config().source.port()};
            // 回复的时候，目标Ip来自 IP数据包, 目标Port 来自TCP数据包
            config_mutable().destination = {inet_ntoa({htobe32(ip_dgram.header().src)}), tcp_seg.header().sport};
            set_listening(false);
        } else {
            return {};
        }
    }
    //非监听下，验证 是否来自对方的port?
    // is the TCP segment from our peer?
    if (tcp_seg.header().sport != config().destination.port()) {
        return {};
    }

    return tcp_seg;
}

            //   __________________________
            //  |   _listen=false          |              -------------
            //  |    src_ip=${myip}        |------------>| Ip Hdr     |
            //  |    dst_ip=${peer_ip}     |             |____________| \
            //  |                          |                              | InternetDatagram |
            //  |    src_port=80           |              ------------- /
            //  |    dst_port=${peer_port} |------------>| TCPHdr     |
            //  |__________________________|             |            |
            //                                           | TcpPayload |
            //                                           |____________|

//! Takes a TCP segment, sets port numbers as necessary, and wraps it in an IPv4 datagram
//! \param[in] seg is the TCP segment to convert
InternetDatagram TCPOverIPv4Adapter::wrap_tcp_in_ip(TCPSegment &seg) {
    //port回写 到TCPSegment
    // set the port numbers in the TCP segment
    seg.header().sport = config().source.port();
    seg.header().dport = config().destination.port();
    //ip回写 InternetDatagram
    // create an Internet Datagram and set its addresses and length
    InternetDatagram ip_dgram;
    ip_dgram.header().src = config().source.ipv4_numeric();
    ip_dgram.header().dst = config().destination.ipv4_numeric();

    // ipheader的长度 + tcp header的长度 +tcp payload的长度
    // 其中 hlen，doff都需要x4来或者真实长度
    ip_dgram.header().len = ip_dgram.header().hlen * 4 + seg.header().doff * 4 + seg.payload().size();

    //TCP是IP的payload !!!
    // set payload, calculating TCP checksum using information from IP header
    ip_dgram.payload() = seg.serialize(ip_dgram.header().pseudo_cksum());

    return ip_dgram;
}
