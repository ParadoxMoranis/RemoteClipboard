#include "test_support.h"

#include "client_common/protocol.h"

#include <cstdint>
#include <string>

int main()
{
    using namespace remoteclipboard::v1;
    TestContext test;

    const Json auth = makeAuth("alice", "secret");
    RC_EXPECT_EQ(test, auth["type"], type::kAuth);
    RC_EXPECT_EQ(test, auth["username"], "alice");
    RC_EXPECT(test, validateClientMessage(auth, 1024).valid);

    const Json text = makeClipboardText("hello");
    RC_EXPECT_EQ(test, text.dump(), R"({"content":"hello","type":"clipboard_text"})");
    RC_EXPECT(test, validateClientMessage(text, 5).valid);
    RC_EXPECT(test, !validateClientMessage(text, 4).valid);

    const Json start = makeTransferStart(
        "transfer-1", "file.txt", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "text/plain");
    RC_EXPECT_EQ(test, start["transfer_id"], "transfer-1");
    RC_EXPECT_EQ(test, start["name"], "file.txt");
    RC_EXPECT(test, start["size"].is_number_unsigned());
    RC_EXPECT(test, validateClientMessage(start, 1024).valid);

    const Json chunk = makeTransferChunk("transfer-1", 0, "YWJj");
    RC_EXPECT_EQ(test, chunk["seq"], 0);
    RC_EXPECT(test, chunk["seq"].is_number_unsigned());
    RC_EXPECT(test, validateClientMessage(chunk, 1024).valid);

    const Json complete = makeTransferComplete("transfer-1");
    RC_EXPECT_EQ(test, complete.dump(),
                 R"({"transfer_id":"transfer-1","type":"file_transfer_complete"})");
    RC_EXPECT(test, validateClientMessage(complete, 1024).valid);

    Json wrongChunk = chunk;
    wrongChunk["seq"] = -1;
    RC_EXPECT(test, !validateClientMessage(wrongChunk, 1024).valid);
    RC_EXPECT(test, !validateClientMessage(Json{{"type", "unknown"}}, 1024).valid);

    return test.result();
}
