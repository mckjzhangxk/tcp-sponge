#include "receiver_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        // auto rd = get_random_generator();

        {
            // Window size decreases appropriately
            size_t cap = 4000;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectWindow{cap});
            test.execute(
                SegmentArrives{}.with_seqno(isn + 1).with_data("abcd").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectWindow{cap - 4});//abcd进入bytestream
            test.execute(
                SegmentArrives{}.with_seqno(isn + 9).with_data("ijkl").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectWindow{cap - 4});//只有 abcd进入bytestream，ijkl还没有被组装
            test.execute(
                SegmentArrives{}.with_seqno(isn + 5).with_data("efgh").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 13}});
            test.execute(ExpectWindow{cap - 12});//abcdefghijkl 都进入了bytestream
        }

        {
            // Window size expands upon read
            size_t cap = 4000;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectWindow{cap});
            test.execute(
                SegmentArrives{}.with_seqno(isn + 1).with_data("abcd").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectWindow{cap - 4});
            test.execute(ExpectBytes{"abcd"});//取出bytestream中的 abcd
            test.execute(ExpectAckno{WrappingInt32{isn + 5}});
            test.execute(ExpectWindow{cap});//window恢复原来的大小
        }

        {
            // high-seqno segment is rejected
            size_t cap = 2;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectWindow{cap});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_seqno(isn + 3).with_data("cd").with_result(
                SegmentArrives::Result::OUT_OF_WINDOW));//目前的window [1,2] 
            test.execute(ExpectAckno{WrappingInt32{isn + 1}});
            test.execute(ExpectWindow{cap});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            // almost-high-seqno segment is accepted, but only some bytes are kept
            size_t cap = 2;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(SegmentArrives{}.with_seqno(isn + 2).with_data("bc").with_result(SegmentArrives::Result::OK));//window 是[1,2]，所以只接受了 'b'
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_seqno(isn + 1).with_data("a").with_result(SegmentArrives::Result::OK));// [1,2]都接受完成，对应ab,被送入bytestream
            test.execute(ExpectAckno{WrappingInt32{isn + 3}});
            test.execute(ExpectWindow{0});//window =2 -2(bytestream.buffer_size() )
            test.execute(ExpectTotalAssembledBytes{2});
            test.execute(ExpectBytes{"ab"});//取出后,bytestream.buffer_size()=0
            test.execute(ExpectWindow{2});
        }

        {
            // low-seqno segment is rejected
            size_t cap = 4;
            uint32_t isn = 294058;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(SegmentArrives{}.with_data("ab").with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectTotalAssembledBytes{2});
            test.execute(ExpectWindow{cap - 2});
            test.execute(SegmentArrives{}.with_data("ab").with_seqno(isn + 1).with_result(
                SegmentArrives::Result::OUT_OF_WINDOW));//此时window [3,4]
            test.execute(ExpectTotalAssembledBytes{2});
        }

        {
            // almost-low-seqno segment is accepted
            size_t cap = 4;
            uint32_t isn = 294058;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(SegmentArrives{}.with_data("ab").with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectTotalAssembledBytes{2});
            test.execute(ExpectWindow{cap - 2});//此时window [3,4,5,6],对外显示的容量是cap-2
            test.execute(SegmentArrives{}.with_data("abc").with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));//接受了c
            test.execute(ExpectTotalAssembledBytes{3});
            test.execute(ExpectWindow{cap - 3});
        }

        {
            // second SYN is rejected
            size_t cap = 2;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));//window=[1,2]
            test.execute(//拒绝 seq=0,窗口之外
                SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OUT_OF_WINDOW));
            test.execute(ExpectWindow{cap});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            // second FIN is rejected
            size_t cap = 2;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));
            test.execute(SegmentArrives{}.with_fin().with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));//window=[2,3]
            test.execute(//fin[1] 在窗口之外
                SegmentArrives{}.with_fin().with_seqno(isn + 1).with_result(SegmentArrives::Result::OUT_OF_WINDOW));
            test.execute(ExpectWindow{cap});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            // Segment overflowing the window on left side is acceptable.
            size_t cap = 4;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));//window=[1,2,3,4]
            test.execute(SegmentArrives{}.with_seqno(isn + 1).with_data("ab").with_result(SegmentArrives::Result::OK));//window=[3,4,5,6]
            test.execute(
                SegmentArrives{}.with_seqno(isn + 3).with_data("cdef").with_result(SegmentArrives::Result::OK));
        }

        {
            // Segment matching the window is acceptable.
            size_t cap = 4;
            uint32_t isn = 23452;
            TCPReceiverTestHarness test{cap};
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn).with_result(SegmentArrives::Result::OK));//window=[1,2,3,4]
            test.execute(SegmentArrives{}.with_seqno(isn + 1).with_data("ab").with_result(SegmentArrives::Result::OK));//window=[3,4,5,6]
            test.execute(SegmentArrives{}.with_seqno(isn + 3).with_data("cd").with_result(SegmentArrives::Result::OK));//window=[5,6,7,8]
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
