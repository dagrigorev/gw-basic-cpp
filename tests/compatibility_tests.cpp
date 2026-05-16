#include "gwbasic/interpreter.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << "Compatibility test failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

std::string run_program(const std::vector<std::string_view>& lines, std::vector<std::string> inputs = {}) {
    std::string output;
    std::size_t input_index = 0;
    gwbasic::RuntimeContext runtime{
        [&](const std::string& text) { output += text; },
        [&]() { return input_index < inputs.size() ? inputs[input_index++] : std::string{}; },
        []() -> std::optional<std::string> { return std::nullopt; },
    };
    gwbasic::Interpreter interpreter{runtime};
    for (const auto line : lines) {
        interpreter.submit(std::string{line});
    }
    interpreter.run();
    return output;
}

void require_contains(const std::string& output, const std::string& expected) {
    require(output.find(expected) != std::string::npos, "missing expected output fragment: " + expected + "\nActual output:\n" + output);
}

} // namespace

int main() {
    {
        const auto output = run_program({
            "10 PRINT 1+2*3",
            "20 PRINT (1+2)*3",
            "30 PRINT .5+1",
            "40 PRINT -2*-3",
            "50 PRINT 17 MOD 5;\":\";17\\5",
            "60 PRINT 2^3^2",
            "70 END",
        });
        require_contains(output, "7\n");
        require_contains(output, "\n9\n");
        require_contains(output, "\n1.500000\n");
        require_contains(output, "\n6\n");
        require_contains(output, "\n2:3\n");
        require_contains(output, "\n512\n");
    }

    {
        const auto output = run_program({
            "10 A$=\"HELLO\"",
            "20 B$=LEFT$(A$,2)+RIGHT$(A$,3)",
            "30 PRINT B$",
            "40 PRINT LEN(B$)",
            "50 END",
        });
        require_contains(output, "HELLO\n");
        require_contains(output, "\n5\n");
    }

    {
        const auto output = run_program({
            "10 A$=\"  AbC  \"",
            "20 PRINT UCASE$(A$);\":\";LCASE$(A$)",
            "30 PRINT \"[\";LTRIM$(A$);\"]\"",
            "40 PRINT \"[\";RTRIM$(A$);\"]\"",
            "50 PRINT HEX$(255);\":\";OCT$(64)",
            "60 END",
        });
        require_contains(output, "  ABC  :  abc  \n");
        require_contains(output, "[AbC  ]\n");
        require_contains(output, "[  AbC]\n");
        require_contains(output, "FF:100\n");
    }

    {
        const auto output = run_program({
            "10 DEF FNSQR(X)=X*X",
            "20 DEF FNTAG$(S$)=LEFT$(S$,1)+\":\"+RIGHT$(S$,1)",
            "30 X=3",
            "40 PRINT FNSQR(5);\":\";X",
            "50 PRINT FNTAG$(\"BASIC\")",
            "60 END",
        });
        require_contains(output, "25:3\n");
        require_contains(output, "B:C\n");
    }

    {
        const auto output = run_program({
            "10 PRINT SGN(-2);\":\";SGN(0);\":\";SGN(2)",
            "20 PRINT FIX(-2.8);\":\";FIX(2.8)",
            "30 PRINT INT(LOG(EXP(1))*1000)",
            "40 PRINT CINT(2.6);\":\";CLNG(-2.6);\":\";CSNG(1.25);\":\";CDBL(1.5)",
            "50 END",
        });
        require_contains(output, "-1:0:1\n");
        require_contains(output, "-2:2\n");
        require_contains(output, "1000\n");
        require_contains(output, "3:-3:1.250000:1.500000\n");
    }

    {
        const auto output = run_program({
            "10 RANDOMIZE 42",
            "20 A=RND",
            "30 RANDOMIZE 42",
            "40 PRINT A=RND",
            "50 END",
        });
        require_contains(output, "-1\n");
    }

    {
        const auto output = run_program({
            "10 PRINT LEN(DATE$);\":\";LEN(TIME$)",
            "20 PRINT TIMER>=0",
            "30 END",
        });
        require_contains(output, "10:8\n");
        require_contains(output, "-1\n");
    }

    {
        const auto output = run_program({
            "10 S=0",
            "20 FOR I=1 TO 5",
            "30 S=S+I",
            "40 NEXT I",
            "50 PRINT S",
            "60 END",
        });
        require_contains(output, "15\n");
    }

    {
        const auto output = run_program({
            "10 GOSUB 100",
            "20 PRINT A",
            "30 GOTO 200",
            "100 A=42",
            "110 RETURN",
            "200 PRINT \"DONE\"",
            "210 END",
        });
        require_contains(output, "42\n");
        require_contains(output, "DONE\n");
    }

    {
        const auto output = run_program({
            "10 DATA 3,\"THREE\"",
            "20 READ N,A$",
            "30 DIM X(2,2)",
            "40 X(1,2)=N",
            "50 PRINT X(1,2);\":\";A$",
            "60 END",
        });
        require_contains(output, "3:THREE\n");
    }

    {
        const auto output = run_program({
            "10 DIM A(1), B$(1)",
            "20 A(1)=7:B$(1)=\"OLD\"",
            "30 ERASE A, B$",
            "40 PRINT A(1);\":\";B$(1)",
            "50 DIM A(2)",
            "60 A(2)=9",
            "70 PRINT A(2)",
            "80 END",
        });
        require_contains(output, "0:\n");
        require_contains(output, "9\n");
    }

    {
        const auto output = run_program({
            "10 OPTION BASE 1",
            "20 DIM A(2)",
            "30 A(1)=5:A(2)=7",
            "40 PRINT A(1);\":\";A(2)",
            "50 ON ERROR GOTO 100",
            "60 PRINT A(0)",
            "70 PRINT \"BAD\"",
            "80 END",
            "100 PRINT \"BASEERR\"",
            "110 RESUME 120",
            "120 B(1)=9",
            "130 PRINT B(1)",
            "140 END",
        });
        require_contains(output, "5:7\n");
        require_contains(output, "BASEERR\n");
        require_contains(output, "9\n");
        require(output.find("BAD") == std::string::npos, "OPTION BASE 1 should reject index zero");
    }

    {
        const auto output = run_program({
            "10 ON ERROR GOTO 100",
            "20 PRINT \"BEFORE\"",
            "30 PRINT 1/0",
            "40 PRINT \"AFTER\"",
            "50 END",
            "100 PRINT \"HANDLED\";ERR;\":\";ERL",
            "110 RESUME NEXT",
        });
        require_contains(output, "BEFORE\n");
        require_contains(output, "HANDLED11:30\n");
        require_contains(output, "AFTER\n");
    }

    {
        const auto output = run_program({
            "10 ON ERROR GOTO 100",
            "20 ERROR 42",
            "30 END",
            "100 PRINT ERR;\":\";ERL",
            "110 RESUME NEXT",
        });
        require_contains(output, "42:20\n");
    }

    {
        const auto output = run_program({
            "10 ON ERROR GOTO 100",
            "20 PRINT 1/0",
            "30 PRINT \"SKIP\"",
            "40 END",
            "100 PRINT \"ROUTE\"",
            "110 RESUME 200",
            "200 PRINT \"TARGET\"",
            "210 END",
        });
        require_contains(output, "ROUTE\n");
        require_contains(output, "TARGET\n");
        require(output.find("SKIP") == std::string::npos, "RESUME line should not continue at skipped line");
    }

    {
        const auto output = run_program({
            "10 A$=INPUT$(2)",
            "20 B$=INPUT$(3)",
            "30 PRINT A$;\":\";B$",
            "40 END",
        }, {"ABCDE"});
        require_contains(output, "AB:CDE\n");
    }

    {
        const auto output = run_program({
            "10 POKE 1234, 201",
            "20 PRINT PEEK(1234)",
            "30 ON ERROR GOTO 100",
            "40 POKE 65536, 1",
            "50 PRINT \"BAD\"",
            "60 END",
            "100 PRINT \"POKEERR\"",
            "110 RESUME 120",
            "120 PRINT \"SAFE\"",
            "130 END",
        });
        require_contains(output, "201\n");
        require_contains(output, "POKEERR\n");
        require_contains(output, "SAFE\n");
        require(output.find("BAD") == std::string::npos, "out-of-range POKE should be handled before BAD");
    }

    {
        const auto output = run_program({
            "10 A=1:B=2",
            "20 SWAP A,B",
            "30 DIM X(1)",
            "40 X(1)=9",
            "50 SWAP B,X(1)",
            "60 PRINT A;\",\";B;\",\";X(1)",
            "70 END",
        });
        require_contains(output, "2,9,1\n");
    }

    {
        const auto output = run_program({
            "10 ON ERROR GOTO 100",
            "20 DIM HUGE(2147483647)",
            "30 PRINT \"BAD\"",
            "40 END",
            "100 PRINT \"DIMERR\"",
            "110 RESUME 120",
            "120 PRINT \"SAFE\"",
            "130 END",
        });
        require_contains(output, "DIMERR\n");
        require_contains(output, "SAFE\n");
        require(output.find("BAD") == std::string::npos, "oversized DIM should not continue through BAD");
    }

    {
        const auto root = std::filesystem::temp_directory_path() / "gwbasic_save_load_test.bas";
        const auto path = root.string();
        const auto output = run_program({
            "10 PRINT \"SAVED\"",
            "20 END",
            "SAVE \"" + path + "\"",
            "NEW",
            "LOAD \"" + path + "\"",
            "RUN",
        });
        std::error_code ec;
        std::filesystem::remove(root, ec);
        require_contains(output, "SAVED\n");
    }

    {
        const auto old_cwd = std::filesystem::current_path();
        const auto root = std::filesystem::temp_directory_path() / "gwbasic_chdir_test";
        const auto subdir = root / "sub";
        const auto relative_file = subdir / "relative.txt";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root);

        const auto output = run_program({
            "10 CHDIR \"" + root.string() + "\"",
            "20 MKDIR \"sub\"",
            "30 CHDIR \"sub\"",
            "40 OPEN \"relative.txt\" FOR OUTPUT AS #1",
            "50 PRINT #1, \"OK\"",
            "60 CLOSE #1",
            "70 CHDIR \"..\"",
            "80 PRINT \"DONE\"",
            "90 END",
        });
        std::filesystem::current_path(old_cwd, ec);
        const bool wrote_relative_file = std::filesystem::exists(relative_file);
        std::filesystem::remove_all(root, ec);
        require_contains(output, "DONE\n");
        require(wrote_relative_file, "CHDIR should affect relative file paths");
    }

    {
        const auto old_cwd = std::filesystem::current_path();
        const auto root = std::filesystem::temp_directory_path() / "gwbasic_files_test";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::create_directories(root);
        {
            std::ofstream(root / "ALPHA.BAS").put('\n');
            std::ofstream(root / "BETA.TXT").put('\n');
        }

        const auto output = run_program({
            "10 CHDIR \"" + root.string() + "\"",
            "20 FILES \"*.BAS\"",
            "30 END",
        });
        std::filesystem::current_path(old_cwd, ec);
        std::filesystem::remove_all(root, ec);
        require_contains(output, "ALPHA.BAS\n");
        require(output.find("BETA.TXT") == std::string::npos, "FILES pattern should filter nonmatching names");
    }

    std::cout << "Compatibility tests passed\n";
    return 0;
}
