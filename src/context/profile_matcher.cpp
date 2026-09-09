#include "context/profile_matcher.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <locale>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace strokes::context {
namespace {

std::optional<std::wstring> decode_utf8(std::string_view text) {
  std::wstring result;
  for (std::size_t index = 0; index < text.size();) {
    const auto first = static_cast<unsigned char>(text[index++]);
    char32_t codepoint = 0;
    int trailing = 0;
    if (first < 0x80)
      codepoint = first;
    else if ((first & 0xE0) == 0xC0) {
      codepoint = first & 0x1F;
      trailing = 1;
    } else if ((first & 0xF0) == 0xE0) {
      codepoint = first & 0x0F;
      trailing = 2;
    } else if ((first & 0xF8) == 0xF0) {
      codepoint = first & 0x07;
      trailing = 3;
    } else
      return std::nullopt;
    if (index + static_cast<std::size_t>(trailing) > text.size()) return std::nullopt;
    for (int count = 0; count < trailing; ++count) {
      const auto next = static_cast<unsigned char>(text[index++]);
      if ((next & 0xC0) != 0x80) return std::nullopt;
      codepoint = (codepoint << 6) | (next & 0x3F);
    }
    if ((trailing == 1 && codepoint < 0x80) || (trailing == 2 && codepoint < 0x800) ||
        (trailing == 3 && codepoint < 0x10000) || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF))
      return std::nullopt;
    if constexpr (sizeof(wchar_t) >= 4) {
      result.push_back(static_cast<wchar_t>(codepoint));
    } else if (codepoint <= 0xFFFF) {
      result.push_back(static_cast<wchar_t>(codepoint));
    } else {
      codepoint -= 0x10000;
      result.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
      result.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
    }
  }
  return result;
}

std::string encode_utf8(std::wstring_view text) {
  std::string result;
  for (std::size_t index = 0; index < text.size(); ++index) {
    char32_t codepoint = static_cast<char32_t>(text[index]);
    if constexpr (sizeof(wchar_t) < 4) {
      if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index + 1 < text.size()) {
        const char32_t low = static_cast<char32_t>(text[++index]);
        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
      }
    }
    if (codepoint <= 0x7F)
      result.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF) {
      result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }
  return result;
}

std::string lowercase(std::string_view text) {
  try {
    auto decoded = decode_utf8(text);
    if (!decoded) throw std::runtime_error("invalid UTF-8");
    std::wstring wide = std::move(*decoded);
    const std::locale user_locale("");
    const auto& facet = std::use_facet<std::ctype<wchar_t>>(user_locale);
    if (!wide.empty()) facet.tolower(wide.data(), wide.data() + wide.size());
    return encode_utf8(wide);
  } catch (const std::exception&) {
    std::string result;
    result.reserve(text.size());
    std::ranges::transform(text, std::back_inserter(result), [](unsigned char character) {
      return static_cast<char>(std::tolower(character));
    });
    return result;
  }
}

std::string_view property_value(ApplicationProperty property,
                                const ApplicationContext& application) {
  switch (property) {
    case ApplicationProperty::process_name:
      return application.executable_name;
    case ApplicationProperty::window_title:
      return application.window_title;
    case ApplicationProperty::window_class:
      return application.window_class;
  }
  return {};
}

}  // namespace

bool ProfileMatcher::matches(const ApplicationProfile& profile,
                             const ApplicationContext& application) const {
  if (!profile.enabled || profile.criteria.empty()) {
    return false;
  }
  return std::ranges::all_of(profile.criteria, [&](const MatchCriterion& criterion) {
    return matches_criterion(criterion, application);
  });
}

bool ProfileMatcher::matches_criterion(const MatchCriterion& criterion,
                                       const ApplicationContext& application) {
  if (criterion.value.empty()) {
    return false;
  }

  const std::string_view actual = property_value(criterion.property, application);
  switch (criterion.mode) {
    case MatchMode::exact:
      return lowercase(actual) == lowercase(criterion.value);
    case MatchMode::contains:
      return lowercase(actual).find(lowercase(criterion.value)) != std::string::npos;
    case MatchMode::regex:
      try {
        if (criterion.compiled_regex) {
          return std::regex_search(actual.begin(), actual.end(), *criterion.compiled_regex);
        }
        return std::regex_search(
            actual.begin(), actual.end(),
            std::regex(criterion.value, std::regex::ECMAScript | std::regex::icase));
      } catch (const std::regex_error&) {
        return false;
      }
  }
  return false;
}

}  // namespace strokes::context
