#include "config/json.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace strokes::config::json {
namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    ParseResult run() {
        skip_space();
        auto value = parse_value();
        if (!value) return {std::nullopt, error_};
        skip_space();
        if (position_ != text_.size()) return fail("unexpected trailing content");
        return {std::move(value), std::nullopt};
    }

private:
    std::optional<Value> parse_value() {
        if (position_ >= text_.size()) return set_error("expected a JSON value");
        switch (text_[position_]) {
        case 'n': return literal("null", Value{});
        case 't': return literal("true", Value{true});
        case 'f': return literal("false", Value{false});
        case '"': {
            auto value = parse_string();
            return value ? std::optional<Value>{Value{std::move(*value)}} : std::nullopt;
        }
        case '[': return parse_array();
        case '{': return parse_object();
        default: return parse_number();
        }
    }

    std::optional<Value> literal(std::string_view token, Value value) {
        if (text_.substr(position_, token.size()) != token) return set_error("invalid literal");
        position_ += token.size();
        return value;
    }

    std::optional<Value> parse_number() {
        const std::size_t start = position_;
        if (peek('-')) ++position_;
        if (peek('0')) {
            ++position_;
        } else {
            if (!digit()) return set_error("invalid number");
            while (digit()) ++position_;
        }
        if (peek('.')) {
            ++position_;
            if (!digit()) return set_error("invalid number fraction");
            while (digit()) ++position_;
        }
        if (peek('e') || peek('E')) {
            ++position_;
            if (peek('+') || peek('-')) ++position_;
            if (!digit()) return set_error("invalid number exponent");
            while (digit()) ++position_;
        }
        double value = 0;
        const auto result = std::from_chars(text_.data() + start, text_.data() + position_, value);
        if (result.ec != std::errc{} || result.ptr != text_.data() + position_ || !std::isfinite(value))
            return set_error("number is out of range");
        return Value{value};
    }

    std::optional<std::string> parse_string() {
        if (!peek('"')) return set_error_string("expected string");
        ++position_;
        std::string result;
        while (position_ < text_.size()) {
            const unsigned char character = static_cast<unsigned char>(text_[position_++]);
            if (character == '"') return result;
            if (character < 0x20) return set_error_string("unescaped control character");
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ >= text_.size()) return set_error_string("incomplete escape");
            const char escaped = text_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                auto codepoint = parse_hex4();
                if (!codepoint) return std::nullopt;
                if (*codepoint >= 0xD800 && *codepoint <= 0xDBFF) {
                    if (text_.substr(position_, 2) != "\\u")
                        return set_error_string("missing low Unicode surrogate");
                    position_ += 2;
                    auto low = parse_hex4();
                    if (!low || *low < 0xDC00 || *low > 0xDFFF)
                        return set_error_string("invalid low Unicode surrogate");
                    *codepoint = 0x10000 + ((*codepoint - 0xD800) << 10) + (*low - 0xDC00);
                } else if (*codepoint >= 0xDC00 && *codepoint <= 0xDFFF) {
                    return set_error_string("unexpected low Unicode surrogate");
                }
                append_utf8(result, *codepoint);
                break;
            }
            default: return set_error_string("invalid escape");
            }
        }
        return set_error_string("unterminated string");
    }

    std::optional<unsigned> parse_hex4() {
        if (position_ + 4 > text_.size()) { set_error("incomplete Unicode escape"); return std::nullopt; }
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value += static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value += static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value += static_cast<unsigned>(c - 'A' + 10);
            else { set_error("invalid Unicode escape"); return std::nullopt; }
        }
        return value;
    }

    std::optional<Value> parse_array() {
        ++position_;
        Array result;
        skip_space();
        if (consume(']')) return Value{std::move(result)};
        for (;;) {
            skip_space();
            auto value = parse_value();
            if (!value) return std::nullopt;
            result.push_back(std::move(*value));
            skip_space();
            if (consume(']')) return Value{std::move(result)};
            if (!consume(',')) return set_error("expected ',' or ']'");
        }
    }

    std::optional<Value> parse_object() {
        ++position_;
        Object result;
        skip_space();
        if (consume('}')) return Value{std::move(result)};
        for (;;) {
            skip_space();
            auto key = parse_string();
            if (!key) return std::nullopt;
            skip_space();
            if (!consume(':')) return set_error("expected ':'");
            skip_space();
            auto value = parse_value();
            if (!value) return std::nullopt;
            if (!result.emplace(std::move(*key), std::move(*value)).second)
                return set_error("duplicate object key");
            skip_space();
            if (consume('}')) return Value{std::move(result)};
            if (!consume(',')) return set_error("expected ',' or '}'");
        }
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    void skip_space() { while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\n' || text_[position_] == '\r' || text_[position_] == '\t')) ++position_; }
    bool peek(char value) const { return position_ < text_.size() && text_[position_] == value; }
    bool digit() const { return position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9'; }
    bool consume(char value) { if (!peek(value)) return false; ++position_; return true; }
    std::optional<Value> set_error(std::string message) { if (!error_) error_ = ParseError{position_, std::move(message)}; return std::nullopt; }
    std::optional<std::string> set_error_string(std::string message) { set_error(std::move(message)); return std::nullopt; }
    ParseResult fail(std::string message) { set_error(std::move(message)); return {std::nullopt, error_}; }

    std::string_view text_;
    std::size_t position_{};
    std::optional<ParseError> error_;
};

void indent(std::string& out, int depth) { out.append(static_cast<std::size_t>(depth * 2), ' '); }

void write_string(std::string& out, std::string_view value) {
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break; case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) { char buffer[7]{}; std::snprintf(buffer, sizeof(buffer), "\\u%04x", c); out += buffer; }
            else out.push_back(static_cast<char>(c));
        }
    }
    out.push_back('"');
}

void write_value(std::string& out, const Value& value, bool pretty, int depth) {
    if (std::holds_alternative<std::nullptr_t>(value.data)) out += "null";
    else if (const auto* b = value.get_if<bool>()) out += *b ? "true" : "false";
    else if (const auto* number = value.get_if<double>()) {
        char buffer[64]{};
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), *number, std::chars_format::general, 17);
        out.append(buffer, result.ptr);
    } else if (const auto* string = value.get_if<std::string>()) write_string(out, *string);
    else if (const auto* array = value.get_if<Array>()) {
        out.push_back('[');
        for (std::size_t i = 0; i < array->size(); ++i) {
            if (i) out.push_back(',');
            if (pretty) { out.push_back('\n'); indent(out, depth + 1); }
            write_value(out, (*array)[i], pretty, depth + 1);
        }
        if (pretty && !array->empty()) { out.push_back('\n'); indent(out, depth); }
        out.push_back(']');
    } else if (const auto* object = value.get_if<Object>()) {
        out.push_back('{');
        std::size_t i = 0;
        for (const auto& [key, child] : *object) {
            if (i++) out.push_back(',');
            if (pretty) { out.push_back('\n'); indent(out, depth + 1); }
            write_string(out, key); out += pretty ? ": " : ":";
            write_value(out, child, pretty, depth + 1);
        }
        if (pretty && !object->empty()) { out.push_back('\n'); indent(out, depth); }
        out.push_back('}');
    }
}

}  // namespace

ParseResult parse(std::string_view text) { return Parser(text).run(); }
std::string serialize(const Value& value, bool pretty) { std::string out; write_value(out, value, pretty, 0); if (pretty) out.push_back('\n'); return out; }

}  // namespace strokes::config::json
