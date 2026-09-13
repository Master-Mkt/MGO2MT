#include "footstep_timeline.h"
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace mgo2win::combat::footsteps;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAILED " << __LINE__ << ": " << #x << "\n"; std::abort(); } } while (false)
int main() {
    Timeline t({.5});
    Input in{1,2,3,4,0x4c078d,9,0.,true,true};
    CHECK(t.advance(in).count == 0);
    in.seconds = 15. / 60.; CHECK(t.advance(in).count == 0);
    in.seconds = 16. / 60.; auto e = t.advance(in);
    CHECK(e.count == 1 && e.values[0].cue == 8071 && e.values[0].bone == 0x5B4A33);
    CHECK(t.advance(in).count == 0);
    in.seconds = 46. / 60.; e = t.advance(in);
    CHECK(e.count == 1 && e.values[0].bone == 0xFB4232);
    in.seconds = 1.; CHECK(t.advance(in).count == 0);
    in.seconds = 1. + 16. / 60.; e = t.advance(in);
    CHECK(e.count == 1 && e.values[0].bone == 0x5B4A33);
    in.sourceKey = 0x0460c5; in.sourceIndex = 10; in.seconds = 0.;
    CHECK(t.advance(in).count == 0);
    in.seconds = 17. / 60.; CHECK(t.advance(in).count == 0);
    in.seconds = 18. / 60.; e = t.advance(in);
    CHECK(e.count == 1 && e.values[0].cue == 8007 && e.values[0].bone == 0x5B4A33);
    in.seconds = 38. / 60.; e = t.advance(in);
    CHECK(e.count == 1 && e.values[0].bone == 0xFB4232);
    in.seconds = .95; CHECK(t.advance(in).count == 0);
    in.seconds = 1.; e = t.advance(in); CHECK(e.count == 1 && e.values[0].bone == 0x5B4A33);
    in.grounded = false; in.seconds = 1.3; CHECK(t.advance(in).count == 0);
    in.grounded = true; in.seconds = 1.4; CHECK(t.advance(in).count == 0);
    in.seconds = 2.; CHECK(t.advance(in).count == 0); // stalled: no backlog.
    in.seconds = 0.; CHECK(t.advance(in).count == 0); // rewind.
    in.seconds = .3; ++in.life; CHECK(t.advance(in).count == 0);
    in.seconds = .64; ++in.epoch; CHECK(t.advance(in).count == 0);
    in.seconds = 1.; ++in.scene; CHECK(t.advance(in).count == 0);
    in.seconds = 1.31; ++in.actor; CHECK(t.advance(in).count == 0);
    in.seconds = 1.65; in.eligible = false; CHECK(t.advance(in).count == 0);
    in.seconds = 1.7; in.eligible = true; CHECK(t.advance(in).count == 0);
    in.seconds = std::numeric_limits<double>::quiet_NaN(); CHECK(t.advance(in).count == 0);
    in.seconds = .3; CHECK(t.advance(in).count == 0);
    in.sourceIndex = 9; in.seconds = .64; CHECK(t.advance(in).count == 0); // key mismatch
    in.sourceIndex = 10; in.seconds = .65; CHECK(t.advance(in).count == 0);
    in.seconds = .9; CHECK(t.advance(in).count == 0);
    in.seconds = 1.4; e = t.advance(in);
    CHECK(e.count == 2 && e.values[0].bone == 0x5B4A33 && e.values[1].bone == 0xFB4232);
    bool rejected = false; try { Timeline bad({.51}); } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
    std::cout << "footstep_timeline_test PASS\n";
}
