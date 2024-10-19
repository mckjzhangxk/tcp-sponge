#ifndef SPONGE_LIBSPONGE_PARSER_HH
#define SPONGE_LIBSPONGE_PARSER_HH

#include "buffer.hh"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

//! The result of parsing or unparsing an IP datagram, TCP segment, Ethernet frame, or ARP message
enum class ParseResult {
    NoError = 0,      //!< Success
    BadChecksum,      //!< Bad checksum，错误的校验和
    PacketTooShort,   //!< Not enough data to finish parsing,  s,收到的buffer size < Ip header的20个字节
    WrongIPVersion,   //!< Got a version of IP other than 4,IPV4版本错误
    HeaderTooShort,   //!< Header length is shorter than minimum required,IP头部不足20字节
    TruncatedPacket,  //!< Packet length is shorter than header claims,收到的buffer size < Ip数据包总长度 
    Unsupported       //!< Packet uses unsupported features

};

//! Output a string representation of a ParseResult
std::string as_string(const ParseResult r);

class NetParser {
  private:
    Buffer _buffer;
    ParseResult _error = ParseResult::NoError;  //!< Result of parsing so far

    //! Check that there is sufficient data to parse the next token
    //检查size是否大于_buffer的长度，如果是，设置set_error
    void _check_size(const size_t size);

    //! Generic integer parsing method (used by u32, u16, u8)
    //从_buffer中 提取不同类型的整数，返回，并且pop对应的内容
    template <typename T>
    T _parse_int();

  public:
    //通过buffer,构建解析器
    NetParser(Buffer buffer) : _buffer(buffer) {}

    Buffer buffer() const { return _buffer; }

    //! Get the current value stored in BaseParser::_error
    ParseResult get_error() const { return _error; }

    //! \brief Set BaseParser::_error
    //! \param[in] res is the value to store in BaseParser::_error
    void set_error(ParseResult res) { _error = res; }

    //是否有错误
    //! Returns `true` if there has been an error
    bool error() const { return get_error() != ParseResult::NoError; }

    ////从_buffer中 取出u32,并移除这4个字节
    //! Parse a 32-bit integer in network byte order from the data stream
    uint32_t u32();

    ///从_buffer中 取出u16,并移除这2个字节
    //! Parse a 16-bit integer in network byte order from the data stream
    uint16_t u16();
    ///从_buffer中 取出u8,并移除这1个字节
    //! Parse an 8-bit integer in network byte order from the data stream
    uint8_t u8();
    ///从_buffer中移除这n个字节
    //! Remove n bytes from the buffer
    void remove_prefix(const size_t n);
};

struct NetUnparser {
    template <typename T>
    static void _unparse_int(std::string &s, T val);//把val的每个byte加入到s，从val的 最高位开始

    //! Write a 32-bit integer into the data stream in network byte order
    static void u32(std::string &s, const uint32_t val);

    //! Write a 16-bit integer into the data stream in network byte order
    static void u16(std::string &s, const uint16_t val);

    //! Write an 8-bit integer into the data stream in network byte order
    static void u8(std::string &s, const uint8_t val);
};

#endif  // SPONGE_LIBSPONGE_PARSER_HH
