#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <vector>
#include <utility>
namespace mgo2win::host {
// Per-peer unsent reliable FIFO. GWCB 8 with 24 players and a SOP footer
// uses up to 23 frames/shot: 24 shooters yield 552 records in one tick.
// A second maximum burst can overflow this bounded queue for a slow peer;
// only that peer is disconnected. Active wire records have a separate limit of 32.
class MandatoryBacklog {
public:
 static constexpr size_t capacity=1024,maxPayload=2000,maxBytes=capacity*maxPayload;
 struct Entry {uint64_t ticket=0;std::vector<uint8_t> payload;};
private:
 std::deque<Entry> entries_;size_t bytes_=0;
public:
 bool push(uint64_t ticket,std::vector<uint8_t> payload){
  if(!ticket||payload.empty()||payload.size()>maxPayload||entries_.size()>=capacity||payload.size()>maxBytes-bytes_)return false;
  const auto size=payload.size();entries_.push_back({ticket,std::move(payload)});bytes_+=size;return true;
 }
 bool empty()const{return entries_.empty();}size_t size()const{return entries_.size();}size_t bytes()const{return bytes_;}
 Entry pop(){auto out=std::move(entries_.front());entries_.pop_front();bytes_-=out.payload.size();return out;}
 void clear(){entries_.clear();bytes_=0;}
};
}
