#include "actions/keyboard_shortcut.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <utility>

namespace strokes::actions {
namespace {

std::string uppercase_trimmed(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    std::string result(value.substr(first, last - first + 1));
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

std::optional<VirtualKey> modifier_key(std::string_view token) {
    if (token == "CTRL" || token == "CONTROL") return VirtualKey::control;
    if (token == "SHIFT") return VirtualKey::shift;
    if (token == "ALT") return VirtualKey::alt;
    if (token == "WIN" || token == "WINDOWS") return VirtualKey::left_windows;
    return std::nullopt;
}

std::optional<VirtualKey> primary_key(std::string_view token) {
    if (token.size() == 1) {
        const char character = token.front();
        if (character >= 'A' && character <= 'Z') {
            return static_cast<VirtualKey>(
                static_cast<std::uint16_t>(VirtualKey::letter_a) + character - 'A');
        }
        if (character >= '0' && character <= '9') {
            return static_cast<VirtualKey>(
                static_cast<std::uint16_t>(VirtualKey::digit_0) + character - '0');
        }
    }

    if (token == "BACKSPACE") return VirtualKey::backspace;
    if (token == "TAB") return VirtualKey::tab;
    if (token == "ENTER" || token == "RETURN") return VirtualKey::enter;
    if (token == "ESC" || token == "ESCAPE") return VirtualKey::escape;
    if (token == "SPACE") return VirtualKey::space;
    if (token == "PAGEUP" || token == "PGUP") return VirtualKey::page_up;
    if (token == "PAGEDOWN" || token == "PGDN") return VirtualKey::page_down;
    if (token == "END") return VirtualKey::end;
    if (token == "HOME") return VirtualKey::home;
    if (token == "LEFT") return VirtualKey::left;
    if (token == "UP") return VirtualKey::up;
    if (token == "RIGHT") return VirtualKey::right;
    if (token == "DOWN") return VirtualKey::down;
    if (token == "DELETE" || token == "DEL") return VirtualKey::delete_key;

    if (token.size() >= 2 && token.front() == 'F') {
        int function_number = 0;
        const auto result = std::from_chars(token.data() + 1, token.data() + token.size(), function_number);
        if (result.ec == std::errc{} && result.ptr == token.data() + token.size() &&
            function_number >= 1 && function_number <= 24) {
            return static_cast<VirtualKey>(
                static_cast<std::uint16_t>(VirtualKey::f1) + function_number - 1);
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<KeyboardShortcut> parse_keyboard_shortcut(std::string_view text) {
    KeyboardShortcut shortcut;
    bool has_primary_key = false;
    std::size_t start = 0;

    while (start <= text.size()) {
        const std::size_t separator = text.find('+', start);
        const std::string token = uppercase_trimmed(text.substr(start, separator - start));
        if (token.empty()) {
            return std::nullopt;
        }

        if (const auto modifier = modifier_key(token)) {
            if (has_primary_key || std::ranges::find(shortcut.modifiers, *modifier) != shortcut.modifiers.end()) {
                return std::nullopt;
            }
            shortcut.modifiers.push_back(*modifier);
        } else if (const auto key = primary_key(token); key && !has_primary_key) {
            shortcut.key = *key;
            has_primary_key = true;
        } else {
            return std::nullopt;
        }

        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
    return has_primary_key ? std::optional{std::move(shortcut)} : std::nullopt;
}

}  // namespace strokes::actions
