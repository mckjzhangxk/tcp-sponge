#ifndef SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
#define SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH

#include "buffer.hh"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>

//! A reference-counted handle to a file descriptor
class FileDescriptor {
    //! \brief A handle on a kernel file descriptor.
    //! \details FileDescriptor objects contain a std::shared_ptr to a FDWrapper.
    class FDWrapper {
      //文件描述符fd的包装类，不可被老被，具有close 方法
      public:
        int _fd;                    //!< The file descriptor number returned by the kernel
        bool _eof = false;          //!< Flag indicating whether FDWrapper::_fd is at EOF
        bool _closed = false;       //!< Flag indicating whether FDWrapper::_fd has been closed
        unsigned _read_count = 0;   //!< The number of times FDWrapper::_fd has been read
        unsigned _write_count = 0;  //!< The numberof times FDWrapper::_fd has been written

        //! Construct from a file descriptor number returned by the kernel
        explicit FDWrapper(const int fd);
        //! Closes the file descriptor upon destruction
        ~FDWrapper();
        //! Calls [close(2)](\ref man2::close) on FDWrapper::_fd
        void close();

        //! \name
        //! An FDWrapper cannot be copied or moved

        //FDWrapper没有拷贝构造器和赋值
        //!@{
        FDWrapper(const FDWrapper &other) = delete;
        FDWrapper &operator=(const FDWrapper &other) = delete;
        FDWrapper(FDWrapper &&other) = delete;
        FDWrapper &operator=(FDWrapper &&other) = delete;
        //!@}
    };

    //! A reference-counted handle to a shared FDWrapper
    std::shared_ptr<FDWrapper> _internal_fd;

    // private constructor used to duplicate the FileDescriptor (increase the reference count)
    explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

  protected:
    void register_read() { ++_internal_fd->_read_count; }    //!< increment read count
    void register_write() { ++_internal_fd->_write_count; }  //!< increment write count

  public:
    //! Construct from a file descriptor number returned by the kernel
    explicit FileDescriptor(const int fd);

    //! Free the std::shared_ptr; the FDWrapper destructor calls close() when the refcount goes to zero.
    ~FileDescriptor() = default;

    //! Read up to `limit` bytes
    std::string read(const size_t limit = std::numeric_limits<size_t>::max());
    //通过 _internal_fd，读取数据到 str,如果读取0个字节， 设置_internal_fd->_eof = true;
    // 并且更新读取次数
    //! Read up to `limit` bytes into `str` (caller can allocate storage)
    void read(std::string &str, const size_t limit = std::numeric_limits<size_t>::max());

    //! Write a string, possibly blocking until all is written
    size_t write(const char *str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //! Write a string, possibly blocking until all is written
    size_t write(const std::string &str, const bool write_all = true) { return write(BufferViewList(str), write_all); }

    //把buffer中不连续地址的数据 按照顺序写出到 _internal_fd，
    // write_all = true会把buffer 全部写完，如果中间有错误会抛出异常
    // 返回写出的字节数量
    //! Write a buffer (or list of buffers), possibly blocking until all is written
    size_t write(BufferViewList buffer, const bool write_all = true);

    //让内部的描述符关闭
    //! Close the underlying file descriptor
    void close() { _internal_fd->close(); }

    //通过本方法，派生出一个FileDescriptor，底层的FDWrapper 的引用次数+1
    //! Copy a FileDescriptor explicitly, increasing the FDWrapper refcount
    FileDescriptor duplicate() const;

    //底层描述符 阻塞/非阻塞设置
    //! Set blocking(true) or non-blocking(false)
    void set_blocking(const bool blocking_state);

    //! \name FDWrapper accessors
    //!@{
    int fd_num() const { return _internal_fd->_fd; }                         //!< \brief underlying descriptor number
    bool eof() const { return _internal_fd->_eof; }                          //!< \brief EOF flag state
    bool closed() const { return _internal_fd->_closed; }                    //!< \brief closed flag state
    unsigned int read_count() const { return _internal_fd->_read_count; }    //!< \brief number of reads
    unsigned int write_count() const { return _internal_fd->_write_count; }  //!< \brief number of writes
    //!@}

    //FileDescriptor 不支持拷贝构造和赋值
    //! \name Copy/move constructor/assignment operators
    //! FileDescriptor can be moved, but cannot be copied (but see duplicate())
    //!@{
    FileDescriptor(const FileDescriptor &other) = delete;             //!< \brief copy construction is forbidden
    FileDescriptor &operator=(const FileDescriptor &other) = delete;  //!< \brief copy assignment is forbidden
    FileDescriptor(FileDescriptor &&other) = default;                 //!< \brief move construction is allowed
    FileDescriptor &operator=(FileDescriptor &&other) = default;      //!< \brief move assignment is allowed
    //!@}
};

//! \class FileDescriptor
//! In addition, FileDescriptor tracks EOF state and calls to FileDescriptor::read and
//! FileDescriptor::write, which EventLoop uses to detect busy loop conditions.
//!
//! For an example of FileDescriptor use, see the EventLoop class documentation.

#endif  // SPONGE_LIBSPONGE_FILE_DESCRIPTOR_HH
