#include "weapon_hand_preview.h"
#include "combat_action_preview.h"
#include "build_version.h"
#include "local_playtest.h"
#include "gekko_preview.h"
#include "combat_audio_bundle.h"
#include "stage_water.h"
// Desktop entry point. No Python, IDA or asset conversion tool is used at runtime.
#include <windows.h>
#include "weapon_icons.h"
#include <algorithm>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include <vector>
#include <stdexcept>
#include <cmath>
#include "http_text.h"
#include "authentication.h"
#include "character_client.h"
#include "stun.h"
#include "port_settings.h"

int run_title_preview(int, wchar_t**);
namespace fs = std::filesystem;

// Explicit diagnostic only. The environment contains a file path, never credentials.
// Report appearance bytes without account IDs, character IDs/names or session data.
static int inspect_account(const fs::path&keys){
    wchar_t path[32768]{};auto n=GetEnvironmentVariableW(L"MGO2WIN_ACCOUNT_FILE",path,32768);
    if(!n||n>=32768)throw std::runtime_error("Account diagnostic file path missing");
    std::ifstream in(fs::path(path),std::ios::binary|std::ios::ate);
    if(!in||in.tellg()<1||in.tellg()>4096)throw std::runtime_error("Account diagnostic file extent");
    std::string raw(static_cast<size_t>(in.tellg()),'\0');in.seekg(0);in.read(raw.data(),raw.size());
    std::array<std::wstring,2> fields;
    struct Wipe {std::string&r;std::array<std::wstring,2>&f;~Wipe(){SecureZeroMemory(r.data(),r.size());for(auto&s:f)SecureZeroMemory(s.data(),s.size()*sizeof(wchar_t));}} wipe{raw,fields};
    if(!in)throw std::runtime_error("Account diagnostic file read");
    unsigned count=0;size_t at=0;
    while(at<raw.size()){
        auto end=raw.find('\n',at);if(end==std::string::npos)end=raw.size();
        std::string_view line(raw.data()+at,end-at);at=end+1;
        while(!line.empty()&&(line.back()=='\r'||line.back()==' '||line.back()=='\t'))line.remove_suffix(1);
        if(line.empty())continue;
        auto colon=line.find(':');if(colon==line.npos||count>=2)throw std::runtime_error("Account diagnostic file format");
        line.remove_prefix(colon+1);while(!line.empty()&&(line.front()==' '||line.front()=='\t'))line.remove_prefix(1);
        if(line.empty()||line.size()>64||std::any_of(line.begin(),line.end(),[](unsigned char c){return c<33||c>126;}))throw std::runtime_error("Account diagnostic field format");
        fields[count++].assign(line.begin(),line.end());
    }
    if(count!=2)throw std::runtime_error("Account diagnostic requires two fields");
    mgo2win::AuthCredentials credentials(fields[0],fields[1]);
    SecureZeroMemory(raw.data(),raw.size());for(auto&s:fields)SecureZeroMemory(s.data(),s.size()*sizeof(wchar_t));
    std::atomic_bool cancel{false};auto auth=mgo2win::authenticate(credentials,cancel);
    std::cout<<"{\"account_read_probe\":true,\"auth_status\":"<<int(auth.status)<<",\"http\":"<<auth.http<<"}"<<std::endl;
    if(auth.status!=mgo2win::AuthStatus::success)return 1;
    auto reply=mgo2win::fetch_characters(keys,auth,cancel);
    std::cout<<"{\"account_appearance_result\":true,\"status\":"<<int(reply.status)<<",\"stage\":"<<int(reply.stage)<<",\"error\":"<<reply.error<<",\"count\":"<<reply.list.entries.size()<<",\"slots\":"<<reply.list.slots<<",\"appearance\":[";
    bool first=true;for(const auto&e:reply.list.entries){if(!first)std::cout<<',';first=false;std::cout<<'[';for(unsigned i=0;i<e.appearance.size();++i){if(i)std::cout<<',';std::cout<<unsigned(e.appearance[i]);}std::cout<<']';}
    std::cout<<"],\"registration_sent\":false}"<<std::endl;
    return reply.status==mgo2win::CharacterStatus::success?0:1;
}

static std::string digest(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream || stream.tellg() < 0 || stream.tellg() > 128LL*1024*1024)
        throw std::runtime_error("Missing or oversized package file: " + path.filename().string());
    stream.seekg(0);
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    auto check = [](NTSTATUS status) { if (status < 0) throw std::runtime_error("SHA-256 provider failure"); };
    try {
        check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
        check(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
        std::vector<unsigned char> buffer(65536);
        while (stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || stream.gcount())
            check(BCryptHashData(hash, buffer.data(), static_cast<ULONG>(stream.gcount()), 0));
        if (!stream.eof()) throw std::runtime_error("Package file read failure");
        unsigned char bytes[32];
        check(BCryptFinishHash(hash, bytes, sizeof(bytes), 0));
        BCryptDestroyHash(hash); hash = nullptr;
        BCryptCloseAlgorithmProvider(algorithm, 0); algorithm = nullptr;
        std::ostringstream text;
        for (auto byte : bytes) text << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
        return text.str();
    } catch (...) {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}

static fs::path executable_folder() {
    std::vector<wchar_t> buffer(32768);
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!size || size >= buffer.size()) throw std::runtime_error("Executable path failure");
    return fs::path(std::wstring(buffer.data(), size)).parent_path();
}

static size_t verify_assets(const fs::path& root) {
    std::set<std::string> required{"lobbies.cfg","character/appearance.gwc","network.gnk","login/frame.m2pv","motion/animated.m2an","agreement/frame.m2pv","audio/lobby.gwa","audio/92.gwa","audio/93.gwa","audio/94.gwa","launch.cfg", "title.gwp", "audio/title.gwa", "title/animated.m2an", "audio/start.gwa", "loading/loading.m2an", "loading/images/0.dds", "loading/images/1.dds"};
    required.insert("movie_01.mp4");
    for(int i=0;i<6;++i)required.insert("login/images/"+std::to_string(i)+".dds");
    for(int g=0;g<2;++g)for(int v=0;v<8;++v)required.insert("voice/"+std::to_string(g)+"_"+std::to_string(v)+".gwa");
    for(int i=0;i<8;++i)required.insert("motion/images/"+std::to_string(i)+".dds");
    for(int i=0;i<6;++i)required.insert("agreement/images/"+std::to_string(i)+".dds");
    for (int i=0; i<10; ++i) required.insert("title/images/"+std::to_string(i)+".dds");
    required.insert("skill_catalog.tsv");
    required.insert("character/selection0.gwmot");
    required.insert("character/selection1.gwmot");
    required.insert("audio/salute.gwa");
    required.insert("audio/sop_native.wav");
    required.insert("audio/notification_native.wav");
    required.insert("character/special_male.gwmot");
    required.insert("character/evade.gwmot");
    required.insert("character/cover.gwmot");
    required.insert("special/gekko.gwc");
    required.insert("special/gekko.gwmot");
    required.insert("special/gekko_salute.gwmot");
    required.insert("special/gekko_traversal.gwmot");
    required.insert("special/gekko_step.wav");
    required.insert("special/gekko_salute.wav");
    for(auto name:{"hands.gwh","ak102.gwm","operator.gwm","ak102_secondary.gwm","operator_secondary.gwm"})required.insert(std::string("weapons/")+name);
    required.insert("skills/index.tsv");
    required.insert("skills/briefing.tsv");
    for(auto name:{"skill_star.png","participants.png","rules.png","deploy.png","skills.png","equipment.png","options.png"})
        required.insert(std::string("skills/")+name);
    const auto skillIcons=mgo2win::weapons::read_icon_index(root/L"skills/index.tsv");
    if(skillIcons.size()!=25)throw std::runtime_error("Incomplete skill icon index");
    for(unsigned id=1;id<=25;++id){auto it=skillIcons.find(uint16_t(id));if(it==skillIcons.end()||it->second!="skill_star.png")throw std::runtime_error("Unexpected skill icon mapping");}
    const auto briefingIcons=mgo2win::weapons::read_icon_index(root/L"skills/briefing.tsv");
    const std::vector<std::string> briefingNames{"start","map","rules","skills","host","options","quit"};
    if(briefingIcons.size()!=14)throw std::runtime_error("Incomplete briefing icon index");
    for(bool selected:{false,true})for(size_t i=0;i<briefingNames.size();++i){const auto name=briefingNames[i]+(selected?"_selected.png":"_normal.png");auto it=briefingIcons.find(uint16_t((selected?101:1)+i));if(it==briefingIcons.end()||it->second!=name)throw std::runtime_error("Unexpected briefing icon mapping");required.insert("skills/"+name);}
    if(fs::exists(root/L"briefing-map/index.tsv")){required.insert("briefing-map/index.tsv");const auto maps=mgo2win::weapons::read_icon_index(root/L"briefing-map/index.tsv");if(maps!=std::map<uint16_t,std::string>{{1,"n022a-online-map-0.png"},{2,"n022a-online-map-1.png"}})throw std::runtime_error("Unexpected briefing map layers");for(const auto&[id,name]:maps)required.insert("briefing-map/"+name);}
    if(fs::exists(root/L"stage/n022a.sky.gwm"))required.insert("stage/n022a.sky.gwm");
    if(fs::exists(root/L"stage/n022a.gwm"))required.insert("stage/n022a.gwm");
    for(auto name:{"weapon_catalog.tsv","stage/n022a.cbox.cfg","stage/n022a.tdm-spawns.cfg","character/player.gwmot"})if(fs::exists(root/name))required.insert(name);
    if(fs::exists(root/L"weapon-icons/index.tsv")){
        required.insert("weapon-icons/index.tsv");
        for(const auto&[id,name]:mgo2win::weapons::read_icon_index(root/L"weapon-icons/index.tsv"))required.insert("weapon-icons/"+name);
    }
    // Reuse runtime PCM/path validation; bind every listed local cue to the hash manifest.
    if(fs::exists(root/L"sfx/combat.txt")||fs::exists(root/L"sfx/body_impact_1369_v0.wav")||fs::exists(root/L"sfx/body_impact_8168_v0.wav")||fs::exists(root/L"sfx/ak102_10002_v0.wav")){
        auto files=mgo2win::combat::audio_bundle_files(root/L"sfx");
        if(!files)throw std::runtime_error("Invalid combat audio bundle");
        for(const auto& name:*files)required.insert(name.generic_string());
    }
    // New local candidate is a complete four-stage bundle. Legacy title/GWP
    // packages without any expansion entry retain their original manifest.
    if(fs::exists(root/L"bullet_penetration_profile.json")||fs::exists(root/L"item_drop_policy.json")||
       fs::exists(root/L"stage/n001a.gwm")||fs::exists(root/L"stage/n004a.gwm")||fs::exists(root/L"stage/n023a.gwm")||fs::exists(root/L"stage/n022a.dm-spawns.cfg")){
        required.insert("bullet_penetration_profile.json");required.insert("item_drop_policy.json");
        for(auto stage:{"n001a","n004a","n022a","n023a"})
            for(auto suffix:{".gwm",".bindings.cfg",".cbox.cfg",".collision.cfg",".lighting.cfg",".objects.cfg",".gww",".dm-spawns.cfg",".tdm-spawns-v2.cfg"})
                required.insert(std::string("stage/")+stage+suffix);
    }
    if(fs::exists(root/L"stage/n007a.gwm")||fs::exists(root/L"stage/n007a.objects.cfg")||fs::exists(root/L"stage/n007a.bindings.cfg")){
        for(auto suffix:{".gwm",".bindings.cfg",".cbox.cfg",".collision.cfg",".lighting.cfg",".objects.cfg",".gww",".dm-spawns.cfg",".tdm-spawns-v2.cfg"})required.insert(std::string("stage/n007a")+suffix);
        for(auto name:{"n007a_light_a0.gwm","n007a_light_b0.gwm","n007a_light_a0.hit.cfg","n007a_light_b0.hit.cfg"})required.insert(std::string("stage/objects/")+name);
    }
    for(auto name:{"stage/items/113.gwm","stage/items/140.gwm","stage/items/ibox_item_mid.gwm"})if(fs::exists(root/name))required.insert(name);
    if(fs::exists(root/L"hold-font/index.tsv")){
        required.insert("hold-font/index.tsv");
        for(const auto&[id,name]:mgo2win::weapons::read_icon_index(root/L"hold-font/index.tsv"))required.insert("hold-font/"+name);
    }
    for(auto name:{"n001a","n004a","n007a","n022a","n023a"}){
        const auto script=std::string("stage/")+name+".gcx-items.cfg";
        if(fs::exists(root/script))required.insert(script);
    }
    if(fs::exists(root/L"equipment-icons/index.tsv")){
        required.insert("equipment-icons/index.tsv");required.insert("equipment-icons/display.tsv");
        for(const auto&[id,name]:mgo2win::weapons::read_icon_index(root/L"equipment-icons/index.tsv"))required.insert("equipment-icons/"+name);
    }
    if(fs::exists(root/L"system-ui/index.tsv")){
        required.insert("system-ui/index.tsv");
        for(const auto&[id,name]:mgo2win::weapons::read_icon_index(root/L"system-ui/index.tsv"))required.insert("system-ui/"+name);
        required.insert("fonts/SCE-PS3-NR-R-JPN.TTF");required.insert("fonts/SCE-PS3-NR-B-JPN.TTF");
    }
    for(auto name:{"bgm/catalog.json","stage/n022a.placements.cfg","stage/props/0.gwm","stage/props/1.gwm","stage/props/2.gwm","stage/props/3.gwm","stage/props/4.gwm","stage/props/5.gwm"})if(fs::exists(root/name))required.insert(name);
    for(auto name:{"stage/n022a.lighting.cfg","stage/n022a.collision.cfg","stage/audio/env_s01a30l_01.gwa","stage/audio/env_s01a30l_04.gwa","stage/audio/env_s01a30l_05.gwa","stage/audio/env_s01a30l_07.gwa","stage/audio/env_s01a30l_08.gwa"})if(fs::exists(root/name))required.insert(name);
    if(fs::exists(root/L"stage/n022a.objects.cfg")||fs::exists(root/L"stage/n022a.bindings.cfg")){
        for(auto name:{"n022a.objects.cfg","n022a.bindings.cfg","n022a.cbox.cfg","n022a.gwm","n022a.collision.cfg","n022a.lighting.cfg"})required.insert(std::string("stage/")+name);
        for(auto name:{"s01a_car_a0_sk","s01a_car_a0_glass","s01a_car_b0_sk","s01a_car_b0_glass","s01a_drum_a0_sk","cbox_a_sk","cbox_a_kuzure_sk","s01a_btle_a0_sk","s01a_btle_b0_sk","s01a_btle_c0_sk","s01a_btle_d0_sk","s01a_btle_e0_sk"})required.insert(std::string("stage/objects/")+name+".gwm");
        for(auto key:{"982f38","982fb8","9ebb66","0ae4e5"})required.insert(std::string("stage/objects/geom_")+key+".collision.cfg");
        for(char c='a';c<='e';++c)required.insert(std::string("stage/objects/s01a_btle_")+c+"0_sk.hit.cfg");
        for(int i=0;i<6;++i)required.insert("stage/objects/blast_drum_"+std::to_string(i)+".hit.cfg");
    }
    // Optional finite water contacts are a separate validated file, never GWW depth.
    if(fs::exists(root/L"stage/n001a_surface.gws")){std::ifstream in(root/L"stage/n001a_surface.gws",std::ios::binary);mgo2win::stage::WaterSurface::read(in);required.insert("stage/n001a_surface.gws");}
    for(auto stage:{"n001a","n004a","n022a","n023a"}){const auto name=std::string("stage/")+stage+"_render.gws";if(fs::exists(root/name)){std::ifstream in(root/name,std::ios::binary);mgo2win::stage::WaterSurface::read(in);required.insert(name);}}
    if(fs::exists(root/L"sfx/native_water_step.wav"))required.insert("sfx/native_water_step.wav");
    for(auto stage:{"n001a","n004a","n007a","n022a","n023a"}){auto name=std::string("stage/")+stage+".gwn";if(fs::exists(root/name))required.insert(name);}
    const auto expectedCount=required.size();
    std::ifstream manifest(root / L"assets.sha256");
    if (!manifest) throw std::runtime_error("Missing data/assets.sha256. Keep the data folder beside MGO2WIN.exe.");
    std::string line;
    unsigned count = 0;
    while (std::getline(manifest, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (++count > expectedCount || line.size()<67 || line.substr(64,2)!="  ")
            throw std::runtime_error("Invalid asset manifest");
        auto hash = line.substr(0,64), name = line.substr(66);
        if (hash.find_first_not_of("0123456789abcdef")!=std::string::npos || required.erase(name)!=1)
            throw std::runtime_error("Unexpected or duplicate asset manifest entry");
        if (digest(root/fs::path(name)) != hash) throw std::runtime_error("Asset hash mismatch: "+name);
    }
    if (!manifest.eof() || !required.empty()) throw std::runtime_error("Incomplete asset manifest");
    return expectedCount;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command, int) {
    const std::wstring mode = command ? command : L"";
    if(mode==L"--version"){std::cout<<"MGO2WIN "<<mgo2win::build_version<<std::endl;return 0;}
    const auto playtest=mgo2win::LocalPlaytest::parse(mode);
    const bool shadowCapture=mode==L"--shadow-test-capture",hemisphereCapture=mode==L"--hemisphere-test-capture";
    const bool weaponHandCapture=mode==L"--weapon-hand-test-capture"||mode==L"--combat-action-test-capture"||shadowCapture||hemisphereCapture;
    const bool gekkoTest=mode==L"--gekko-test"||mode==L"--gekko-test-capture",gekkoCapture=mode==L"--gekko-test-capture";
    const bool smokeCreation=mode==L"--smoke-creation";
    const bool smokeSelection=mode==L"--smoke-selection";
    const bool smokeAppearance=mode==L"--smoke-appearance";
    const bool smokeSlots=mode==L"--smoke-slots";
    const bool smokeCharacters=mode==L"--smoke-characters",probeGate=mode==L"--probe-gate";
    const bool smokeLogin=mode==L"--smoke-login";
    const bool smokePorts=mode==L"--smoke-ports";
    const bool smokeControls=mode==L"--smoke-controls",smokeGraphics=mode==L"--smoke-graphics",safeGraphics=mode==L"--safe-graphics";
    const bool smokeStun=mode==L"--smoke-stun",probeStun=mode==L"--probe-stun";
    const bool probeAuth=mode==L"--probe-auth",probeAccount=mode==L"--probe-account";
    const bool smokeNo=mode==L"--smoke-no";
    const bool smoke = mode == L"--smoke-test"||smokeNo||smokeLogin||smokePorts||smokeStun||smokeControls||smokeGraphics||smokeCharacters||smokeSlots||smokeAppearance||smokeCreation||smokeSelection, checkOnly = mode == L"--check";
    std::ofstream log;
    auto* oldOut = std::cout.rdbuf(); auto* oldError = std::cerr.rdbuf();
    int result = 1;
    std::string error;
    try {
        if (!mode.empty() && !smoke && !checkOnly && !probeAuth && !probeAccount && !probeStun && !safeGraphics && !probeGate && !playtest.enabled && !gekkoTest && !weaponHandCapture) throw std::runtime_error("Supported options: --check, --smoke-test, --gekko-test, --gekko-test-capture, --probe-auth, --probe-account, --probe-stun");
        const auto root = executable_folder(), data = root/L"data";
        const auto verifiedFiles=verify_assets(data);
        std::ifstream config(data/L"launch.cfg");
        std::string tag, extra, policy; unsigned version=0, entry=0, sound=2; double seconds=0;
        if (!(config >> tag >> version >> seconds >> entry >> sound >> policy) || config >> extra ||
            tag!="MGO2WIN.TITLE" || version!=7 || !std::isfinite(seconds) || seconds<=0 || seconds>600 ||
            !entry || entry>32767 || sound>1) throw std::runtime_error("Invalid title launch settings");
        std::wstring policyUrl(policy.begin(),policy.end());
        if(!mgo2win::allowed_policy_url(policyUrl))throw std::runtime_error("Invalid OpenMGO2 policy URL");
        if (checkOnly) return 0;
        wchar_t appData[32768];
        DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", appData, 32768);
        if (!size || size>=32768) throw std::runtime_error("LOCALAPPDATA is unavailable");
        SYSTEMTIME now; GetSystemTime(&now);
        wchar_t runName[80];
        swprintf_s(runName, L"%04u%02u%02uT%02u%02u%02u_%03u_%lu", now.wYear, now.wMonth, now.wDay,
                   now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, GetCurrentProcessId());
        const auto run = playtest.profile(fs::path(appData)/L"MGO2WIN")/L"logs"/runName;
        fs::create_directories(run);
        log.open(run/L"run.log");
        if (!log) throw std::runtime_error("Cannot create the title log");
        std::cout.rdbuf(log.rdbuf()); std::cerr.rdbuf(log.rdbuf());
        std::cout << "{\"build_version\":\"" << mgo2win::build_version << "\"}" << std::endl;
        std::cout << "{\"desktop_package\":true,\"verified_files\":" << verifiedFiles << ",\"smoke_test\":" << (smoke?"true":"false") << "}" << std::endl;
        if(weaponHandCapture){result=(mode==L"--combat-action-test-capture"||shadowCapture||hemisphereCapture)?mgo2win::run_combat_action_preview(data,run/L"combat-action",shadowCapture,hemisphereCapture):mgo2win::run_weapon_hand_preview(data,run/L"weapon-hand");std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;}
        if(gekkoTest){result=mgo2win::run_gekko_preview(data,gekkoCapture,run/L"gekko");std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;}
        if(probeAccount){result=inspect_account(data/L"network.gnk");std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;}
        if(probeGate){std::atomic_bool cancel{false};auto r=mgo2win::probe_character_gate(data/L"network.gnk",cancel);result=r.status==mgo2win::CharacterStatus::success?0:1;std::cout<<"{\"gate_probe\":true,\"status\":"<<int(r.status)<<",\"stage\":"<<int(r.stage)<<",\"error\":"<<r.error<<",\"account_port\":"<<r.account_port<<"}"<<std::endl;std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;}
        if(probeAuth){
            auto response=mgo2win::probe_login_route();
            result=response.status==mgo2win::AuthStatus::denied?0:1;
            std::cout<<"{\"empty_auth_probe\":true,\"http_status\":"<<response.http<<",\"expected_denial\":"<<(result?"false":"true")<<"}"<<std::endl;
            std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;
        }
        if(probeStun){
            mgo2win::PortReservation socket;auto local=socket.check({true,5730});std::atomic_bool cancel{false};
            auto reply=local.status==mgo2win::PortStatus::available?mgo2win::check_stun(socket.native_socket(),cancel):mgo2win::StunResult{};
            result=reply.status==mgo2win::StunStatus::success?0:1;
            std::cout<<"{\"stun_probe\":true,\"success\":"<<(result?"false":"true")<<",\"status\":"<<int(reply.status)<<",\"local_port\":"<<local.port<<",\"mapped_port\":"<<reply.mapped_port<<",\"attempts\":"<<reply.attempts<<",\"error\":"<<reply.error<<",\"peer_inbound_tested\":false}"<<std::endl;
            std::cout.rdbuf(oldOut);std::cerr.rdbuf(oldError);return result;
        }
        std::vector<std::wstring> arguments{L"MGO2WIN", (data/L"title/animated.m2an").wstring(),
            smokeSelection?L"35":smokeCreation?L"25":smokeAppearance?L"25":smokeSlots?L"25":smokeGraphics?L"32":smokeCharacters?L"20":smoke?L"15":std::to_wstring(seconds), (run/L"title.bmp").wstring(),
            L"--gcx", (data/L"title.gwp").wstring(), L"--entry", std::to_wstring(entry),
            L"--wav", (data/L"audio/title.gwa").wstring(),L"--se",(data/L"audio/start.gwa").wstring(),L"--loading",(data/L"loading/loading.m2an").wstring()};
        arguments.insert(arguments.end(),{L"--voice-directory",(data/L"voice").wstring(),L"--character-catalog",(data/L"character/appearance.gwc").wstring(),L"--network-keys",(data/L"network.gnk").wstring(),L"--login-background",(data/L"login/frame.m2pv").wstring(),L"--agreement-motion",(data/L"motion/animated.m2an").wstring(),L"--agreement-background",(data/L"agreement/frame.m2pv").wstring(),L"--lobby-music",(data/L"audio/lobby.gwa").wstring(),L"--policy-url",policyUrl,L"--menu-cancel",(data/L"audio/92.gwa").wstring(),L"--menu-confirm",(data/L"audio/93.gwa").wstring(),L"--menu-move",(data/L"audio/94.gwa").wstring()});
        if(playtest.enabled)arguments.push_back(mode);
        if (sound) arguments.push_back(L"--audio");
        if(safeGraphics)arguments.push_back(L"--safe-graphics");
        arguments.push_back(smokeSelection?L"--scripted-selection":smokeCreation?L"--scripted-creation":smokeAppearance?L"--scripted-appearance":smokeSlots?L"--scripted-slots":smokeCharacters?L"--scripted-characters":smokeGraphics?L"--scripted-graphics":smokeControls?L"--scripted-controls":smokeNo?L"--scripted-no":smokeStun?L"--scripted-stun":smokePorts?L"--scripted-ports":smokeLogin?L"--scripted-login":smoke?L"--scripted-input":L"--no-capture");
        std::vector<wchar_t*> pointers;
        for (auto& argument : arguments) pointers.push_back(argument.data());
        result = run_title_preview(static_cast<int>(pointers.size()), pointers.data());
        if (result) error = "Title playback failed. See the log in %LOCALAPPDATA%\\MGO2WIN\\logs.";
    } catch (const std::exception& failure) {
        error = failure.what();
        if (log.is_open()) log << error << std::endl;
        else std::cerr << error << std::endl;
    }
    std::cout.rdbuf(oldOut); std::cerr.rdbuf(oldError);
    if (!error.empty() && !smoke && !checkOnly && !probeAccount && !gekkoCapture) {
        auto text = std::wstring(error.begin(), error.end());
        text += L"\n\n起動できませんでした。dataフォルダーを含めて再生成してください。\nPlease rebuild the complete local package, including its data folder.";
        MessageBoxW(nullptr, text.c_str(), L"MGO2WIN", MB_OK|MB_ICONERROR);
    }
    return result;
}
