#include "gwbasic/interpreter.hpp"
#include <iostream>
#include <vector>
#include <string>
int main(){
  gwbasic::Lexer lx; gwbasic::Parser p;
  std::vector<std::string> lines={
"10 OPEN \"x\" FOR OUTPUT AS #1",
"20 WRITE #1, \"ALPHA\", 42, \"A,B\"",
"50 INPUT #1, A$, B, C$",
"110 LINE INPUT #2, L$",
"335 LINE INPUT \"PROMPT:\"; U$",
"360 IF 1 THEN PRINT \"T\" ELSE PRINT \"F\"",
"370 IF 0 THEN PRINT \"BAD\" ELSE PRINT \"ELSE\"",
"380 INPUT \"NUMSTR\", N, S$",
"390 PRINT N;\"/\";S$",
"400 CLS"};
 for(auto &line: lines){
   try{ auto t=lx.tokenize(line); auto pl=p.parse_line(t,line); std::cout<<"OK: "<<line<<"\n"; }
   catch(std::exception const& e){ std::cout<<"ERR: "<<line<<" => "<<e.what()<<"\n"; }
 }
}
