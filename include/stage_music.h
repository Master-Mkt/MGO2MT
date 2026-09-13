#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
namespace mgo2win::stage {
struct Track {std::string id;std::wstring title;std::filesystem::path path;bool additional=false;};
struct MusicLibrary {
 std::vector<Track> tracks;unsigned rejected=0,overflow=0,playlistErrors=0;
 static MusicLibrary scan(const std::filesystem::path&);
 void reload_titles(const std::filesystem::path&);
 const Track* find(const std::string&)const;
};
struct MusicChoice {const Track* track=nullptr;bool missingForced=false;};
// This is the audio-thread replacement gate. A selection/respawn or title edit
// cannot restart the current voice unless the resolved ID or immutable layer
// layout changes. A preset/gain change keeps the same sample position.
class MusicPlayback {
 std::string id_;
 std::filesystem::path alternate_;
public:
 bool select(const Track* track,const std::filesystem::path& alternate={}){std::string next=track?track->id:std::string{};if(next==id_&&alternate==alternate_)return false;id_=std::move(next);alternate_=alternate;return true;}
 void clear(){id_.clear();alternate_.clear();}
};
class MusicSelection {
 std::string selected_,pending_;std::optional<std::string> forced_;
public:
 bool choose(const MusicLibrary&,const std::string&,bool debug,bool respawning);
 void respawn();
 void force(std::optional<std::string> id){forced_=std::move(id);}
 MusicChoice resolve(const MusicLibrary&)const;
 const std::string& selected()const{return selected_;}
};
}
