#include "gwbasic/parser.hpp"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <utility>

namespace gwbasic {
namespace {

class LineParser {
public:
    LineParser(const std::vector<Token>& tokens, std::string original)
        : tokens_(tokens), original_(std::move(original)) {}

    [[nodiscard]] auto parse() -> ParsedLine {
        ParsedLine line;
        line.original_text = std::move(original_);

        if (check(TokenType::LineNumber)) {
            line.line_number = std::stoi(advance().lexeme);
        }

        while (!check(TokenType::EndOfLine)) {
            line.statements.push_back(parse_statement());
            if (match(TokenType::Colon)) {
                continue;
            }
            if (!check(TokenType::EndOfLine)) {
                throw std::runtime_error("Expected ':' or end of line");
            }
        }
        return line;
    }

private:
    [[nodiscard]] auto parse_statement() -> StmtPtr {
        if (match(TokenType::KeywordPrint)) {
            if (check(TokenType::Hash)) {
                return parse_print_file_statement();
            }
            auto stmt = std::make_unique<Statement>();
            stmt->node = parse_print_statement();
            return stmt;
        }
        if (match(TokenType::KeywordOpen)) { return parse_open_statement(); }
        if (match(TokenType::KeywordClose)) { return parse_close_statement(); }
        if (match(TokenType::KeywordWrite)) { return parse_write_statement(); }
        if (match(TokenType::KeywordLine)) { return parse_line_statement(); }
        if (match(TokenType::KeywordPaint)) { return parse_paint_statement(); }
        if (match(TokenType::KeywordDraw)) { return parse_draw_statement(); }
        if (match(TokenType::KeywordView)) { return parse_view_statement(); }
        if (match(TokenType::KeywordWindow)) { return parse_window_statement(); }
        if (match(TokenType::KeywordPalette)) { return parse_palette_statement(); }
        if (check(TokenType::KeywordInput) && tokens_[current_ + 1].type == TokenType::Hash) { advance(); return parse_input_file_statement(); }
        if (match(TokenType::KeywordLet)) {
            return parse_assignment();
        }
        if (looks_like_assignment()) {
            return parse_assignment();
        }
        if (match(TokenType::KeywordInput)) {
            auto stmt = std::make_unique<Statement>();
            InputStmt s;
            if (check(TokenType::String)) {
                s.prompt = advance().lexeme;
                if (match(TokenType::Semicolon)) {
                    s.suppress_question = true;
                } else if (match(TokenType::Comma)) {
                    s.suppress_question = false;
                } else {
                    throw std::runtime_error("Expected ';' or ',' after INPUT prompt");
                }
            }
            s.targets.push_back(parse_variable_reference());
            while (match(TokenType::Comma)) {
                s.targets.push_back(parse_variable_reference());
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordIf)) {
            return parse_if_statement();
        }
        if (match(TokenType::KeywordGoto)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = GotoStmt{std::stoi(consume_any_number("Expected line number after GOTO").lexeme)};
            return stmt;
        }
        if (match(TokenType::KeywordGosub)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = GosubStmt{std::stoi(consume_any_number("Expected line number after GOSUB").lexeme)};
            return stmt;
        }
        if (match(TokenType::KeywordOn)) {
            return parse_on_statement();
        }
        if (match(TokenType::KeywordReturn)) { auto stmt = std::make_unique<Statement>(); stmt->node = ReturnStmt{}; return stmt; }
        if (match(TokenType::KeywordFor)) {
            auto stmt = std::make_unique<Statement>();
            ForStmt s;
            s.variable = consume(TokenType::Identifier, "Expected loop variable after FOR").lexeme;
            (void)consume(TokenType::Equal, "Expected '=' in FOR statement");
            s.start = parse_expression();
            (void)consume(TokenType::KeywordTo, "Expected TO in FOR statement");
            s.finish = parse_expression();
            if (match(TokenType::KeywordStep)) {
                s.step = parse_expression();
            } else {
                auto expr = std::make_unique<Expr>();
                expr->node = NumberExpr{1.0};
                s.step = std::move(expr);
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordNext)) {
            auto stmt = std::make_unique<Statement>();
            NextStmt s;
            if (check(TokenType::Identifier)) {
                s.variable = advance().lexeme;
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordWhile)) {
            auto stmt = std::make_unique<Statement>();
            WhileStmt s;
            s.condition = parse_expression();
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordWend)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = WendStmt{};
            return stmt;
        }
        if (match(TokenType::KeywordData)) {
            auto stmt = std::make_unique<Statement>();
            DataStmt s;
            if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                s.items.push_back(parse_expression());
                while (match(TokenType::Comma)) {
                    s.items.push_back(parse_expression());
                }
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordRead)) {
            auto stmt = std::make_unique<Statement>();
            ReadStmt s;
            s.targets.push_back(parse_variable_reference());
            while (match(TokenType::Comma)) {
                s.targets.push_back(parse_variable_reference());
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordRestore)) {
            auto stmt = std::make_unique<Statement>();
            RestoreStmt s;
            if (check(TokenType::Number) || check(TokenType::LineNumber)) {
                s.line = std::stoi(advance().lexeme);
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordDim)) {
            auto stmt = std::make_unique<Statement>();
            DimStmt s;
            s.declarations.push_back(parse_dim_decl());
            while (match(TokenType::Comma)) {
                s.declarations.push_back(parse_dim_decl());
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordDefint)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = DefIntStmt{parse_def_type_ranges()};
            return stmt;
        }
        if (match(TokenType::KeywordDefstr)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = DefStrStmt{parse_def_type_ranges()};
            return stmt;
        }
        if (match(TokenType::KeywordDefsng)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = DefSngStmt{parse_def_type_ranges()};
            return stmt;
        }
        if (match(TokenType::KeywordDefdbl)) {
            auto stmt = std::make_unique<Statement>();
            stmt->node = DefDblStmt{parse_def_type_ranges()};
            return stmt;
        }
        if (match(TokenType::KeywordStop)) { auto stmt = std::make_unique<Statement>(); stmt->node = StopStmt{}; return stmt; }
        if (match(TokenType::KeywordCont)) { auto stmt = std::make_unique<Statement>(); stmt->node = ContStmt{}; return stmt; }
        if (match(TokenType::KeywordEnd)) { auto stmt = std::make_unique<Statement>(); stmt->node = EndStmt{}; return stmt; }
        if (match(TokenType::KeywordRem)) { auto stmt = std::make_unique<Statement>(); stmt->node = RemStmt{check(TokenType::String) ? advance().lexeme : ""}; return stmt; }
        if (match(TokenType::KeywordList)) { auto stmt = std::make_unique<Statement>(); stmt->node = ListStmt{}; return stmt; }
        if (match(TokenType::KeywordRun)) { auto stmt = std::make_unique<Statement>(); stmt->node = RunStmt{}; return stmt; }
        if (match(TokenType::KeywordNew)) { auto stmt = std::make_unique<Statement>(); stmt->node = NewStmt{}; return stmt; }
        if (match(TokenType::KeywordClear)) { auto stmt = std::make_unique<Statement>(); stmt->node = ClearStmt{}; return stmt; }
        if (match(TokenType::KeywordField)) { return parse_field_statement(); }
        if (match(TokenType::KeywordGet)) { return parse_get_put_statement(false); }
        if (match(TokenType::KeywordPut)) { return parse_get_put_statement(true); }
        if (match(TokenType::KeywordLset)) { auto stmt = std::make_unique<Statement>(); LsetStmt s; s.target = parse_variable_reference(); (void)consume(TokenType::Equal, "Expected = after LSET target"); s.value = parse_expression(); stmt->node = std::move(s); return stmt; }
        if (match(TokenType::KeywordRset)) { auto stmt = std::make_unique<Statement>(); RsetStmt s; s.target = parse_variable_reference(); (void)consume(TokenType::Equal, "Expected = after RSET target"); s.value = parse_expression(); stmt->node = std::move(s); return stmt; }
        if (match(TokenType::KeywordKill)) { auto stmt = std::make_unique<Statement>(); stmt->node = KillStmt{parse_expression()}; return stmt; }
        if (match(TokenType::KeywordName)) { auto stmt = std::make_unique<Statement>(); NameStmt s; s.old_path = parse_expression(); (void)consume(TokenType::KeywordAs, "Expected AS in NAME statement"); s.new_path = parse_expression(); stmt->node = std::move(s); return stmt; }
        if (match(TokenType::KeywordMkdir)) { auto stmt = std::make_unique<Statement>(); stmt->node = MkdirStmt{parse_expression()}; return stmt; }
        if (match(TokenType::KeywordRmdir)) { auto stmt = std::make_unique<Statement>(); stmt->node = RmdirStmt{parse_expression()}; return stmt; }
        if (match(TokenType::KeywordCls)) { auto stmt = std::make_unique<Statement>(); stmt->node = ClsStmt{}; return stmt; }
        if (match(TokenType::KeywordLocate)) { return parse_locate_statement(); }
        if (match(TokenType::KeywordColor)) { return parse_color_statement(); }
        if (match(TokenType::KeywordBeep)) { auto stmt = std::make_unique<Statement>(); stmt->node = BeepStmt{}; return stmt; }
        if (match(TokenType::KeywordScreen)) { return parse_screen_statement(); }
        if (match(TokenType::KeywordKey)) { return parse_key_statement(); }
        if (match(TokenType::KeywordSound)) { return parse_sound_statement(); }
        if (match(TokenType::KeywordPlay)) { return parse_play_statement(); }
        if (match(TokenType::KeywordPset)) { return parse_pset_statement(); }
        if (match(TokenType::KeywordCircle)) { return parse_circle_statement(); }

        throw std::runtime_error("Unsupported or invalid statement near column " + std::to_string(peek().column));
    }

    [[nodiscard]] auto parse_print_statement() -> Statement::Variant {
        if (match(TokenType::KeywordUsing)) {
            PrintUsingStmt stmt;
            stmt.format = consume(TokenType::String, "Expected format string after PRINT USING").lexeme;
            if (match(TokenType::Semicolon) || match(TokenType::Comma)) {
                if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                    stmt.arguments.push_back(parse_expression());
                    while (match(TokenType::Semicolon) || match(TokenType::Comma)) {
                        stmt.trailing_newline = false;
                        if (check(TokenType::EndOfLine) || check(TokenType::Colon)) {
                            return stmt;
                        }
                        stmt.arguments.push_back(parse_expression());
                    }
                } else {
                    stmt.trailing_newline = false;
                }
            }
            return stmt;
        }

        PrintStmt stmt;
        if (check(TokenType::EndOfLine) || check(TokenType::Colon)) {
            return stmt;
        }

        while (true) {
            PrintItem item;
            item.expression = parse_expression();
            if (match(TokenType::Comma)) {
                item.separator = PrintSeparator::Comma;
                stmt.trailing_newline = false;
            } else if (match(TokenType::Semicolon)) {
                item.separator = PrintSeparator::Semicolon;
                stmt.trailing_newline = false;
            } else {
                item.separator = PrintSeparator::None;
                stmt.trailing_newline = true;
            }
            stmt.items.push_back(std::move(item));

            if (stmt.items.back().separator == PrintSeparator::None || check(TokenType::EndOfLine) || check(TokenType::Colon)) {
                break;
            }
        }
        return stmt;
    }


    [[nodiscard]] auto parse_open_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        OpenStmt s;
        s.path = parse_expression();
        if (!match(TokenType::KeywordFor)) throw std::runtime_error("Expected FOR in OPEN statement");
        if (match(TokenType::KeywordInput)) s.mode = FileMode::Input;
        else if (match(TokenType::KeywordOutput)) s.mode = FileMode::Output;
        else if (match(TokenType::KeywordAppend)) s.mode = FileMode::Append;
        else if (match(TokenType::KeywordRandom)) s.mode = FileMode::Random;
        else throw std::runtime_error("Expected INPUT, OUTPUT, APPEND, or RANDOM in OPEN statement");
        (void)consume(TokenType::KeywordAs, "Expected AS in OPEN statement");
        (void)consume(TokenType::Hash, "Expected # before file number");
        s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
        if (s.mode == FileMode::Random && match(TokenType::KeywordLen)) {
            (void)consume(TokenType::Equal, "Expected = after LEN");
            s.record_len = std::stoi(consume_any_number("Expected record length after LEN=").lexeme);
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_close_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        CloseStmt s;
        if (match(TokenType::Hash)) {
            s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_print_file_statement() -> StmtPtr {
        (void)consume(TokenType::Hash, "Expected # after PRINT");
        auto stmt = std::make_unique<Statement>();
        PrintFileStmt s;
        s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
        (void)consume(TokenType::Comma, "Expected ',' after file number in PRINT#");
        if (check(TokenType::EndOfLine) || check(TokenType::Colon)) { stmt->node = std::move(s); return stmt; }
        while (true) {
            PrintItem item;
            item.expression = parse_expression();
            if (match(TokenType::Comma)) { item.separator = PrintSeparator::Comma; s.trailing_newline = false; }
            else if (match(TokenType::Semicolon)) { item.separator = PrintSeparator::Semicolon; s.trailing_newline = false; }
            else { item.separator = PrintSeparator::None; s.trailing_newline = true; }
            s.items.push_back(std::move(item));
            if (s.items.back().separator == PrintSeparator::None || check(TokenType::EndOfLine) || check(TokenType::Colon)) break;
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_input_file_statement() -> StmtPtr {
        (void)consume(TokenType::Hash, "Expected # after INPUT");
        auto stmt = std::make_unique<Statement>();
        InputFileStmt s;
        s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
        (void)consume(TokenType::Comma, "Expected ',' after file number in INPUT#");
        s.targets.push_back(parse_variable_reference());
        while (match(TokenType::Comma)) s.targets.push_back(parse_variable_reference());
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_write_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        if (match(TokenType::Hash)) {
            WriteFileStmt s;
            s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
            (void)consume(TokenType::Comma, "Expected ',' after file number in WRITE#");
            if (check(TokenType::EndOfLine) || check(TokenType::Colon)) { stmt->node = std::move(s); return stmt; }
            s.items.push_back(parse_expression());
            while (match(TokenType::Comma)) s.items.push_back(parse_expression());
            stmt->node = std::move(s);
            return stmt;
        }
        WriteStmt s;
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            s.items.push_back(parse_expression());
            while (match(TokenType::Comma)) s.items.push_back(parse_expression());
        }
        stmt->node = std::move(s);
        return stmt;
    }


    [[nodiscard]] auto parse_field_statement() -> StmtPtr {
        (void)consume(TokenType::Hash, "Expected # after FIELD");
        auto stmt = std::make_unique<Statement>();
        FieldStmt s;
        s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
        (void)consume(TokenType::Comma, "Expected ',' after FIELD file number");
        while (true) {
            const int width = std::stoi(consume_any_number("Expected field width").lexeme);
            (void)consume(TokenType::KeywordAs, "Expected AS in FIELD statement");
            s.bindings.push_back(FieldBinding{width, consume(TokenType::Identifier, "Expected field variable").lexeme});
            if (!match(TokenType::Comma)) break;
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_get_put_statement(bool is_put) -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        if (match(TokenType::Hash)) {
            const int file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
            std::optional<ExprPtr> record;
            if (match(TokenType::Comma) && !check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                record = parse_expression();
            }
            if (is_put) stmt->node = PutStmt{file_number, std::move(record)};
            else stmt->node = GetStmt{file_number, std::move(record)};
            return stmt;
        }

        if (!match(TokenType::LeftParen)) {
            throw std::runtime_error("Expected # or '(' after GET/PUT");
        }

        if (is_put) {
            GraphicsPutStmt s;
            s.x = parse_expression();
            (void)consume(TokenType::Comma, "Expected ',' after PUT x");
            s.y = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after PUT coordinates");
            (void)consume(TokenType::Comma, "Expected ',' after PUT coordinates");
            s.source = parse_variable_reference();
            if (match(TokenType::Comma)) {
                if (check(TokenType::Identifier) || check(TokenType::KeywordPset) || check(TokenType::KeywordAnd) || check(TokenType::KeywordOr)) {
                    s.mode = advance().lexeme;
                } else {
                    throw std::runtime_error("Expected graphics PUT mode after ','");
                }
            }
            stmt->node = std::move(s);
            return stmt;
        }

        GraphicsGetStmt s;
        s.x1 = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after GET x1");
        s.y1 = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after GET start point");
        (void)consume(TokenType::Minus, "Expected '-' between GET points");
        (void)consume(TokenType::LeftParen, "Expected '(' before GET end point");
        s.x2 = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after GET x2");
        s.y2 = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after GET end point");
        (void)consume(TokenType::Comma, "Expected ',' after GET region");
        s.target = parse_variable_reference();
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_line_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        if (match(TokenType::KeywordInput)) {
            if (match(TokenType::Hash)) {
                LineInputFileStmt s;
                s.file_number = std::stoi(consume_any_number("Expected file number after #").lexeme);
                (void)consume(TokenType::Comma, "Expected ',' after file number in LINE INPUT#");
                s.target = parse_variable_reference();
                stmt->node = std::move(s);
                return stmt;
            }
            LineInputStmt s;
            if (check(TokenType::String)) {
                s.prompt = advance().lexeme;
                (void)consume(TokenType::Semicolon, "Expected ';' after LINE INPUT prompt");
            }
            s.target = parse_variable_reference();
            stmt->node = std::move(s);
            return stmt;
        }

        GraphicsLineStmt s;
        (void)consume(TokenType::LeftParen, "Expected '(' after LINE");
        s.x1 = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after LINE x1");
        s.y1 = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after LINE start point");
        (void)consume(TokenType::Minus, "Expected '-' between LINE points");
        (void)consume(TokenType::LeftParen, "Expected '(' before LINE end point");
        s.x2 = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after LINE x2");
        s.y2 = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after LINE end point");
        if (match(TokenType::Comma)) {
            if (!check(TokenType::Comma) && !check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                s.color = parse_expression();
            }
            if (match(TokenType::Comma)) {
                if (check(TokenType::Identifier)) {
                    const auto mode = peek().lexeme;
                    if (mode == "B") {
                        advance();
                        s.box = true;
                    } else if (mode == "BF") {
                        advance();
                        s.box = true;
                        s.fill = true;
                    } else if (mode == "F") {
                        advance();
                        s.fill = true;
                    }
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_locate_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        LocateStmt s;
        auto parse_optional_arg = [&]() -> std::optional<ExprPtr> {
            if (check(TokenType::Comma) || check(TokenType::EndOfLine) || check(TokenType::Colon)) {
                return std::nullopt;
            }
            return parse_expression();
        };
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            s.row = parse_optional_arg();
            if (match(TokenType::Comma)) {
                s.column = parse_optional_arg();
                if (match(TokenType::Comma)) {
                    s.cursor = parse_optional_arg();
                    if (match(TokenType::Comma)) {
                        s.start = parse_optional_arg();
                        if (match(TokenType::Comma)) {
                            s.stop = parse_optional_arg();
                        }
                    }
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_color_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        ColorStmt s;
        auto parse_optional_arg = [&]() -> std::optional<ExprPtr> {
            if (check(TokenType::Comma) || check(TokenType::EndOfLine) || check(TokenType::Colon)) {
                return std::nullopt;
            }
            return parse_expression();
        };
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            s.foreground = parse_optional_arg();
            if (match(TokenType::Comma)) {
                s.background = parse_optional_arg();
                if (match(TokenType::Comma)) {
                    s.border = parse_optional_arg();
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }


    [[nodiscard]] auto parse_screen_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        ScreenStmt s;
        auto parse_optional_arg = [&]() -> std::optional<ExprPtr> {
            if (check(TokenType::Comma) || check(TokenType::EndOfLine) || check(TokenType::Colon)) {
                return std::nullopt;
            }
            return parse_expression();
        };
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            s.mode = parse_optional_arg();
            if (match(TokenType::Comma)) {
                s.color_switch = parse_optional_arg();
                if (match(TokenType::Comma)) {
                    s.active_page = parse_optional_arg();
                    if (match(TokenType::Comma)) {
                        s.visual_page = parse_optional_arg();
                    }
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_key_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        KeyStmt s;
        if (match(TokenType::KeywordOn)) {
            s.enabled = true;
        } else if (check(TokenType::Identifier) && peek().lexeme == "OFF") {
            advance();
            s.enabled = false;
        } else {
            throw std::runtime_error("Expected ON or OFF after KEY");
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_sound_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        SoundStmt s;
        s.frequency = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after SOUND frequency");
        s.duration = parse_expression();
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_play_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        PlayStmt s;
        s.sequence = parse_expression();
        stmt->node = std::move(s);
        return stmt;
    }


    [[nodiscard]] auto parse_pset_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        PsetStmt s;
        (void)consume(TokenType::LeftParen, "Expected '(' after PSET");
        s.x = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after PSET x");
        s.y = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after PSET coordinates");
        if (match(TokenType::Comma)) {
            s.color = parse_expression();
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_circle_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        CircleStmt s;
        (void)consume(TokenType::LeftParen, "Expected '(' after CIRCLE");
        s.x = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after CIRCLE x");
        s.y = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after CIRCLE center");
        (void)consume(TokenType::Comma, "Expected ',' after CIRCLE center");
        s.radius = parse_expression();
        if (match(TokenType::Comma)) {
            s.color = parse_expression();
        }
        stmt->node = std::move(s);
        return stmt;
    }


    [[nodiscard]] auto parse_paint_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        PaintStmt s;
        (void)consume(TokenType::LeftParen, "Expected '(' after PAINT");
        s.x = parse_expression();
        (void)consume(TokenType::Comma, "Expected ',' after PAINT x");
        s.y = parse_expression();
        (void)consume(TokenType::RightParen, "Expected ')' after PAINT coordinates");
        if (match(TokenType::Comma)) {
            if (!check(TokenType::Comma) && !check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                s.color = parse_expression();
            }
            if (match(TokenType::Comma)) {
                if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
                    s.border = parse_expression();
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_draw_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        DrawStmt s;
        s.commands = parse_expression();
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_view_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        ViewStmt s;
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            (void)consume(TokenType::LeftParen, "Expected '(' after VIEW");
            s.x1 = parse_expression();
            (void)consume(TokenType::Comma, "Expected ',' after VIEW x1");
            s.y1 = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after VIEW start point");
            (void)consume(TokenType::Minus, "Expected '-' between VIEW points");
            (void)consume(TokenType::LeftParen, "Expected '(' before VIEW end point");
            s.x2 = parse_expression();
            (void)consume(TokenType::Comma, "Expected ',' after VIEW x2");
            s.y2 = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after VIEW end point");
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_window_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        WindowStmt s;
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            if (match(TokenType::KeywordScreen)) {
                s.screen_coordinates = true;
            }
            (void)consume(TokenType::LeftParen, "Expected '(' after WINDOW");
            s.x1 = parse_expression();
            (void)consume(TokenType::Comma, "Expected ',' after WINDOW x1");
            s.y1 = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after WINDOW start point");
            (void)consume(TokenType::Minus, "Expected '-' between WINDOW points");
            (void)consume(TokenType::LeftParen, "Expected '(' before WINDOW end point");
            s.x2 = parse_expression();
            (void)consume(TokenType::Comma, "Expected ',' after WINDOW x2");
            s.y2 = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after WINDOW end point");
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_palette_statement() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        PaletteStmt s;
        if (!check(TokenType::EndOfLine) && !check(TokenType::Colon)) {
            if (match(TokenType::KeywordUsing)) {
                s.using_mode = true;
                s.using_source = parse_variable_reference();
            } else {
                s.attribute = parse_expression();
                if (match(TokenType::Comma)) {
                    s.color = parse_expression();
                }
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_if_statement() -> StmtPtr {
        auto condition = parse_expression();
        (void)consume(TokenType::KeywordThen, "Expected THEN in IF statement");

        if ((check(TokenType::Number) || check(TokenType::LineNumber)) &&
            (tokens_[current_ + 1].type == TokenType::EndOfLine || tokens_[current_ + 1].type == TokenType::Colon || tokens_[current_ + 1].type == TokenType::KeywordElse)) {
            auto stmt = std::make_unique<Statement>();
            IfStmt s;
            s.condition = std::move(condition);
            s.target_line = std::stoi(advance().lexeme);
            stmt->node = std::move(s);
            return stmt;
        }

        auto stmt = std::make_unique<Statement>();
        IfThenStmt s;
        s.condition = std::move(condition);
        while (!check(TokenType::EndOfLine) && !check(TokenType::KeywordElse)) {
            s.then_statements.push_back(parse_statement());
            if (!match(TokenType::Colon)) break;
        }
        if (match(TokenType::KeywordElse)) {
            while (!check(TokenType::EndOfLine)) {
                s.else_statements.push_back(parse_statement());
                if (!match(TokenType::Colon)) break;
            }
        }
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_on_statement() -> StmtPtr {
        auto selector = parse_expression();
        if (match(TokenType::KeywordGoto)) {
            auto stmt = std::make_unique<Statement>();
            OnGotoStmt s;
            s.selector = std::move(selector);
            s.targets.push_back(std::stoi(consume_any_number("Expected line number after ON ... GOTO").lexeme));
            while (match(TokenType::Comma)) {
                s.targets.push_back(std::stoi(consume_any_number("Expected line number after comma").lexeme));
            }
            stmt->node = std::move(s);
            return stmt;
        }
        if (match(TokenType::KeywordGosub)) {
            auto stmt = std::make_unique<Statement>();
            OnGosubStmt s;
            s.selector = std::move(selector);
            s.targets.push_back(std::stoi(consume_any_number("Expected line number after ON ... GOSUB").lexeme));
            while (match(TokenType::Comma)) {
                s.targets.push_back(std::stoi(consume_any_number("Expected line number after comma").lexeme));
            }
            stmt->node = std::move(s);
            return stmt;
        }
        throw std::runtime_error("Expected GOTO or GOSUB after ON expression");
    }

    [[nodiscard]] auto parse_dim_decl() -> DimDecl {
        DimDecl decl;
        decl.name = consume(TokenType::Identifier, "Expected array name after DIM").lexeme;
        (void)consume(TokenType::LeftParen, "Expected '(' after DIM name");
        decl.dimensions.push_back(parse_expression());
        while (match(TokenType::Comma)) {
            decl.dimensions.push_back(parse_expression());
        }
        (void)consume(TokenType::RightParen, "Expected ')' after DIM bounds");
        return decl;
    }

    [[nodiscard]] auto parse_def_type_ranges() -> std::vector<DefTypeRange> {
        std::vector<DefTypeRange> ranges;
        do {
            const auto start = consume(TokenType::Identifier, "Expected letter or letter range after DEF type").lexeme;
            if (start.size() != 1) {
                throw std::runtime_error("DEF type ranges must use single letters");
            }
            char end = start.front();
            if (match(TokenType::Minus)) {
                const auto finish = consume(TokenType::Identifier, "Expected range end after '-'").lexeme;
                if (finish.size() != 1) {
                    throw std::runtime_error("DEF type range end must be a single letter");
                }
                end = finish.front();
            }
            ranges.push_back(DefTypeRange{start.front(), end});
        } while (match(TokenType::Comma));
        return ranges;
    }

    [[nodiscard]] auto parse_variable_reference() -> VariableRef {
        VariableRef ref;
        ref.name = consume(TokenType::Identifier, "Expected variable name").lexeme;
        if (match(TokenType::LeftParen)) {
            ref.indices.push_back(parse_expression());
            while (match(TokenType::Comma)) {
                ref.indices.push_back(parse_expression());
            }
            (void)consume(TokenType::RightParen, "Expected ')' after array indices");
        }
        return ref;
    }

    [[nodiscard]] auto parse_assignment() -> StmtPtr {
        auto stmt = std::make_unique<Statement>();
        LetStmt s;
        s.target = parse_variable_reference();
        (void)consume(TokenType::Equal, "Expected '=' in assignment");
        s.value = parse_expression();
        stmt->node = std::move(s);
        return stmt;
    }

    [[nodiscard]] auto parse_expression() -> ExprPtr { return parse_or(); }

    [[nodiscard]] auto parse_or() -> ExprPtr {
        auto expr = parse_and();
        while (match(TokenType::KeywordOr)) {
            auto right = parse_and();
            auto out = std::make_unique<Expr>();
            out->node = BinaryExpr{std::move(expr), "OR", std::move(right)};
            expr = std::move(out);
        }
        return expr;
    }

    [[nodiscard]] auto parse_and() -> ExprPtr {
        auto expr = parse_comparison();
        while (match(TokenType::KeywordAnd)) {
            auto right = parse_comparison();
            auto out = std::make_unique<Expr>();
            out->node = BinaryExpr{std::move(expr), "AND", std::move(right)};
            expr = std::move(out);
        }
        return expr;
    }

    [[nodiscard]] auto parse_comparison() -> ExprPtr {
        auto expr = parse_term();
        while (match(TokenType::Equal) || match(TokenType::NotEqual) || match(TokenType::Less) ||
               match(TokenType::LessEqual) || match(TokenType::Greater) || match(TokenType::GreaterEqual)) {
            const auto op = previous().lexeme;
            auto right = parse_term();
            auto out = std::make_unique<Expr>();
            out->node = BinaryExpr{std::move(expr), op, std::move(right)};
            expr = std::move(out);
        }
        return expr;
    }

    [[nodiscard]] auto parse_term() -> ExprPtr {
        auto expr = parse_factor();
        while (match(TokenType::Plus) || match(TokenType::Minus)) {
            const auto op = previous().lexeme;
            auto right = parse_factor();
            auto out = std::make_unique<Expr>();
            out->node = BinaryExpr{std::move(expr), op, std::move(right)};
            expr = std::move(out);
        }
        return expr;
    }

    [[nodiscard]] auto parse_factor() -> ExprPtr {
        auto expr = parse_unary();
        while (match(TokenType::Star) || match(TokenType::Slash)) {
            const auto op = previous().lexeme;
            auto right = parse_unary();
            auto out = std::make_unique<Expr>();
            out->node = BinaryExpr{std::move(expr), op, std::move(right)};
            expr = std::move(out);
        }
        return expr;
    }

    [[nodiscard]] auto parse_unary() -> ExprPtr {
        if (match(TokenType::KeywordNot) || match(TokenType::Minus) || match(TokenType::Plus)) {
            const auto op = previous().lexeme;
            auto out = std::make_unique<Expr>();
            out->node = UnaryExpr{op, parse_unary()};
            return out;
        }
        return parse_primary();
    }

    [[nodiscard]] auto parse_primary() -> ExprPtr {
        if (match(TokenType::Number) || match(TokenType::LineNumber)) {
            auto expr = std::make_unique<Expr>();
            expr->node = NumberExpr{std::stod(previous().lexeme)};
            return expr;
        }
        if (match(TokenType::String)) {
            auto expr = std::make_unique<Expr>();
            expr->node = StringExpr{previous().lexeme};
            return expr;
        }
        if (check(TokenType::Identifier)) {
            const auto name = advance().lexeme;
            if (match(TokenType::LeftParen)) {
                std::vector<ExprPtr> args;
                if (!check(TokenType::RightParen)) {
                    args.push_back(parse_expression());
                    while (match(TokenType::Comma)) {
                        args.push_back(parse_expression());
                    }
                }
                (void)consume(TokenType::RightParen, "Expected ')' after argument list");
                auto expr = std::make_unique<Expr>();
                if (is_intrinsic_function_name(name)) {
                    FunctionCallExpr call;
                    call.name = name;
                    call.arguments = std::move(args);
                    expr->node = std::move(call);
                } else {
                    VariableRef ref;
                    ref.name = name;
                    ref.indices = std::move(args);
                    expr->node = VariableExpr{std::move(ref)};
                }
                return expr;
            }
            auto expr = std::make_unique<Expr>();
            if (is_zero_arg_intrinsic_function_name(name)) {
                FunctionCallExpr call;
                call.name = name;
                expr->node = std::move(call);
            } else {
                expr->node = VariableExpr{VariableRef{name, {}}};
            }
            return expr;
        }
        if (match(TokenType::LeftParen)) {
            auto expr = parse_expression();
            (void)consume(TokenType::RightParen, "Expected ')' after expression");
            return expr;
        }
        throw std::runtime_error("Expected expression near column " + std::to_string(peek().column));
    }

    [[nodiscard]] static auto is_intrinsic_function_name(const std::string& name) -> bool {
        return name == "ABS" || name == "INT" || name == "LEN" || name == "VAL" ||
               name == "LEFT$" || name == "RIGHT$" || name == "MID$" ||
               name == "CHR$" || name == "ASC" || name == "STR$" ||
               name == "SPC" || name == "TAB" || name == "SPACE$" ||
               name == "STRING$" || name == "INSTR" || name == "POS" || name == "EOF" || name == "LOF" || name == "LOC" || name == "INKEY$" || name == "POINT" || name == "PMAP" || name == "RND" || name == "SQR" || name == "SIN" || name == "COS" || name == "TAN" || name == "ATN";
    }

    [[nodiscard]] static auto is_zero_arg_intrinsic_function_name(const std::string& name) -> bool {
        return name == "INKEY$" || name == "RND";
    }

    [[nodiscard]] auto looks_like_assignment() const -> bool {
        if (!check(TokenType::Identifier)) {
            return false;
        }
        if (peek_next().type == TokenType::Equal) {
            return true;
        }
        if (peek_next().type != TokenType::LeftParen) {
            return false;
        }
        std::size_t depth = 0;
        for (std::size_t i = current_ + 1; i < tokens_.size(); ++i) {
            const auto type = tokens_[i].type;
            if (type == TokenType::LeftParen) {
                ++depth;
            } else if (type == TokenType::RightParen) {
                if (depth == 0) {
                    return false;
                }
                --depth;
                if (depth == 0) {
                    return i + 1 < tokens_.size() && tokens_[i + 1].type == TokenType::Equal;
                }
            }
        }
        return false;
    }

    [[nodiscard]] auto match(TokenType type) -> bool {
        if (check(type)) { ++current_; return true; }
        return false;
    }

    [[nodiscard]] auto check(TokenType type) const -> bool { return peek().type == type; }
    [[nodiscard]] auto advance() -> const Token& { return tokens_.at(current_++); }
    [[nodiscard]] auto previous() const -> const Token& { return tokens_.at(current_ - 1); }
    [[nodiscard]] auto peek() const -> const Token& { return tokens_.at(current_); }
    [[nodiscard]] auto peek_next() const -> const Token& { return current_ + 1 < tokens_.size() ? tokens_.at(current_ + 1) : tokens_.back(); }

    [[nodiscard]] auto consume(TokenType type, const std::string& message) -> const Token& {
        if (!check(type)) {
            throw std::runtime_error(message);
        }
        return advance();
    }

    [[nodiscard]] auto consume_any_number(const std::string& message) -> const Token& {
        if (check(TokenType::Number) || check(TokenType::LineNumber)) {
            return advance();
        }
        throw std::runtime_error(message);
    }

    const std::vector<Token>& tokens_;
    std::string original_;
    std::size_t current_{};
};

} // namespace

auto Parser::parse_line(const std::vector<Token>& tokens, std::string original_text) const -> ParsedLine {
    return LineParser(tokens, std::move(original_text)).parse();
}

} // namespace gwbasic
