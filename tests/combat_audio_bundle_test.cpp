#include "combat_audio_bundle.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace mgo2win::combat;
namespace fs = std::filesystem;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const fs::path& path, const std::string& bytes) {
    std::ofstream f(path, std::ios::binary); f.write(bytes.data(), std::streamsize(bytes.size()));
    check(bool(f), "fixture write");
}
std::string wave() {
    std::string b;
    auto put = [&](uint32_t n, unsigned width) { for (unsigned i=0;i<width;++i) b+=char(n>>(8*i)); };
    b+="RIFF";put(40,4);b+="WAVEfmt ";put(16,4);put(1,2);put(1,2);
    put(48000,4);put(96000,4);put(2,2);put(16,2);b+="data";put(4,4);put(0,4);return b;
}
int main(int argc, char** argv) {
    const auto root=fs::temp_directory_path()/("mgo2win-audio-bundle-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::array<const char*,5> names{"body_impact_1369_v0.wav","body_impact_8168_v0.wav",
        "ak102_10002_v0.wav","extra.wav","combat.txt"};
    auto cleanup=[&] { std::error_code ec;for(const auto* name:names)fs::remove(root/name,ec);fs::remove(root,ec); };
    try {
        check(argc==2,"provide real 22-cue sfx directory");
        const auto real=audio_bundle_files(fs::path(argv[1]));
        check(real && real->size()==23,"real 22-cue bundle accepted with manifest");
        check((*real)[0]==fs::path("sfx/combat.txt"),"relative manifest included");
        for(const auto& p:*real)check(p.is_relative()&&p.parent_path()=="sfx","only relative sfx files returned");
        check(fs::create_directory(root),"exclusive fixture directory");
        for(unsigned i=0;i<4;++i)write(root/names[i],wave());
        const std::string required="1369 body_impact_1369_v0.wav\n8168 body_impact_8168_v0.wav\n";
        auto manifest=[&](unsigned count,const std::string& rows) {
            write(root/"combat.txt","MGO2WIN.COMBAT_AUDIO 1 "+std::to_string(count)+"\n"+rows);
        };
        manifest(2,required);check(!audio_bundle_files(root),"unbound legacy AK file rejected");fs::remove(root/names[2]);auto files=audio_bundle_files(root);
        check(files&&files->size()==3,"legacy two cue bundle");
        write(root/names[2],wave());manifest(3,required+"10002 ak102_10002_v0.wav\n");files=audio_bundle_files(root);
        check(files&&files->size()==4,"legacy three cue bundle");
        manifest(4,required+"10002 ak102_10002_v0.wav\n8071 extra.wav\n");
        check(audio_bundle_files(root).has_value(),"additional checked cue allowed");
        manifest(2,"1369 body_impact_8168_v0.wav\n8168 body_impact_1369_v0.wav\n");
        check(!audio_bundle_files(root),"required binding swap rejected");
        manifest(1,"1369 body_impact_1369_v0.wav\n");check(!audio_bundle_files(root),"missing required cue rejected");
        manifest(3,required+"10002 extra.wav\n");check(!audio_bundle_files(root),"optional AK binding fixed");
        manifest(3,required+"8071 ../extra.wav\n");check(!audio_bundle_files(root),"traversal rejected");
        manifest(3,required+"8071 C:/extra.wav\n");check(!audio_bundle_files(root),"absolute path rejected");
        manifest(3,required+"1369 extra.wav\n");check(!audio_bundle_files(root),"duplicate cue rejected");
        manifest(3,required+"8071 missing.wav\n");check(!audio_bundle_files(root),"missing WAV rejected");
        manifest(3,required+"8071 extra.wav\n");write(root/"extra.wav","not PCM");
        check(!audio_bundle_files(root),"malformed additional WAV rejected");write(root/"extra.wav",wave());
        fs::remove(root/names[2]);std::string rows=required;
        for(unsigned i=0;i<126;++i)rows+=std::to_string(100+i)+" extra.wav\n";
        manifest(128,rows);files=audio_bundle_files(root);
        check(files&&files->size()==4,"128 valid cues and shared file deduplication");
        manifest(129,rows+"999 extra.wav\n");check(!audio_bundle_files(root),"129 cues rejected");
        manifest(2,required+"extra token");check(!audio_bundle_files(root),"manifest tail rejected");
        manifest(2,required);fs::remove(root/names[1]);check(!audio_bundle_files(root),"missing required WAV rejected");
        cleanup();std::cout<<"combat_audio_bundle_test PASS\n";return 0;
    } catch(const std::exception& e) {cleanup();std::cerr<<e.what()<<'\n';return 1;}
}
