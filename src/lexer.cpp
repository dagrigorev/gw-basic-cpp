#include "gwbasic/lexer.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace gwbasic {
namespace {

[[nodiscard]] auto upper(std::string text) -> std::string {
    std::ranges::transform(text, text.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return text;
}

[[nodiscard]] auto keyword_type(const std::string& text) -> TokenType {
    static const std::unordered_map<std::string, TokenType> map{
        {"PRINT", TokenType::KeywordPrint}, {"USING", TokenType::KeywordUsing}, {"OPEN", TokenType::KeywordOpen}, {"CLOSE", TokenType::KeywordClose}, {"WRITE", TokenType::KeywordWrite}, {"LINE", TokenType::KeywordLine}, {"AS", TokenType::KeywordAs}, {"LET", TokenType::KeywordLet},
        {"INPUT", TokenType::KeywordInput}, {"OUTPUT", TokenType::KeywordOutput}, {"APPEND", TokenType::KeywordAppend}, {"RANDOM", TokenType::KeywordRandom}, {"LEN", TokenType::KeywordLen}, {"IF", TokenType::KeywordIf}, {"AND", TokenType::KeywordAnd}, {"OR", TokenType::KeywordOr}, {"NOT", TokenType::KeywordNot},
        {"THEN", TokenType::KeywordThen}, {"ELSE", TokenType::KeywordElse}, {"GOTO", TokenType::KeywordGoto},
        {"GOSUB", TokenType::KeywordGosub}, {"RETURN", TokenType::KeywordReturn},
        {"FOR", TokenType::KeywordFor}, {"TO", TokenType::KeywordTo},
        {"STEP", TokenType::KeywordStep}, {"NEXT", TokenType::KeywordNext},
        {"DATA", TokenType::KeywordData}, {"READ", TokenType::KeywordRead},
        {"RESTORE", TokenType::KeywordRestore}, {"DIM", TokenType::KeywordDim},
        {"WHILE", TokenType::KeywordWhile}, {"WEND", TokenType::KeywordWend},
        {"ON", TokenType::KeywordOn}, {"DEFINT", TokenType::KeywordDefint}, {"DEFSTR", TokenType::KeywordDefstr}, {"DEFSNG", TokenType::KeywordDefsng}, {"DEFDBL", TokenType::KeywordDefdbl}, {"STOP", TokenType::KeywordStop},
        {"CONT", TokenType::KeywordCont}, {"END", TokenType::KeywordEnd}, {"REM", TokenType::KeywordRem},
        {"LIST", TokenType::KeywordList}, {"RUN", TokenType::KeywordRun},
        {"NEW", TokenType::KeywordNew}, {"CLEAR", TokenType::KeywordClear}, {"FIELD", TokenType::KeywordField}, {"GET", TokenType::KeywordGet}, {"PUT", TokenType::KeywordPut}, {"LSET", TokenType::KeywordLset}, {"RSET", TokenType::KeywordRset}, {"KILL", TokenType::KeywordKill}, {"NAME", TokenType::KeywordName}, {"MKDIR", TokenType::KeywordMkdir}, {"RMDIR", TokenType::KeywordRmdir}, {"CLS", TokenType::KeywordCls}, {"LOCATE", TokenType::KeywordLocate}, {"COLOR", TokenType::KeywordColor}, {"BEEP", TokenType::KeywordBeep}, {"SCREEN", TokenType::KeywordScreen}, {"KEY", TokenType::KeywordKey}, {"SOUND", TokenType::KeywordSound}, {"PLAY", TokenType::KeywordPlay}, {"PSET", TokenType::KeywordPset}, {"CIRCLE", TokenType::KeywordCircle}, {"PAINT", TokenType::KeywordPaint}, {"DRAW", TokenType::KeywordDraw}, {"VIEW", TokenType::KeywordView}, {"WINDOW", TokenType::KeywordWindow}, {"PALETTE", TokenType::KeywordPalette}
    };
    if (const auto it = map.find(text); it != map.end()) {
        return it->second;
    }
    return TokenType::Identifier;
}

} // namespace

auto Lexer::tokenize(const std::string& line) const -> std::vector<Token> {
    std::vector<Token> tokens;
    std::size_t i = 0;
    bool first_token = true;

    while (i < line.size()) {
        const auto ch = static_cast<unsigned char>(line[i]);
        if (std::isspace(ch)) {
            ++i;
            continue;
        }

        const auto col = i + 1;
        if (std::isdigit(ch) || (!first_token && ch == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            std::size_t start = i;
            if (line[i] == '.') {
                ++i;
            } else {
                while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
                    ++i;
                }
            }
            if (!first_token && i < line.size() && line[i] == '.') {
                ++i;
                while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
                    ++i;
                }
            }
            auto lexeme = line.substr(start, i - start);
            tokens.push_back({first_token ? TokenType::LineNumber : TokenType::Number, lexeme, col});
            first_token = false;
            continue;
        }

        if (std::isalpha(ch) || ch == '_') {
            std::size_t start = i;
            while (i < line.size()) {
                const auto c = static_cast<unsigned char>(line[i]);
                if (!std::isalnum(c) && c != '_' && c != '$') {
                    break;
                }
                ++i;
            }
            auto text = upper(line.substr(start, i - start));
            auto type = keyword_type(text);
            if (type == TokenType::KeywordRem) {
                tokens.push_back({type, text, col});
                if (i < line.size()) {
                    tokens.push_back({TokenType::String, line.substr(i), i + 1});
                }
                tokens.push_back({TokenType::EndOfLine, "", line.size() + 1});
                return tokens;
            }
            tokens.push_back({type, text, col});
            first_token = false;
            continue;
        }

        if (ch == '"') {
            ++i;
            std::size_t start = i;
            while (i < line.size() && line[i] != '"') {
                ++i;
            }
            if (i >= line.size()) {
                throw std::runtime_error("Unterminated string literal");
            }
            tokens.push_back({TokenType::String, line.substr(start, i - start), col});
            ++i;
            first_token = false;
            continue;
        }

        auto push = [&](TokenType type, std::size_t advance) {
            tokens.push_back({type, line.substr(i, advance), col});
            i += advance;
            first_token = false;
        };

        if (i + 1 < line.size()) {
            const auto two = line.substr(i, 2);
            if (two == "<>") { push(TokenType::NotEqual, 2); continue; }
            if (two == "<=") { push(TokenType::LessEqual, 2); continue; }
            if (two == ">=") { push(TokenType::GreaterEqual, 2); continue; }
        }

        switch (line[i]) {
            case ',': push(TokenType::Comma, 1); break;
            case ':': push(TokenType::Colon, 1); break;
            case ';': push(TokenType::Semicolon, 1); break;
            case '-': push(TokenType::Minus, 1); break;
            case '(': push(TokenType::LeftParen, 1); break;
            case ')': push(TokenType::RightParen, 1); break;
            case '+': push(TokenType::Plus, 1); break;
            case '*': push(TokenType::Star, 1); break;
            case '/': push(TokenType::Slash, 1); break;
            case '#': push(TokenType::Hash, 1); break;
            case '=': push(TokenType::Equal, 1); break;
            case '<': push(TokenType::Less, 1); break;
            case '>': push(TokenType::Greater, 1); break;
            default: throw std::runtime_error("Unexpected character at column " + std::to_string(col));
        }
    }

    tokens.push_back({TokenType::EndOfLine, "", line.size() + 1});
    return tokens;
}

} // namespace gwbasic
