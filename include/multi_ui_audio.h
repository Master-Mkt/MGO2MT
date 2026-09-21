#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace mgo2mt::multi_ui {
enum class AudioResult {started,completed,failed};
struct AudioNotification {uint64_t token=0;AudioResult result=AudioResult::failed;std::string message;};
// Completion is a media callback, never a duration estimate. Injectable so
// ordering, cancellation and stale callback tests do not need a sound device.
class AudioBackend {
public:
 virtual ~AudioBackend()=default;
 virtual bool start(const std::filesystem::path&,float volume,uint64_t token,std::string& error)=0;
 virtual std::vector<AudioNotification> poll()=0;
 virtual void stop()=0;
 virtual void mute(bool)=0;
};
std::unique_ptr<AudioBackend> make_audio_backend();
}
