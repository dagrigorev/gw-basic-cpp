#pragma once

#include "gwbasic/ast.hpp"
#include "gwbasic/token.hpp"

#include <vector>

namespace gwbasic {

class Parser {
public:
    [[nodiscard]] auto parse_line(const std::vector<Token>& tokens, std::string original_text) const -> ParsedLine;
};

} // namespace gwbasic
