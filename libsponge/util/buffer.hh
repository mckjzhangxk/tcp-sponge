#ifndef SPONGE_LIBSPONGE_BUFFER_HH
#define SPONGE_LIBSPONGE_BUFFER_HH

#include <algorithm>
#include <deque>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <sys/uio.h>
#include <vector>
//  Buffer:
//                                   ________                     size()
//     _storage            --------->|      |                   /
//     _starting_offset    --------->|str() |  ---> string_view -> at(n)
//           ^                       |______|                   \
//           |                                                    copy()
//      remove_prefix(n)

//! \brief A reference-counted read-only string that can discard bytes from the front
class Buffer {
  private:
    std::shared_ptr<std::string> _storage{};
    size_t _starting_offset{};

  public:
    Buffer() = default;

    //! \brief Construct by taking ownership of a string
    // Buffer通过对底层 str进行引用计数，实现 轻量化的操作 底层的str
    Buffer(std::string &&str) noexcept : _storage(std::make_shared<std::string>(std::move(str))) {}

    //! \name Expose contents as a std::string_view
    //!@{
    //<_storage.data,_storage.size() >
    std::string_view str() const {
        if (not _storage) {
            return {};
        }
        return {_storage->data() + _starting_offset, _storage->size() - _starting_offset};
    }
    //把Buffer 转成string_view
    operator std::string_view() const { return str(); }
    //!@}

     //返回Buffer[n]
    //! \brief Get character at location `n`
    uint8_t at(const size_t n) const { return str().at(n); }

    //Buffer的大小
    //! \brief Size of the string
    size_t size() const { return str().size(); }

    //拷贝出一个新的string
    //! \brief Make a copy to a new std::string
    std::string copy() const { return std::string(str()); }

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    //! \note Doesn't free any memory until the whole string has been discarded in all copies of the Buffer.
    //通过 设置偏移 表示消耗字节,当全部移除的时候，智能指针重置为null
    void remove_prefix(const size_t n);
};

//! \brief A reference-counted discontiguous string that can discard bytes from the front
//! \note Used to model packets that contain multiple sets of headers
//! + a payload. This allows us to prepend headers (e.g., to
//! encapsulate a TCP payload in a TCPSegment, and then encapsulate
//! the TCPSegment in an IPv4Datagram) without copying the payload.

// BufferList表示不连续的Buffer,主要用于 封装数据帧，比如在TCP(Buffer)前面添加 IPV4(Buffer) Hdr
class BufferList {
  private:
    std::deque<Buffer> _buffers{};

  public:
    //! \name Constructors
    //!@{

    BufferList() = default;

    //! \brief Construct from a Buffer
    BufferList(Buffer buffer) : _buffers{buffer} {}

    //! \brief Construct by taking ownership of a std::string
    BufferList(std::string &&str) noexcept {
        Buffer buf{std::move(str)};
        append(buf);// BufferList{Buffer}
    }
    //!@}

    //! \brief Access the underlying queue of Buffers
    const std::deque<Buffer> &buffers() const { return _buffers; }

    //! \brief Append a BufferList
    // other下面的buffer 追加到本bufferlist中
    void append(const BufferList &other);

    //! \brief Transform to a Buffer
    //! \note Throws an exception unless BufferList is contiguous
    //转换成为Buffer,要求_buffers只有一个元素
    operator Buffer() const;

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    //类似BufferViewList的remove_prefix,用于移出n个字节
    //移除BufferList中的n个字节
    void remove_prefix(size_t n);

    //! \brief Size of the string
    // 总字节数量
    size_t size() const;

    //! \brief Make a copy to a new std::string
    //把BufferList的buffer组成一个大的string
    std::string concatenate() const;
};

//! \brief A non-owning temporary view (similar to std::string_view) of a discontiguous string
class BufferViewList {
    std::deque<std::string_view> _views{};

  public:
    //! \name Constructors
    //!@{

    //! \brief Construct from a std::string
    BufferViewList(const std::string &str) : BufferViewList(std::string_view(str)) {}

    //! \brief Construct from a C string (must be NULL-terminated)
    BufferViewList(const char *s) : BufferViewList(std::string_view(s)) {}

    //! \brief Construct from a BufferList
    //BufferList转 BufferViewList
    // BufferViewList里面存的是stringview,而BufferList里面存的是Buffer
    BufferViewList(const BufferList &buffers);

    //! \brief Construct from a std::string_view
    //核心构造方法，把string_view加入到_views中
    // StringView= <*data,size>
    BufferViewList(std::string_view str) { _views.push_back({const_cast<char *>(str.data()), str.size()}); }
    //!@}

    //! \brief Discard the first `n` bytes of the string (does not require a copy or move)
    // 从 _views中移出n个字节
    void remove_prefix(size_t n);

    //! \brief Size of the string
    // _views中 字节的总数
    size_t size() const;

    //iovec是一个数组，由起始地址p和长度sz组成
    //本方法把BufferViewList 穿换成 iovec 数组
    // iovec 数组 可用于 高效处理不连续的数据 
    //! \brief Convert to a vector of `iovec` structures
    //! \note used for system calls that write discontiguous buffers,
    //! e.g. [writev(2)](\ref man2::writev) and [sendmsg(2)](\ref man2::sendmsg)
    std::vector<iovec> as_iovecs() const;
};

#endif  // SPONGE_LIBSPONGE_BUFFER_HH
