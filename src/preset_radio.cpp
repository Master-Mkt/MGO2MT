#include "preset_radio.h"

namespace mgo2::radio {
const std::array<Category, 4>& defaultCategories() noexcept {
    static constexpr std::array<Category, 4> categories{{
        {"攻撃系", 7446, {{{0,"ゴーゴーゴー！",7493}, {1,"敵がいるぞ!!",7518},
                            {2,"カバー！",7595}, {3,"待て！",7657}}}},
        {"応答系", 7452, {{{4,"了解",7709}, {5,"ついて来い！",7774},
                            {6,"反対だ",7854}, {7,"異状なし",7954}}}},
        {"防御系", 7458, {{{9,"ターゲットを守れ!",8109}, {10,"固まって進もう!",8170},
                            {11,"一時撤退！",8268}, {12,"隠れろ!!",8345}}}},
        {"コミュニケーション系", 7464, {{{13,"すまない",8379}, {14,"すごいな",8467},
                            {15,"幸運を",8568}, {16,"頼んだ",8657}}}},
    }};
    return categories;
}
MenuState Menu::select() noexcept {
    category_.reset();
    if (state_ == MenuState::closed) state_ = MenuState::chat;
    else if (state_ == MenuState::chat) state_ = MenuState::preset_categories;
    else state_ = MenuState::chat;
    return state_;
}
std::optional<Selection> Menu::digital(Direction direction) noexcept {
    const auto index = static_cast<std::uint8_t>(direction);
    if (index >= 4) return std::nullopt;
    if (state_ == MenuState::preset_categories) {
        category_ = direction;
        state_ = MenuState::preset_messages;
    } else if (state_ == MenuState::preset_messages && category_) {
        const Selection selected{*category_, direction,
            defaultCategories()[static_cast<std::uint8_t>(*category_)].messages[index].presetId};
        reset();
        return selected;
    }
    return std::nullopt;
}
void Menu::cancel() noexcept {
    if (state_ == MenuState::preset_messages) {
        category_.reset();
        state_ = MenuState::preset_categories;
    } else reset();
}
void Menu::reset() noexcept { state_ = MenuState::closed; category_.reset(); }
} // namespace mgo2::radio
