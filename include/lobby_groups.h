#pragma once
#include "character_client.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>

namespace mgo2win {
// Native presentation order. Original UI selectors 0x98BDB4..0x98BF1C
// write modes 4,3,10,7,1,2. Combat (server subtype 8) shares Training here.
enum class LobbyGroup : unsigned {automatching,free_battle,training,survival,tournament,registration,unknown};
inline LobbyGroup lobby_group(uint8_t subtype){
 switch(subtype){case 2:return LobbyGroup::automatching;case 1:return LobbyGroup::free_battle;
 case 7:case 8:return LobbyGroup::training;case 4:return LobbyGroup::survival;
 case 3:return LobbyGroup::tournament;case 10:return LobbyGroup::registration;default:return LobbyGroup::unknown;}
}
inline const wchar_t* lobby_group_name(unsigned group){
 static constexpr const wchar_t* names[]={L"オートマッチング",L"フリーバトル",L"トレーニング",L"サバイバル",L"トーナメント",L"トーナメント受付",L"未分類"};
 return names[group<7?group:6];
}
struct LobbyMembership {uint16_t id,port;uint8_t subtype;};
inline std::vector<LobbyMembership> read_lobby_membership(std::istream& in){
 auto token=[&]{std::string s;if(!(in>>s)||s.size()>64)throw std::runtime_error("lobby membership token");return s;};
 auto number=[&](unsigned min,unsigned max){auto s=token();unsigned v=0;auto r=std::from_chars(s.data(),s.data()+s.size(),v);
  if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||v<min||v>max)throw std::runtime_error("lobby membership number");return v;};
 if(token()!="MGO2WIN.LOBBIES"||number(1,1)!=1||token()!="49.212.132.180")throw std::runtime_error("lobby membership contract");
 auto count=number(0,256);std::vector<LobbyMembership> rows;std::set<unsigned> ids;
 for(unsigned i=0;i<count;++i){auto id=number(1,65535),port=number(1,65535),subtype=number(0,255);
  if(!ids.insert(id).second)throw std::runtime_error("duplicate lobby membership");rows.push_back({uint16_t(id),uint16_t(port),uint8_t(subtype)});}
 std::string extra;if(in>>extra)throw std::runtime_error("trailing lobby membership");return rows;
}
inline std::vector<LobbyMembership> load_lobby_membership(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>16384)throw std::runtime_error("lobby membership size");
 std::ifstream in(path);return read_lobby_membership(in);
}
// A reviewed configuration snapshot, not a field decoded from the gate reply.
// Do not guess from names; keep unknown/new endpoints visible as unclassified.
inline void apply_lobby_membership(std::vector<GameLobbyEntry>& games,const std::vector<LobbyMembership>& rows){
 for(auto& game:games){game.subtype=0;for(const auto& row:rows)if(game.id==row.id&&game.port==row.port){game.subtype=row.subtype;break;}}
}
inline std::vector<size_t> lobby_group_rows(const std::vector<GameLobbyEntry>& games,unsigned group){
 std::vector<size_t> rows;for(size_t i=0;i<games.size();++i)if(unsigned(lobby_group(games[i].subtype))==group)rows.push_back(i);return rows;
}
inline unsigned lobby_group_count(const std::vector<GameLobbyEntry>& games){
 return std::any_of(games.begin(),games.end(),[](const auto& g){return lobby_group(g.subtype)==LobbyGroup::unknown;})?7:6;
}
}
