#include "preset_radio.h"
#include <cstdlib>
#include <iostream>

using namespace mgo2::radio;
static void check(bool ok, const char* why) {
    if (!ok) { std::cerr << why << '\n'; std::exit(1); }
}
int main() {
    constexpr unsigned ids[4][4]{{0,1,2,3},{4,5,6,7},{9,10,11,12},{13,14,15,16}};
    Menu menu;
    check(!menu.digital(Direction::up), "closed direction ignored");
    check(menu.select() == MenuState::chat, "first SELECT chat");
    check(!menu.digital(Direction::left), "chat direction does not select radio");
    check(menu.select() == MenuState::preset_categories, "second SELECT radio");
    for (unsigned c=0;c<4;++c) for (unsigned d=0;d<4;++d) {
        menu.reset();menu.select();menu.select();
        check(!menu.digital(static_cast<Direction>(c)), "first direction category only");
        check(menu.state()==MenuState::preset_messages && menu.category()==static_cast<Direction>(c), "category retained");
        auto picked=menu.digital(static_cast<Direction>(d));
        check(picked && picked->presetId==ids[c][d] && picked->category==static_cast<Direction>(c)
              && picked->choice==static_cast<Direction>(d), "original fallback placement");
        check(menu.state()==MenuState::closed && !menu.category(), "selection closes");
        check(!menu.digital(static_cast<Direction>(d)), "no repeated emission after close");
        check(defaultCategories()[c].messages[d].presetId==ids[c][d]
              && !defaultCategories()[c].messages[d].text.empty(), "catalog UI agreement");
    }
    menu.select();menu.select();
    check(!menu.digital(static_cast<Direction>(255)) && menu.state()==MenuState::preset_categories, "invalid direction ignored");
    menu.digital(Direction::down);menu.cancel();
    check(menu.state()==MenuState::preset_categories && !menu.category(), "cancel choice returns category");
    menu.digital(Direction::right);check(menu.select()==MenuState::chat && !menu.category(), "SELECT leaves choice for chat");
    menu.select();menu.digital(Direction::up);menu.reset();
    check(!menu.digital(Direction::right), "focus/identity reset clears partial selection");
    menu.select();menu.cancel();check(menu.state()==MenuState::closed, "cancel chat closes");
    std::cout << "preset radio 16 placements, selection/reset boundaries passed\n";
}
