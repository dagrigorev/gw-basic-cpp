#include "gwbasic/lexer.hpp"
#include "gwbasic/parser.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

namespace {

void fail(const std::string& message) {
    std::cerr << "Parser test failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

template <typename T>
const T& as_statement(const gwbasic::Statement& statement) {
    if (!std::holds_alternative<T>(statement.node)) {
        fail("unexpected statement variant");
    }
    return std::get<T>(statement.node);
}

template <typename T>
const T& as_expr(const gwbasic::Expr& expression) {
    if (!std::holds_alternative<T>(expression.node)) {
        fail("unexpected expression variant");
    }
    return std::get<T>(expression.node);
}

bool number_is(const gwbasic::ExprPtr& expression, double expected) {
    const auto& number = as_expr<gwbasic::NumberExpr>(*expression);
    return std::fabs(number.value - expected) < 0.000001;
}

gwbasic::ParsedLine parse(const std::string& source) {
    const gwbasic::Lexer lexer;
    const gwbasic::Parser parser;
    return parser.parse_line(lexer.tokenize(source), source);
}

void require_parse_throws(const std::string& source) {
    try {
        (void)parse(source);
    } catch (const std::exception&) {
        return;
    }
    fail("expected parse failure for: " + source);
}

} // namespace

int main() {
    {
        const auto line = parse("10 LET A = 1 + 2 * 3");
        require(line.line_number == 10, "line number");
        require(line.statements.size() == 1, "LET statement count");
        const auto& stmt = as_statement<gwbasic::LetStmt>(*line.statements[0]);
        require(stmt.target.name == "A", "LET target");
        const auto& plus = as_expr<gwbasic::BinaryExpr>(*stmt.value);
        require(plus.op == "+", "outer expression operator");
        require(number_is(plus.left, 1.0), "left operand");
        const auto& multiply = as_expr<gwbasic::BinaryExpr>(*plus.right);
        require(multiply.op == "*", "operator precedence");
        require(number_is(multiply.left, 2.0) && number_is(multiply.right, 3.0), "multiplication operands");
    }

    {
        const auto line = parse("11 LET A = 2 ^ 3 ^ 2");
        const auto& stmt = as_statement<gwbasic::LetStmt>(*line.statements[0]);
        const auto& power = as_expr<gwbasic::BinaryExpr>(*stmt.value);
        require(power.op == "^", "power operator");
        require(number_is(power.left, 2.0), "power left operand");
        const auto& nested = as_expr<gwbasic::BinaryExpr>(*power.right);
        require(nested.op == "^" && number_is(nested.left, 3.0) && number_is(nested.right, 2.0), "power is right-associative");
    }

    {
        const auto line = parse("20 PRINT \"A\";B, C");
        const auto& stmt = as_statement<gwbasic::PrintStmt>(*line.statements[0]);
        require(stmt.items.size() == 3, "PRINT item count");
        require(stmt.items[0].separator == gwbasic::PrintSeparator::Semicolon, "PRINT semicolon separator");
        require(stmt.items[1].separator == gwbasic::PrintSeparator::Comma, "PRINT comma separator");
        require(stmt.trailing_newline, "PRINT trailing newline");
        require(as_expr<gwbasic::StringExpr>(*stmt.items[0].expression).value == "A", "PRINT string item");
    }

    {
        const auto line = parse("30 IF A > 10 THEN PRINT \"HIGH\" ELSE GOTO 90");
        const auto& stmt = as_statement<gwbasic::IfThenStmt>(*line.statements[0]);
        require(std::holds_alternative<gwbasic::BinaryExpr>(stmt.condition->node), "IF condition");
        require(stmt.then_statements.size() == 1, "IF then statement count");
        require(stmt.else_statements.size() == 1, "IF else statement count");
        (void)as_statement<gwbasic::PrintStmt>(*stmt.then_statements[0]);
        require(as_statement<gwbasic::GotoStmt>(*stmt.else_statements[0]).target_line == 90, "IF ELSE GOTO target");
    }

    {
        const auto line = parse("40 IF A THEN 100");
        const auto& stmt = as_statement<gwbasic::IfStmt>(*line.statements[0]);
        require(stmt.target_line == 100, "IF THEN line target");
    }

    {
        const auto line = parse("50 FOR I = 1 TO 10 STEP 2: NEXT I");
        require(line.statements.size() == 2, "FOR/NEXT statement count");
        const auto& for_stmt = as_statement<gwbasic::ForStmt>(*line.statements[0]);
        require(for_stmt.variable == "I", "FOR variable");
        require(number_is(for_stmt.start, 1.0) && number_is(for_stmt.finish, 10.0) && number_is(for_stmt.step, 2.0), "FOR bounds");
        require(as_statement<gwbasic::NextStmt>(*line.statements[1]).variable == "I", "NEXT variable");
    }

    {
        const auto line = parse("60 DIM A(10), B$(2,3)");
        const auto& stmt = as_statement<gwbasic::DimStmt>(*line.statements[0]);
        require(stmt.declarations.size() == 2, "DIM declaration count");
        require(stmt.declarations[0].name == "A", "DIM first name");
        require(stmt.declarations[1].name == "B$", "DIM second name");
        require(stmt.declarations[1].dimensions.size() == 2, "DIM multidimensional bounds");
    }

    {
        const auto line = parse("61 ERASE A, B$");
        const auto& stmt = as_statement<gwbasic::EraseStmt>(*line.statements[0]);
        require(stmt.names.size() == 2, "ERASE name count");
        require(stmt.names[0] == "A" && stmt.names[1] == "B$", "ERASE names");
    }

    {
        const auto line = parse("62 OPTION BASE 1");
        const auto& stmt = as_statement<gwbasic::OptionBaseStmt>(*line.statements[0]);
        require(stmt.base == 1, "OPTION BASE value");
    }

    {
        const auto line = parse("70 ON X GOSUB 100, 200, 300");
        const auto& stmt = as_statement<gwbasic::OnGosubStmt>(*line.statements[0]);
        require(stmt.targets.size() == 3, "ON GOSUB target count");
        require(stmt.targets[0] == 100 && stmt.targets[2] == 300, "ON GOSUB targets");
    }

    {
        const auto line = parse("75 ON ERROR GOTO 900: RESUME NEXT");
        require(line.statements.size() == 2, "ON ERROR/RESUME statement count");
        require(as_statement<gwbasic::OnErrorGotoStmt>(*line.statements[0]).target_line == 900, "ON ERROR target");
        require(as_statement<gwbasic::ResumeStmt>(*line.statements[1]).next, "RESUME NEXT flag");
    }

    {
        const auto line = parse("75 ERROR 42");
        const auto& stmt = as_statement<gwbasic::ErrorStmt>(*line.statements[0]);
        require(number_is(stmt.code, 42.0), "ERROR code");
    }

    {
        const auto line = parse("76 RESUME 120");
        const auto& stmt = as_statement<gwbasic::ResumeStmt>(*line.statements[0]);
        require(!stmt.next, "RESUME line is not NEXT");
        require(stmt.target_line == 120, "RESUME target line");
    }

    {
        const auto line = parse("80 LINE (1,2)-(3,4),5,BF");
        const auto& stmt = as_statement<gwbasic::GraphicsLineStmt>(*line.statements[0]);
        require(number_is(stmt.x1, 1.0) && number_is(stmt.y2, 4.0), "LINE coordinates");
        require(stmt.color.has_value(), "LINE color");
        require(stmt.box && stmt.fill, "LINE BF mode");
    }

    {
        const auto line = parse("85 WIDTH 40");
        const auto& stmt = as_statement<gwbasic::WidthStmt>(*line.statements[0]);
        require(number_is(stmt.columns, 40.0), "WIDTH columns");
    }

    {
        const auto line = parse("86 POKE 100, 255");
        const auto& stmt = as_statement<gwbasic::PokeStmt>(*line.statements[0]);
        require(number_is(stmt.address, 100.0), "POKE address");
        require(number_is(stmt.value, 255.0), "POKE value");
    }

    {
        const auto line = parse("87 SWAP A, B(1)");
        const auto& stmt = as_statement<gwbasic::SwapStmt>(*line.statements[0]);
        require(stmt.left.name == "A", "SWAP left");
        require(stmt.right.name == "B", "SWAP right");
        require(stmt.right.indices.size() == 1, "SWAP array target");
    }

    {
        const auto line = parse("87 RANDOMIZE 123");
        const auto& stmt = as_statement<gwbasic::RandomizeStmt>(*line.statements[0]);
        require(stmt.seed.has_value(), "RANDOMIZE seed");
        require(number_is(*stmt.seed, 123.0), "RANDOMIZE seed value");
    }

    {
        const auto line = parse("88 DEF FNSQR(X)=X*X: PRINT FNSQR(4)");
        require(line.statements.size() == 2, "DEF FN/PRINT statement count");
        const auto& stmt = as_statement<gwbasic::DefFnStmt>(*line.statements[0]);
        require(stmt.name == "FNSQR", "DEF FN name");
        require(stmt.parameters.size() == 1 && stmt.parameters[0] == "X", "DEF FN parameter");
        const auto& print = as_statement<gwbasic::PrintStmt>(*line.statements[1]);
        const auto& call = as_expr<gwbasic::FunctionCallExpr>(*print.items[0].expression);
        require(call.name == "FNSQR" && call.arguments.size() == 1, "DEF FN call expression");
    }

    {
        const auto line = parse("89 PRINT DATE$;TIME$;TIMER");
        const auto& stmt = as_statement<gwbasic::PrintStmt>(*line.statements[0]);
        require(stmt.items.size() == 3, "zero-argument time intrinsic count");
        require(as_expr<gwbasic::FunctionCallExpr>(*stmt.items[0].expression).name == "DATE$", "DATE$ call");
        require(as_expr<gwbasic::FunctionCallExpr>(*stmt.items[1].expression).name == "TIME$", "TIME$ call");
        require(as_expr<gwbasic::FunctionCallExpr>(*stmt.items[2].expression).name == "TIMER", "TIMER call");
    }

    {
        const auto line = parse("90 DATA 1, \"TWO\": READ A, B$: RESTORE 90");
        require(line.statements.size() == 3, "DATA/READ/RESTORE statement count");
        require(as_statement<gwbasic::DataStmt>(*line.statements[0]).items.size() == 2, "DATA items");
        require(as_statement<gwbasic::ReadStmt>(*line.statements[1]).targets.size() == 2, "READ targets");
        require(as_statement<gwbasic::RestoreStmt>(*line.statements[2]).line == 90, "RESTORE line");
    }

    require_parse_throws("100 PRINT (1 +");
    require_parse_throws("110 FOR I = 1 10");

    {
        const gwbasic::Lexer lexer;
        const gwbasic::Parser parser;
        const auto result = parser.parse_line_recovering(
            lexer.tokenize("120 PRINT 1: FOR I = 1 10: PRINT 2: BOGUS: PRINT 3"),
            "120 PRINT 1: FOR I = 1 10: PRINT 2: BOGUS: PRINT 3");
        require(result.line.line_number == 120, "recovering line number");
        require(result.line.statements.size() == 3, "recovering keeps valid statements");
        require(result.diagnostics.size() == 2, "recovering diagnostics count");
        (void)as_statement<gwbasic::PrintStmt>(*result.line.statements[0]);
        (void)as_statement<gwbasic::PrintStmt>(*result.line.statements[1]);
        (void)as_statement<gwbasic::PrintStmt>(*result.line.statements[2]);
    }

    std::cout << "Parser tests passed\n";
    return 0;
}
