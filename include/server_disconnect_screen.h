#pragma once
#include "lobby_keepalive.h"
#include <span>
#include <string_view>
namespace mgo2win {
constexpr std::wstring_view server_disconnect_text(LobbyDisconnectReason reason){
 switch(reason){
 case LobbyDisconnectReason::beacon_timeout:return L"サーバーから30秒間応答がないため、接続を解除しました。\nサーバーや通信の状態を確認してから、STARTで入り直してください。";
 case LobbyDisconnectReason::invalid_beacon:return L"サーバーの応答を確認できないため、接続を解除しました。\nSTARTで入り直してください。";
 case LobbyDisconnectReason::network_error:return L"サーバーとの接続が切断されました。\nサーバーや通信の状態を確認してから、STARTで入り直してください。";
 default:return {};
 }
}
void paint_server_disconnect(std::span<uint32_t>,LobbyDisconnectReason);
void paint_lobby_ping(std::span<uint32_t>,std::optional<uint32_t>);
}
