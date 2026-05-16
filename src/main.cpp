#include "gwbasic/interpreter.hpp"
#include "gwbasic/graphics_presenter.hpp"

#include <array>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
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
void print_usage(std::ostream& os, std::string_view exe){ os<<"Usage:\n"<<"  "<<exe<<"                         Start interactive REPL\n"<<"  "<<exe<<" --edit [path]           Open the console program editor\n"<<"  "<<exe<<" --file <path>           Load and run a .bas program\n"<<"  "<<exe<<" --check --file <path>   Parse a .bas program and report syntax errors\n"<<"  "<<exe<<" --headless --file <p>   Disable native graphics window\n"<<"  "<<exe<<" --help                  Show this help\n"; }
auto load_program_file(const std::string& path)->std::vector<std::string>{ std::ifstream in(path); if(!in) throw std::runtime_error("Unable to open BASIC file: "+path); std::vector<std::string> lines; std::string line; while(std::getline(in,line)){ if(!line.empty()&&line.back()=='\r') line.pop_back(); if(!line.empty()) lines.push_back(line);} return lines; }
int check_file_mode(const std::string& path){ std::ifstream in(path); if(!in) throw std::runtime_error("Unable to open BASIC file: "+path); gwbasic::Lexer lexer; gwbasic::Parser parser; std::size_t errors=0; std::size_t physical_line=0; std::string line; while(std::getline(in,line)){ ++physical_line; if(!line.empty()&&line.back()=='\r') line.pop_back(); try{ const auto tokens=lexer.tokenize(line); const auto result=parser.parse_line_recovering(tokens,line); for(const auto& diagnostic: result.diagnostics){ std::cerr<<path<<':'<<physical_line<<':'<<diagnostic.column<<": "<<diagnostic.message<<'\n'; ++errors; } } catch(const std::exception& ex){ std::cerr<<path<<':'<<physical_line<<": "<<ex.what()<<'\n'; ++errors; } } if(errors==0){ std::cout<<"Syntax check passed\n"; return 0; } std::cerr<<errors<<" syntax error(s)\n"; return 1; }
int run_file_mode(gwbasic::Interpreter& interpreter,const std::string& path,gwbasic::graphics::Presenter* presenter,bool headless){ for(const auto& line: load_program_file(path)) interpreter.submit(line); interpreter.run(); if(!headless && presenter!=nullptr && presenter->is_open()) presenter->wait_until_closed(); return 0; }
int run_repl_mode(gwbasic::Interpreter& interpreter){ std::cout<<"GW-BASIC Modern Rewrite Prototype (C++20)\n"<<"Type BASIC lines, RUN, LIST, NEW, CLEAR, or Ctrl+D to exit.\n"; std::string line; while(std::cout<<"] "<<std::flush,std::getline(std::cin,line)){ try{ interpreter.submit(line);} catch(const std::exception& ex){ std::cerr<<"Error: "<<ex.what()<<'\n'; }} return 0; }

[[nodiscard]] auto trim_copy(std::string value)->std::string{ const auto first=value.find_first_not_of(" \t\r\n"); if(first==std::string::npos) return {}; const auto last=value.find_last_not_of(" \t\r\n"); return value.substr(first,last-first+1); }
[[nodiscard]] auto upper_copy(std::string value)->std::string{ std::ranges::transform(value,value.begin(),[](unsigned char ch){ return static_cast<char>(std::toupper(ch)); }); return value; }
[[nodiscard]] auto starts_with_ci(const std::string& text,std::string_view prefix)->bool{ if(text.size()<prefix.size()) return false; for(std::size_t i=0;i<prefix.size();++i){ if(std::toupper(static_cast<unsigned char>(text[i]))!=std::toupper(static_cast<unsigned char>(prefix[i]))) return false; } return true; }
[[nodiscard]] auto parse_leading_line_number(const std::string& text)->std::optional<int>{ std::size_t i=0; while(i<text.size()&&std::isspace(static_cast<unsigned char>(text[i]))) ++i; if(i>=text.size()||!std::isdigit(static_cast<unsigned char>(text[i]))) return std::nullopt; int line=0; while(i<text.size()&&std::isdigit(static_cast<unsigned char>(text[i]))){ line=line*10+(text[i]-'0'); ++i; } return line; }

class LazyPresenter final : public gwbasic::graphics::Presenter {
public:
    void present(const std::vector<std::uint8_t>& pixels,int width,int height,const std::array<std::uint8_t,256>& palette_map) override{
        ensure_presenter();
        if(presenter_) presenter_->present(pixels,width,height,palette_map);
    }
    [[nodiscard]] auto poll_key()->std::optional<std::string> override{
        if(!presenter_) return std::nullopt;
        return presenter_->poll_key();
    }
    void pump_events() override{
        if(presenter_) presenter_->pump_events();
    }
    void wait_until_closed() override{
        if(presenter_&&presenter_->is_open()) presenter_->wait_until_closed();
    }
    [[nodiscard]] bool is_open() const override{
        return presenter_!=nullptr&&presenter_->is_open();
    }
    [[nodiscard]] bool created() const{
        return presenter_!=nullptr;
    }
private:
    void ensure_presenter(){
        if(!presenter_) presenter_=gwbasic::graphics::create_default_presenter();
    }
    std::unique_ptr<gwbasic::graphics::Presenter> presenter_;
};

class ConsoleEditor {
public:
    ConsoleEditor(gwbasic::RuntimeContext runtime,gwbasic::graphics::Presenter* presenter,bool headless)
        : runtime_(std::move(runtime)), presenter_(presenter), headless_(headless) {}

    int run(const std::optional<std::string>& initial_path){
        if(initial_path.has_value()) load(*initial_path);
        draw();
        std::string input;
        while(std::cout<<"EDIT> "<<std::flush,std::getline(std::cin,input)){
            try{
                if(!handle_command(trim_copy(input))) break;
            }catch(const std::exception& ex){
                std::cerr<<"Editor error: "<<ex.what()<<'\n';
            }
        }
        return 0;
    }

private:
    void draw() const{
        std::cout<<"\n=== GW-BASIC Console Editor ===\n";
        std::cout<<"File: "<<(path_.empty()?"<untitled>":path_)<<"    Lines: "<<program_.size()<<"\n";
        std::cout<<"Commands: HELP, OPEN <file>, SAVE [file], RUN, CHECK, LIST, EDIT <line>, DEL <line>, RENUM, NEW, QUIT\n";
        std::cout<<"Direct numbered input replaces/deletes a BASIC line, e.g. 10 PRINT \"HELLO\" or 10\n";
        list(18);
    }

    void list(std::size_t limit=0) const{
        std::size_t shown=0;
        for(const auto& [line,text]:program_){
            (void)line;
            std::cout<<text<<'\n';
            if(limit!=0&&++shown>=limit){
                if(program_.size()>shown) std::cout<<"... "<<(program_.size()-shown)<<" more line(s). Use LIST to show all.\n";
                break;
            }
        }
        if(program_.empty()) std::cout<<"<empty program>\n";
    }

    void load(const std::string& path){
        program_.clear();
        for(const auto& line:load_program_file(path)) store_numbered_line(line);
        path_=path;
    }

    void save(const std::string& path){
        std::ofstream out(path);
        if(!out) throw std::runtime_error("Unable to save BASIC file: "+path);
        for(const auto& [line,text]:program_){ (void)line; out<<text<<'\n'; }
        path_=path;
        std::cout<<"Saved "<<program_.size()<<" line(s) to "<<path_<<'\n';
    }

    void store_numbered_line(const std::string& text){
        const auto line=parse_leading_line_number(text);
        if(!line.has_value()) throw std::runtime_error("Expected a numbered BASIC line");
        const auto trimmed=trim_copy(text);
        std::size_t pos=0; while(pos<trimmed.size()&&std::isdigit(static_cast<unsigned char>(trimmed[pos]))) ++pos;
        if(trim_copy(trimmed.substr(pos)).empty()) program_.erase(*line);
        else program_[*line]=trimmed;
    }

    void run_program(){
        gwbasic::Interpreter interpreter{runtime_};
        for(const auto& [line,text]:program_){ (void)line; interpreter.submit(text); }
        interpreter.run();
        if(!headless_&&presenter_!=nullptr&&presenter_->is_open()) presenter_->wait_until_closed();
    }

    void check_program() const{
        gwbasic::Lexer lexer;
        gwbasic::Parser parser;
        std::size_t errors=0;
        for(const auto& [line,text]:program_){
            try{
                const auto result=parser.parse_line_recovering(lexer.tokenize(text),text);
                for(const auto& diagnostic:result.diagnostics){
                    std::cerr<<line<<':'<<diagnostic.column<<": "<<diagnostic.message<<'\n';
                    ++errors;
                }
            }catch(const std::exception& ex){
                std::cerr<<line<<": "<<ex.what()<<'\n';
                ++errors;
            }
        }
        if(errors==0) std::cout<<"Syntax check passed\n";
        else std::cerr<<errors<<" syntax error(s)\n";
    }

    void edit_line(int line){
        const auto it=program_.find(line);
        std::cout<<"Current: "<<(it==program_.end()?std::to_string(line):it->second)<<'\n';
        std::cout<<"New line (empty keeps current, number only deletes): "<<std::flush;
        std::string value;
        if(!std::getline(std::cin,value)) return;
        if(trim_copy(value).empty()) return;
        if(!parse_leading_line_number(value).has_value()) value=std::to_string(line)+" "+value;
        store_numbered_line(value);
    }

    void renumber(int start=10,int step=10){
        std::map<int,std::string> updated;
        int next=start;
        for(const auto& [line,text]:program_){
            (void)line;
            std::size_t pos=0; while(pos<text.size()&&std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
            updated[next]=std::to_string(next)+text.substr(pos);
            next+=step;
        }
        program_=std::move(updated);
    }

    bool handle_command(const std::string& input){
        if(input.empty()){ draw(); return true; }
        if(parse_leading_line_number(input).has_value()){ store_numbered_line(input); return true; }
        const auto command=upper_copy(input);
        if(command=="Q"||command=="QUIT"||command=="EXIT") return false;
        if(command=="HELP"||command=="?"){ draw(); return true; }
        if(command=="LIST"||command=="L"){ list(); return true; }
        if(command=="RUN"||command=="F5"){ run_program(); return true; }
        if(command=="CHECK"){ check_program(); return true; }
        if(command=="NEW"){ program_.clear(); path_.clear(); draw(); return true; }
        if(command=="RENUM"){ renumber(); list(); return true; }
        if(starts_with_ci(input,"OPEN ")){ load(trim_copy(input.substr(5))); draw(); return true; }
        if(starts_with_ci(input,"SAVE")){
            auto target=trim_copy(input.size()>4?input.substr(4):std::string{});
            if(target.empty()) target=path_;
            if(target.empty()) throw std::runtime_error("SAVE needs a file path for an untitled program");
            save(target);
            return true;
        }
        if(starts_with_ci(input,"DEL ")){ program_.erase(std::stoi(trim_copy(input.substr(4)))); return true; }
        if(starts_with_ci(input,"EDIT ")){ edit_line(std::stoi(trim_copy(input.substr(5)))); return true; }
        if(starts_with_ci(input,"RENUM ")){
            const auto rest=trim_copy(input.substr(6));
            const auto split=rest.find(' ');
            const int start=std::stoi(split==std::string::npos?rest:rest.substr(0,split));
            const int step=split==std::string::npos?10:std::stoi(trim_copy(rest.substr(split+1)));
            renumber(start,step);
            list();
            return true;
        }
        std::cerr<<"Unknown editor command. Type HELP.\n";
        return true;
    }

    gwbasic::RuntimeContext runtime_;
    gwbasic::graphics::Presenter* presenter_{nullptr};
    bool headless_{false};
    std::map<int,std::string> program_;
    std::string path_;
};
}
int main(int argc,char* argv[]){ try{ bool headless=false; bool check_only=false; bool editor_mode=false; std::vector<std::string> args; for(int i=1;i<argc;++i) if(argv[i]!=nullptr) args.emplace_back(argv[i]); TerminalKeyReader key_reader; gwbasic::RuntimeContext runtime{[](const std::string& t){ std::cout<<t<<std::flush; }, [](){ std::string line; if(!std::getline(std::cin,line)) return std::string{}; return line; }, {}}; LazyPresenter presenter; runtime.set_graphics_presenter([&](const std::vector<std::uint8_t>& pixels,int width,int height,const std::array<std::uint8_t,256>& palette){ if(!headless) presenter.present(pixels,width,height,palette); }); runtime.set_key_input([&]()->std::optional<std::string>{ if(!headless){ if(auto value = presenter.poll_key(); value.has_value()) return value; } return key_reader.poll(); }); runtime.set_engine_tick([&](){ if(!headless && presenter.created()){ presenter.pump_events(); if(!presenter.is_open()) runtime.request_stop(); } }); gwbasic::Interpreter interpreter{runtime}; const std::string exe=(argc>0&&argv[0]!=nullptr)?argv[0]:"gwbasic"; if(args.empty()) return run_repl_mode(interpreter); std::optional<std::string> file_path; for(std::size_t i=0;i<args.size();++i){ if(args[i]=="--help"||args[i]=="-h"){ print_usage(std::cout,exe); return 0; } if(args[i]=="--headless"){ headless=true; continue; } if(args[i]=="--check"){ check_only=true; continue; } if(args[i]=="--edit"||args[i]=="--editor"){ editor_mode=true; if(i+1<args.size()&&!args[i+1].empty()&&args[i+1].rfind("--",0)!=0) file_path=args[++i]; continue; } if(args[i]=="--file"){ if(i+1>=args.size()||args[i+1].empty()){ print_usage(std::cerr,exe); std::cerr<<"Missing path after --file\n"; return 2; } file_path=args[++i]; continue; } print_usage(std::cerr,exe); std::cerr<<"Unknown argument: "<<args[i]<<'\n'; return 2; } if(editor_mode){ ConsoleEditor editor{runtime,&presenter,headless}; return editor.run(file_path); } if(check_only){ if(!file_path.has_value()){ print_usage(std::cerr,exe); std::cerr<<"--check requires --file <path>\n"; return 2; } return check_file_mode(*file_path); } if(file_path.has_value()) return run_file_mode(interpreter,*file_path,&presenter,headless); return run_repl_mode(interpreter);} catch(const std::exception& ex){ std::cerr<<"Error: "<<ex.what()<<'\n'; return 1; }}
