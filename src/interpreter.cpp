#include "gwbasic/interpreter.hpp"

#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace gwbasic {
namespace {

template <typename... Ts>
struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts>
Overload(Ts...) -> Overload<Ts...>;

[[nodiscard]] auto parse_input_value(const std::string& raw, bool is_string) -> Value {
    if (is_string) {
        return Value{raw};
    }
    try {
        return Value{std::stod(raw)};
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid numeric input: '" + raw + "'");
    }
}



[[nodiscard]] auto print_control_padding(const Expr& expr, std::size_t column) -> std::optional<std::size_t> {
    if (const auto* call = std::get_if<FunctionCallExpr>(&expr.node)) {
        if (call->name == "SPC") {
            if (call->arguments.size() != 1) {
                throw std::runtime_error("SPC expects 1 argument");
            }
            return 0;
        }
        if (call->name == "TAB") {
            if (call->arguments.size() != 1) {
                throw std::runtime_error("TAB expects 1 argument");
            }
            return column;
        }
    }
    return std::nullopt;
}
[[nodiscard]] auto comma_padding(std::size_t current_column) -> std::size_t {
    constexpr std::size_t zone = 14;
    const auto next_stop = ((current_column / zone) + 1) * zone;
    return next_stop - current_column;
}


[[nodiscard]] auto render_print_items(const std::vector<PrintItem>& items, const std::function<Value(const Expr&)>& eval_fn, bool trailing_newline) -> std::string {
    std::string out;
    std::size_t column = 0;
    for (const auto& item : items) {
        if (const auto* call = std::get_if<FunctionCallExpr>(&item.expression->node)) {
            if (call->name == "SPC") {
                const auto count = std::max(0, static_cast<int>(std::trunc(eval_fn(*call->arguments[0]).as_number())));
                out.append(static_cast<std::size_t>(count), ' ');
                column += static_cast<std::size_t>(count);
            } else if (call->name == "TAB") {
                const auto target = std::max(1, static_cast<int>(std::trunc(eval_fn(*call->arguments[0]).as_number())));
                const auto target_col = static_cast<std::size_t>(target - 1);
                const auto pad = target_col > column ? target_col - column : 0;
                out.append(pad, ' ');
                column += pad;
            } else {
                const auto text = eval_fn(*item.expression).as_string();
                out += text;
                column += text.size();
            }
        } else {
            const auto text = eval_fn(*item.expression).as_string();
            out += text;
            column += text.size();
        }
        if (item.separator == PrintSeparator::Comma) { const auto pad = comma_padding(column); out.append(pad, ' '); column += pad; }
    }
    if (trailing_newline) out.push_back('\n');
    return out;
}


[[nodiscard]] auto render_file_print_items(const std::vector<PrintItem>& items, const std::function<Value(const Expr&)>& eval_fn, bool trailing_newline) -> std::string {
    std::string out;
    for (const auto& item : items) {
        out += eval_fn(*item.expression).as_string();
        if (item.separator == PrintSeparator::Comma) out.push_back(',');
    }
    if (trailing_newline) out.push_back('\n');
    return out;
}

[[nodiscard]] auto quote_write_field(const Value& value) -> std::string {
    if (value.is_string()) {
        std::string text = value.as_string();
        std::string escaped;
        escaped.reserve(text.size());
        for (char ch : text) {
            if (ch == '"') {
                escaped += """";
            } else {
                escaped.push_back(ch);
            }
        }
        return '"' + escaped + '"';
    }
    return value.as_string();
}

[[nodiscard]] auto render_write_file_items(const std::vector<ExprPtr>& items, const std::function<Value(const Expr&)>& eval_fn, bool trailing_newline) -> std::string {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out.push_back(',');
        out += quote_write_field(eval_fn(*items[i]));
    }
    if (trailing_newline) out.push_back('\n');
    return out;
}


[[nodiscard]] auto render_write_items(const std::vector<ExprPtr>& items, const std::function<Value(const Expr&)>& eval_fn, bool trailing_newline) -> std::string {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out.push_back(',');
        out += quote_write_field(eval_fn(*items[i]));
    }
    if (trailing_newline) out.push_back('\n');
    return out;
}

[[nodiscard]] auto format_numeric_with_picture(double value, const std::string& picture) -> std::string {
    const bool negative = value < 0.0;
    const double abs_value = std::fabs(value);
    const auto dot = picture.find('.');
    const std::size_t frac_digits = dot == std::string::npos ? 0 : picture.size() - dot - 1;

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(static_cast<int>(frac_digits));
    oss << abs_value;
    std::string raw = oss.str();

    std::string int_part;
    std::string frac_part;
    if (const auto raw_dot = raw.find('.'); raw_dot != std::string::npos) {
        int_part = raw.substr(0, raw_dot);
        frac_part = raw.substr(raw_dot + 1);
    } else {
        int_part = raw;
    }

    const std::string int_picture = dot == std::string::npos ? picture : picture.substr(0, dot);
    std::string out_int(int_picture.size(), ' ');
    int src = static_cast<int>(int_part.size()) - 1;
    for (int i = static_cast<int>(int_picture.size()) - 1; i >= 0; --i) {
        const char pic = int_picture[static_cast<std::size_t>(i)];
        if (pic == '#') {
            out_int[static_cast<std::size_t>(i)] = src >= 0 ? int_part[static_cast<std::size_t>(src--)] : ' ';
        } else if (pic == ',') {
            out_int[static_cast<std::size_t>(i)] = ',';
        } else {
            out_int[static_cast<std::size_t>(i)] = pic;
        }
    }
    while (src >= 0) {
        out_int.insert(out_int.begin(), int_part[static_cast<std::size_t>(src--)]);
    }

    std::string out = out_int;
    if (dot != std::string::npos) {
        out.push_back('.');
        for (std::size_t i = 0; i < frac_digits; ++i) {
            out.push_back(i < frac_part.size() ? frac_part[i] : '0');
        }
    }
    if (negative) {
        const auto pos = out.find_first_not_of(' ');
        if (pos == std::string::npos) {
            out.insert(out.begin(), '-');
        } else if (pos > 0) {
            out[pos - 1] = '-';
        } else {
            out.insert(out.begin(), '-');
        }
    }
    return out;
}

[[nodiscard]] auto apply_print_using_format(const std::string& format, const std::vector<Value>& values) -> std::string {
    std::string out;
    std::size_t value_index = 0;
    std::size_t i = 0;
    while (i < format.size()) {
        if (format[i] == '\\') {
            const auto end = format.find('\\', i + 1);
            if (end == std::string::npos) {
                out.push_back(format[i++]);
                continue;
            }
            const std::size_t width = end - i - 1;
            std::string text = value_index < values.size() ? values[value_index++].as_string() : std::string{};
            if (text.size() < width) {
                text.append(width - text.size(), ' ');
            } else if (text.size() > width) {
                text = text.substr(0, width);
            }
            out += text;
            i = end + 1;
            continue;
        }
        if (format[i] == '!') {
            std::string text = value_index < values.size() ? values[value_index++].as_string() : std::string{};
            out.push_back(text.empty() ? ' ' : text.front());
            ++i;
            continue;
        }
        if (format[i] == '&') {
            std::string text = value_index < values.size() ? values[value_index++].as_string() : std::string{};
            out += text;
            ++i;
            continue;
        }
        if (format[i] == '#') {
            std::size_t end = i;
            while (end < format.size() && (format[end] == '#' || format[end] == '.' || format[end] == ',')) {
                ++end;
            }
            const std::string picture = format.substr(i, end - i);
            const double number = value_index < values.size() ? values[value_index++].as_number() : 0.0;
            out += format_numeric_with_picture(number, picture);
            i = end;
            continue;
        }
        out.push_back(format[i++]);
    }
    return out;
}

} // namespace

Interpreter::Interpreter() = default;
Interpreter::Interpreter(RuntimeContext runtime) : runtime_(std::move(runtime)) {}

void Interpreter::submit(const std::string& line) {
    const auto tokens = lexer_.tokenize(line);
    auto parsed = parser_.parse_line(tokens, line);
    if (parsed.line_number.has_value()) {
        program_.store(std::move(parsed));
        rebuild_data_cache();
        return;
    }

    for (const auto& stmt : parsed.statements) {
        if (std::holds_alternative<ContStmt>(stmt->node)) {
            if (!continuation_point_.has_value()) {
                throw std::runtime_error("CONT without STOP");
            }
            execute_program(continuation_point_->first, continuation_point_->second, false);
            continue;
        }

        int current_line = -1;
        std::size_t statement_index = 0;
        bool running = true;
        execute(*stmt, current_line, statement_index, running);
    }
    runtime_.flush_graphics();
}

void Interpreter::run() {
    if (program_.empty()) {
        return;
    }
    runtime_.clear_stop_request();
    execute_program(program_.lines().begin()->first, 0, true);
    runtime_.flush_graphics();
}

void Interpreter::execute_program(int start_line, std::size_t start_statement_index, bool reset_runtime_state) {
    rebuild_data_cache();
    if (reset_runtime_state) {
        runtime_.clear_variables();
        runtime_.restore_data();
        continuation_point_.reset();
    }

    int current_line = start_line;
    std::size_t statement_index = start_statement_index;
    bool running = true;
    std::size_t statement_budget = 0;

    while (running) {
        if ((statement_budget++ % 64U) == 0U) {
            runtime_.tick_engine();
        }
        if (runtime_.stop_requested()) {
            break;
        }
        const auto it = program_.lines().find(current_line);
        if (it == program_.lines().end()) {
            throw std::runtime_error("No such line: " + std::to_string(current_line));
        }

        if (statement_index >= it->second.statements.size()) {
            const auto next = next_line_number(current_line);
            if (next < 0) {
                break;
            }
            current_line = next;
            statement_index = 0;
            continue;
        }

        try {
            execute(*it->second.statements.at(statement_index), current_line, statement_index, running);
        } catch (const std::exception& ex) {
            throw std::runtime_error("At BASIC line " + std::to_string(it->first) + ": " + ex.what());
        }
    }
}

void Interpreter::list() const {
    for (const auto& [line, program_line] : program_.lines()) {
        runtime_.print(program_line.source);
    }
}

void Interpreter::clear_program() {
    program_.clear();
    continuation_point_.reset();
    rebuild_data_cache();
}

auto Interpreter::eval(const Expr& expr) -> Value {
    return std::visit(Overload{
        [&](const NumberExpr& node) -> Value { return Value{node.value}; },
        [&](const StringExpr& node) -> Value { return Value{node.value}; },
        [&](const VariableExpr& node) -> Value {
            if (node.ref.indices.empty()) {
                return runtime_.get_variable(node.ref.name);
            }
            std::vector<int> indices;
            indices.reserve(node.ref.indices.size());
            for (const auto& index_expr : node.ref.indices) {
                indices.push_back(static_cast<int>(eval(*index_expr).as_number()));
            }
            return runtime_.get_array_value(node.ref.name, indices);
        },
        [&](const FunctionCallExpr& node) -> Value { return eval_function(node); },
        [&](const UnaryExpr& node) -> Value {
            auto value = eval(*node.operand);
            if (node.op == "+") { return Value{value.as_number()}; }
            if (node.op == "-") { return Value{-value.as_number()}; }
            if (node.op == "NOT") {
                const auto bits = static_cast<long long>(std::llround(std::trunc(value.as_number())));
                return Value{static_cast<double>(~bits)};
            }
            throw std::runtime_error("Unsupported unary operator");
        },
        [&](const BinaryExpr& node) -> Value {
            auto left = eval(*node.left);
            auto right = eval(*node.right);

            if (node.op == "+") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() + right.as_string()};
                }
                return Value{left.as_number() + right.as_number()};
            }
            if (node.op == "-") { return Value{left.as_number() - right.as_number()}; }
            if (node.op == "*") { return Value{left.as_number() * right.as_number()}; }
            if (node.op == "/") {
                const auto divisor = right.as_number();
                if (std::fabs(divisor) < 1e-12) {
                    throw std::runtime_error("Division by zero");
                }
                return Value{left.as_number() / divisor};
            }
            if (node.op == "=") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() == right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() == right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == "<>") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() != right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() != right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == "<") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() < right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() < right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == "<=") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() <= right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() <= right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == ">") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() > right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() > right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == ">=") {
                if (left.is_string() || right.is_string()) {
                    return Value{left.as_string() >= right.as_string() ? -1.0 : 0.0};
                }
                return Value{left.as_number() >= right.as_number() ? -1.0 : 0.0};
            }
            if (node.op == "AND") {
                const auto lhs = static_cast<long long>(std::llround(std::trunc(left.as_number())));
                const auto rhs = static_cast<long long>(std::llround(std::trunc(right.as_number())));
                return Value{static_cast<double>(lhs & rhs)};
            }
            if (node.op == "OR") {
                const auto lhs = static_cast<long long>(std::llround(std::trunc(left.as_number())));
                const auto rhs = static_cast<long long>(std::llround(std::trunc(right.as_number())));
                return Value{static_cast<double>(lhs | rhs)};
            }
            throw std::runtime_error("Unsupported operator: " + node.op);
        }}, expr.node);
}

auto Interpreter::eval_function(const FunctionCallExpr& expr) -> Value {
    if (expr.name == "RND") {
        if (expr.arguments.size() > 1) { throw std::runtime_error("RND expects 0 or 1 arguments"); }
        if (expr.arguments.size() == 1) { (void)eval(*expr.arguments[0]).as_number(); }
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value{dist(rng_)};
    }
    if (expr.name == "ABS") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("ABS expects 1 argument"); }
        return Value{std::fabs(eval(*expr.arguments.front()).as_number())};
    }
    if (expr.name == "INT") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("INT expects 1 argument"); }
        return Value{std::floor(eval(*expr.arguments.front()).as_number())};
    }
    if (expr.name == "LEN") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("LEN expects 1 argument"); }
        return Value{static_cast<double>(eval(*expr.arguments.front()).as_string().size())};
    }
    if (expr.name == "VAL") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("VAL expects 1 argument"); }
        return Value{std::stod(eval(*expr.arguments.front()).as_string())};
    }
    if (expr.name == "LEFT$") {
        if (expr.arguments.size() != 2) { throw std::runtime_error("LEFT$ expects 2 arguments"); }
        const auto source = eval(*expr.arguments[0]).as_string();
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[1]).as_number())));
        return Value{source.substr(0, static_cast<std::size_t>(count))};
    }
    if (expr.name == "RIGHT$") {
        if (expr.arguments.size() != 2) { throw std::runtime_error("RIGHT$ expects 2 arguments"); }
        const auto source = eval(*expr.arguments[0]).as_string();
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[1]).as_number())));
        if (static_cast<std::size_t>(count) >= source.size()) {
            return Value{source};
        }
        return Value{source.substr(source.size() - static_cast<std::size_t>(count))};
    }
    if (expr.name == "MID$") {
        if (expr.arguments.size() != 2 && expr.arguments.size() != 3) { throw std::runtime_error("MID$ expects 2 or 3 arguments"); }
        const auto source = eval(*expr.arguments[0]).as_string();
        const auto start = std::max(1, static_cast<int>(std::trunc(eval(*expr.arguments[1]).as_number())));
        if (static_cast<std::size_t>(start - 1) >= source.size()) {
            return Value{std::string{}};
        }
        const auto pos = static_cast<std::size_t>(start - 1);
        if (expr.arguments.size() == 2) {
            return Value{source.substr(pos)};
        }
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[2]).as_number())));
        return Value{source.substr(pos, static_cast<std::size_t>(count))};
    }
    if (expr.name == "CHR$") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("CHR$ expects 1 argument"); }
        const auto code = static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number()));
        if (code < 0 || code > 255) { throw std::runtime_error("CHR$ argument out of range"); }
        return Value{std::string(1, static_cast<char>(code))};
    }
    if (expr.name == "ASC") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("ASC expects 1 argument"); }
        const auto source = eval(*expr.arguments[0]).as_string();
        if (source.empty()) { throw std::runtime_error("ASC requires a non-empty string"); }
        return Value{static_cast<double>(static_cast<unsigned char>(source.front()))};
    }
    if (expr.name == "STR$") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("STR$ expects 1 argument"); }
        return Value{eval(*expr.arguments[0]).as_string()};
    }
    if (expr.name == "SPC") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("SPC expects 1 argument"); }
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number())));
        return Value{std::string(static_cast<std::size_t>(count), ' ')};
    }
    if (expr.name == "TAB") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("TAB expects 1 argument"); }
        const auto target = std::max(1, static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number())));
        return Value{std::string(static_cast<std::size_t>(target - 1), ' ')};
    }
    if (expr.name == "SPACE$") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("SPACE$ expects 1 argument"); }
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number())));
        return Value{std::string(static_cast<std::size_t>(count), ' ')};
    }
    if (expr.name == "STRING$") {
        if (expr.arguments.size() != 2) { throw std::runtime_error("STRING$ expects 2 arguments"); }
        const auto count = std::max(0, static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number())));
        auto fill = eval(*expr.arguments[1]);
        char ch = ' ';
        if (fill.is_string()) {
            const auto text = fill.as_string();
            ch = text.empty() ? ' ' : text.front();
        } else {
            const auto code = static_cast<int>(std::trunc(fill.as_number()));
            if (code < 0 || code > 255) { throw std::runtime_error("STRING$ fill code out of range"); }
            ch = static_cast<char>(code);
        }
        return Value{std::string(static_cast<std::size_t>(count), ch)};
    }
    if (expr.name == "INSTR") {
        if (expr.arguments.size() != 2 && expr.arguments.size() != 3) { throw std::runtime_error("INSTR expects 2 or 3 arguments"); }
        int start = 1;
        std::string haystack;
        std::string needle;
        if (expr.arguments.size() == 2) {
            haystack = eval(*expr.arguments[0]).as_string();
            needle = eval(*expr.arguments[1]).as_string();
        } else {
            start = std::max(1, static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number())));
            haystack = eval(*expr.arguments[1]).as_string();
            needle = eval(*expr.arguments[2]).as_string();
        }
        if (needle.empty()) {
            return Value{static_cast<double>(start)};
        }
        const auto begin = static_cast<std::size_t>(start - 1);
        if (begin >= haystack.size()) {
            return Value{0.0};
        }
        const auto pos = haystack.find(needle, begin);
        return Value{pos == std::string::npos ? 0.0 : static_cast<double>(pos + 1)};
    }
    if (expr.name == "POS") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("POS expects 1 argument"); }
        (void)eval(*expr.arguments[0]);
        return Value{static_cast<double>(runtime_.current_print_column() + 1)};
    }
    if (expr.name == "EOF") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("EOF expects 1 argument"); }
        const auto file_number = static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number()));
        return Value{runtime_.eof_file(file_number) ? -1.0 : 0.0};
    }
    if (expr.name == "LOF") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("LOF expects 1 argument"); }
        const auto file_number = static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number()));
        return Value{static_cast<double>(runtime_.file_length(file_number))};
    }
    if (expr.name == "LOC") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("LOC expects 1 argument"); }
        const auto file_number = static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number()));
        return Value{static_cast<double>(runtime_.file_loc(file_number))};
    }
    if (expr.name == "SQR") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("SQR expects 1 argument"); }
        const auto value = eval(*expr.arguments[0]).as_number();
        if (value < 0.0) { throw std::runtime_error("SQR domain error"); }
        return Value{std::sqrt(value)};
    }
    if (expr.name == "SIN") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("SIN expects 1 argument"); }
        return Value{std::sin(eval(*expr.arguments[0]).as_number())};
    }
    if (expr.name == "COS") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("COS expects 1 argument"); }
        return Value{std::cos(eval(*expr.arguments[0]).as_number())};
    }
    if (expr.name == "TAN") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("TAN expects 1 argument"); }
        return Value{std::tan(eval(*expr.arguments[0]).as_number())};
    }
    if (expr.name == "ATN") {
        if (expr.arguments.size() != 1) { throw std::runtime_error("ATN expects 1 argument"); }
        return Value{std::atan(eval(*expr.arguments[0]).as_number())};
    }
    if (expr.name == "INKEY$") {
        if (!expr.arguments.empty()) { throw std::runtime_error("INKEY$ expects 0 arguments"); }
        return Value{runtime_.read_key()};
    }
    if (expr.name == "POINT") {
        if (expr.arguments.size() != 2) { throw std::runtime_error("POINT expects 2 arguments"); }
        const auto x = static_cast<int>(std::trunc(eval(*expr.arguments[0]).as_number()));
        const auto y = static_cast<int>(std::trunc(eval(*expr.arguments[1]).as_number()));
        return Value{runtime_.point(x, y)};
    }
    if (expr.name == "PMAP") {
        if (expr.arguments.size() != 2) { throw std::runtime_error("PMAP expects 2 arguments"); }
        const auto coordinate = eval(*expr.arguments[0]).as_number();
        const auto mode = static_cast<int>(std::trunc(eval(*expr.arguments[1]).as_number()));
        return Value{runtime_.pmap(coordinate, mode)};
    }
    throw std::runtime_error("Unknown function: " + expr.name);
}

void Interpreter::execute(const Statement& stmt, int& current_line, std::size_t& statement_index, bool& running) {
    auto assign_target = [&](const VariableRef& target, Value value) {
        if (target.indices.empty()) {
            runtime_.set_variable(target.name, std::move(value));
            return;
        }
        std::vector<int> indices;
        indices.reserve(target.indices.size());
        for (const auto& index_expr : target.indices) {
            indices.push_back(static_cast<int>(eval(*index_expr).as_number()));
        }
        runtime_.set_array_value(target.name, indices, std::move(value));
    };

    auto graphics_block_key = [&](const VariableRef& target) -> std::string {
        std::string key = target.name;
        if (!target.indices.empty()) {
            key.push_back('(');
            for (std::size_t i = 0; i < target.indices.size(); ++i) {
                if (i != 0) key.push_back(',');
                key += std::to_string(static_cast<int>(std::trunc(eval(*target.indices[i]).as_number())));
            }
            key.push_back(')');
        }
        return key;
    };

    auto execute_inline = [&](const std::vector<StmtPtr>& statements) {
        int inline_line = current_line;
        std::size_t inline_index = 0;
        bool inline_running = true;
        while (inline_running && inline_index < statements.size()) {
            execute(*statements[inline_index], inline_line, inline_index, inline_running);
            if (!inline_running) {
                current_line = inline_line;
                statement_index = inline_index;
                running = false;
                return false;
            }
            if (inline_line != current_line) {
                current_line = inline_line;
                statement_index = inline_index;
                return false;
            }
        }
        return true;
    };

    std::visit(Overload{
        [&](const PrintStmt& node) {
            runtime_.print(render_print_items(node.items, [this](const Expr& e) { return eval(e); }, node.trailing_newline));
            ++statement_index;
        },
        [&](const WriteStmt& node) {
            runtime_.print(render_write_items(node.items, [this](const Expr& e) { return eval(e); }, node.trailing_newline));
            ++statement_index;
        },
        [&](const OpenStmt& node) {
            runtime_.open_file(node.file_number, eval(*node.path).as_string(), node.mode, node.record_len);
            ++statement_index;
        },
        [&](const CloseStmt& node) {
            runtime_.close_file(node.file_number);
            ++statement_index;
        },
        [&](const PrintFileStmt& node) {
            runtime_.write_file(node.file_number, render_file_print_items(node.items, [this](const Expr& e) { return eval(e); }, node.trailing_newline));
            ++statement_index;
        },
        [&](const WriteFileStmt& node) {
            runtime_.write_file(node.file_number, render_write_file_items(node.items, [this](const Expr& e) { return eval(e); }, node.trailing_newline));
            ++statement_index;
        },
        [&](const InputFileStmt& node) {
            auto fields = runtime_.read_file_record(node.file_number);
            for (std::size_t i = 0; i < node.targets.size(); ++i) {
                const std::string raw = i < fields.size() ? fields[i] : std::string{};
                assign_target(node.targets[i], parse_input_value(raw, runtime_.is_string_variable(node.targets[i].name)));
            }
            ++statement_index;
        },
        [&](const LineInputFileStmt& node) {
            if (!runtime_.is_string_variable(node.target.name)) {
                throw std::runtime_error("LINE INPUT# target must be a string variable");
            }
            assign_target(node.target, Value{runtime_.read_file_line(node.file_number)});
            ++statement_index;
        },
        [&](const FieldStmt& node) {
            std::vector<std::pair<int, std::string>> bindings;
            bindings.reserve(node.bindings.size());
            for (const auto& binding : node.bindings) bindings.emplace_back(binding.width, binding.variable);
            runtime_.set_field(node.file_number, std::move(bindings));
            ++statement_index;
        },
        [&](const PutStmt& node) {
            std::optional<int> record;
            if (node.record_number.has_value()) record = static_cast<int>(std::trunc(eval(**node.record_number).as_number()));
            runtime_.put_record(node.file_number, record);
            ++statement_index;
        },
        [&](const GetStmt& node) {
            std::optional<int> record;
            if (node.record_number.has_value()) record = static_cast<int>(std::trunc(eval(**node.record_number).as_number()));
            runtime_.get_record(node.file_number, record);
            ++statement_index;
        },
        [&](const GraphicsGetStmt& node) {
            runtime_.get_graphics_block(
                graphics_block_key(node.target),
                static_cast<int>(std::trunc(eval(*node.x1).as_number())),
                static_cast<int>(std::trunc(eval(*node.y1).as_number())),
                static_cast<int>(std::trunc(eval(*node.x2).as_number())),
                static_cast<int>(std::trunc(eval(*node.y2).as_number())));
            ++statement_index;
        },
        [&](const GraphicsPutStmt& node) {
            runtime_.put_graphics_block(
                graphics_block_key(node.source),
                static_cast<int>(std::trunc(eval(*node.x).as_number())),
                static_cast<int>(std::trunc(eval(*node.y).as_number())),
                node.mode);
            ++statement_index;
        },
        [&](const PrintUsingStmt& node) {
            std::vector<Value> values;
            values.reserve(node.arguments.size());
            for (const auto& argument : node.arguments) {
                values.push_back(eval(*argument));
            }
            auto out = apply_print_using_format(node.format, values);
            if (node.trailing_newline) {
                out.push_back('\n');
            }
            runtime_.print(out);
            ++statement_index;
        },
        [&](const LetStmt& node) {
            assign_target(node.target, eval(*node.value));
            ++statement_index;
        },
        [&](const InputStmt& node) {
            if (!node.prompt.empty()) {
                runtime_.print(node.prompt);
            }
            if (!node.suppress_question) {
                runtime_.print("? ");
            }
            auto raw_line = runtime_.read_line();
            std::vector<std::string> fields;
            std::string current;
            bool in_quotes = false;
            for (std::size_t i = 0; i < raw_line.size(); ++i) {
                char ch = raw_line[i];
                if (ch == '"') {
                    if (in_quotes && i + 1 < raw_line.size() && raw_line[i + 1] == '"') {
                        current.push_back('"');
                        ++i;
                    } else {
                        in_quotes = !in_quotes;
                    }
                } else if (ch == ',' && !in_quotes) {
                    fields.push_back(current);
                    current.clear();
                } else {
                    current.push_back(ch);
                }
            }
            fields.push_back(current);
            for (auto& field : fields) {
                if (!field.empty() && field.front() == ' ') field.erase(0, field.find_first_not_of(' '));
                while (!field.empty() && field.back() == ' ') field.pop_back();
            }
            for (std::size_t i = 0; i < node.targets.size(); ++i) {
                const std::string raw = i < fields.size() ? fields[i] : std::string{};
                assign_target(node.targets[i], parse_input_value(raw, runtime_.is_string_variable(node.targets[i].name)));
            }
            ++statement_index;
        },
        [&](const LineInputStmt& node) {
            if (!runtime_.is_string_variable(node.target.name)) {
                throw std::runtime_error("LINE INPUT target must be a string variable");
            }
            if (!node.prompt.empty()) {
                runtime_.print(node.prompt);
            }
            assign_target(node.target, Value{runtime_.read_line()});
            ++statement_index;
        },
        [&](const IfStmt& node) {
            if (eval(*node.condition).truthy()) {
                current_line = node.target_line;
                statement_index = 0;
                return;
            }
            ++statement_index;
        },
        [&](const IfThenStmt& node) {
            const auto& branch = eval(*node.condition).truthy() ? node.then_statements : node.else_statements;
            if (!execute_inline(branch)) {
                return;
            }
            ++statement_index;
        },
        [&](const GotoStmt& node) {
            if (node.target_line <= current_line) {
                runtime_.tick_engine();
            }
            current_line = node.target_line;
            statement_index = 0;
        },
        [&](const GosubStmt& node) {
            if (node.target_line <= current_line) {
                runtime_.tick_engine();
            }
            runtime_.push_return(current_line, statement_index + 1);
            current_line = node.target_line;
            statement_index = 0;
        },
        [&](const OnGotoStmt& node) {
            jump_to_selected_target(node.targets, eval(*node.selector).as_number(), current_line, statement_index, false);
        },
        [&](const OnGosubStmt& node) {
            jump_to_selected_target(node.targets, eval(*node.selector).as_number(), current_line, statement_index, true);
        },
        [&](const ReturnStmt&) {
            auto target = runtime_.pop_return();
            if (!target.has_value()) {
                throw std::runtime_error("RETURN without GOSUB");
            }
            current_line = target->first;
            statement_index = target->second;
        },
        [&](const ForStmt& node) {
            const auto start = eval(*node.start).as_number();
            const auto finish = eval(*node.finish).as_number();
            const auto step = eval(*node.step).as_number();
            runtime_.set_variable(node.variable, Value{start});
            runtime_.push_for(ForFrame{node.variable, finish, step, current_line, statement_index + 1});
            ++statement_index;
        },
        [&](const NextStmt& node) {
            auto* frame = runtime_.top_for();
            if (frame == nullptr) {
                throw std::runtime_error("NEXT without FOR");
            }
            if (node.variable.has_value() && *node.variable != frame->variable) {
                throw std::runtime_error("NEXT variable mismatch");
            }
            auto value = runtime_.get_variable(frame->variable).as_number() + frame->step;
            runtime_.set_variable(frame->variable, Value{value});
            const bool continue_loop = frame->step >= 0.0 ? value <= frame->limit : value >= frame->limit;
            if (continue_loop) {
                runtime_.tick_engine();
                current_line = frame->line_after_for;
                statement_index = frame->statement_index_after_for;
                return;
            }
            runtime_.pop_for();
            ++statement_index;
        },
        [&](const WhileStmt& node) {
            const bool matched_top = [&]() {
                auto* frame = runtime_.top_while();
                return frame != nullptr && frame->while_line == current_line && frame->while_statement_index == statement_index;
            }();

            if (eval(*node.condition).truthy()) {
                if (!matched_top) {
                    runtime_.push_while(WhileFrame{current_line, statement_index, current_line, statement_index + 1});
                }
                ++statement_index;
                return;
            }

            if (matched_top) {
                runtime_.pop_while();
            }
            const auto [wend_line, wend_statement] = find_matching_wend(current_line, statement_index);
            current_line = wend_line;
            statement_index = wend_statement + 1;
        },
        [&](const WendStmt&) {
            auto* frame = runtime_.top_while();
            if (frame == nullptr) {
                throw std::runtime_error("WEND without WHILE");
            }
            runtime_.tick_engine();
            current_line = frame->while_line;
            statement_index = frame->while_statement_index;
        },
        [&](const DataStmt&) { ++statement_index; },
        [&](const ReadStmt& node) {
            for (const auto& target : node.targets) {
                assign_target(target, runtime_.read_data());
            }
            ++statement_index;
        },
        [&](const RestoreStmt& node) {
            runtime_.restore_data(node.line);
            ++statement_index;
        },
        [&](const DimStmt& node) {
            for (const auto& decl : node.declarations) {
                std::vector<int> dims;
                dims.reserve(decl.dimensions.size());
                for (const auto& dim_expr : decl.dimensions) {
                    dims.push_back(static_cast<int>(eval(*dim_expr).as_number()));
                }
                runtime_.dim_array(decl.name, std::move(dims));
            }
            ++statement_index;
        },
        [&](const DefIntStmt& node) {
            for (const auto& range : node.ranges) {
                runtime_.set_default_numeric_range(range.start, range.end, VariableKind::Integer);
            }
            ++statement_index;
        },
        [&](const DefStrStmt& node) {
            for (const auto& range : node.ranges) {
                runtime_.set_default_numeric_range(range.start, range.end, VariableKind::String);
            }
            ++statement_index;
        },
        [&](const DefSngStmt& node) {
            for (const auto& range : node.ranges) {
                runtime_.set_default_numeric_range(range.start, range.end, VariableKind::Numeric);
            }
            ++statement_index;
        },
        [&](const DefDblStmt& node) {
            for (const auto& range : node.ranges) {
                runtime_.set_default_numeric_range(range.start, range.end, VariableKind::Numeric);
            }
            ++statement_index;
        },
        [&](const StopStmt&) {
            continuation_point_ = std::pair<int, std::size_t>{current_line, statement_index + 1};
            running = false;
        },
        [&](const ContStmt&) {
            if (!continuation_point_.has_value()) {
                throw std::runtime_error("CONT without STOP");
            }
            current_line = continuation_point_->first;
            statement_index = continuation_point_->second;
        },
        [&](const EndStmt&) { runtime_.close_file(); continuation_point_.reset(); running = false; },
        [&](const RemStmt&) { ++statement_index; },
        [&](const ListStmt&) { list(); ++statement_index; },
        [&](const RunStmt&) { run(); ++statement_index; },
        [&](const NewStmt&) { clear_program(); ++statement_index; },
        [&](const ClearStmt&) { runtime_.clear_variables(); runtime_.restore_data(); runtime_.close_file(); ++statement_index; },
        [&](const KillStmt& node) { runtime_.delete_file(eval(*node.path).as_string()); ++statement_index; },
        [&](const NameStmt& node) { runtime_.rename_file(eval(*node.old_path).as_string(), eval(*node.new_path).as_string()); ++statement_index; },
        [&](const MkdirStmt& node) { runtime_.create_directory(eval(*node.path).as_string()); ++statement_index; },
        [&](const RmdirStmt& node) { runtime_.remove_directory(eval(*node.path).as_string()); ++statement_index; },
        [&](const LsetStmt& node) { runtime_.set_record_field(node.target.name, eval(*node.value).as_string(), false); ++statement_index; },
        [&](const RsetStmt& node) { runtime_.set_record_field(node.target.name, eval(*node.value).as_string(), true); ++statement_index; },
        [&](const ClsStmt&) { runtime_.cls(); ++statement_index; },
        [&](const LocateStmt& node) {
            auto to_opt_int = [&](const std::optional<ExprPtr>& expr) -> std::optional<int> {
                if (!expr.has_value()) {
                    return std::nullopt;
                }
                return static_cast<int>(eval(*expr->get()).as_number());
            };
            runtime_.locate_cursor(to_opt_int(node.row), to_opt_int(node.column), to_opt_int(node.cursor), to_opt_int(node.start), to_opt_int(node.stop));
            ++statement_index;
        },
        [&](const ColorStmt& node) {
            auto to_opt_int = [&](const std::optional<ExprPtr>& expr) -> std::optional<int> {
                if (!expr.has_value()) {
                    return std::nullopt;
                }
                return static_cast<int>(eval(*expr->get()).as_number());
            };
            runtime_.set_color(to_opt_int(node.foreground), to_opt_int(node.background), to_opt_int(node.border));
            ++statement_index;
        },
        [&](const BeepStmt&) { runtime_.print("\a"); ++statement_index; },
        [&](const ScreenStmt& node) {
            auto to_opt_int = [&](const std::optional<ExprPtr>& expr) -> std::optional<int> {
                if (!expr.has_value()) {
                    return std::nullopt;
                }
                return static_cast<int>(eval(*expr->get()).as_number());
            };
            runtime_.set_screen(to_opt_int(node.mode), to_opt_int(node.color_switch), to_opt_int(node.active_page), to_opt_int(node.visual_page));
            ++statement_index;
        },
        [&](const KeyStmt& node) { runtime_.set_key_display(node.enabled); ++statement_index; },
        [&](const SoundStmt& node) { runtime_.sound(eval(*node.frequency).as_number(), eval(*node.duration).as_number()); ++statement_index; },
        [&](const PlayStmt& node) { runtime_.play(eval(*node.sequence).as_string()); ++statement_index; },
        [&](const PsetStmt& node) {
            std::optional<int> color;
            if (node.color.has_value()) color = static_cast<int>(std::trunc(eval(**node.color).as_number()));
            runtime_.pset(runtime_.map_window_x(eval(*node.x).as_number()), runtime_.map_window_y(eval(*node.y).as_number()), color);
            ++statement_index;
        },
        [&](const GraphicsLineStmt& node) {
            std::optional<int> color;
            if (node.color.has_value()) color = static_cast<int>(std::trunc(eval(**node.color).as_number()));
            runtime_.draw_line(
                runtime_.map_window_x(eval(*node.x1).as_number()),
                runtime_.map_window_y(eval(*node.y1).as_number()),
                runtime_.map_window_x(eval(*node.x2).as_number()),
                runtime_.map_window_y(eval(*node.y2).as_number()),
                color,
                node.box,
                node.fill);
            ++statement_index;
        },
        [&](const CircleStmt& node) {
            std::optional<int> color;
            if (node.color.has_value()) color = static_cast<int>(std::trunc(eval(**node.color).as_number()));
            runtime_.draw_circle(
                runtime_.map_window_x(eval(*node.x).as_number()),
                runtime_.map_window_y(eval(*node.y).as_number()),
                static_cast<int>(std::trunc(eval(*node.radius).as_number())),
                color);
            ++statement_index;
        },
        [&](const PaintStmt& node) {
            std::optional<int> color;
            std::optional<int> border;
            if (node.color.has_value()) color = static_cast<int>(std::trunc(eval(**node.color).as_number()));
            if (node.border.has_value()) border = static_cast<int>(std::trunc(eval(**node.border).as_number()));
            runtime_.paint(
                runtime_.map_window_x(eval(*node.x).as_number()),
                runtime_.map_window_y(eval(*node.y).as_number()),
                color,
                border);
            ++statement_index;
        },
        [&](const DrawStmt& node) {
            runtime_.draw_commands(eval(*node.commands).as_string());
            ++statement_index;
        },
        [&](const ViewStmt& node) {
            auto to_opt_int = [&](const std::optional<ExprPtr>& expr) -> std::optional<int> {
                if (!expr.has_value()) return std::nullopt;
                return static_cast<int>(std::trunc(eval(*expr->get()).as_number()));
            };
            runtime_.set_view(to_opt_int(node.x1), to_opt_int(node.y1), to_opt_int(node.x2), to_opt_int(node.y2));
            ++statement_index;
        },
        [&](const WindowStmt& node) {
            auto to_opt_double = [&](const std::optional<ExprPtr>& expr) -> std::optional<double> {
                if (!expr.has_value()) return std::nullopt;
                return eval(*expr->get()).as_number();
            };
            runtime_.set_window(node.screen_coordinates, to_opt_double(node.x1), to_opt_double(node.y1), to_opt_double(node.x2), to_opt_double(node.y2));
            ++statement_index;
        },
        [&](const PaletteStmt& node) {
            if (node.using_mode) {
                if (!node.using_source.has_value()) {
                    throw std::runtime_error("PALETTE USING requires an array variable");
                }
                runtime_.set_palette_using(*node.using_source);
            } else {
                auto to_opt_int = [&](const std::optional<ExprPtr>& expr) -> std::optional<int> {
                    if (!expr.has_value()) return std::nullopt;
                    return static_cast<int>(std::trunc(eval(*expr->get()).as_number()));
                };
                runtime_.set_palette(to_opt_int(node.attribute), to_opt_int(node.color));
            }
            ++statement_index;
        }
    }, stmt.node);
}

void Interpreter::jump_to_selected_target(const std::vector<int>& targets, double selector, int& current_line, std::size_t& statement_index, bool gosub) {
    const auto index = static_cast<int>(selector);
    if (index < 1 || static_cast<std::size_t>(index) > targets.size()) {
        ++statement_index;
        return;
    }
    if (gosub) {
        runtime_.push_return(current_line, statement_index + 1);
    }
    current_line = targets[static_cast<std::size_t>(index - 1)];
    statement_index = 0;
}

auto Interpreter::next_line_number(int line) const -> int {
    const auto it = program_.lines().upper_bound(line);
    return it == program_.lines().end() ? -1 : it->first;
}

auto Interpreter::find_matching_wend(int from_line, std::size_t from_statement_index) const -> std::pair<int, std::size_t> {
    int depth = 0;
    bool started = false;
    for (const auto& [line_number, line] : program_.lines()) {
        if (line_number < from_line) {
            continue;
        }
        std::size_t start_index = 0;
        if (line_number == from_line) {
            start_index = from_statement_index;
        }
        for (std::size_t i = start_index; i < line.statements.size(); ++i) {
            if (!started) {
                started = true;
                depth = 1;
                continue;
            }
            const auto& variant = line.statements[i]->node;
            if (std::holds_alternative<WhileStmt>(variant)) {
                ++depth;
            } else if (std::holds_alternative<WendStmt>(variant)) {
                --depth;
                if (depth == 0) {
                    return {line_number, i};
                }
            }
        }
    }
    throw std::runtime_error("WHILE without matching WEND");
}

void Interpreter::rebuild_data_cache() {
    std::vector<Value> values;
    std::unordered_map<int, std::size_t> line_index;

    for (const auto& [line_number, line] : program_.lines()) {
        for (const auto& stmt : line.statements) {
            if (const auto* data = std::get_if<DataStmt>(&stmt->node)) {
                if (!line_index.contains(line_number)) {
                    line_index[line_number] = values.size();
                }
                for (const auto& item : data->items) {
                    values.push_back(eval(*item));
                }
            }
        }
    }

    runtime_.set_data(std::move(values), std::move(line_index));
}

} // namespace gwbasic
