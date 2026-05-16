#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace gwbasic {

struct Expr;
struct Statement;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Statement>;

struct VariableRef {
    std::string name;
    std::vector<ExprPtr> indices;
};

struct BinaryExpr { ExprPtr left; std::string op; ExprPtr right; };
struct UnaryExpr { std::string op; ExprPtr operand; };
struct NumberExpr { double value{}; };
struct StringExpr { std::string value; };
struct VariableExpr { VariableRef ref; };
struct FunctionCallExpr { std::string name; std::vector<ExprPtr> arguments; };

struct Expr {
    using Variant = std::variant<BinaryExpr, UnaryExpr, NumberExpr, StringExpr, VariableExpr, FunctionCallExpr>;
    Variant node;
};

enum class PrintSeparator {
    None,
    Comma,
    Semicolon
};

struct PrintItem {
    ExprPtr expression;
    PrintSeparator separator{PrintSeparator::None};
};

enum class FileMode { Input, Output, Append, Random };

struct OpenStmt { ExprPtr path; FileMode mode{FileMode::Input}; int file_number{}; std::optional<int> record_len; };
struct FieldBinding { int width{}; std::string variable; };
struct FieldStmt { int file_number{}; std::vector<FieldBinding> bindings; };
struct PutStmt { int file_number{}; std::optional<ExprPtr> record_number; };
struct GetStmt { int file_number{}; std::optional<ExprPtr> record_number; };
struct GraphicsGetStmt { ExprPtr x1; ExprPtr y1; ExprPtr x2; ExprPtr y2; VariableRef target; };
struct GraphicsPutStmt { ExprPtr x; ExprPtr y; VariableRef source; std::optional<std::string> mode; };
struct CloseStmt { std::optional<int> file_number; };
struct PrintFileStmt { int file_number{}; std::vector<PrintItem> items; bool trailing_newline{true}; };
struct WriteFileStmt { int file_number{}; std::vector<ExprPtr> items; bool trailing_newline{true}; };
struct InputFileStmt { int file_number{}; std::vector<VariableRef> targets; };
struct LineInputFileStmt { int file_number{}; VariableRef target; };

struct PrintStmt { std::vector<PrintItem> items; bool trailing_newline{true}; };
struct WriteStmt { std::vector<ExprPtr> items; bool trailing_newline{true}; };
struct PrintUsingStmt { std::string format; std::vector<ExprPtr> arguments; bool trailing_newline{true}; };
struct LetStmt { VariableRef target; ExprPtr value; };
struct InputStmt { std::vector<VariableRef> targets; std::string prompt; bool suppress_question{false}; };
struct LineInputStmt { VariableRef target; std::string prompt; };
struct IfStmt { ExprPtr condition; int target_line{}; };
struct IfThenStmt { ExprPtr condition; std::vector<StmtPtr> then_statements; std::vector<StmtPtr> else_statements; };
struct GotoStmt { int target_line{}; };
struct GosubStmt { int target_line{}; };
struct ReturnStmt {};
struct ForStmt { std::string variable; ExprPtr start; ExprPtr finish; ExprPtr step; };
struct NextStmt { std::optional<std::string> variable; };
struct DataStmt { std::vector<ExprPtr> items; };
struct ReadStmt { std::vector<VariableRef> targets; };
struct RestoreStmt { std::optional<int> line; };
struct DimDecl { std::string name; std::vector<ExprPtr> dimensions; };
struct DimStmt { std::vector<DimDecl> declarations; };
struct EraseStmt { std::vector<std::string> names; };
struct OptionBaseStmt { int base{}; };
struct WhileStmt { ExprPtr condition; };
struct WendStmt {};
struct OnGotoStmt { ExprPtr selector; std::vector<int> targets; };
struct OnGosubStmt { ExprPtr selector; std::vector<int> targets; };
struct OnErrorGotoStmt { int target_line{}; };
struct ResumeStmt { bool next{false}; std::optional<int> target_line; };
struct ErrorStmt { ExprPtr code; };
struct DefTypeRange { char start{}; char end{}; };
struct DefIntStmt { std::vector<DefTypeRange> ranges; };
struct DefStrStmt { std::vector<DefTypeRange> ranges; };
struct DefSngStmt { std::vector<DefTypeRange> ranges; };
struct DefDblStmt { std::vector<DefTypeRange> ranges; };
struct DefFnStmt { std::string name; std::vector<std::string> parameters; ExprPtr body; };
struct RandomizeStmt { std::optional<ExprPtr> seed; };
struct StopStmt {};
struct ContStmt {};
struct EndStmt {};
struct RemStmt { std::string comment; };
struct ListStmt {};
struct RunStmt {};
struct LoadStmt { ExprPtr path; };
struct SaveStmt { ExprPtr path; };
struct NewStmt {};
struct ClearStmt {};
struct FilesStmt { std::optional<ExprPtr> pattern; };
struct KillStmt { ExprPtr path; };
struct NameStmt { ExprPtr old_path; ExprPtr new_path; };
struct MkdirStmt { ExprPtr path; };
struct ChdirStmt { ExprPtr path; };
struct RmdirStmt { ExprPtr path; };
struct LsetStmt { VariableRef target; ExprPtr value; };
struct RsetStmt { VariableRef target; ExprPtr value; };
struct SwapStmt { VariableRef left; VariableRef right; };
struct ClsStmt {};
struct LocateStmt { std::optional<ExprPtr> row; std::optional<ExprPtr> column; std::optional<ExprPtr> cursor; std::optional<ExprPtr> start; std::optional<ExprPtr> stop; };
struct WidthStmt { ExprPtr columns; };
struct ColorStmt { std::optional<ExprPtr> foreground; std::optional<ExprPtr> background; std::optional<ExprPtr> border; };
struct BeepStmt {};
struct ScreenStmt { std::optional<ExprPtr> mode; std::optional<ExprPtr> color_switch; std::optional<ExprPtr> active_page; std::optional<ExprPtr> visual_page; };
struct KeyStmt { bool enabled{true}; };
struct PokeStmt { ExprPtr address; ExprPtr value; };
struct SoundStmt { ExprPtr frequency; ExprPtr duration; };
struct PlayStmt { ExprPtr sequence; };
struct PsetStmt { ExprPtr x; ExprPtr y; std::optional<ExprPtr> color; };
struct GraphicsLineStmt { ExprPtr x1; ExprPtr y1; ExprPtr x2; ExprPtr y2; std::optional<ExprPtr> color; bool box{false}; bool fill{false}; };
struct CircleStmt { ExprPtr x; ExprPtr y; ExprPtr radius; std::optional<ExprPtr> color; };
struct PaintStmt { ExprPtr x; ExprPtr y; std::optional<ExprPtr> color; std::optional<ExprPtr> border; };
struct DrawStmt { ExprPtr commands; };
struct ViewStmt { std::optional<ExprPtr> x1; std::optional<ExprPtr> y1; std::optional<ExprPtr> x2; std::optional<ExprPtr> y2; };
struct WindowStmt { bool screen_coordinates{false}; std::optional<ExprPtr> x1; std::optional<ExprPtr> y1; std::optional<ExprPtr> x2; std::optional<ExprPtr> y2; };
struct PaletteStmt { bool using_mode{false}; std::optional<VariableRef> using_source; std::optional<ExprPtr> attribute; std::optional<ExprPtr> color; };

struct Statement {
    using Variant = std::variant<PrintStmt, WriteStmt, PrintUsingStmt, OpenStmt, CloseStmt, PrintFileStmt, WriteFileStmt, InputFileStmt, LineInputFileStmt, FieldStmt, PutStmt, GetStmt, GraphicsGetStmt, GraphicsPutStmt, LetStmt, InputStmt, LineInputStmt, IfStmt, IfThenStmt, GotoStmt, GosubStmt,
                                 ReturnStmt, ForStmt, NextStmt, DataStmt, ReadStmt, RestoreStmt,
                                 DimStmt, EraseStmt, OptionBaseStmt, WhileStmt, WendStmt, OnGotoStmt, OnGosubStmt, OnErrorGotoStmt, ResumeStmt, ErrorStmt, DefFnStmt, DefIntStmt,
                                 DefStrStmt, DefSngStmt, DefDblStmt, StopStmt, ContStmt, EndStmt, RemStmt, ListStmt, RunStmt, LoadStmt, SaveStmt,
                                 NewStmt, ClearStmt, FilesStmt, KillStmt, NameStmt, MkdirStmt, ChdirStmt, RmdirStmt, LsetStmt, RsetStmt, SwapStmt, ClsStmt, LocateStmt, WidthStmt, ColorStmt, BeepStmt, ScreenStmt, KeyStmt, PokeStmt, SoundStmt, PlayStmt, RandomizeStmt, PsetStmt, GraphicsLineStmt, CircleStmt, PaintStmt, DrawStmt, ViewStmt, WindowStmt, PaletteStmt>;
    Variant node;
};

struct ParsedLine {
    std::optional<int> line_number;
    std::vector<StmtPtr> statements;
    std::string original_text;
};

} // namespace gwbasic
