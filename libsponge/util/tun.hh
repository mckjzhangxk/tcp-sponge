#ifndef SPONGE_LIBSPONGE_TUN_HH
#define SPONGE_LIBSPONGE_TUN_HH

#include "file_descriptor.hh"

#include <string>


//! A FileDescriptor to a [Linux TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) device
class TunFD : public FileDescriptor {
  public:
    //打开tun 设备文件，设置tun属性
    //! Open an existing persistent [TUN device](https://www.kernel.org/doc/Documentation/networking/tuntap.txt).
    explicit TunFD(const std::string &devname);
};

#endif  // SPONGE_LIBSPONGE_TUN_HH
