#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <optional>


  class Timer{
    size_t _current_ticks;
    size_t _rto;//超时数值
    uint32_t _muls;
    bool _running;
    public:
    Timer(size_t rto):_current_ticks(0),_rto(rto),_muls(1),_running(false){};

    //秒表从0 开始计时，ts是已经过去多少ms
    // 当【秒表计时】>=_rto*_muls,返回true
    bool expire(size_t ts){
      if(_running)
        return (_current_ticks+=ts)>=_rto*_muls;
      else
        return false;
    };

    //秒表启动计时
    void start(){
      _running=true;
    }
    //秒表关闭计时，并且重置的默认
    void stop(){
      _running=false;
      _current_ticks=0;
      _muls=1;
    }
    uint32_t get_muls()const{
       return _muls;
    }
    //从0 开始计时间，但是 延长超时时间
    void double_mul(){
      _current_ticks=0;
      _muls*=2;
    }
  };

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    // 发送数据的 初始seq
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    // 输出
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    // 重传超时值，ms
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    // 输入
    ByteStream _stream;

    //发送窗口的大小
    int _sws;//send window size

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno;
    

    //被确认 的【绝对序号】，也是发生窗口的开始索引
    uint64_t _ackno;
    Timer _timer;

    //是否发送syn
    bool _syn_send;
    //是否发送fin
    bool _fin_send;
    
    //保存发送出去，但是没有被确认的segment
    BufferList _outstanding_segs;


    void _transmit(TCPSegment & seg,bool need_retry=true);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    // 1.更新内部的 窗口(_ackno,_sws),如果无效，或者过期确认，直接返回
    // 2.当 【触发窗口滑动】或者【窗口长度增长】的时候， 调用fill_window,发生更多的数据
    // 3.【触发窗口滑动】的时候，也就是有新的字节被确认的时候，关闭【计时器】，从_outstanding中
    // 移除确认的字节
    // 返回false:表示
    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    // 构造一个 TCPSegment,发送到【输出_segment_out】
    // ,seq( _next_seqno的数值 )
    void send_empty_segment();
    

    // 1.把【输入_stream】的数据 转换成TCPSegment,发送到【输出_segment_out】
    // 2.发送的 时候， 正确设置 syn(如果第一次发生，fin(如果_stream.eof()),seq( _next_seqno的数值 )
    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //1.当 计时器 发现超时的时候， 先重置并且延长超时时间。
    //2.从 _outstanding中 取出需要重传的Buffer,组成TCP.payload(),如果_outstanding.size()==0,TCP.payload()也是空
    //3.设置好对应的 syn(_ackno==0),fin,这样就实现的重传 特殊的数据包
    //4.开启定时器
    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}


    //! \name Accessors
    //!@{
    //多少个字节 已经发送 但是没有被确认,>0表示还有没被确认的字节
    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //发送了多少次重传
    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{
    //下一个发送字节的 序号
    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const {
        return _next_seqno;
    }
    //下一个发送字节的 序号
    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
