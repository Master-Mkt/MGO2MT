#include "chat_wire.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <functional>

using namespace mgo2win::chat;
namespace {
void check(bool value, const char* what) { if (!value) throw std::runtime_error(what); }
void reject(const std::function<void()>& operation, const char* what) {
    bool threw = false;
    try { operation(); } catch (const std::exception&) { threw = true; }
    check(threw, what);
}
std::string u8text(std::u8string_view text) {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
std::vector<uint8_t> reply(std::string_view text, size_t utf16units, uint8_t mode = '0') {
    std::vector<uint8_t> bytes(6 + 3 * utf16units, 0);
    bytes[0] = 1; bytes[1] = 2; bytes[2] = 3; bytes[3] = 4; bytes[4] = mode;
    std::copy(text.begin(), text.end(), bytes.begin() + 5);
    return bytes;
}
}
int main() {
    try {
        auto bytes = room_payload("hello", Encoding::utf8);
        check(bytes.size() == 129 && bytes[0] == 0 && bytes[1] == 0x30, "retail 4400 header");
        check(bytes[2] == 'h' && bytes[6] == 'o' && bytes[7] == 0 && bytes.back() == 0, "request field");
        check(std::all_of(bytes.begin() + 7, bytes.end(), [](auto b) { return b == 0; }), "request padding");
        room_payload(std::string(126, 'x'), Encoding::utf8);
        reject([] { room_payload(std::string(127, 'x'), Encoding::utf8); }, "127 bytes rejected");
        const auto jp = u8text(u8"日本語");
        check(room_payload(jp, Encoding::utf8)[2] == 0xe6, "JP encoding");
        std::string japanese;
        for (int i = 0; i != 42; ++i) japanese += u8text(u8"日");
        room_payload(japanese, Encoding::utf8);
        reject([&] { room_payload(japanese + "a", Encoding::utf8); }, "UTF8 boundary");
        std::string astral;
        for (int i = 0; i != 31; ++i) astral += u8text(u8"😀");
        room_payload(astral + "ab", Encoding::utf8);
        reject([&] { room_payload(astral + "abc", Encoding::utf8); }, "4-byte boundary");
        reject([] { room_payload("", Encoding::utf8); }, "empty");
        for (const auto s : {std::string("a\0b", 3), std::string("a\nb"), std::string("\x7f"),
              std::string("\xc0\xaf"), std::string("\xe6\x97"), std::string("\xed\xa0\x80"),
              std::string("\xf4\x90\x80\x80"), std::string("\x80")})
            reject([&] { room_payload(s, Encoding::utf8); }, "invalid input");
        bytes = room_payload(u8text(u8"café"), Encoding::latin1);
        check(bytes[5] == 0xe9 && bytes[6] == 0, "Latin1 encoding");
        reject([&] { room_payload(jp, Encoding::latin1); }, "no lossy Latin1 encoding");
        auto message = receive_payload(reply("hello", 5), Encoding::utf8);
        check(message.character == 0x01020304 && message.mode == 0 && message.text == "hello", "server padded ASCII");
        check(receive_payload(reply(jp, 3), Encoding::utf8).text == jp, "server JP");
        const auto emoji = u8text(u8"😀");
        check(receive_payload(reply(emoji, 2), Encoding::utf8).text == emoji, "server UTF16 allocation");
        check(receive_payload(reply(std::string("caf\xe9"), 4), Encoding::latin1).text == u8text(u8"café"), "Latin1 receive");
        for (uint8_t mode = '0'; mode <= '4'; ++mode)
            check(receive_payload(reply("x", 1, mode), Encoding::utf8).mode == mode - '0', "mode preserved");
        // Server-prefixed notices can exceed the request limit and are not ACKs.
        check(receive_payload(reply(std::string(200, 'x'), 200), Encoding::utf8).text.size() == 200, "long server reply");
        check(receive_payload(reply("", 0), Encoding::utf8).text.empty(), "empty server text");
        auto bad = reply("x", 1); bad.back() = 1;
        reject([&] { receive_payload(bad, Encoding::utf8); }, "trailing garbage");
        bad = reply("x", 1); bad.resize(6);
        reject([&] { receive_payload(bad, Encoding::utf8); }, "missing terminator");
        bad = reply("x", 1); bad[4] = '5';
        reject([&] { receive_payload(bad, Encoding::utf8); }, "unknown mode");
        bad = reply(std::string("\xe6\x97"), 2);
        reject([&] { receive_payload(bad, Encoding::utf8); }, "malformed receive UTF8");
        for (const size_t size : {size_t(0), size_t(5), size_t(1024)}) {
            bad.assign(size, 0);
            reject([&] { receive_payload(bad, Encoding::utf8); }, "packet bound");
        }
        const std::vector<uint8_t> goldenRequest{0x47,0x57,0x43,0x48,1,1,0,0,
            1,2,3,4,5,6,7,8,0,0x0f,0x3c,0x2b,0,0,0,14};
        check(capability_payload(0x0102030405060708ULL,998443,14)==goldenRequest,"GWCH request golden");
        auto cap = goldenRequest;cap.resize(28);cap[5]=0;cap[6]=1;cap[7]=1;
        auto capabilities=capability_reply(cap);
        check(capabilities.status==0&&capabilities.encoding==Encoding::utf8&&capabilities.flags==1
            &&capabilities.nonce==0x0102030405060708ULL&&capabilities.room==998443
            &&capabilities.character==14,"GWCH response golden");
        cap[6]=2;check(capability_reply(cap).encoding==Encoding::latin1,"GWCH ENG");
        for(size_t offset : {size_t(0),size_t(4),size_t(5),size_t(6),size_t(7),size_t(24)}) {
            auto malformed=cap;malformed[offset]=0xff;
            reject([&]{capability_reply(malformed);},"GWCH invalid field");
        }
        cap[5]=2;cap[6]=0;cap[7]=0;
        check(capability_reply(cap).status==2,"GWCH session failure echoes identity");
        cap[6]=1;reject([&]{capability_reply(cap);},"GWCH failure encoding leak");
        cap[5]=1;std::fill(cap.begin()+6,cap.end(),uint8_t{0});
        check(capability_reply(cap).status==1,"GWCH malformed clears identity");
        cap[23]=14;reject([&]{capability_reply(cap);},"GWCH malformed echo leak");
        reject([]{capability_payload(0,1,1);},"GWCH zero nonce");
        reject([]{capability_payload(1,0x80000000,1);},"GWCH signed identity bound");
        std::cout << "chat_wire_test PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
