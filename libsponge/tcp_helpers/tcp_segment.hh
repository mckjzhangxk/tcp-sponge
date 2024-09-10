#ifndef SPONGE_LIBSPONGE_TCP_SEGMENT_HH
#define SPONGE_LIBSPONGE_TCP_SEGMENT_HH

#include "buffer.hh"
#include "tcp_header.hh"

#include <cstdint>

//! \brief [TCP](\ref rfc::rfc793) segment
class TCPSegment {
  private:
    TCPHeader _header{};
    Buffer _payload{};

  public:
    //! \brief Parse the segment from a string
    // 把buffer 的内容解析到 TCPHeader和payload中,并且做了checksum的校验
    ParseResult parse(const Buffer buffer, const uint32_t datagram_layer_checksum = 0);

    //! \brief Serialize the segment to a string
    // TCP Header,TCP Payload 成为BufferList的2个成员
    // TCP Header的checksum 是 TCP Hdr,TCP Payload,  上层的pseudo-checksum 三者 只和 取反
    BufferList serialize(const uint32_t datagram_layer_checksum = 0) const;

    //! \name Accessors
    //!@{
    const TCPHeader &header() const { return _header; }
    TCPHeader &header() { return _header; }

    const Buffer &payload() const { return _payload; }
    Buffer &payload() { return _payload; }
    //!@}

    //! \brief Segment's length in sequence space
    //! \note Equal to payload length plus one byte if SYN is set, plus one byte if FIN is set
    // 表示这个TCPSegment的长度，_header中的 SYN,FIN标志各占一个字节
    size_t length_in_sequence_space() const;
};

#endif  // SPONGE_LIBSPONGE_TCP_SEGMENT_HH
