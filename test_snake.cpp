#include "gwbasic/interpreter.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <optional>
int main(){
    std::string output;
    gwbasic::RuntimeContext runtime{
        [&](const std::string& t){ output += t; },
        [](){ return std::string{}; },
        []()->std::optional<std::string>{ return std::nullopt; }
    };
    gwbasic::Interpreter it{runtime};
    std::vector<std::string> lines = {
R"(10 SCREEN 1: CLS)",
R"(20 KEY OFF: COLOR 0, 7)",
R"(30 REM --- INITIALIZE VARIABLES ---)",
R"(40 HX = 320: HY = 100: DX = 8: DY = 0: LN = 3)",
R"(50 DIM BX(500), BY(500))",
R"(60 FX = INT(RND * 79) * 8: FY = INT(RND * 24) * 8)",
R"(70 SC = 0)",
R"(80 REM --- MAIN GAME LOOP ---)",
R"(90 K$ = INKEY$: IF K$ = "" THEN GOTO 150)",
R"(100 IF K$ = "W" AND DY = 0 THEN DX = 0: DY = -8)",
R"(110 IF K$ = "S" AND DY = 0 THEN DX = 0: DY = 8)",
R"(120 IF K$ = "A" AND DX = 0 THEN DX = -8: DY = 0)",
R"(130 IF K$ = "D" AND DX = 0 THEN DX = 8: DY = 0)",
R"(140 IF K$ = CHR$(27) THEN END)",
R"(150 REM --- UPDATE BODY POSITIONS ---)",
R"(160 FOR I = LN TO 2 STEP -1)",
R"(170 BX(I) = BX(I-1): BY(I) = BY(I-1))",
R"(180 NEXT I)",
R"(190 BX(1) = HX: BY(1) = HY)",
R"(200 REM --- MOVE HEAD ---)",
R"(210 HX = HX + DX: HY = HY + DY)",
R"(220 REM --- COLLISION CHECK: WALLS ---)",
R"(230 IF HX < 0 OR HX > 632 OR HY < 0 OR HY > 192 THEN GOTO 400)",
R"(240 REM --- COLLISION CHECK: SELF ---)",
R"(250 FOR I = 2 TO LN)",
R"(260 IF HX = BX(I) AND HY = BY(I) THEN GOTO 400)",
R"(270 NEXT I)",
R"(280 REM --- EAT FOOD ---)",
R"(290 IF HX = FX AND HY = FY THEN SC = SC + 10: LN = LN + 1: GOTO 310)",
R"(300 REM ERASE TAIL)",
R"(310 LINE(BX(LN), BY(LN))-(BX(LN)+7, BY(LN)+7), 0, BF)",
R"(320 REM --- GENERATE NEW FOOD IF EATEN ---)",
R"(330 IF HX = FX AND HY = FY THEN FX = INT(RND * 79) * 8: FY = INT(RND * 24) * 8)",
R"(340 REM --- DRAWING ---)",
R"(350 LINE(FX, FY)-(FX+7, FY+7), 4, BF)",
R"(360 LINE(HX, HY)-(HX+7, HY+7), 10, BF)",
R"(370 LOCATE 1, 1: PRINT "SCORE:"; SC)",
R"(380 REM --- SPEED CONTROL (DELAY) ---)",
R"(390 FOR T = 1 TO 2000: NEXT T: GOTO 90)",
R"(400 REM --- GAME OVER ---)",
R"(410 SOUND 500, 1: SOUND 300, 2)",
R"(420 LOCATE 12, 35: PRINT "GAME OVER!")",
R"(430 LOCATE 13, 33: PRINT "FINAL SCORE:"; SC)",
R"(440 END)"
};
    try{
      for(auto&s:lines) it.submit(s);
      it.run();
      std::cout << "OK\n" << output.substr(output.find("GAME OVER!")!=std::string::npos?output.find("GAME OVER!"):0) << "\n";
    }catch(const std::exception& e){ std::cerr<<e.what()<<"\n"; return 1; }
}
