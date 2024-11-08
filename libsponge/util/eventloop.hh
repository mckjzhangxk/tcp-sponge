#ifndef SPONGE_LIBSPONGE_EVENTLOOP_HH
#define SPONGE_LIBSPONGE_EVENTLOOP_HH

#include "file_descriptor.hh"

#include <cstdlib>
#include <functional>
#include <list>
#include <poll.h>

//! Waits for events on file descriptors and executes corresponding callbacks.
class EventLoop {
  public:
    //! Indicates interest in reading (In) or writing (Out) a polled fd.
    enum class Direction : short {
        In = POLLIN,   //!< Callback will be triggered when Rule::fd is readable.
        Out = POLLOUT  //!< Callback will be triggered when Rule::fd is writable.
    };

  private:
    using CallbackT = std::function<void(void)>;  //!< Callback for ready Rule::fd
    using InterestT = std::function<bool(void)>;  //!< `true` return indicates Rule::fd should be polled.

    //! \brief Specifies a condition and callback that an EventLoop should handle.
    //! \details Created by calling EventLoop::add_rule() or EventLoop::add_cancelable_rule().
    class Rule {
      public:
        FileDescriptor fd;    //!< FileDescriptor to monitor for activity.
        Direction direction;  //!< Direction::In for reading from fd, Direction::Out for writing to fd.
        CallbackT callback;   //!< A callback that reads or writes fd.如果fd上触发direction事件，调用callback
        InterestT interest;   //!< A callback that returns `true` whenever fd should be polled.表示是否可以被poll
        
        CallbackT cancel;     //!< A callback that is called when the rule is cancelled (e.g. on hangup)，
        //针对fd hangup或者 cloesed的回调函数

        //canel的原因是由于异常导致
        //! Returns the number of times fd has been read or written, depending on the value of Rule::direction.
        //! \details This function is used internally by EventLoop; you will not need to call it
        // 统计读/写 bytes数量
        unsigned int service_count() const;
    };

    std::list<Rule> _rules{};  //!< All rules that have been added and not canceled.

  public:
    //! Returned by each call to EventLoop::wait_next_event.
    enum class Result {
        Success,  //!< At least one Rule was triggered.
        Timeout,  //!< No rules were triggered before timeout.
        Exit  //!< All rules have been canceled or were uninterested; make no further calls to EventLoop::wait_next_event.
    };
    //把规则添加到_rules中
    //! Add a rule whose callback will be called when `fd` is ready in the specified Direction.
    void add_rule(const FileDescriptor &fd,
                  const Direction direction,
                  const CallbackT &callback,
                  const InterestT &interest = [] { return true; },
                  const CallbackT &cancel = [] {});
    //1.根据_rules，筛选(_rules[i]->interest()->true)出需要 被poll的 全部fds
    //2.如果fd closed()或者hangup,移除规则，并且调用 cancel（）
    //3.1调用epoll(fds,timeout),如果超时，返回TIMEOUT,失败（poll被中断）返回EXIT
    //3.2 如果 没有需要被poll的fd,返回EXIT
    //4.遍历每个fds
    
    //   A. 如果fd发生错误（POLLERR | POLLNVAL），抛出异常
    //   B. 如果fd hangup(),同上 调用cancle,并且移除规则
    //   C.,事件被触发:调用对应rule[i].callback(), 如果rule[i].service_count()没有发生变化，抛出异常
    //! Calls [poll(2)](\ref man2::poll) and then executes callback for each ready fd.


    //返回值
    // EXIT: 没有可被POLL 或者 POLL被中断
    // TIMEOUT: poll超时
    // 异常：
    //  1.某个fd 上 发生错误（POLLERR | POLLNVAL）
    //  2.busy wait detected: callback did not read/write fd and is still interested
    // SUCCESS: 以上都未发生
    Result wait_next_event(const int timeout_ms);
};

using Direction = EventLoop::Direction;

//! \class EventLoop
//!
//! An EventLoop holds a std::list of Rule objects. Each time EventLoop::wait_next_event is
//! executed, the EventLoop uses the Rule objects to construct a call to [poll(2)](\ref man2::poll).
//!
//! When a Rule is installed using EventLoop::add_rule, it will be polled for the specified Rule::direction
//! whenver the Rule::interest callback returns `true`, until Rule::fd is no longer readable
//! (for Rule::direction == Direction::In) or writable (for Rule::direction == Direction::Out).
//! Once this occurs, the Rule is canceled, i.e., the EventLoop deletes it.
//!
//! A Rule installed using EventLoop::add_cancelable_rule will be polled and canceled under the
//! same conditions, with the additional condition that if Rule::callback returns `true`, the
//! Rule will be canceled.

#endif  // SPONGE_LIBSPONGE_EVENTLOOP_HH
