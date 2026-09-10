#include <atomic>
#include "audio_control.h"
int run_audio_probe(int, wchar_t**, const std::atomic_bool*,const mgo2win::AudioControl*);
int wmain(int argc, wchar_t** argv) { return run_audio_probe(argc, argv, nullptr,nullptr); }
