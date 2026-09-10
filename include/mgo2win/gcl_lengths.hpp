#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace mgo2win::gcl {
struct Length {
    std::uint32_t value;
    std::size_t header_bytes;
};
// Decode the header only; callers must separately validate the payload extent.
// nullopt for truncated headers is a host-side addition to the original contract.
std::optional<Length> block_length(std::span<const std::uint8_t> bytes);
std::optional<Length> command_length(std::span<const std::uint8_t> bytes);
}
