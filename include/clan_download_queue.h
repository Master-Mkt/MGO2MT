#pragma once
#include "clan_hud.h"
#include <array>
#include <list>
#include <memory>
#include <stdexcept>

namespace mgo2mt::clan {
// Owned by one authenticated connection thread. UI threads only touch Cache.
// A pending reply always belongs to the Download that emitted its request.
class DownloadQueue {
    struct Slot {std::shared_ptr<Cache> cache;Download download;uint64_t published=~uint64_t(0);};
    struct Memo {uint32_t clan;std::optional<Image> image;};
    std::array<Slot,2> slots_;
    std::list<Memo> memo_;
    std::optional<size_t> pending_;
    uint64_t sent_=0;
    bool disabled_=false;
    static bool valid(uint32_t id){return id&&id<=0x7fffffffu;}
    void stop()noexcept {disabled_=true;pending_.reset();for(auto& s:slots_)s.download.cancel();}
    void remember(uint32_t id,std::optional<Image> image) {
        for(auto i=memo_.begin();i!=memo_.end();++i)if(i->clan==id){memo_.erase(i);break;}
        memo_.push_back({id,std::move(image)});
        if(memo_.size()>24)memo_.pop_front();
    }
    bool publish(Slot& slot,const State& wanted) {
        for(auto i=memo_.begin();i!=memo_.end();++i)if(i->clan==wanted.clan) {
            if(slot.published!=wanted.serial) {
                slot.cache->put(wanted.clan,i->image);
                auto after=slot.cache->state();
                // If the UI changed identity concurrently, retry its new request next pump.
                if(after.clan==wanted.clan&&after.serial==wanted.serial+1)slot.published=after.serial;
            }
            memo_.splice(memo_.end(),memo_,i);
            return true;
        }
        return false;
    }
public:
    static constexpr uint64_t timeout_ms=3000;
    DownloadQueue(std::shared_ptr<Cache> own,std::shared_ptr<Cache> enemy)
      :slots_{Slot{std::move(own)},Slot{std::move(enemy)}} {
        if(!slots_[0].cache||!slots_[1].cache)throw std::invalid_argument("Missing clan cache");
    }
    bool disabled()const noexcept{return disabled_;}
    void tick(uint64_t now)noexcept {
        if(pending_&&now-sent_>=timeout_ms)stop();
    }
    std::optional<std::vector<uint8_t>> take(uint64_t now) {
        tick(now);if(disabled_||pending_)return {};
        for(size_t i=0;i<slots_.size();++i) {
            auto& slot=slots_[i];const auto wanted=slot.cache->state();
            if(!valid(wanted.clan)){slot.download.cancel();continue;}
            if(publish(slot,wanted)){slot.download.cancel();continue;}
            if(slot.download.clan()!=wanted.clan)slot.download.begin(wanted.clan);
            if(slot.download.done()) {
                remember(wanted.clan,slot.download.image());publish(slot,wanted);continue;
            }
            auto bytes=slot.download.requestPayload();pending_=i;sent_=now;return bytes;
        }
        return {};
    }
    void receive(std::span<const uint8_t> payload)noexcept {
        if(disabled_||!pending_)return;
        const auto channel=*pending_;pending_.reset();
        try {
            auto& download=slots_[channel].download;
            if(download.accept(payload)) {
                remember(download.clan(),download.image());
                // Includes same-clan own/enemy sharing. Old identity is never relabelled.
                for(auto& slot:slots_)publish(slot,slot.cache->state());
            }
        }catch(...){stop();}
    }
};
}
