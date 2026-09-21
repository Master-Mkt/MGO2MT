#include "collision_preview.h"
#include "character_catalog.h"
#include "character_renderer.h"
#include "combat_authority.h"
#include "evade_motion.h"
#include "evade_travel_curve.h"
#include "gameplay_config.h"
#include "menu_font.h"
#include "stage_floor_blend.h"
#include "stage_navigation.h"
#include "stage_normals.h"
#include "stage_sky.h"
#include "stage_surface_alpha.h"
#include "stage_water.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace mgo2mt {
namespace {
using stage::Vec3;
constexpr uint64_t epoch = 12;
constexpr combat::Identity actor{0, 1, 101};
constexpr float facing = -2.356193f;
// Independently checked against n022a GEOM primitive 0x192CD0, triangle 33589.
// Its 0x20A004030 surface has Cliff and Player, but no Floor or Don't Fall.
constexpr Vec3 cliffCenter{-54461.960f, 1437.5f, 71078.958f};
constexpr Vec3 startHint{-54108.407f, 2100.f, 71432.512f};

void require(bool value, const char* message) {
 if (!value) throw std::runtime_error(message);
}
std::vector<char> read(const std::filesystem::path& path) {
 std::ifstream input(path, std::ios::binary);
 if (!input) throw std::runtime_error("Missing capture resource: " + path.string());
 return {std::istreambuf_iterator<char>(input), {}};
}
Vec3 subtract(Vec3 a, const Vec3& b) {
 for (unsigned i = 0; i < 3; ++i) a[i] -= b[i];
 return a;
}
float length(Vec3 v) {
 return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
void vector_json(std::ostream& out, Vec3 value) {
 out << '[' << value[0] << ',' << value[1] << ',' << value[2] << ']';
}

struct Sample {
 uint64_t time = 0;
 combat::Player player;
 bool grounded = false;
};
struct Replay {
 bool roll = false;
 uint64_t rollAt = 0;
 std::vector<Sample> samples;
 unsigned accepted = 0;
 float drop = 0;
};

Replay replay(const std::shared_ptr<const stage::Collision>& raw,
              const std::shared_ptr<const stage::Collision>& movement,
              const gameplay::Config& config, bool rolling,
              std::ostream& trace) {
 Replay result;
 result.roll = rolling;
 stage::Navigation navigation({350, 1700, 2});
 require(navigation.place(*movement, startHint, 10000), "QQ ledge capsule placement");
 navigation.facing(facing);
 combat::Pose pose;
 pose.feet = navigation.feet();
 pose.yaw = navigation.yaw();
 pose.capsule = navigation.capsule();
 const float initialHeight = pose.feet[1];
 auto host = std::make_unique<combat::Authority>();
 host->begin(epoch, raw, config.profiles(20));
 require(host->configure_evade(combat::evade_runtime::roll, combat::evade_runtime::backstep),
         "QQ original roll timing profile");
 const uint16_t inventory[]{25};
 require(host->join(actor, 1, pose, 10000, 10000, inventory, 0), "QQ HOST join");
 host->active(true);
 result.samples.push_back({0, *host->snapshot().players[0], navigation.grounded()});
 bool airborne = false;
 bool invalidNavigationLogged = false;
 uint32_t sequence = 0;
 for (uint64_t now = 10; now <= 2200; now += 10) {
  stage::WalkInput input;
  input.forward = 1;
  input.speed = 3500;
  if (result.rollAt) {
   const double age = double(now - result.rollAt) / 1000.;
   const double previous = double(now - 10 - result.rollAt) / 1000.;
   input.speed = (combat::evade_runtime::distance_seconds(age) -
                  combat::evade_runtime::distance_seconds(previous)) / .01f;
  }
  navigation.advance(*movement, input, .01f);
  if (!invalidNavigationLogged && !movement->clear(navigation.feet(), navigation.capsule())) {
   invalidNavigationLogged = true;
   auto a = navigation.feet(), b = a;
   a[1] += navigation.capsule().radius;
   b[1] += navigation.capsule().height - navigation.capsule().radius;
   trace << "{\"navigationInvalidAtMs\":" << now << ",\"feet\":";
   vector_json(trace, navigation.feet());
   trace << ",\"contacts\":[";
   bool first = true;
   for (const auto& contact : movement->contacts(a, b, navigation.capsule().radius, 0, 4)) {
    if (!first) trace << ',';
    first = false;
    trace << "{\"penetration\":" << contact.penetration << ",\"normal\":";
    vector_json(trace, contact.normal);
    trace << ",\"attribute\":" << movement->triangles[contact.triangle].attribute << ",\"vertices\":[";
    unsigned vertex = 0;
    for (auto index : movement->triangles[contact.triangle].vertices) {
     if (vertex++) trace << ',';
     vector_json(trace, movement->vertices[index]);
    }
    trace << "]}";
   }
   trace << "]}\n";
  }
  host->advance(now);
  if (now % 50) continue;
  const auto old = host->snapshot().players[0]->pose;
  pose.feet = navigation.feet();
  pose.yaw = navigation.yaw();
  pose.pitch = navigation.pitch();
  const auto rejection = host->pose(actor, epoch, ++sequence, pose, now);
  trace << "{\"action\":\"" << (rolling ? "roll" : "walk")
        << "\",\"timeMs\":" << now << ",\"accepted\":"
        << (rejection == combat::Reject::none ? "true" : "false")
        << ",\"reject\":" << unsigned(rejection) << ",\"feet\":";
  vector_json(trace, pose.feet);
  trace << ",\"grounded\":" << (navigation.grounded() ? "true" : "false");
  if (rejection != combat::Reject::none) {
   auto delta = subtract(pose.feet, old.feet);
   trace << ",\"previousFeet\":";
   vector_json(trace, old.feet);
   trace << ",\"endpointClear\":" << movement->clear(pose.feet, pose.capsule);
   auto corner = pose.feet;
   corner[1] = old.feet[1];
   trace << ",\"cornerClear\":" << movement->clear(corner, pose.capsule);
   if (auto support = movement->sweep(old.feet, {0, -4, 0}, old.capsule, stage::query::floor)) {
    trace << ",\"supportFraction\":" << support->fraction << ",\"supportNormal\":";
    vector_json(trace, support->normal);
   }
   for (const auto& segment : {std::pair{old.feet, corner}, std::pair{corner, pose.feet}}) {
    if (auto hit = movement->sweep(segment.first, subtract(segment.second, segment.first), old.capsule)) {
     trace << (segment.first == old.feet ? ",\"horizontalFraction\":" : ",\"verticalFraction\":")
           << hit->fraction << (segment.first == old.feet ? ",\"horizontalNormal\":" : ",\"verticalNormal\":");
     vector_json(trace, hit->normal);
    }
   }
   if (auto hit = movement->sweep(old.feet, delta, old.capsule)) {
    trace << ",\"chordHitFraction\":" << hit->fraction << ",\"hitAttribute\":"
          << movement->triangles[hit->triangle].attribute;
   }
  }
  trace << "}\n";
  trace.flush();
  if (rejection != combat::Reject::none)
   throw std::runtime_error(std::string("QQ ") + (rolling ? "roll" : "walk") +
                            " pose rejected at " + std::to_string(now) +
                            " ms, code " + std::to_string(unsigned(rejection)));
  ++result.accepted;
  if (rolling && !result.rollAt) {
   require(host->evade(actor, epoch, sequence, combat::EvadeKind::roll, 1, now) ==
           combat::Reject::none, "QQ HOST admits forward roll after running evidence");
   result.rollAt = now;
  }
  result.samples.push_back({now, *host->snapshot().players[0], navigation.grounded()});
  result.drop = (std::max)(result.drop, initialHeight - pose.feet[1]);
  airborne |= !navigation.grounded() && result.drop > 100;
  if (airborne && navigation.grounded() && result.drop > 250) break;
 }
 require(airborne && result.drop > 250, "QQ movement must really leave the unguarded ledge");
 const auto travel = subtract(result.samples.back().player.pose.feet,
                              result.samples.front().player.pose.feet);
 require(std::hypot(travel[0], travel[2]) > 850, "QQ crossing must advance beyond capsule radius");
 return result;
}

void capture(ID3D11Device* device, ID3D11DeviceContext* context,
             const CharacterRenderer& renderer, const std::filesystem::path& path,
             const wchar_t* title) {
 ComPtr<ID3D11Resource> resource;
 renderer.view()->GetResource(&resource);
 ComPtr<ID3D11Texture2D> texture;
 require(SUCCEEDED(resource.As(&texture)), "Cliff capture texture");
 D3D11_TEXTURE2D_DESC description{};
 texture->GetDesc(&description);
 require(description.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "Cliff capture RGBA surface");
 description.BindFlags = 0;
 description.Usage = D3D11_USAGE_STAGING;
 description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> copy;
 require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, &copy)), "Cliff capture staging");
 context->CopyResource(copy.Get(), texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};
 require(SUCCEEDED(context->Map(copy.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Cliff capture readback");
 std::vector<uint32_t> pixels(size_t(description.Width) * description.Height);
 for (unsigned y = 0; y < description.Height; ++y) {
  for (unsigned x = 0; x < description.Width; ++x) {
   const auto* p = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch + x * 4;
   pixels[size_t(y) * description.Width + x] = 0xff000000u | uint32_t(p[0]) << 16 |
                                              uint32_t(p[1]) << 8 | p[2];
  }
 }
 context->Unmap(copy.Get(), 0);
 HDC dc = CreateCompatibleDC(nullptr);
 BITMAPINFO info{};
 info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
 info.bmiHeader.biWidth = LONG(description.Width);
 info.bmiHeader.biHeight = -LONG(description.Height);
 info.bmiHeader.biPlanes = 1;
 info.bmiHeader.biBitCount = 32;
 void* raw = nullptr;
 HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &raw, nullptr, 0);
 require(dc && bitmap && raw, "Cliff capture DIB");
 std::memcpy(raw, pixels.data(), pixels.size() * sizeof(uint32_t));
 auto oldBitmap = SelectObject(dc, bitmap);
 auto font = create_menu_font(27, FW_BOLD);
 auto oldFont = SelectObject(dc, font);
 SetBkMode(dc, TRANSPARENT);
 RECT label{26, 22, LONG(description.Width) - 24, 110};
 SetTextColor(dc, RGB(0, 0, 0));
 DrawTextW(dc, title, -1, &label, DT_LEFT | DT_TOP);
 OffsetRect(&label, -2, -2);
 SetTextColor(dc, RGB(255, 225, 130));
 DrawTextW(dc, title, -1, &label, DT_LEFT | DT_TOP);
 GdiFlush();
 BITMAPFILEHEADER header{};
 header.bfType = 0x4d42;
 header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
 header.bfSize = header.bfOffBits + DWORD(pixels.size() * 4);
 std::ofstream output(path, std::ios::binary);
 output.write(reinterpret_cast<const char*>(&header), sizeof(header));
 output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
 output.write(static_cast<const char*>(raw), std::streamsize(pixels.size() * 4));
 SelectObject(dc, oldFont);
 DeleteObject(font);
 SelectObject(dc, oldBitmap);
 DeleteObject(bitmap);
 DeleteDC(dc);
 require(bool(output), "Cliff capture image write");
}

WorldView camera_for(const stage::Collision& collision) {
 WorldView camera;
 camera.aspect = 16.f / 9;
 camera.verticalFov = .9f;
 Vec3 target = cliffCenter;
 target[1] += 500;
 const float radius = 4500;
 bool found = false;
 for (float turn : {1.45f, -1.45f, .8f, -.8f, 2.4f, -2.4f, 0.f}) {
  Vec3 eye{target[0] - std::sin(facing + turn) * radius,
           target[1] + 1700, target[2] - std::cos(facing + turn) * radius};
  auto direction = subtract(eye, target);
  const float distance = length(direction);
  for (auto& v : direction) v /= distance;
  auto hit = collision.ray(target, direction, distance, stage::query::camera);
  if (hit && hit->distance < distance - 20) continue;
  camera.eye = eye;
  camera.direction = subtract(target, eye);
  found = true;
  break;
 }
 require(found, "QQ ledge camera line of sight");
 return camera;
}
}

int collision_test_capture(const std::filesystem::path& data,
                           const std::filesystem::path& output) {
 try {
  std::filesystem::create_directories(output);
  gameplay::Config config;
  std::string error;
  require(config.load(data / "gameplay.json", error), error.c_str());
  std::ifstream collisionFile(data / "stage/n022a.collision.cfg");
  auto raw = std::make_shared<const stage::Collision>(stage::Collision::read(collisionFile));
  const Vec3 direction{std::sin(facing), 0, std::cos(facing)};
  Vec3 probe = cliffCenter;
  for (unsigned i = 0; i < 3; ++i) probe[i] -= direction[i] * 500;
  const auto cliff = raw->ray(probe, direction, 1000, stage::query::cliff);
  require(cliff && length(subtract(cliff->position, cliffCenter)) < 20 &&
          raw->triangles[cliff->triangle].attribute == 0x20A004030ULL,
          "QQ capture uses the inspected original Cliff band without DontFall");
  auto movement = stage::movement_collision(raw);
  std::ofstream trace(output / "trajectory.jsonl");
  trace << std::setprecision(9);
  const auto walking = replay(raw, movement, config, false, trace);
  const auto rolling = replay(raw, movement, config, true, trace);

  auto stageBytes = read(data / "stage/n022a.gwm");
  CharacterModel model(stageBytes);
  stage::load_original_normals(model, stageBytes, data / "stage/n022a.gwn");
  stage::load_floor_blend(model, stageBytes, data / "stage/n022a.gfb");
  stage::load_surface_alpha(model, stageBytes, data / "stage/n022a.gsa");
  std::ifstream lightFile(data / "stage/n022a.lighting.cfg");
  const auto lighting = stage::Lighting::read(lightFile);
  for (auto& vertex : model.vertices) {
   const auto light = lighting.sample({vertex.x, vertex.y, vertex.z}, {vertex.nx, vertex.ny, vertex.nz});
   vertex.lr = light.color[0]; vertex.lg = light.color[1]; vertex.lb = light.color[2]; vertex.lit = 1;
  }
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  D3D_FEATURE_LEVEL featureLevel;
  require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
          D3D11_SDK_VERSION, &device, &featureLevel, &context)), "Cliff capture WARP device");
  CharacterRenderer ground(device.Get(), model);
  ground.resize_target(device.Get(), 1280, 720);
  CharacterModel skyModel(read(data / "stage/n022a.sky.gwm"));
  std::ifstream skyFile(data / "stage/n022a.sky.cfg");
  const auto skySettings = stage::SkySettings::read(skyFile);
  skySettings.validate(skyModel);
  skySettings.prepare(skyModel);
  CharacterRenderer sky(device.Get(), skyModel, true);
  CharacterCatalog catalog(read(data / "character/appearance.gwc"));
  std::array<uint8_t, 28> appearance{};
  appearance[2] = 11; appearance[3] = 22; appearance[15] = 46; appearance[17] = 57;
  auto body = catalog.assemble(appearance);
  require(body.ready(), "QQ original character assembly");
  CharacterRenderer person(device.Get(), body.model);
  PlayerMotionBank motions(read(data / "character/player.gwmot"));
  player::EvadeMotionBank evasion(read(data / "character/evade.gwmot"));
  menu_font_resources().load(data / "fonts");
  const auto camera = camera_for(*raw);
  auto falling_sample = [](const Replay& run) -> const Sample& {
   const float startY = run.samples.front().player.pose.feet[1];
   auto sample = std::find_if(run.samples.begin(), run.samples.end(), [&](const Sample& s) {
    return !s.grounded && startY - s.player.pose.feet[1] > 300 &&
           (!run.roll || (s.player.evadeKind == combat::EvadeKind::roll && s.time - run.rollAt >= 150));
   });
   require(sample != run.samples.end(), "QQ HOST accepted airborne capture sample");
   return *sample;
  };
  std::ofstream report(output / "capture.json");
  report << std::setprecision(9)
         << "{\"offlineRenderFixture\":true,\"map\":20,\"stage\":\"n022a\","
         << "\"sourceCliffPrimitive\":\"0x192CD0\",\"sourceCliffTriangle\":33589,"
         << "\"sourceCliffAttribute\":\"0x20A004030\",\"dontFall\":false,"
         << "\"navigationStepMs\":10,\"hostPoseStepMs\":50,\"frames\":[";
  unsigned frames = 0;
  auto draw = [&](const Replay& run, const Sample& sample, const char* name, const wchar_t* label) {
   const double seconds = double(sample.time) / 1000.;
   auto motion = run.roll && sample.time >= run.rollAt
       ? std::optional<MotionPose>(evasion.sample(combat::EvadeKind::roll, double(sample.time - run.rollAt) / 1000.)->pose)
       : motions.sample(sample.time ? PlayerMotion::Run : PlayerMotion::Idle, seconds);
   require(bool(motion), "QQ original movement pose");
   catalog.pose(body, *motion);
   person.update_vertices(context.Get(), body.model.vertices);
   ground.render(context.Get(), 0, false, &camera);
   const auto skyPose = skySettings.sample(seconds);
   const SkyFrame skyFrame{skyPose.position, skyPose.degrees, skySettings.color,
                           skySettings.fogColor, skySettings.fog, skyPose.cloudU};
   sky.render(context.Get(), 0, false, &camera, &ground, nullptr, {}, nullptr,
              nullptr, nullptr, 1.f, CharacterPass::all, &skyFrame);
   const auto& pose = sample.player.pose;
   const auto environment = lighting.environment(pose.feet);
   person.render(context.Get(), pose.yaw, false, &camera, &ground, &pose.feet, {}, nullptr, nullptr, &environment);
   capture(device.Get(), context.Get(), ground, output / name, label);
   if (frames++) report << ',';
   report << "{\"file\":\"" << name << "\",\"timeMs\":" << sample.time
          << ",\"action\":\"" << (run.roll ? "roll" : "walk") << "\",\"feet\":";
   vector_json(report, pose.feet);
   report << ",\"grounded\":" << (sample.grounded ? "true" : "false")
          << ",\"evadeKind\":" << unsigned(sample.player.evadeKind) << '}';
  };
  draw(walking, walking.samples.front(), "01-qq-ledge.bmp", L"QQ / 原作の段差・落下禁止なし");
  draw(walking, falling_sample(walking), "02-qq-walk-fall.bmp", L"QQ / 歩いて段差を越える・HOST承認済み");
  draw(rolling, falling_sample(rolling), "03-qq-roll-fall.bmp", L"QQ / ローリングで段差を越える・HOST承認済み");
  report << "],\"walkingAcceptedPoses\":" << walking.accepted
         << ",\"rollingAcceptedPoses\":" << rolling.accepted
         << ",\"walkingDropMm\":" << walking.drop << ",\"rollingDropMm\":" << rolling.drop
         << ",\"rejectedPoses\":0,\"passed\":true}\n";
  require(bool(report) && bool(trace), "QQ capture report write");
  std::cout << "PASS original QQ cliff capture: " << frames << " frames, "
            << walking.accepted + rolling.accepted << " HOST accepted poses\n";
  return 0;
 } catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
 }
}
}
