#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <memory>
#include <string>

namespace mgo2mt::title_movie {
enum class Status { idle, loading, ready, playing, ended, failed };
class Player {
 struct Impl;
 std::unique_ptr<Impl> impl_;
public:
 explicit Player(HWND parent);
 ~Player();
 Player(const Player&)=delete;
 Player& operator=(const Player&)=delete;
 // All methods and destruction belong to the constructing UI thread. pump()
 // is called once each frame, including while the parent loses foreground.
 // open() never starts playback; play() during loading reserves playback.
 void open(const std::filesystem::path& localPath);
 void play();
 // Stop releases the source/player, hides the child and returns to idle.
 // open() can create a fresh source after stop, ended or failed.
 void stop();
 // Focus loss also pauses automatically. While paused, an already started
 // presentation retains Status::playing; focus return honors this manual flag.
 void pause(bool paused);
 // Persist across stop/end/failure and reopen, including calls before open().
 // The parent supplies its existing global sound option through mute(!sound).
 void mute(bool muted);
 // Fit a 16:9 child inside this parent-client rectangle. User-selected source
 // stretching fills that child; outer borders on non-16:9 windows stay black.
 void place(int x,int y,int width,int height);
 void visible(bool show);
 void pump();
 Status status() const;
 std::string error() const;
};
}
