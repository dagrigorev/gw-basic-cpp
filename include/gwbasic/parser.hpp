#pragma once

#include "gwbasic/ast.hpp"
#include "gwbasic/token.hpp"

#include <string>
#include <vector>

namespace gwbasic {

/** A recoverable parser diagnostic with a 1-based source column. */
struct ParseDiagnostic {
    std::string message;
    std::size_t column{};
};

/** Result returned by tolerant parsing: partial AST plus diagnostics. */
struct ParseResult {
    ParsedLine line;
    std::vector<ParseDiagnostic> diagnostics;
};

/**
 * Recursive-descent parser for one tokenized BASIC line.
 *
 * `parse_line` is strict and is used by execution paths. `parse_line_recovering`
 * is intended for editor/checker workflows that need multiple diagnostics from
 * a single physical line.
 */
class Parser {
public:
    /** Parse a line and throw on the first syntax error. */
    [[nodiscard]] auto parse_line(const std::vector<Token>& tokens, std::string original_text) const -> ParsedLine;

    /**
     * Parse a line while recovering after bad statements.
     *
     * Recovery skips tokens up to the next statement separator (`:`) or end of
     * line, preserving valid statements before and after the error.
     */
    [[nodiscard]] auto parse_line_recovering(const std::vector<Token>& tokens, std::string original_text) const -> ParseResult;
};

} // namespace gwbasic
