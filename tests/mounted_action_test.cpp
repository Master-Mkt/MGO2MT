#include "combat_service.h"
#include "controller_input.h"
#include "mounted_input.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
unsigned checks = 0;
constexpr Identity self{0, 1, 100}, enemy{1, 2, 200}, other{2, 3, 300};
void check(bool condition, const char* message) {
 ++checks;
 if (!condition) throw std::runtime_error(message);
}
Pose pose(float z, float x = 0) {
 Pose result;
 result.feet = {x, 2, z};
 return result;
}
std::shared_ptr<const stage::Collision> floor_world() {
 return std::make_shared<const stage::Collision>(stage::Collision::make(
     {{-100000, 0, -100000}, {100000, 0, -100000},
      {100000, 0, 100000}, {-100000, 0, 100000}},
     {{{0, 2, 1}, stage::attribute::native_solid},
      {{0, 3, 2}, stage::attribute::native_solid}}));
}
std::vector<Weapon> profiles() {
 Weapon carried;
 carried.id = 25;
 carried.damage = 100;
 carried.intervalMs = 100;
 carried.reloadMs = 1000;
 carried.magazine = 10;
 carried.reserve = 20;
 carried.range = 20000;
 auto mounted = carried;
 mounted.id = 500;
 mounted.damage = 120;
 mounted.magazine = 5;
 mounted.reserve = 0;
 mounted.mountedOnly = true;
 return {carried, mounted};
}
mounted::Registry registry() {
 mounted::Type type;
 type.id = "fixture";
 type.name = "Action fixture";
 type.model = "mounted/fixture.gwm";
 type.weapon = 500;
 type.pivot = {0, 1000, 0};
 type.muzzle = {0, 1200, 1000};
 type.operatorOffset = {0, 0, -600};
 mounted::Registry result;
 result.types.push_back(type);
 result.placements.push_back({20, 1, type.id, {0, 2, 0}, 0});
 return result;
}
struct Fixture {
 Service service{1};
 mounted::Registry definitions = registry();
 Fixture() {
  service.configure(floor_world(), profiles());
  check(service.authority().configure_mounted(definitions, 20), "configure mounted scene");
  const uint16_t inventory[]{25};
  check(service.authority().join(self, 1, pose(-600), 1000, 1000, inventory, 0), "join operator");
  check(service.authority().join(other, 1, pose(-1600), 1000, 1000, inventory, 0), "join nearby other PC");
  check(service.authority().join(enemy, 2, pose(5000), 1000, 1000, inventory, 0), "join distant PC");
  service.authority().active(true);
  for (auto id : {self, other}) {
   check(service.admit(id), "admit input peer");
   check(service.receive(id, wire::encode(wire::Accept{1}), 0), "accept native connection");
  }
  service.deliveries();
 }
 Player player(Identity id = self) { return *service.authority().snapshot().players[id.slot]; }
 uint16_t nearby(Identity id = self) {
  return mounted::within_use_range(definitions.placements[0], definitions.types[0], player(id).pose.feet) ? 1 : 0;
 }
 void send(mounted::Input& input, uint32_t sequence, uint64_t now, Identity id = self) {
  const auto current = player(id);
  wire::Input packet;
  packet.epoch = 1;
  packet.sequence = sequence;
  packet.life = current.life;
  packet.pose = current.pose;
  packet.weapon = current.weapon;
  packet.mounted = input.intent(true);
  check(service.receive(id, wire::encode(packet), now), "HOST receives encoded Action request");
  input.sent(sequence);
  service.poll(now);
  bool acknowledged = false;
  for (const auto& delivery : service.deliveries()) {
   if (delivery.recipient != id) continue;
   const auto record = wire::decode(delivery.payload);
   if (const auto* frame = std::get_if<wire::Frame>(&record)) {
    input.acknowledge(frame->sop.inputSequence, frame->sop.inputSequenced);
    acknowledged |= frame->sop.inputSequenced && frame->sop.inputSequence == sequence;
   }
  }
  check(acknowledged && !input.pending(), "actual HOST frame/SOP ACK completes request");
 }
};

void ranges() {
 auto definitions = registry();
 auto& type = definitions.types[0];
 auto& instance = definitions.placements[0];
 const auto position = mounted::operator_position(instance, type);
 check(type.useRadius == 1500, "existing native JSON default radius is unchanged");
 check(mounted::within_use_range(instance, type, position), "operator position is in range");
 auto boundary = position;
 boundary[0] += type.useRadius;
 check(mounted::within_use_range(instance, type, boundary), "inclusive exact radius");
 boundary[0] = std::nextafter(boundary[0], std::numeric_limits<float>::infinity());
 check(!mounted::within_use_range(instance, type, boundary), "outside radius by one float ULP rejected");
 boundary = position;
 boundary[1] += type.useRadius;
 check(mounted::within_use_range(instance, type, boundary), "vertical distance uses same sphere");
 boundary[1] += 1;
 check(!mounted::within_use_range(instance, type, boundary), "range is not an infinite vertical cylinder");
 for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
  boundary = position;
  boundary[0] = invalid;
  check(!mounted::within_use_range(instance, type, boundary), "nonfinite operator candidate rejected");
  auto invalidType = type;
  invalidType.useRadius = invalid;
  check(!mounted::within_use_range(instance, invalidType, position), "nonfinite radius rejected");
  auto invalidInstance = instance;
  invalidInstance.yaw = invalid;
  check(!mounted::within_use_range(invalidInstance, type, position), "nonfinite placement yaw rejected");
 }
 for (float radius : {0.f, -1500.f, 99.f, 5001.f}) {
  type.useRadius = radius;
  check(!mounted::within_use_range(instance, type, position), "invalid native JSON radius rejected");
 }
 type.useRadius = 100;
 instance.yaw = 1.2f;
 instance.origin = {1200, 20, -3000};
 for (auto kind : {mounted::Kind::gun, mounted::Kind::mortar, mounted::Kind::catapult}) {
  type.kind = kind;
  check(mounted::within_use_range(instance, type, mounted::operator_position(instance, type)),
        "all heavy equipment kinds use rotated configured operator position");
  check(!mounted::within_use_range(instance, type, instance.origin), "machine origin is not substituted for operator offset");
 }
 // Prove the HOST uses the same inclusive boundary, not another native radius.
 for (float offset : {1500.f, 1501.f}) {
  Authority host;
  const auto definitions = registry();
  host.begin(1, floor_world(), profiles());
  check(host.configure_mounted(definitions, 20), "boundary HOST scene");
  const auto start = pose(-600, offset);
  const uint16_t inventory[]{25};
  check(host.join(self, 1, start, 1000, 1000, inventory, 0), "boundary operator join");
  host.active(true);
  check(host.pose(self, 1, 1, start, 0) == Reject::none, "boundary pose admitted");
  const auto result = host.mount(self, 1, 1, {mounted::Action::mount, 1, 1}, 0);
  check(result == (offset == 1500 ? Reject::none : Reject::invalid_pose), "client range and HOST admission agree");
 }
}

void action_mapping() {
 static_assert(mounted::action_button == 7);
 ControllerInput controller{std::filesystem::temp_directory_path() /
     ("mgo2mt-mounted-action-" + std::to_string(GetCurrentProcessId()) + ".unused")};
 controller.config = InputConfig{};
 controller.config.device = 1;
 XINPUT_STATE physical{};
 controller.reader = [&](DWORD, XINPUT_STATE* state) { *state = physical; return DWORD(ERROR_SUCCESS); };
 auto values = [&] { return controller.action_values(controller.poll(true)); };
 Fixture fixture;
 mounted::Input input;
 input.scope(1, self.character, 1);
 check(!input.step(values()[mounted::action_button] > .12f, true, 0, fixture.nearby(), 0), "neutral arms Action edge");
 physical.Gamepad.wButtons = XINPUT_GAMEPAD_Y;
 const auto pressed = values();
 check(pressed[mounted::action_button] == 1, "default pad Y resolves logical Action/Salute 7");
 check(input.step(pressed[mounted::action_button] > .12f, true, 0, fixture.nearby(), 10), "nearby Action press requests Armed");
 check(!fixture.player().mountedId, "Armed is not predicted before HOST approval");
 check(bool(fixture.service.authority().fire(self, {1, 1, 25, {0, 0, 1}}, 0)), "consume carried round before mounting");
 const auto carried = fixture.player();
 const auto inventory = fixture.service.authority().item_held(self, 1)->slots;
 fixture.send(input, 1, 10);
 check(fixture.player().mountedId == 1 && fixture.player().weapon == 500, "HOST approved Action enters Armed");
 check(!input.step(values()[mounted::action_button] > .12f, true, 1, 1, 11), "held Action does not immediately leave Armed after ACK");
 check(!input.step(values()[mounted::action_button] > .12f, true, 1, 1, 12), "holding continues to produce no repeated request");

 mounted::Input occupied;
 occupied.scope(1, other.character, 1);
 occupied.step(false, true, 0, fixture.nearby(other), 11);
 check(occupied.step(true, true, 0, fixture.nearby(other), 12), "other nearby PC sends independent fresh edge");
 fixture.send(occupied, 1, 12, other);
 check(!fixture.player(other).mountedId && fixture.player().mountedId == 1, "one occupant only; denied request still acknowledged");
 check(!occupied.step(true, true, 0, 1, 13), "denied held request does not retry after ACK");

 physical = {};
 input.step(values()[mounted::action_button] > .12f, true, 1, 1, 20);
 physical.Gamepad.wButtons = XINPUT_GAMEPAD_Y;
 check(input.step(values()[mounted::action_button] > .12f, true, 1, 1, 30), "release and new Action edge requests leave");
 fixture.send(input, 2, 30);
 const auto restored = fixture.player();
 check(!restored.mountedId && restored.weapon == carried.weapon && restored.ammo == carried.ammo && restored.reserve == carried.reserve,
       "Action leave restores selected carried weapon and its partially used ammunition");
 check(fixture.service.authority().item_held(self, 1)->slots == inventory, "Action cycle preserves original inventory exactly");

 // Route the real controller mapping through the same logical action again.
 physical = {};
 input.step(values()[mounted::action_button] > .12f, true, 0, 1, 40);
 assign_input(controller.config, mounted::action_button, 9); // Physical RB.
 physical.Gamepad.wButtons = XINPUT_GAMEPAD_Y;
 check(values()[mounted::action_button] == 0, "after remap physical Y no longer requests Armed");
 check(!input.step(values()[mounted::action_button] > .12f, true, 0, 1, 50), "old physical binding does not mount");
 physical = {};
 input.step(values()[mounted::action_button] > .12f, true, 0, 1, 60);
 physical.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
 check(input.step(values()[mounted::action_button] > .12f, true, 0, 1, 70), "remapped physical RB resolves Action7 and requests Armed");
 fixture.send(input, 3, 70);
 check(fixture.player().mountedId == 1, "remapped Action is HOST admitted");

 InputConfig keyboard;
 check(keyboard.keyboard[mounted::action_button] == 'X', "default keyboard Action/Salute is X");
 assign_input(keyboard, mounted::action_button, 'F');
 check(keyboard.keyboard[mounted::action_button] == 'F' && valid_input_config(keyboard), "keyboard Action binding remains remappable");
}

void unavailable() {
 Fixture fixture;
 mounted::Input input;
 input.scope(1, enemy.character, 1);
 input.step(false, true, 0, fixture.nearby(enemy), 0);
 check(!input.step(true, true, 0, fixture.nearby(enemy), 1) && !input.pending(), "Action outside JSON range does not request Armed");
 check(fixture.service.authority().pose(enemy, 1, 1, fixture.player(enemy).pose, 0) == Reject::none, "distant pose admitted");
 check(fixture.service.authority().mount(enemy, 1, 1, {mounted::Action::mount, 1, 1}, 0) == Reject::invalid_pose,
       "HOST rejects forged distant Action request");
 check(fixture.service.authority().pose(self, 1, 1, fixture.player().pose, 0) == Reject::none, "living pose primed");
 burning::Blast lethal{{{1, enemy.slot, enemy.instance, enemy.character, 1}, 52, 0, 2},
                       1, {0, 1000, -600}, 500, 10000, false};
 fixture.service.authority().explode(lethal, 1);
 check(!fixture.player().alive, "dead operator fixture");
 check(fixture.service.authority().mount(self, 1, 1, {mounted::Action::mount, 1, 1}, 1) == Reject::dead,
       "dead player cannot enter Armed");
 input.scope(1, self.character, 1);
 check(!input.step(true, false, 0, 1, 2) && !input.pending(), "inactive/dead client context cancels pending Action");
 check(!input.step(true, true, 0, 1, 3), "returning to active context requires a button release");
}

void consume_context_action() {
 mounted::Input input;
 input.scope(1, self.character, 1);
 check(!input.step(true, true, 0, 1, 0) && input.consumes_action(),
       "held button entering nearby context is consumed without mounting");
 check(!input.step(true, true, 0, 0, 1) && input.consumes_action(),
       "leaving range with the same held button cannot start Salute");
 input.step(false, true, 0, 0, 2);
 check(!input.consumes_action(), "physical release ends consumed Action context");
 check(!input.step(true, true, 0, 0, 3) && !input.consumes_action(),
       "fresh Action away from equipment remains available for Salute");
 input.step(false, true, 0, 1, 4);
 check(input.step(true, true, 0, 1, 5) && input.consumes_action(), "pending mount consumes Action");
 input.sent(7);
 input.acknowledge(7, true);
 check(!input.pending() && input.consumes_action(), "HOST ACK does not leak held Action to Salute");
 check(!input.step(true, true, 0, 0, 6) && input.consumes_action(),
       "denied ACK followed by leaving range still consumes the held press");
 input.step(false, true, 0, 0, 7);
 check(!input.consumes_action(), "release after ACK restores normal Action routing");
}
}

int main() {
 try {
  ranges();
  action_mapping();
  unavailable();
  consume_context_action();
  std::cout << "PASS " << checks << " mounted Action7/Salute checks: range, remapping, single edge, HOST ACK, Armed, exclusivity and inventory\n";
  return 0;
 } catch (const std::exception& error) {
  std::cerr << "after " << checks << " checks: " << error.what() << '\n';
  return 1;
 }
}
