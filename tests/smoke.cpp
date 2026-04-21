#include "gwbasic/interpreter.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::string output;
    std::vector<std::string> inputs{"K", "typed line", "12,\"HELLO,THERE\""};
    std::size_t input_index = 0;
    gwbasic::RuntimeContext runtime{
        [&](const std::string& text) { output += text; },
        [&]() { return input_index < inputs.size() ? inputs[input_index++] : std::string{}; }
    };
    gwbasic::Interpreter interpreter{runtime};

    const auto root = std::filesystem::temp_directory_path() / "gwbasic_smoke_v22";
    const auto data1 = root / "data1.txt";
    const auto data2 = root / "data2.txt";
    const auto raw = root / "raw.txt";
    const auto moved = root / "moved.txt";
    const auto randomf = root / "random.dat";
    const auto subdir = root / "subdir";

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    {
        std::ofstream raw_out(raw);
        raw_out << "Hello,raw line\n";
    }

    interpreter.submit("10 OPEN \"" + data1.string() + "\" FOR OUTPUT AS #1");
    interpreter.submit("20 WRITE #1, \"ALPHA\", 42, \"A,B\"");
    interpreter.submit("30 CLOSE #1");
    interpreter.submit("40 OPEN \"" + data1.string() + "\" FOR INPUT AS #1");
    interpreter.submit("50 INPUT #1, A$, B, C$");
    interpreter.submit("60 IF EOF(1) THEN 80");
    interpreter.submit("70 END");
    interpreter.submit("80 CLOSE #1");
    interpreter.submit("90 PRINT A$;\"-\";B;\"-\";C$");
    interpreter.submit("100 OPEN \"" + raw.string() + "\" FOR INPUT AS #2");
    interpreter.submit("110 LINE INPUT #2, L$");
    interpreter.submit("120 CLOSE #2");
    interpreter.submit("130 PRINT L$");
    interpreter.submit("140 NAME \"" + raw.string() + "\" AS \"" + moved.string() + "\"");
    interpreter.submit("150 MKDIR \"" + subdir.string() + "\"");
    interpreter.submit("160 RMDIR \"" + subdir.string() + "\"");
    interpreter.submit("170 OPEN \"" + data2.string() + "\" FOR OUTPUT AS #3");
    interpreter.submit("180 PRINT #3, \"ONE\", 2");
    interpreter.submit("190 CLOSE #3");
    interpreter.submit("200 KILL \"" + data2.string() + "\"");
    interpreter.submit("210 OPEN \"" + randomf.string() + "\" FOR RANDOM AS #4 LEN=12");
    interpreter.submit("220 FIELD #4, 8 AS N$, 4 AS C$");
    interpreter.submit("230 LSET N$ = \"ALPHA\"");
    interpreter.submit("240 RSET C$ = \"7\"");
    interpreter.submit("250 PUT #4, 1");
    interpreter.submit("260 LSET N$ = \"BETA\"");
    interpreter.submit("270 RSET C$ = \"9\"");
    interpreter.submit("280 PUT #4, 2");
    interpreter.submit("290 GET #4, 1");
    interpreter.submit("300 PRINT N$;\"|\";C$");
    interpreter.submit("310 PRINT LOF(4);\"/\";LOC(4)");
    interpreter.submit("320 CLOSE #4");
    interpreter.submit("325 K$ = INKEY$");
    interpreter.submit("326 PRINT K$");
    interpreter.submit("327 BEEP");
    interpreter.submit("335 LINE INPUT \"PROMPT:\"; U$");
    interpreter.submit("340 WRITE \"CSV\", 5, \"A,B\"");
    interpreter.submit("350 PRINT U$");
    interpreter.submit("360 IF 1 THEN PRINT \"T\" ELSE PRINT \"F\"");
    interpreter.submit("370 IF 0 THEN PRINT \"BAD\" ELSE PRINT \"ELSE\"");
    interpreter.submit("380 INPUT \"NUMSTR\", N, S$");
    interpreter.submit("390 PRINT N;\"/\";S$");
    interpreter.submit("400 COLOR 14,1");
    interpreter.submit("405 LOCATE 3,5");
    interpreter.submit("410 CLS");
    interpreter.submit("412 SCREEN 0");
    interpreter.submit("414 KEY OFF");
    interpreter.submit("416 SOUND 440,2");
    interpreter.submit("418 PLAY \"CDE\"");
    interpreter.submit("419 KEY ON");
    interpreter.submit("420 SCREEN 1");
    interpreter.submit("422 PSET (10,10),3");
    interpreter.submit("424 LINE (1,1)-(4,1),5");
    interpreter.submit("426 LINE (20,20)-(22,22),6,BF");
    interpreter.submit("428 CIRCLE (30,30),2,7");
    interpreter.submit(R"(430 PRINT POINT(10,10);",";POINT(2,1);",";POINT(21,21);",";POINT(32,30))");
    interpreter.submit("432 LINE (40,40)-(44,44),2,B");
    interpreter.submit("434 PAINT (42,42),9,2");
    interpreter.submit(R"(436 PRINT POINT(42,42);",";POINT(40,40))");
    interpreter.submit("438 VIEW (50,50)-(55,55)");
    interpreter.submit("439 PSET (49,49),4");
    interpreter.submit("440 PSET (52,52),4");
    interpreter.submit(R"(441 PRINT POINT(49,49);",";POINT(52,52))");
    interpreter.submit("442 VIEW");
    interpreter.submit("443 DRAW \"C7M60,60R3D2L3U2\"");
    interpreter.submit(R"(444 PRINT POINT(63,60);",";POINT(60,62))");
    interpreter.submit("445 GET (20,20)-(22,22), SPR$");
    interpreter.submit("446 LINE (20,20)-(22,22),0,BF");
    interpreter.submit("447 PUT (70,70), SPR$");
    interpreter.submit(R"(448 PRINT POINT(21,21);",";POINT(71,71))");
    interpreter.submit("449 PALETTE 2,9");
    interpreter.submit("450 PSET (5,5),2");
    interpreter.submit(R"(451 PRINT POINT(5,5))");
    interpreter.submit("452 VIEW (100,100)-(110,110)");
    interpreter.submit("453 WINDOW SCREEN (0,0)-(10,10)");
    interpreter.submit("454 PSET (5,5),4");
    interpreter.submit(R"(455 PRINT POINT(105,105);",";PMAP(5,0);",";PMAP(5,1);",";PMAP(105,2);",";PMAP(105,3))");
    interpreter.submit("456 WINDOW");
    interpreter.submit("457 VIEW");
    interpreter.submit("458 LINE (80,80)-(82,82),3,BF");
    interpreter.submit(R"(459 PUT (70,70), SPR$, XOR: PRINT POINT(71,71): PUT (70,70), SPR$, OR: PRINT POINT(71,71): PUT (70,70), SPR$, PRESET: PRINT POINT(71,71))");
    interpreter.submit("460 PALETTE");
    interpreter.submit("461 DIM PAL(3)");
    interpreter.submit("462 PAL(0)=0: PAL(1)=1: PAL(2)=15: PAL(3)=4");
    interpreter.submit("463 PALETTE USING PAL");
    interpreter.submit("464 PSET (6,6),2");
    interpreter.submit(R"(465 PRINT POINT(6,6))");
    interpreter.submit("466 PALETTE");
    interpreter.submit("470 END");
    interpreter.run();

    const bool renamed = std::filesystem::exists(moved);
    const bool deleted = !std::filesystem::exists(data2);
    const bool dir_removed = !std::filesystem::exists(subdir);
    std::filesystem::remove_all(root, ec);

    if (output.find("ALPHA-42-A,B") == std::string::npos ||
        output.find("Hello,raw line") == std::string::npos ||
        output.find("ALPHA   |   7") == std::string::npos ||
        output.find("24/2") == std::string::npos ||
        output.find("\nK\n") == std::string::npos ||
        output.find("\a") == std::string::npos ||
        output.find("PROMPT:") == std::string::npos ||
        output.find("\"CSV\",5,\"A,B\"") == std::string::npos ||
        output.find("typed line") == std::string::npos ||
        output.find("NUMSTR? ") == std::string::npos ||
        output.find("12/HELLO,THERE") == std::string::npos ||
        output.find("3,5,6,7") == std::string::npos ||
        output.find("9,2") == std::string::npos ||
        output.find("0,4") == std::string::npos ||
        output.find("7,7") == std::string::npos ||
        output.find("0,6") == std::string::npos ||
        output.find("\n9\n") == std::string::npos ||
        output.find("4,105,105,5,5") == std::string::npos ||
        output.find("\n0\n") == std::string::npos ||
        output.find("\n6\n") == std::string::npos ||
        output.find("\n0\n", output.find("\n0\n") + 1) == std::string::npos ||
        output.find("\x1b[93;44m") == std::string::npos ||
        output.find("\x1b[3;5H") == std::string::npos ||
        output.find("\x1b[2J\x1b[H") == std::string::npos ||
        output.find("\nT\n") == std::string::npos ||
        output.find("\nELSE\n") == std::string::npos ||
        output.find("\nF\n") != std::string::npos ||
        output.find("BAD") != std::string::npos ||
        !renamed || !deleted || !dir_removed) {
        std::cerr << "Unexpected output or filesystem state:\n" << output << '\n';
        std::cerr << "renamed=" << renamed << " deleted=" << deleted << " dir_removed=" << dir_removed << '\n';
        return 1;
    }

    std::cout << "Smoke test passed\n";
    return 0;
}
