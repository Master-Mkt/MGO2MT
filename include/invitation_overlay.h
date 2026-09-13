#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace mgo2win::invitation_ui {
// Presentation only. The connection-scoped core owns eligibility and expiry.
struct View {
 uint64_t scope=0,id=0,receivedAt=0,expiresAt=0;
 std::wstring marquee;
 bool available=false,respondable=false;
};
struct Response {uint64_t scope=0,id=0;bool accept=false;};
class Overlay {
 struct Impl;std::unique_ptr<Impl> impl_;
 View view_;uint64_t now_=0,began_=0;bool open_=false,accept_=false,confirmed_=false,menuHint_=false;
 bool live(uint64_t now)const;
public:
 Overlay();~Overlay();
 Overlay(const Overlay&)=delete;Overlay&operator=(const Overlay&)=delete;
 void sync(View,uint64_t now);
 void clear();
 bool available()const;
 bool visible()const;
 bool selected_accept()const{return accept_;}
 void set_menu_hint(bool enabled){menuHint_=enabled;}
 // Logical canvas coordinates; selects a row only, never confirms/sends.
 std::optional<bool> hit_test(int x,int y)const;
 // A received invitation never opens a modal. Only an explicit menu operation does.
 bool open(uint64_t now);
 void close(){open_=false;}
 void move(int direction);
 void choose(bool accept);
 std::optional<Response> confirm(uint64_t now);
 // Paint after normal UI alpha finalization. Logical 1280x720 coordinates;
 // smaller surfaces are clipped, not scaled. Input/output is straight BGRA.
 bool paint(std::span<uint32_t>,int width,int height,uint64_t now);
};
}
