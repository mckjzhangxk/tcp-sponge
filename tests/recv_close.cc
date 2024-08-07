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
        auto rd = get_random_generator();

        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            //期望isn可能被正确接受
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 0).with_result(SegmentArrives::Result::OK));
            ////期望receiver的状态 变成正在接受数据
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            //期望isn+1可能被正确接受
            test.execute(SegmentArrives{}.with_fin().with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            //期望此时的ackno=isn+2
            test.execute(ExpectAckno{WrappingInt32{isn + 2}});
            //期望此时没有数据要被再次组装
            test.execute(ExpectUnassembledBytes{0});
            //期望此时bytestream是空
            test.execute(ExpectBytes{""});
            //全部 被bytestream接受的payload长度是0
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
        }

        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 0).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(
                SegmentArrives{}.with_fin().with_seqno(isn + 1).with_data("a").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 3}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{"a"});
            test.execute(ExpectTotalAssembledBytes{1});
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
