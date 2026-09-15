#pragma once
#include "notification_wire.h"
#include <filesystem>
#include <memory>
#include <span>
namespace mgo2win::alert_media {
std::vector<uint8_t> verified_bytes(const std::filesystem::path& directory,const notices::Media&,bool video);
struct Frame {uint32_t width=0,height=0;uint64_t index=0;std::vector<uint32_t> pixels;};
struct Status {bool running=false,finished=false;uint64_t decodedFrames=0;std::string error;
 bool cancelled=false,audioValidated=false,audioStarted=false;std::string audioError,videoError;
};
class Player {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 explicit Player(std::filesystem::path directory);~Player();
 // Native optional assets: immutable SHA-checked bytes, one presentation per
 // alert revision and connection; no network downloads or shell/URL launches.
 // sound=false still validates a supplied WAV, but never opens an audio device.
 // Cancellation prevents subsequent audio/frame publication and stops an active
 // voice. clear returns without joining; select defers new playback while a
 // cancelled worker retires, so callers repeat select on presentation updates.
 // At most one codec worker exists. Destruction still joins and synchronous MF
 // initialization/ReadSample can delay process exit: no hard-cancel guarantee.
 // Presentation mutation (select/clear) is single-thread owned. frame/status
 // return snapshots; connection history saturates at128 revisions, never evicts.
 void select(uint64_t scope,const std::optional<notices::Alert>&,uint64_t unixNowMs,uint64_t monotonicNowMs,bool sound);
 void clear();
 std::shared_ptr<const Frame> frame()const;
 Status status()const;
 bool paint(std::span<uint32_t> pixels,int width,int height,int left=920,int top=154)const;
};
}
