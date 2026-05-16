#pragma once

#include "gwbasic/token.hpp"

#include <string>
#include <vector>

namespace gwbasic {

/**
 * Converts one physical BASIC source line into a token stream.
 *
 * The lexer is intentionally stateless: callers can reuse one instance for
 * stored program lines, immediate statements, syntax checking, and fuzz tests.
 */
class Lexer {
public:
    /**
     * Tokenize a single line of BASIC source.
     * @throws std::runtime_error when the input contains an invalid character
     *         or an unterminated string literal.
     */
    [[nodiscard]] auto tokenize(const std::string& line) const -> std::vector<Token>;
};

} // namespace gwbasic
