#pragma once

#include "gwbasic/token.hpp"

#include <string>
#include <vector>

namespace gwbasic {

class Lexer {
public:
    [[nodiscard]] auto tokenize(const std::string& line) const -> std::vector<Token>;
};

} // namespace gwbasic
