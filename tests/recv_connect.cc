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
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectWindow{4000});
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(0).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{1}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(89347598).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{89347599}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_seqno(893475).with_result(SegmentArrives::Result::NOT_SYN));//没有sync，所以
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});//，所以没有ackno
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_ack(0).with_fin().with_seqno(893475).with_result(
                SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_ack(0).with_fin().with_seqno(893475).with_result(
                SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(89347598).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{89347599}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            TCPReceiverTestHarness test{4000};
            test.execute(SegmentArrives{}.with_syn().with_seqno(5).with_fin().with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectAckno{WrappingInt32{7}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }

        {
            // Window overflow
            //虽然window大小 超过了tcp window field，但是receiver.window_size()->size_t
            // 不会有溢出错误
            size_t cap = static_cast<size_t>(UINT16_MAX) + 5;
            TCPReceiverTestHarness test{cap};
            test.execute(ExpectWindow{cap});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
