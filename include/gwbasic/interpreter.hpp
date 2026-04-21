#pragma once

#include "gwbasic/lexer.hpp"
#include "gwbasic/parser.hpp"
#include "gwbasic/runtime.hpp"

#include <optional>
#include <random>
#include <string>
#include <utility>

namespace gwbasic {

class Interpreter {
public:
    Interpreter();
    explicit Interpreter(RuntimeContext runtime);

    void submit(const std::string& line);
    void run();
    void list() const;
    void clear_program();

private:
    [[nodiscard]] auto eval(const Expr& expr) -> Value;
    [[nodiscard]] auto eval_function(const FunctionCallExpr& expr) -> Value;
    void execute(const Statement& stmt, int& current_line, std::size_t& statement_index, bool& running);
    void execute_program(int start_line, std::size_t start_statement_index, bool reset_runtime_state);
    [[nodiscard]] auto next_line_number(int line) const -> int;
    [[nodiscard]] auto find_matching_wend(int from_line, std::size_t from_statement_index) const -> std::pair<int, std::size_t>;
    void rebuild_data_cache();
    void jump_to_selected_target(const std::vector<int>& targets, double selector, int& current_line, std::size_t& statement_index, bool gosub);

    Lexer lexer_;
    Parser parser_;
    Program program_;
    RuntimeContext runtime_;
    std::optional<std::pair<int, std::size_t>> continuation_point_;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace gwbasic
