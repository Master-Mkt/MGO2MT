#include "product_identity.h"
#include "gcx_round_items.h"
#include "stage_profiles.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace mgo2mt::items {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
uint32_t number(std::istream& in,uint32_t maximum=0xffffffffu){
 std::string text;require(bool(in>>text),"Missing GCX integer");uint32_t n=0;
 auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),n);
 require(error==std::errc{}&&end==text.data()+text.size()&&n<=maximum,"Invalid GCX integer");return n;
}
float scalar(std::istream& in){
 std::string text;require(bool(in>>text),"Missing GCX coordinate");float n=0;
 auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),n);
 require(error==std::errc{}&&end==text.data()+text.size()&&std::isfinite(n)&&std::abs(n)<1000000,"Invalid GCX coordinate");return n;
}
void end_row(std::istream& in){std::string extra;require(!(in>>extra),"Unexpected GCX field");}
bool position_valid(const Position& p){for(float v:{p.x,p.y,p.z,p.yaw})if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
void validate(const GcxItemLayout& layout){
 require(stage::runtime_stage_supported(layout.map)&&layout.groups.size()<=512,"Invalid GCX map/groups");
 require(layout.verified||layout.groups.empty(),"Unverified GCX layout contains placements");
 std::set<std::pair<uint8_t,uint32_t>> groups,offsets;size_t total=0;
 for(const auto& g:layout.groups){
  require(g.rule<=1&&g.group&&groups.emplace(g.rule,g.group).second,"Invalid or duplicate GCX group");
  // Confirmed constructor-to-runtime namespace joins, not weapon IDs.
  require(g.domain==Domain::equipment&&((g.worldType==113&&g.item==10)||(g.worldType==140&&g.item==22)),"Unresolved GCX world item mapping");
  require(!g.anchors.empty()&&g.anchors.size()<=64&&(total+=g.anchors.size())<=4096,"Invalid GCX anchor count");
  for(const auto& a:g.anchors)require(a.sourceOffset&&a.key<=0xffffff&&position_valid(a.position)&&offsets.emplace(g.rule,a.sourceOffset).second,"Invalid or duplicate GCX source anchor");
 }
}
}

GcxItemLayout GcxItemLayout::read(std::istream& in){
 GcxItemLayout layout;std::string line;size_t bytes=0;bool header=false,counts=false;
 uint32_t groupCount=0,pending=0;
 while(std::getline(in,line)){
  bytes+=line.size()+1;require(bytes<=1048576&&line.size()<=1024,"GCX file extent");
  std::istringstream row(line);std::string key;if(!(row>>key)||key.front()=='#')continue;
  if(!header){
   require(key==mgo2mt::brand::Format{"MGO2MT.GCX_ROUND_ITEMS"}&&number(row)==1,"GCX layout version");
   layout.map=uint8_t(number(row,255));std::string state;require(bool(row>>state)&&(state=="verified"||state=="unavailable"),"GCX verification state");
   layout.verified=state=="verified";end_row(row);header=true;continue;
  }
  if(!counts){require(key=="groups","GCX group count required");groupCount=number(row,512);end_row(row);counts=true;continue;}
  if(pending){
   require(key=="anchor","GCX anchor required");GcxPickupAnchor a;a.sourceOffset=number(row);a.key=number(row,0xffffff);
   a.position={scalar(row),scalar(row),scalar(row),scalar(row)};end_row(row);layout.groups.back().anchors.push_back(a);--pending;continue;
  }
  require(key=="group"&&layout.groups.size()<groupCount,"Unexpected GCX group");GcxPickupGroup g;
  g.rule=uint8_t(number(row,1));g.group=number(row);g.worldType=number(row);
  std::string domain;require(bool(row>>domain)&&domain=="equipment","GCX source namespace unresolved");g.domain=Domain::equipment;
  g.item=number(row,65535);pending=number(row,64);require(pending>0,"Empty GCX candidate group");end_row(row);layout.groups.push_back(g);
 }
 require(in.eof()&&!in.bad()&&header&&counts&&!pending&&layout.groups.size()==groupCount,"Incomplete GCX layout");validate(layout);return layout;
}

std::optional<GcxItemLayout> load_gcx_item_layout(const std::filesystem::path& root,uint8_t map){
 const auto* profile=stage::runtime_profile(map);require(profile!=nullptr,"Unknown GCX stage route");
 const auto path=root/(std::string(profile->stage)+".gcx-items.cfg");std::error_code error;
 const auto status=std::filesystem::symlink_status(path,error);
 if(error==std::errc::no_such_file_or_directory||(!error&&status.type()==std::filesystem::file_type::not_found))return {};
 require(!error&&std::filesystem::is_regular_file(status)&&!std::filesystem::is_symlink(status),"Invalid GCX sidecar file");
 const auto size=std::filesystem::file_size(path,error);require(!error&&size>0&&size<=1048576,"GCX sidecar extent");
 std::ifstream in(path,std::ios::binary);require(bool(in),"Cannot read GCX sidecar");auto layout=GcxItemLayout::read(in);
 require(layout.map==map,"GCX sidecar map mismatch");return layout;
}

void replace_pickup_item(RoundItems& settings,const GcxItemLayout& layout,uint32_t originalItem,Domain domain,uint32_t replacementItem){
 validate(layout);require(layout.verified&&(originalItem==22||originalItem==10)&&domain<=Domain::equipment&&replacementItem&&replacementItem<=65535,"Invalid pickup replacement choice");
 std::set<uint32_t> offsets;
 for(const auto& group:layout.groups)if(group.item==originalItem)for(const auto& anchor:group.anchors)offsets.insert(anchor.sourceOffset);
 require(!offsets.empty(),"Original pickup source unavailable for this stage");
 for(const auto& group:layout.groups)if(group.item!=originalItem)for(const auto& anchor:group.anchors)
  require(!offsets.contains(anchor.sourceOffset),"Pickup offset shared by different source items");
 auto next=settings;
 std::erase_if(next.replacements,[&](const auto&r){return r.map==layout.map&&r.source==GcxSource::pickup&&offsets.contains(r.sourceOffset);});
 require(std::none_of(next.replacements.begin(),next.replacements.end(),[&](const auto&r){return r.map==layout.map&&r.source==GcxSource::pickup&&!r.sourceOffset;}),"Wildcard pickup replacement must be resolved before editing");
 if(domain!=Domain::equipment||replacementItem!=originalItem)
  for(auto offset:offsets)next.replacements.push_back({layout.map,GcxSource::pickup,offset,domain,replacementItem});
 require(next.valid(),"Invalid pickup replacement settings");settings=std::move(next);
}

GcxRoundPlan plan_gcx_round_items(const RoundItems& settings,const GcxItemLayout* layout,
 const stage::CboxLayout& cbox,uint8_t map,uint8_t rule,uint8_t generation){
 require(settings.valid()&&stage::runtime_stage_supported(map)&&rule<=1,"Invalid GCX round request");
 GcxRoundPlan result;if(!settings.useGcx){result.verified=true;return result;}
 if(!layout)return result;validate(*layout);require(layout->map==map,"GCX round map mismatch");if(!layout->verified)return result;
 result.verified=true;
 // Validate requested offsets against all original candidates, including
 // candidates not selected this generation. Exact overrides beat wildcard.
 for(const auto& r:settings.replacements)if(r.map==map&&r.sourceOffset){
  bool found=false;
  if(r.source==GcxSource::pickup){for(const auto& g:layout->groups)for(const auto& a:g.anchors)found=found||a.sourceOffset==r.sourceOffset;}
  else for(const auto& a:cbox.anchors)found=found||a.sourceOffset==r.sourceOffset;
  require(found,"GCX replacement refers to an unknown source offset");
 }
 auto replacement=[&](GcxSource source,uint32_t offset){
  const GcxReplacement* selected=nullptr;
  for(const auto& r:settings.replacements)if(r.map==map&&r.source==source){if(r.sourceOffset==offset)return &r;if(!r.sourceOffset)selected=&r;}
  return selected;
 };
 // Native reproducible seed: original pickup RNG is clock seeded, unlike
 // CBOX. This does not claim original clock timing or probability parity.
 uint32_t state=uint32_t(generation)|(uint32_t(map)<<8)|(uint32_t(rule)<<16);
 for(const auto& g:layout->groups)if(g.rule==rule){
  state=state*0x5d588b65u+1u;const auto& a=g.anchors[(state>>8)%g.anchors.size()];
  GcxPlacement p{GcxSource::pickup,a.sourceOffset,g.domain,g.item,a.position,{}};
  if(const auto* r=replacement(p.source,p.sourceOffset)){p.domain=r->domain;p.item=r->item;}result.items.push_back(p);
 }
 // Selected ordinal is the dynamic binding suffix. candidateIndex is not.
 const auto selected=cbox.select(generation);
 for(uint32_t i=0;i<selected.size();++i){const auto& c=selected[i];if(const auto* r=replacement(GcxSource::cbox,c.anchor.sourceOffset)){
  result.items.push_back({GcxSource::cbox,c.anchor.sourceOffset,r->domain,r->item,
    {c.anchor.position[0],c.anchor.position[1],c.anchor.position[2],c.rotationRadians},i});
  result.removeCboxBindings.push_back(0xc0000000u+i);
 }}
 require(result.items.size()<=4096,"GCX plan exceeds item capacity");return result;
}

std::vector<Seed> resolve_gcx_round_items(const GcxRoundPlan& plan,const stage::Collision& collision,const ItemTemplate& lookup){
 require(plan.verified&&bool(lookup)&&plan.items.size()<=4096,"Unverified GCX item plan");std::vector<Seed> result;
 std::set<std::pair<GcxSource,uint32_t>> sources;
 for(const auto& p:plan.items){
  require(p.source<=GcxSource::cbox&&p.sourceOffset&&p.domain<=Domain::equipment&&p.item&&p.item<=65535&&position_valid(p.position)&&sources.emplace(p.source,p.sourceOffset).second,"Invalid GCX placement");
  const auto contents=lookup(p.domain,p.item);require(contents&&contents->domain==p.domain&&contents->item==p.item&&contents->quantity==1,"Unresolved GCX item template");
  // n022a GEOM CBOX anchors include Y=0/50 while the movement-floor record
  // is at Y=125 (e.g. source offsets 5561248/5560480). A bounded 150-unit
  // upward tolerance covers that source offset; never search sideways or
  // start at a roof far above the original anchor.
  const auto& a=p.position;auto floor=collision.ray({a.x,a.y+150,a.z},{0,-1,0},3200,stage::query::floor);
  if(!floor||floor->normal[1]<.707f)throw std::runtime_error("GCX source has no walkable floor: offset="+std::to_string(p.sourceOffset)+" xyz="+std::to_string(a.x)+","+std::to_string(a.y)+","+std::to_string(a.z)+(floor?" hitY="+std::to_string(floor->position[1])+" normalY="+std::to_string(floor->normal[1]):" no hit"));
  Position resolved{a.x,floor->position[1]+12,a.z,a.yaw};
  require(collision.clear({resolved.x,resolved.y,resolved.z},{25,60,2}),"GCX source is obstructed");
  require(std::none_of(result.begin(),result.end(),[&](const Seed& s){return std::hypot(s.position.x-resolved.x,s.position.z-resolved.z)<150&&std::abs(s.position.y-resolved.y)<200;}),"GCX source placements overlap");
  result.push_back({*contents,resolved});
 }
 return result;
}
}
