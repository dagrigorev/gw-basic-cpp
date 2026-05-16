#pragma once

#include "gwbasic/lexer.hpp"
#include "gwbasic/parser.hpp"
#include "gwbasic/runtime.hpp"

#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gwbasic {

/**
 * Owns the parser, stored program, runtime context, and execution state.
 *
 * The interpreter supports both immediate statements and line-numbered stored
 * programs. It is embeddable: callers can inject RuntimeContext callbacks for
 * output, input, graphics presentation, and event pumping.
 */
class Interpreter {
public:
    /** Construct an interpreter with a default runtime context. */
    Interpreter();
    /** Construct an interpreter with caller-provided runtime callbacks. */
    explicit Interpreter(RuntimeContext runtime);

    /**
     * Submit one BASIC line.
     *
     * Numbered lines are stored in the program. Unnumbered lines are executed
     * immediately.
     */
    void submit(const std::string& line);
    /** Run the stored program from its first line. */
    void run();
    /** Print the stored program through the runtime output callback. */
    void list() const;
    /** Remove stored program lines and reset continuation/data caches. */
    void clear_program();
    /** Load text BASIC lines from a file into the stored program. */
    void load_program(const std::string& path);
    /** Save the stored program as text BASIC lines. */
    void save_program(const std::string& path) const;

private:
    struct UserFunctionDefinition {
        std::vector<std::string> parameters;
        ExprPtr body;
    };
    using UserFunctionLocals = std::unordered_map<std::string, Value>;

    [[nodiscard]] auto eval(const Expr& expr) -> Value;
    [[nodiscard]] auto eval_function(const FunctionCallExpr& expr) -> Value;
    void execute(const Statement& stmt, int& current_line, std::size_t& statement_index, bool& running);
    void execute_program(int start_line, std::size_t start_statement_index, bool reset_runtime_state);
    [[nodiscard]] auto next_line_number(int line) const -> int;
    [[nodiscard]] auto find_matching_wend(int from_line, std::size_t from_statement_index) const -> std::pair<int, std::size_t>;
    void rebuild_data_cache();
    void jump_to_selected_target(const std::vector<int>& targets, double selector, int& current_line, std::size_t& statement_index, bool gosub);
    void define_user_function(const DefFnStmt& stmt);
    [[nodiscard]] auto lookup_local_variable(const std::string& name) const -> std::optional<Value>;

    Lexer lexer_;
    Parser parser_;
    Program program_;
    RuntimeContext runtime_;
    std::optional<std::pair<int, std::size_t>> continuation_point_;
    std::optional<int> error_handler_line_;
    std::optional<std::pair<int, std::size_t>> error_resume_point_;
    int last_error_code_{0};
    int last_error_line_{0};
    bool handling_error_{false};
    std::mt19937 rng_{std::random_device{}()};
    std::unordered_map<std::string, UserFunctionDefinition> user_functions_;
    std::vector<UserFunctionLocals> user_function_locals_;
};

} // namespace gwbasic
