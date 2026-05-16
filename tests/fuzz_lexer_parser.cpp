#include "gwbasic/lexer.hpp"
#include "gwbasic/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || size > 4096) {
        return 0;
    }

    std::string source(reinterpret_cast<const char*>(data), size);
    for (char& ch : source) {
        if (ch == '\0') {
            ch = ' ';
        }
    }

    try {
        const gwbasic::Lexer lexer;
        const gwbasic::Parser parser;
        const auto tokens = lexer.tokenize(source);
        (void)parser.parse_line(tokens, source);
    } catch (...) {
    }

    return 0;
}
