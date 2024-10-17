#ifndef SPONGE_LIBSPONGE_TCP_FACTORED_HH
#define SPONGE_LIBSPONGE_TCP_FACTORED_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"

//! \brief A complete endpoint of a TCP connection
class TCPConnection {
  private:
    TCPConfig _cfg;
    TCPReceiver _receiver{_cfg.recv_capacity};
    TCPSender _sender{_cfg.send_capacity, _cfg.rt_timeout, _cfg.fixed_isn};

    //TCPConnection的输出segment
    //! outbound queue of segments that the TCPConnection wants sent
    std::queue<TCPSegment> _segments_out{};

    //! Should the TCPConnection stay active (and keep ACKing)
    //! for 10 * _cfg.rt_timeout milliseconds after both streams have ended,
    //! in case the remote TCPConnection doesn't know we've received its whole stream?
    bool _linger_after_streams_finish{true};

    size_t _time_since_last_segment_received{0};
    bool _active{true};
    
    //把segment 从_sender的输出队列sender.segments_out() 移送到 _segments_out中
    // 1.并且 填写 关于 _receiver的信息，包括 ack,ackno,win
    // 2.rst标志
    void _push_segments_out();


    // //针对 【本次会话】 的异常终止 与是否回复rst，执行以下
    // 1. receriver.output.seterror(),sender.input.seterror()
    // 2.全局的 _active设置成false
    // 3.如果send_rst=true,发送给对方一个rst数据
    void _unclean_shutdown(bool send_rst=true);


    //本方法目的是 根据sender,receriver的状态，修改_active=false
    void _clean_shutdown();
    

    //回复使用_sender发生空的ack,数据包的seqno已经被_sender填写好，
    // 如果发送队列有segment,不需要单独发生确认包，否则可以回复一个空的
    void _push_ack_segment();


    bool _fin_sent();
  public:
    //! \name "Input" interface for the writer
    //!@{
    //发送syn数据包，直接 调用_sender.fill_window
    //! \brief Initiate a connection by sending a SYN segment
    void connect();
    
    //从 _sender的输入中 写入数据，然后同connect(),返回实际写入的数据大小 
    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    size_t write(const std::string &data);

    //_终止sender的输入。sender.stream_in()..end_input();
    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    void end_input_stream();


    //!@}

    //! \name "Output" interface for the reader
    //!@{
    //_receriver的输出，相当于 TCPConnection的输入
    //! \brief The inbound byte stream received from the peer
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}


    //_sender的输入还有多少空间，调用的是_sender.stream_in().remaining_capacity();
    //! \returns the number of `bytes` that can be written right now.
    size_t remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); };



    //! \name Accessors used for testing
    // 发送出去但是没有被接收确认的字节数，调用 _sender.bytes_in_flight()
    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    size_t bytes_in_flight() const{ return _sender.bytes_in_flight(); }

    //收到但是没有被组装的数据，调用  _receiver.unassembled_bytes()
    //! \brief number of bytes not yet reassembled
    size_t unassembled_bytes() const{return _receiver.unassembled_bytes();};


    //! \brief Number of milliseconds since the last segment was received
    size_t time_since_last_segment_received() const{ return _time_since_last_segment_received; }
    //!< \brief summarize the state of the sender, receiver, and the connection
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    //获得对端的一个tcpsegment。
    // 本端可以正确的更新
    // 1._sender: 可以发新的segment
    // 2._receriver的 ackno,win


    //以下满足之一，需要回复确认包
    // 1._receriver seg正确接受，并且用seg.len_in_seq_space()>0， 或者 seg不能正确接受
    // 2._sender收到的确认包有误
    
    void segment_received(const TCPSegment &seg);

    //外部告诉我们过去了多少时间ms
    // 1、本方法判断 是否重试过多，如果是，发送rst给对方
    // 2. 本方法调用_sender.tick(ms_since_last_tick)，所以有可能触发_push_segments_out
    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    bool active() const{ return _active; };
    //!@}

    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open

    //TCPConnection 不支持拷贝构造和赋值
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH
