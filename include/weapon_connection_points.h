#pragma once
#include "first_person_sight.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace mgo2mt::weapon_hand::connections {
inline constexpr uint32_t ejection=0x443037;
inline constexpr uint32_t sight_line=0;
struct Point {uint32_t weapon=0;std::string mdnSha256;uint32_t key=0;first_person_sight::Axis axis;};
// GCP1 stores user-provided original coordinates outside executable/source.
// All mutations occur during model loading. Failed loads clear stale data.
class Table {
 std::vector<Point> rows_;
public:
 bool load(const std::filesystem::path&,std::string& error);
 bool decode(std::span<const char>,std::string& error);
 void clear(){rows_.clear();}
 std::span<const Point> rows()const{return rows_;}
 const Point* find(uint32_t weapon,uint32_t key)const;
};
}
