#pragma once
#include "unicode_character_name.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mgo2win::names {
inline constexpr uint16_t account_request_opcode=0x30e0,account_reply_opcode=0x30e1;
inline constexpr uint16_t game_request_opcode=0x40e0,game_reply_opcode=0x40e1;
inline constexpr uint16_t create_request_opcode=0x30e2,create_reply_opcode=0x30e3;
inline constexpr uint32_t magic=0x47574e4d,capability_ids=1,capability_own=2,capability_create=4;
inline constexpr unsigned version=1,maximum_records=8,maximum_scalars=16,maximum_name_bytes=64;
inline constexpr size_t query_header_bytes=20,reply_header_bytes=24,maximum_reply_bytes=600,appearance_bytes=27;
enum class Operation:uint8_t {caps=0,ids=1,own=2,create=3};
struct Record {uint32_t id=0;std::string name;bool main=false;bool operator==(const Record&)const=default;};
struct Reply {
 uint8_t status=0;Operation op=Operation::caps;uint64_t nonce=0;uint32_t capabilities=0;
 std::vector<Record> records;uint32_t result=0,createdId=0;
};
namespace detail {
[[noreturn]] inline void invalid(){throw std::invalid_argument("invalid native character-name frame");}
inline uint64_t read(std::span<const uint8_t> b,size_t at,unsigned n){
 if(at>b.size()||n>b.size()-at)invalid();uint64_t value=0;for(unsigned i=0;i<n;++i)value=(value<<8)|b[at+i];return value;
}
inline void put(std::vector<uint8_t>& out,uint64_t value,unsigned n){for(unsigned i=n;i;--i)out.push_back(uint8_t(value>>((i-1)*8)));}
// Read names may predate the current creation minimum/reserved-name rules.
// Strict scalar decoding; no normalization, trimming or MAIN-marker removal.
inline bool display_name(std::string_view s){
 if(s.empty()||s.size()>maximum_name_bytes)return false;unsigned count=0;
 for(size_t at=0;at<s.size();){
  uint32_t cp=uint8_t(s[at++]),minimum=0;unsigned extra=0;
  if(cp<0x80){}
  else if(cp>=0xc2&&cp<=0xdf){cp&=31;minimum=0x80;extra=1;}
  else if(cp>=0xe0&&cp<=0xef){cp&=15;minimum=0x800;extra=2;}
  else if(cp>=0xf0&&cp<=0xf4){cp&=7;minimum=0x10000;extra=3;}
  else return false;
  if(extra>s.size()-at)return false;
  for(unsigned i=0;i<extra;++i){const auto c=uint8_t(s[at++]);if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
  if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||cp<32||(cp>=127&&cp<=159)||++count>maximum_scalars)return false;
 }
 return count>0;
}
inline std::vector<uint8_t> header(Operation op,uint64_t nonce,uint16_t first,uint16_t second){
 if(!nonce)invalid();std::vector<uint8_t> out;out.reserve(query_header_bytes+maximum_name_bytes+appearance_bytes);
 put(out,magic,4);out.push_back(version);out.push_back(uint8_t(op));put(out,0,2);put(out,nonce,8);put(out,first,2);put(out,second,2);return out;
}
}
inline std::vector<uint8_t> query(Operation op,uint64_t nonce,std::span<const uint32_t> ids={}){
 if(op!=Operation::caps&&op!=Operation::ids&&op!=Operation::own)detail::invalid();
 if(op==Operation::ids){if(ids.empty()||ids.size()>maximum_records)detail::invalid();}
 else if(!ids.empty())detail::invalid();
 auto out=detail::header(op,nonce,uint16_t(ids.size()),0);
 for(size_t i=0;i<ids.size();++i){if(!ids[i]||ids[i]>0x7fffffffu||std::find(ids.begin(),ids.begin()+i,ids[i])!=ids.begin()+i)detail::invalid();detail::put(out,ids[i],4);}
 return out;
}
inline std::vector<uint8_t> create(uint64_t nonce,std::string_view name,std::span<const uint8_t> appearance){
 if(!unicode_character_name::validate(name)||appearance.size()!=appearance_bytes)detail::invalid();
 auto out=detail::header(Operation::create,nonce,uint16_t(name.size()),uint16_t(appearance.size()));
 out.insert(out.end(),name.begin(),name.end());out.insert(out.end(),appearance.begin(),appearance.end());return out;
}
inline Reply parse(std::span<const uint8_t> bytes){
 if(bytes.size()<reply_header_bytes||bytes.size()>maximum_reply_bytes||detail::read(bytes,0,4)!=magic||bytes[4]!=version)detail::invalid();
 if(bytes[5]>4||bytes[6]>uint8_t(Operation::create))detail::invalid();
 Reply reply;reply.status=bytes[5];reply.op=Operation(bytes[6]);reply.nonce=detail::read(bytes,8,8);reply.capabilities=uint32_t(detail::read(bytes,20,4));
 const auto count=detail::read(bytes,16,2);
 if(reply.status){
  // Invalid request headers report op0/nonce0. Valid headers echo both. The
  // caller still binds the expected opcode, operation, nonce and scope.
  if(count||bytes[7]||bytes[18]||bytes[19]||reply.capabilities||(!reply.nonce&&reply.op!=Operation::caps))detail::invalid();
  if(bytes.size()==32){reply.result=uint32_t(detail::read(bytes,24,4));reply.createdId=uint32_t(detail::read(bytes,28,4));
   if(reply.createdId||(reply.status==4?!reply.result:reply.result!=0))detail::invalid();
  }else if(bytes.size()!=24||reply.status==4)detail::invalid();
  return reply;
 }
 if(bytes[7]!=1||bytes[18]!=maximum_scalars||bytes[19]!=maximum_name_bytes||!reply.nonce||count>maximum_records||(reply.capabilities&~7u))detail::invalid();
 if(reply.op==Operation::create){
  if(count||bytes.size()!=32)detail::invalid();reply.result=uint32_t(detail::read(bytes,24,4));reply.createdId=uint32_t(detail::read(bytes,28,4));
  if(reply.result||!reply.createdId||reply.createdId>0x7fffffffu)detail::invalid();return reply;
 }
 if(reply.op==Operation::caps&&count)detail::invalid();
 size_t at=reply_header_bytes;reply.records.reserve(size_t(count));
 for(size_t i=0;i<count;++i){
  if(bytes.size()-at<8)detail::invalid();Record record;record.id=uint32_t(detail::read(bytes,at,4));
  const auto flags=bytes[at+4];const size_t length=size_t(detail::read(bytes,at+6,2));
  if(!record.id||record.id>0x7fffffffu||(flags&~1u)||bytes[at+5]||length>maximum_name_bytes||length>bytes.size()-at-8)detail::invalid();
  if(std::any_of(reply.records.begin(),reply.records.end(),[&](const Record&r){return r.id==record.id;}))detail::invalid();
  record.main=(flags&1)!=0;record.name.assign(reinterpret_cast<const char*>(bytes.data()+at+8),length);
  if(!detail::display_name(record.name))detail::invalid();reply.records.push_back(std::move(record));at+=8+length;
 }
 if(at!=bytes.size())detail::invalid();return reply;
}
}
