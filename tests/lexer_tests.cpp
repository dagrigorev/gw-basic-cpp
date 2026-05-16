#include "gwbasic/lexer.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

using gwbasic::TokenType;

void fail(const std::string& message) {
    std::cerr << "Lexer test failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

void require_tokens(const std::vector<gwbasic::Token>& tokens, const std::vector<TokenType>& expected) {
    require(tokens.size() == expected.size(), "unexpected token count: got " + std::to_string(tokens.size()) + ", expected " + std::to_string(expected.size()));
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(tokens[i].type == expected[i], "unexpected token type at index " + std::to_string(i));
    }
}

void require_throws(const gwbasic::Lexer& lexer, const std::string& source) {
    try {
        (void)lexer.tokenize(source);
    } catch (const std::exception&) {
        return;
    }
    fail("expected tokenization failure for: " + source);
}

} // namespace

int main() {
    const gwbasic::Lexer lexer;

    {
        const auto tokens = lexer.tokenize("");
        require_tokens(tokens, {TokenType::EndOfLine});
        require(tokens.front().column == 1, "empty input end column");
    }

    {
        const auto tokens = lexer.tokenize("10 print A$, 12.5: REM keep: raw text");
        require_tokens(tokens, {
            TokenType::LineNumber,
            TokenType::KeywordPrint,
            TokenType::Identifier,
            TokenType::Comma,
            TokenType::Number,
            TokenType::Colon,
            TokenType::KeywordRem,
            TokenType::String,
            TokenType::EndOfLine,
        });
        require(tokens[1].lexeme == "PRINT", "keywords are normalized to upper case");
        require(tokens[2].lexeme == "A$", "string variable suffix is preserved");
        require(tokens[4].lexeme == "12.5", "decimal number lexeme");
        require(tokens[7].lexeme == " keep: raw text", "REM keeps the rest of the line verbatim");
    }

    {
        const auto tokens = lexer.tokenize("A=.5:IF A<>0 AND A<=1 OR NOT A>=2 THEN GOTO 100");
        require_tokens(tokens, {
            TokenType::Identifier,
            TokenType::Equal,
            TokenType::Number,
            TokenType::Colon,
            TokenType::KeywordIf,
            TokenType::Identifier,
            TokenType::NotEqual,
            TokenType::Number,
            TokenType::KeywordAnd,
            TokenType::Identifier,
            TokenType::LessEqual,
            TokenType::Number,
            TokenType::KeywordOr,
            TokenType::KeywordNot,
            TokenType::Identifier,
            TokenType::GreaterEqual,
            TokenType::Number,
            TokenType::KeywordThen,
            TokenType::KeywordGoto,
            TokenType::Number,
            TokenType::EndOfLine,
        });
        require(tokens[2].lexeme == ".5", "fraction without leading zero");
    }

    {
        const auto tokens = lexer.tokenize("PRINT 17 MOD 5, 17\\5, 2^3");
        require_tokens(tokens, {
            TokenType::KeywordPrint,
            TokenType::Number,
            TokenType::KeywordMod,
            TokenType::Number,
            TokenType::Comma,
            TokenType::Number,
            TokenType::Backslash,
            TokenType::Number,
            TokenType::Comma,
            TokenType::Number,
            TokenType::Caret,
            TokenType::Number,
            TokenType::EndOfLine,
        });
    }

    {
        const auto tokens = lexer.tokenize("RANDOMIZE 123");
        require_tokens(tokens, {
            TokenType::KeywordRandomize,
            TokenType::Number,
            TokenType::EndOfLine,
        });
    }

    {
        const auto tokens = lexer.tokenize("ERASE A, B$");
        require_tokens(tokens, {
            TokenType::KeywordErase,
            TokenType::Identifier,
            TokenType::Comma,
            TokenType::Identifier,
            TokenType::EndOfLine,
        });
    }

    {
        const auto tokens = lexer.tokenize("OPTION BASE 1");
        require_tokens(tokens, {
            TokenType::KeywordOption,
            TokenType::KeywordBase,
            TokenType::Number,
            TokenType::EndOfLine,
        });
    }

    {
        const auto tokens = lexer.tokenize("LONG_IDENTIFIER_12345678901234567890$ = \"quoted text\"");
        require_tokens(tokens, {
            TokenType::Identifier,
            TokenType::Equal,
            TokenType::String,
            TokenType::EndOfLine,
        });
        require(tokens[0].lexeme == "LONG_IDENTIFIER_12345678901234567890$", "long identifier is preserved");
    }

    require_throws(lexer, "10 PRINT \"unterminated");
    require_throws(lexer, "10 PRINT @");

    std::cout << "Lexer tests passed\n";
    return 0;
}
