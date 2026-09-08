#include "config/json.h"
#include "test_support.h"
#include <cmath>
#include <string>

namespace strokes::tests {
namespace {
namespace json = config::json;

void complete_value_tests() {
    const std::string source = R"json({"enabled":true,"items":[1,false,"x"],"number":-1.25e3,"nothing":null,"object":{"name":"Strokes++"}})json";
    const auto parsed = json::parse(source);
    check(static_cast<bool>(parsed), "complete JSON value parses");
    const auto* object = parsed.value->get_if<json::Object>();
    check(object && object->at("enabled").get_if<bool>() && *object->at("enabled").get_if<bool>(), "JSON boolean is retained");
    check(object && object->at("items").get_if<json::Array>()->size() == 3, "JSON array is retained");
    check(object && std::abs(*object->at("number").get_if<double>() + 1250.0) < 0.001, "JSON exponent number is retained");
}

void string_tests() {
    const auto parsed = json::parse(R"json("line\nquote:\" unicode:\u263A music:\uD834\uDD1E")json");
    check(static_cast<bool>(parsed), "escaped JSON string parses");
    const auto* value = parsed.value->get_if<std::string>();
    check(value && value->find('\n') != std::string::npos, "escaped newline is decoded");
    check(value && value->find("\xE2\x98\xBA") != std::string::npos, "BMP Unicode escape becomes UTF-8");
    check(value && value->find("\xF0\x9D\x84\x9E") != std::string::npos, "Unicode surrogate pair becomes UTF-8");
}

void rejection_tests() {
    check(!json::parse("").value, "empty JSON is rejected");
    check(!json::parse("{\"a\":1,}").value, "trailing object comma is rejected");
    check(!json::parse("[1 2]").value, "missing array comma is rejected");
    check(!json::parse("{\"a\":1,\"a\":2}").value, "duplicate object key is rejected");
    check(!json::parse("01").value, "leading-zero number is rejected");
    check(!json::parse(R"json("\uD800")json").value, "unpaired Unicode surrogate is rejected");
    const auto trailing = json::parse("true false");
    check(trailing.error && trailing.error->offset == 5, "parse error reports its byte offset");
}

void serialization_tests() {
    json::Object object;
    object.emplace("message", "hello\nworld");
    object.emplace("number", 42.5);
    object.emplace("values", json::Array{true, nullptr, "text"});
    const std::string encoded = json::serialize(object);
    const auto reparsed = json::parse(encoded);
    check(static_cast<bool>(reparsed), "serialized JSON can be parsed again");
    check(encoded.find("\n  \"message\"") != std::string::npos, "pretty serializer emits deterministic indentation and sorted keys");
}
}

void run_json_tests() {
    complete_value_tests();
    string_tests();
    rejection_tests();
    serialization_tests();
}
}  // namespace strokes::tests
