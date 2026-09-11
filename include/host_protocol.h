#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <stdexcept>
namespace mgo2win::host {
// Recovered from retail PPC 264C78/2666C8, not the lobby TCP cipher.
constexpr uint32_t initial_cipher=0x87103c2f,initial_mac=0x2b58de69,version=0x4d258ab7;
constexpr size_t max_datagram=2048;
enum class Error {extent,integrity,compression,message,identity,version_mismatch,sequence};
struct Invalid : std::runtime_error {Error code;explicit Invalid(Error e):runtime_error("host protocol"),code(e){}};
struct Keys {uint32_t cipher=initial_cipher,mac=initial_mac;};
struct Message {uint16_t channel=0;bool reliable=true,ack=false,timing=false;uint8_t serial=0;std::vector<uint8_t> payload;};
struct Packet {uint16_t sequence=0;std::vector<Message> messages;};
struct Endpoint {std::array<uint8_t,4> address{};uint16_t port=0;bool operator==(const Endpoint&)const=default;};
struct Hello {uint32_t character=0,seed=0;uint8_t flags=2;uint16_t extra=2;std::vector<Endpoint> endpoints;};
bool valid_endpoint(const Endpoint&,bool allowLoopback=false);
std::vector<uint8_t> encode_hello(const Hello&);
Hello decode_hello(std::span<const uint8_t>,uint32_t expectedCharacter);
std::vector<uint8_t> encode_messages(std::span<const Message>);
std::vector<Message> decode_messages(std::span<const uint8_t>);
std::vector<uint8_t> inflate(std::span<const uint8_t>);
std::vector<uint8_t> encode(const Packet&,Keys={});
// expectedSequence supplies the high bit across the 32768-packet boundary.
Packet decode(std::span<const uint8_t>,Keys={},uint16_t expectedSequence=0);
}
