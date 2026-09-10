#pragma once
#include <atomic>
#include <string>
namespace mgo2win {
struct HttpText { unsigned status=0; std::wstring text; std::string error,sha256; size_t bytes=0; };
bool allowed_policy_url(const std::wstring& url);
std::wstring decode_policy(unsigned status,const std::wstring& contentType,const std::string& bytes);
HttpText fetch_policy(const std::wstring& url,const std::atomic_bool& cancel);
}
