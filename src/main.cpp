#include "gwbasic/interpreter.hpp"
#include "gwbasic/graphics_presenter.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace {
#ifdef _WIN32
class TerminalKeyReader { public: [[nodiscard]] auto poll() const -> std::optional<std::string> { if(!_kbhit()) return std::nullopt; int ch=_getch(); if(ch==0||ch==224){ if(_kbhit()) (void)_getch(); return std::string{}; } if(ch==3) throw std::runtime_error("Interrupted"); return std::string(1, static_cast<char>(ch)); } };
#else
class TerminalKeyReader { public: [[nodiscard]] auto poll() const -> std::optional<std::string> { if(!isatty(STDIN_FILENO)) return std::nullopt; const int ok=tcgetattr(STDIN_FILENO,&old_)==0?1:0; termios raw=old_; if(ok){ raw.c_lflag &= static_cast<unsigned long>(~(ICANON|ECHO)); raw.c_cc[VMIN]=0; raw.c_cc[VTIME]=0; tcsetattr(STDIN_FILENO,TCSANOW,&raw);} fd_set set; FD_ZERO(&set); FD_SET(STDIN_FILENO,&set); timeval timeout{}; int ready=select(STDIN_FILENO+1,&set,nullptr,nullptr,&timeout); std::optional<std::string> r=std::nullopt; if(ready>0&&FD_ISSET(STDIN_FILENO,&set)){ char ch=0; ssize_t c=::read(STDIN_FILENO,&ch,1); r=c==1?std::optional<std::string>(std::string(1,ch)):std::optional<std::string>(std::string{});} if(ok) tcsetattr(STDIN_FILENO,TCSANOW,&old_); return r; } private: mutable termios old_{}; };
#endif
void print_usage(std::ostream& os, std::string_view exe){ os<<"Usage:\n"<<"  "<<exe<<"                         Start interactive REPL\n"<<"  "<<exe<<" --file <path>           Load and run a .bas program\n"<<"  "<<exe<<" --headless --file <p>   Disable native graphics window\n"<<"  "<<exe<<" --help                  Show this help\n"; }
auto load_program_file(const std::string& path)->std::vector<std::string>{ std::ifstream in(path); if(!in) throw std::runtime_error("Unable to open BASIC file: "+path); std::vector<std::string> lines; std::string line; while(std::getline(in,line)){ if(!line.empty()&&line.back()=='\r') line.pop_back(); if(!line.empty()) lines.push_back(line);} return lines; }
int run_file_mode(gwbasic::Interpreter& interpreter,const std::string& path,gwbasic::graphics::Presenter* presenter,bool headless){ for(const auto& line: load_program_file(path)) interpreter.submit(line); interpreter.run(); if(!headless && presenter!=nullptr) presenter->wait_until_closed(); return 0; }
int run_repl_mode(gwbasic::Interpreter& interpreter){ std::cout<<"GW-BASIC Modern Rewrite Prototype (C++20)\n"<<"Type BASIC lines, RUN, LIST, NEW, CLEAR, or Ctrl+D to exit.\n"; std::string line; while(std::cout<<"] "<<std::flush,std::getline(std::cin,line)){ try{ interpreter.submit(line);} catch(const std::exception& ex){ std::cerr<<"Error: "<<ex.what()<<'\n'; }} return 0; }
}
int main(int argc,char* argv[]){ try{ bool headless=false; std::vector<std::string> args; for(int i=1;i<argc;++i) if(argv[i]!=nullptr) args.emplace_back(argv[i]); TerminalKeyReader key_reader; gwbasic::RuntimeContext runtime{[](const std::string& t){ std::cout<<t<<std::flush; }, [](){ std::string line; if(!std::getline(std::cin,line)) return std::string{}; return line; }, {}}; auto presenter=gwbasic::graphics::create_default_presenter(); runtime.set_graphics_presenter([&](const std::vector<std::uint8_t>& pixels,int width,int height,const std::array<std::uint8_t,256>& palette){ if(!headless&&presenter) presenter->present(pixels,width,height,palette); }); runtime.set_key_input([&]()->std::optional<std::string>{ if(!headless && presenter){ if(auto value = presenter->poll_key(); value.has_value()) return value; } return key_reader.poll(); }); runtime.set_engine_tick([&](){ if(!headless && presenter){ presenter->pump_events(); if(!presenter->is_open()) runtime.request_stop(); } }); gwbasic::Interpreter interpreter{runtime}; const std::string exe=(argc>0&&argv[0]!=nullptr)?argv[0]:"gwbasic"; if(args.empty()) return run_repl_mode(interpreter); std::optional<std::string> file_path; for(std::size_t i=0;i<args.size();++i){ if(args[i]=="--help"||args[i]=="-h"){ print_usage(std::cout,exe); return 0; } if(args[i]=="--headless"){ headless=true; continue; } if(args[i]=="--file"){ if(i+1>=args.size()||args[i+1].empty()){ print_usage(std::cerr,exe); std::cerr<<"Missing path after --file\n"; return 2; } file_path=args[++i]; continue; } print_usage(std::cerr,exe); std::cerr<<"Unknown argument: "<<args[i]<<'\n'; return 2; } if(file_path.has_value()) return run_file_mode(interpreter,*file_path,presenter.get(),headless); return run_repl_mode(interpreter);} catch(const std::exception& ex){ std::cerr<<"Error: "<<ex.what()<<'\n'; return 1; }}
