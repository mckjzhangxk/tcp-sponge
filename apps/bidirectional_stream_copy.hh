#ifndef SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH
#define SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH

#include "socket.hh"

// read(stdin)-->ByteStream(_outbound) -->write(socket)
//  stdin.eof() ->_outbound.end_input(),  _outbound.eof()-> socket.shutdown()

// read(socket)-->ByteStream(_inbound) -->write(stdout)
// socket.eof() -> _inbound.end_input(),  _inbound->eof()-> stdout.close()

// 设置好eventloop,直到Loop的退出

//! Copy socket input/output to stdin/stdout until finished
void bidirectional_stream_copy(Socket &socket);

#endif  // SPONGE_APPS_BIDIRECTIONAL_STREAM_COPY_HH
