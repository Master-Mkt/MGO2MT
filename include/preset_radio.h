#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mgo2::radio {
// Original digital masks 0x10/0x20/0x40/0x80, clockwise. Native action
// indices are up=0, down=1, left=2, right=3; the adapter must reorder them.
enum class Direction : std::uint8_t { up, right, down, left };
enum class MenuState : std::uint8_t { closed, chat, preset_categories, preset_messages };
struct Message {
    std::uint8_t presetId;
    std::string_view text;
    std::uint32_t gcxResource;
};
struct Category {
    std::string_view text;
    std::uint32_t gcxResource;
    std::array<Message, 4> messages;
};
// Current ELF fallback assignment and Japanese text for original type 7.
// This display choice does not assign an original voice to a native character.
const std::array<Category, 4>& defaultCategories() noexcept;
struct Selection {
    Direction category;
    Direction choice;
    std::uint8_t presetId;
};
class Menu {
public:
    MenuState state() const noexcept { return state_; }
    std::optional<Direction> category() const noexcept { return category_; }
    // Call only on a fresh SELECT press, never from key repeat. Requested
    // native order: chat then radio (original help describes the reverse).
    MenuState select() noexcept;
    // Digital press edges only. Two fresh edges choose category then message.
    // A selection is returned once, with no audio or transport side effect.
    std::optional<Selection> digital(Direction direction) noexcept;
    void cancel() noexcept;
    // Use on focus loss, scene/identity change, disconnect and modal takeover.
    void reset() noexcept;
private:
    MenuState state_ = MenuState::closed;
    std::optional<Direction> category_;
};
} // namespace mgo2::radio
