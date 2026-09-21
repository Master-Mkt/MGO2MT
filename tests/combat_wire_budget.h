#pragma once
#include "combat_wire.h"
#include <algorithm>
#include <stdexcept>
namespace combat_test {
struct Budget {size_t capacity=0,baseBytes=0,eventBytes=0,fullBytes=0;};
// Homogeneous-event fixture only: capacity is for repetitions of the first kind.
// Service streams with mixed kinds must use check_batches below.
inline Budget budget(mgo2mt::combat::wire::Frame frame){
 using namespace mgo2mt::combat;Event event;if(!frame.events.empty())event=frame.events.front();else{for(const auto&p:frame.snapshot.players)if(p){event.source=p->identity;event.sourceLife=p->life;break;}event.weapon=25;event.kind=EventKind::impact;}
 event.epoch=frame.snapshot.epoch;event.id=1;frame.events.clear();frame.snapshot.eventWatermark=(std::max)(frame.snapshot.eventWatermark,uint64_t(5));
 const size_t base=wire::encode(frame).size();frame.events={event};const size_t stride=wire::encode(frame).size()-base;
 if(!stride||base+stride>2000)throw std::runtime_error("one full-roster event must fit");
 const size_t capacity=(std::min)(size_t(4),(2000-base)/stride);frame.events.resize(capacity,event);for(size_t n=0;n<capacity;++n)frame.events[n].id=n+1;
 const size_t full=wire::encode(frame).size();if(full!=base+capacity*stride||full>2000)throw std::runtime_error("encoded event stride capacity mismatch");
 event.id=capacity+1;frame.events.push_back(event);bool rejected=false;try{wire::encode(frame);}catch(const wire::Invalid&){rejected=true;}
 if(!rejected)throw std::runtime_error("next event must exceed byte or four-event format limit");
 return {capacity,base,stride,full};
}
struct BatchBudget {size_t chunks=0,maximumBytes=0,conservativeCapacity=4;};
// Reconstruct each complete producer batch from actual ordered deliveries, then
// measure its individual event kinds against the same roster/footer. The oracle
// sums measured bytes; it does not copy Service's encode-and-shrink loop.
inline BatchBudget check_batches(std::span<const mgo2mt::combat::wire::Frame> frames,
                                 size_t eventsPerBatch,size_t expectedBatches){
 using namespace mgo2mt::combat;
 if(!eventsPerBatch||!expectedBatches)throw std::runtime_error("empty batch fixture");
 BatchBudget result;size_t index=0;
 for(size_t batch=0;batch<expectedBatches;++batch){
  if(index>=frames.size())throw std::runtime_error("missing complete event batch");
  auto base=frames[index];base.events.clear();const size_t baseBytes=wire::encode(base).size();
  std::vector<Event> events;const size_t first=index;
  while(events.size()<eventsPerBatch){
   if(index>=frames.size())throw std::runtime_error("truncated producer event batch");
   const auto&f=frames[index++];
   if(f.snapshot!=base.snapshot||f.sop!=base.sop||f.status!=base.status||f.events.empty())
    throw std::runtime_error("producer batch metadata changed between chunks");
   events.insert(events.end(),f.events.begin(),f.events.end());
  }
  if(events.size()!=eventsPerBatch)throw std::runtime_error("chunk crosses producer batch boundary");
  std::vector<size_t> bytes;size_t largest=0;
  for(const auto&e:events){auto one=base;one.events={e};const size_t cost=wire::encode(one).size()-baseBytes;
   if(!cost||baseBytes+cost>2000)throw std::runtime_error("individual actual event does not fit");
   bytes.push_back(cost);largest=(std::max)(largest,cost);
  }
  const size_t conservative=(std::min)(size_t(4),(2000-baseBytes)/largest);
  result.conservativeCapacity=(std::min)(result.conservativeCapacity,conservative);
  size_t offset=0,chunk=first;
  while(offset<events.size()){
   size_t count=0,size=baseBytes;
   while(count<4&&offset+count<events.size()&&size+bytes[offset+count]<=2000)size+=bytes[offset+count++];
   if(!count||chunk>=index)throw std::runtime_error("missing predicted greedy chunk");
   const auto&actual=frames[chunk++];
   if(actual.events.size()!=count||!std::equal(actual.events.begin(),actual.events.end(),events.begin()+offset)||wire::encode(actual).size()!=size)
    throw std::runtime_error("actual event-kind chunk differs from measured byte budget");
   result.maximumBytes=(std::max)(result.maximumBytes,size);++result.chunks;offset+=count;
  }
  if(chunk!=index||index-first>(eventsPerBatch+conservative-1)/conservative)
   throw std::runtime_error("extra chunks or conservative delivery bound exceeded");
 }
 if(index!=frames.size())throw std::runtime_error("unexpected extra event batch");
 return result;
}
}
