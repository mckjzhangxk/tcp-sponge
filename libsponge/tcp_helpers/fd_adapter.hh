#ifndef SPONGE_LIBSPONGE_FD_ADAPTER_HH
#define SPONGE_LIBSPONGE_FD_ADAPTER_HH

#include "file_descriptor.hh"
#include "lossy_fd_adapter.hh"
#include "socket.hh"
#include "tcp_config.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"

#include <optional>
#include <utility>

//适配器的基类，主要就是返回配置（local,remote地址信息）与 设置是否listen
//! \brief Basic functionality for file descriptor adaptors
//! \details See TCPOverUDPSocketAdapter and TCPOverIPv4OverTunFdAdapter for more information.
class FdAdapterBase {
  private:
    FdAdapterConfig _cfg{};  //!< Configuration values，主要用于保存local,remote地址信息
    bool _listen = false;    //!< Is the connected TCP FSM in listen state?

  protected:
    FdAdapterConfig &config_mutable() { return _cfg; }

  public:
    //! \brief Set the listening flag
    //! \param[in] l is the new value for the flag
    void set_listening(const bool l) { _listen = l; }

    //! \brief Get the listening flag
    //! \returns whether the FdAdapter is listening for a new connection
    bool listening() const { return _listen; }

    //! \brief Get the current configuration
    //! \returns a const reference
    const FdAdapterConfig &config() const { return _cfg; }

    //! \brief Get the current configuration (mutable)
    //! \returns a mutable reference
    FdAdapterConfig &config_mut() { return _cfg; }

    //! Called periodically when time elapses
    void tick(const size_t) {}
};

//结合UDPSocket和 FdAdapterBase, 组成一个TCPSocket
//  UDPSocket提供 发送/接受 能力
//  FdAdapterBase： 提供 发送/接受 端口信息
//! \brief A FD adaptor that reads and writes TCP segments in UDP payloads
class TCPOverUDPSocketAdapter : public FdAdapterBase {
  private:
    UDPSocket _sock;

  public:
    //! Construct from a UDPSocket sliced into a FileDescriptor

    explicit TCPOverUDPSocketAdapter(UDPSocket &&sock) : _sock(std::move(sock)) {}
    
    //使用udp的recv方法，读取一个数据包，解析验证TCPSegment的合法性
    //如果是一个syn数据包， 表示一个新的connection建立
    // 1.取消listen状态
    // 2.保存对端地址到config().dest中，用于将来的通信


    //! Attempts to read and return a TCP segment related to the current connection from a UDP payload
    std::optional<TCPSegment> read();

    //把seg发送出去， 根据config 填写好 源端口，目的端口信息
    //! Writes a TCP segment into a UDP payload
    void write(TCPSegment &seg);

    //! Access the underlying UDP socket
    operator UDPSocket &() { return _sock; }

    //! Access the underlying UDP socket
    operator const UDPSocket &() const { return _sock; }
};

//! Typedef for TCPOverUDPSocketAdapter
using LossyTCPOverUDPSocketAdapter = LossyFdAdapter<TCPOverUDPSocketAdapter>;

#endif  // SPONGE_LIBSPONGE_FD_ADAPTER_HH
