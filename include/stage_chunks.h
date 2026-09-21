#pragma once
#include "character_model.h"
#include <filesystem>
#include <memory>
#include <stop_token>
namespace mgo2mt::stage {
struct ChunkLimits {
 size_t count=32,fileBytes=64u*1024*1024,totalBytes=512u*1024*1024,decodedBytes=1024u*1024*1024;
};
// Strict local assembler manifest; throws on malformed/partial data or cancellation.
// All sidecars are applied to their own original chunk before texture/index rebasing.
std::shared_ptr<CharacterModel> load_stage_chunks(const std::filesystem::path& manifest,
 std::stop_token stop={},ChunkLimits limits={});
// Geometry is already authored in world coordinates. No axis or bone conversion.
void append_stage_chunk(CharacterModel& target,CharacterModel chunk,std::stop_token stop={});
}
