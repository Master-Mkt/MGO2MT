#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace mgo2win::host {
// Local host policy, not original network phase numbers or a ready packet.
enum class RoundPhase { waiting, preparing };
enum class StartReason { none, all_ready, countdown };
enum class ParticipantRole { player, spectator, dedicated_host };
struct ParticipantToken {
 uint8_t slot=0; uint32_t incarnation=0,character=0;
 bool operator==(const ParticipantToken&)const=default;
};
struct ReadyParticipant {
 ParticipantToken token; ParticipantRole role=ParticipantRole::player;
 bool prepared=false,ready=false;
 bool operator==(const ReadyParticipant&)const=default;
};
struct RoundPolicy {
 uint64_t countdown_ms=120000; unsigned minimum_players=1; bool dp_enabled=false;
};
class RoundRules {
 RoundPolicy policy_;std::array<std::optional<ReadyParticipant>,24> participants_{};
 RoundPhase phase_=RoundPhase::waiting;uint64_t generation_=1;
 std::optional<uint64_t> deadline_;
 ReadyParticipant* find(const ParticipantToken&);
 void arm(uint64_t now_ms);
public:
 explicit RoundRules(RoundPolicy={});
 // Repeated joins preserve ready state. A different occupant must first leave.
 bool join(const ParticipantToken&,ParticipantRole,uint64_t now_ms);
 bool leave(const ParticipantToken&);
 // A new round invalidates every prepared/START decision, including queued ones.
 void reset(uint64_t now_ms);
 bool set_prepared(const ParticipantToken&,uint64_t generation,bool);
 bool set_ready(const ParticipantToken&,uint64_t generation,bool);
 StartReason advance(uint64_t now_ms);
 RoundPhase phase()const{return phase_;}
 // Preparation is not evidence that gameplay or scene replication is ready.
 bool preparation_started()const{return phase_!=RoundPhase::waiting;}
 uint64_t generation()const{return generation_;}
 std::optional<uint64_t> deadline()const{return deadline_;}
 const RoundPolicy& policy()const{return policy_;}
 const auto& participants()const{return participants_;}
 std::size_t player_count()const;
};

// Prices, original weapon IDs and rule restrictions must come from a reviewed
// catalog. This policy never invents a price, awards points or emits a packet.
struct WeaponOption {
 uint16_t id=0;uint32_t dp_cost=0;
 bool available_without_dp=true,allowed=true;
};
enum class WeaponAccess { allowed, restricted, dp_disabled, insufficient_dp };
WeaponAccess weapon_access(const WeaponOption&,bool dp_enabled,uint32_t balance);
struct LoadoutQuote {WeaponAccess access=WeaponAccess::allowed;uint64_t cost=0;};
LoadoutQuote quote_loadout(std::span<const WeaponOption>,bool dp_enabled,uint32_t balance);

// Windows product policy: every catalog entry is available to every player.
// No red/blue pair, global occupancy count or duplicate-character reservation.
class SpecialPolicy {
 std::vector<uint16_t> catalog_;
public:
 explicit SpecialPolicy(std::span<const uint16_t> catalog);
 bool allows(uint16_t id)const;
 const auto& choices()const{return catalog_;}
};
}
