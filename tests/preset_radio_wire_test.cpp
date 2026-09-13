#include "preset_radio_wire.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mgo2::radio::wire;
namespace {
void check(bool v,const char* label) { if (!v) throw std::runtime_error(label); }
template<class F> void reject(F f,const char* label) {
    bool failed=false;try { f(); } catch (const std::exception&) { failed=true; }
    check(failed,label);
}
}
int main() { try {
    static_assert(object_channel == 600 && object_channel != 1);
    constexpr std::array<uint8_t,4> clientGolden{1,3,9,2};
    constexpr std::array<uint8_t,4> hostGolden{2,23,16,2};
    check(encode({Kind::client_request,3,9,2}) == clientGolden,"client bytes from B55A48");
    check(encode({Kind::host_notification,23,16,2}) == hostGolden,"host bytes from B558D0");
    check(decode(clientGolden) == Record{Kind::client_request,3,9,2},"client decode");
    check(decode(hostGolden) == Record{Kind::host_notification,23,16,2},"host decode");
    unsigned validIds=0;
    for (unsigned id=0;id<256;++id) {
        const bool reviewed=id<=7||(id>=9&&id<=16);
        check(reviewed_id(uint8_t(id)) == reviewed,"independent reviewed-ID boundary");
        if (reviewed) {
            ++validIds;
            for (unsigned slot=0;slot<24;++slot) for (auto kind:{Kind::client_request,Kind::host_notification}) {
                Record r{kind,uint8_t(slot),uint8_t(id),2};
                check(decode(encode(r)) == r,"all 768 supported combinations");
            }
        } else {
            reject([&]{encode({Kind::client_request,0,uint8_t(id),2});},"unreviewed preset encode");
            const std::array<uint8_t,4> bytes{1,0,uint8_t(id),2};
            reject([&]{decode(bytes);},"unreviewed preset decode");
        }
    }
    check(validIds==16,"reviewed preset count");
    for (unsigned value=0;value<256;++value) {
        if (value!=1&&value!=2) {
            const std::array<uint8_t,4> b{uint8_t(value),0,0,2};
            reject([&]{decode(b);},"unknown direction");
            reject([&]{encode({static_cast<Kind>(value),0,0,2});},"unknown kind encode");
        }
        if (value>=24) {
            const std::array<uint8_t,4> b{1,uint8_t(value),0,2};
            reject([&]{decode(b);},"slot bound");
            reject([&]{encode({Kind::client_request,uint8_t(value),0,2});},"slot encode bound");
        }
        if (value!=2) {
            const std::array<uint8_t,4> b{1,0,0,uint8_t(value)};
            reject([&]{decode(b);},"unknown third meaning");
            reject([&]{encode({Kind::client_request,0,0,uint8_t(value)});},"third encode");
        }
    }
    for (size_t size:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(5),size_t(16)}) {
        const std::vector<uint8_t> bytes(size,0);
        reject([&]{decode(bytes);},"exact four bytes only");
    }
    std::cout<<"preset_radio_wire_test PASS: 768 supported combinations and exhaustive field rejection\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
