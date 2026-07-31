#include "test_support.h"

#include "server_common/frameparser.h"

#include <string>

int main()
{
    TestContext test;

    FrameParser parser(8);
    RC_EXPECT(test, parser.append("one\ntwo\n", 8));
    std::string frame;
    RC_EXPECT(test, parser.popFrame(frame));
    RC_EXPECT_EQ(test, frame, "one");
    RC_EXPECT(test, parser.popFrame(frame));
    RC_EXPECT_EQ(test, frame, "two");
    RC_EXPECT(test, !parser.popFrame(frame));

    RC_EXPECT(test, parser.append("12345678\n", 9));
    RC_EXPECT(test, parser.popFrame(frame));
    RC_EXPECT_EQ(test, frame, "12345678");

    FrameParser noNewline(4);
    RC_EXPECT(test, !noNewline.append("12345", 5));
    RC_EXPECT(test, noNewline.overflowed());
    RC_EXPECT(test, !noNewline.popFrame(frame));

    FrameParser withNewline(4);
    RC_EXPECT(test, !withNewline.append("12345\n", 6));
    RC_EXPECT(test, withNewline.overflowed());
    RC_EXPECT(test, !withNewline.popFrame(frame));

    withNewline.reset();
    RC_EXPECT(test, withNewline.append("ok\n", 3));
    RC_EXPECT(test, withNewline.popFrame(frame));
    RC_EXPECT_EQ(test, frame, "ok");

    return test.result();
}
