#pragma once
#include <filesystem>
#include <fstream>
#include <chrono>
#include <stdexcept>
namespace mgo2mt::test {
// Independently chosen synthetic menu fixtures for source-only UI tests.
// These are not a recovered game catalog or a production fallback.
struct WeaponFixture {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("mgo2mt-ui-weapons-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
 WeaponFixture(){
  std::ofstream out(path);out<<"MGO2MT_WEAPON_CATALOG\t1\nINITIAL_DP\t1000\n"
  "WEAPON\tPRIMARY\t0\tNONE\t0\t0\t?\n"
  "WEAPON\tPRIMARY\t23\tFixture Main\t600\t1\t23\n"
  "WEAPON\tSECONDARY\t0\tNONE\t0\t0\t?\n"
  "WEAPON\tSECONDARY\t7\tFixture Secondary\t300\t1\t7\n"
  "WEAPON\tSUPPORT\t0\tNONE\t0\t0\t?\n"
  "WEAPON\tSUPPORT\t52\tFixture Support\t200\t1\t52\n";
  if(!out)throw std::runtime_error("weapon UI fixture write");
 }
 WeaponFixture(const WeaponFixture&)=delete;
 WeaponFixture& operator=(const WeaponFixture&)=delete;
 ~WeaponFixture(){std::error_code ec;std::filesystem::remove(path,ec);}
};
}
