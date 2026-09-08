#pragma once

#include <cstdint>
#include <string>

namespace strokes::context {

struct ApplicationContext {
    std::uintptr_t window_handle{};
    std::uint32_t process_id{};
    std::string executable_name;
    std::string window_title;
    std::string window_class;
};

}  // namespace strokes::context
