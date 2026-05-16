#pragma once

#include "gwbasic/ast.hpp"
#include "gwbasic/value.hpp"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace gwbasic {

/** Stored representation of one numbered BASIC program line. */
struct ProgramLine {
    int number{};
    std::vector<StmtPtr> statements;
    std::string source;
};

/** Line-numbered program storage with ordered iteration and direct lookup. */
class Program {
public:
    /** Insert, replace, or delete a parsed numbered line. */
    void store(ParsedLine line);
    /** Remove all stored lines. */
    void clear();
    [[nodiscard]] auto empty() const -> bool;
    /** Ordered lines used for listing and sequential execution. */
    [[nodiscard]] auto lines() const -> const std::map<int, ProgramLine>&;
    /** O(1) lookup by BASIC line number. */
    [[nodiscard]] auto find_line(int line) const -> const ProgramLine*;
    [[nodiscard]] auto has_line(int line) const -> bool;

private:
    std::map<int, ProgramLine> lines_;
    std::unordered_map<int, const ProgramLine*> line_index_;
};

/** FOR/NEXT execution frame. */
struct ForFrame {
    std::string variable;
    double limit{};
    double step{};
    int line_after_for{};
    std::size_t statement_index_after_for{};
};

/** Default variable kind selected by suffixes and DEF type statements. */
enum class VariableKind {
    Numeric,
    Integer,
    String
};

/** Runtime storage for one BASIC array. */
struct ArrayValue {
    std::vector<int> lower_bounds;
    std::vector<int> dimensions;
    std::vector<Value> elements;
    VariableKind kind{VariableKind::Numeric};
};

/** WHILE/WEND execution frame. */
struct WhileFrame {
    int while_line{};
    std::size_t while_statement_index{};
    int body_line{};
    std::size_t body_statement_index{};
};

/**
 * Runtime services and mutable BASIC machine state.
 *
 * RuntimeContext owns variables, arrays, files, DATA state, graphics state,
 * input buffers, and control-flow stacks. It is deliberately callback-driven so
 * the interpreter can run in tests, CLI mode, or a platform graphics window.
 */
class RuntimeContext {
public:
    /** Text output sink used by PRINT and diagnostics-like runtime output. */
    using Output = std::function<void(const std::string&)>;
    /** Blocking line input source used by INPUT and INPUT$. */
    using Input = std::function<std::string()>;
    /** Nonblocking key source used by INKEY$. */
    using KeyInput = std::function<std::optional<std::string>()>;
    /** Indexed-color graphics presenter callback. */
    using GraphicsPresenter = std::function<void(const std::vector<std::uint8_t>&, int, int, const std::array<std::uint8_t, 256>&)>;
    /** Cooperative event/timing hook called by long-running execution. */
    using EngineTick = std::function<void()>;

    /** Construct a runtime with optional output, line input, and key input. */
    explicit RuntimeContext(Output output = {}, Input input = {}, KeyInput key_input = {});

    void set_output(Output output);
    void set_input(Input input);
    void set_key_input(KeyInput key_input);
    void set_graphics_presenter(GraphicsPresenter presenter);
    void set_engine_tick(EngineTick tick);

    /** Print text either to the output callback or the graphics text surface. */
    void print(const std::string& text) const;
    void cls();
    [[nodiscard]] auto graphics_text_mode() const -> bool;
    void render_text(const std::string& text) const;
    void draw_glyph_cell(int column, int row, char ch, int fg, int bg) const;
    [[nodiscard]] auto read_line() const -> std::string;
    [[nodiscard]] auto read_key() const -> std::string;
    [[nodiscard]] auto read_chars(std::size_t count) const -> std::string;
    void tick_engine() const;
    void flush_graphics() const;
    void request_stop();
    void clear_stop_request();
    [[nodiscard]] bool stop_requested() const;

    void set_variable(const std::string& name, Value value);
    [[nodiscard]] auto get_variable(const std::string& name) const -> Value;
    void clear_variables();

    /** Allocate a BASIC array. Dimensions are inclusive GW-BASIC bounds. */
    void dim_array(const std::string& name, std::vector<int> dimensions);
    /** Remove a BASIC array so it can be DIMed again. */
    void erase_array(const std::string& name);
    /** Set default lower bound for subsequently DIMed arrays. */
    void set_option_base(int base);
    void set_array_value(const std::string& name, const std::vector<int>& indices, Value value);
    [[nodiscard]] auto get_array_value(const std::string& name, const std::vector<int>& indices) const -> Value;

    [[nodiscard]] auto variable_kind(const std::string& name) const -> VariableKind;
    [[nodiscard]] auto is_string_variable(const std::string& name) const -> bool;
    void set_default_numeric_range(char start, char end, VariableKind kind);

    void push_return(int line, std::size_t statement_index);
    [[nodiscard]] auto pop_return() -> std::optional<std::pair<int, std::size_t>>;
    void clear_control_stacks();

    void push_for(ForFrame frame);
    [[nodiscard]] auto top_for() -> ForFrame*;
    void pop_for();

    void push_while(WhileFrame frame);
    [[nodiscard]] auto top_while() -> WhileFrame*;
    void pop_while();
    void clear_while_frames_for_line(int line, std::size_t statement_index);

    void set_data(std::vector<Value> values, std::unordered_map<int, std::size_t> line_index);
    [[nodiscard]] auto read_data() -> Value;
    void restore_data(std::optional<int> line = std::nullopt);
    [[nodiscard]] auto current_print_column() const -> std::size_t;

    void open_file(int file_number, const std::string& path, FileMode mode, std::optional<int> record_len = std::nullopt);
    void close_file(std::optional<int> file_number = std::nullopt);
    void write_file(int file_number, const std::string& text);
    void list_files(const std::string& pattern = "*") const;
    void set_field(int file_number, std::vector<std::pair<int, std::string>> bindings);
    void set_record_field(const std::string& variable, const std::string& value, bool right_align);
    void put_record(int file_number, std::optional<int> record_number = std::nullopt);
    void get_record(int file_number, std::optional<int> record_number = std::nullopt);
    [[nodiscard]] auto file_length(int file_number) -> std::size_t;
    [[nodiscard]] auto file_loc(int file_number) -> std::size_t;
    [[nodiscard]] auto read_file_record(int file_number) -> std::vector<std::string>;
    [[nodiscard]] auto read_file_line(int file_number) -> std::string;
    [[nodiscard]] auto eof_file(int file_number) -> bool;
    void delete_file(const std::string& path);
    void rename_file(const std::string& old_path, const std::string& new_path);
    void create_directory(const std::string& path);
    void change_directory(const std::string& path);
    void remove_directory(const std::string& path);
    void locate_cursor(std::optional<int> row, std::optional<int> column, std::optional<int> cursor = std::nullopt, std::optional<int> start = std::nullopt, std::optional<int> stop = std::nullopt);
    void set_text_width(int columns);
    void set_color(std::optional<int> foreground, std::optional<int> background = std::nullopt, std::optional<int> border = std::nullopt);
    void set_screen(std::optional<int> mode, std::optional<int> color_switch = std::nullopt, std::optional<int> active_page = std::nullopt, std::optional<int> visual_page = std::nullopt);
    void set_key_display(bool enabled);
    /** Write to isolated 64 KiB virtual BASIC memory, never native memory. */
    void poke(int address, int value);
    /** Read from isolated 64 KiB virtual BASIC memory. */
    [[nodiscard]] auto peek(int address) const -> double;
    void sound(std::optional<double> frequency, std::optional<double> duration);
    void play(const std::string& sequence);
    void pset(int x, int y, std::optional<int> color = std::nullopt);
    void draw_line(int x1, int y1, int x2, int y2, std::optional<int> color = std::nullopt, bool box = false, bool fill = false);
    void draw_circle(int x, int y, int radius, std::optional<int> color = std::nullopt);
    void paint(int x, int y, std::optional<int> color = std::nullopt, std::optional<int> border = std::nullopt);
    void draw_commands(const std::string& commands);
    void set_view(std::optional<int> x1, std::optional<int> y1, std::optional<int> x2, std::optional<int> y2);
    void set_window(bool screen_coordinates, std::optional<double> x1, std::optional<double> y1, std::optional<double> x2, std::optional<double> y2);
    void set_palette(std::optional<int> attribute, std::optional<int> color);
    void set_palette_using(const VariableRef& source);
    [[nodiscard]] auto pmap(double coordinate, int mode) const -> double;
    [[nodiscard]] auto map_window_x(double x) const -> int;
    [[nodiscard]] auto map_window_y(double y) const -> int;
    void get_graphics_block(const std::string& name, int x1, int y1, int x2, int y2);
    void put_graphics_block(const std::string& name, int x, int y, std::optional<std::string> mode = std::nullopt);
    [[nodiscard]] auto point(int x, int y) const -> double;

private:
    void mark_graphics_dirty(bool immediate = false) const;
    void present_graphics() const;
    [[nodiscard]] auto compute_flat_index(const ArrayValue& array, const std::vector<int>& indices) const -> std::size_t;
    [[nodiscard]] auto normalize_value_for_kind(VariableKind kind, Value value) const -> Value;
    [[nodiscard]] auto default_value_for_kind(VariableKind kind) const -> Value;

    Output output_;
    Input input_;
    KeyInput key_input_;
    GraphicsPresenter graphics_presenter_;
    EngineTick engine_tick_;
    std::unordered_map<std::string, Value> variables_;
    std::unordered_map<std::string, ArrayValue> arrays_;
    int option_base_{0};
    std::vector<std::pair<int, std::size_t>> return_stack_;
    std::vector<ForFrame> for_stack_;
    std::vector<WhileFrame> while_stack_;
    std::vector<Value> data_items_;
    std::unordered_map<int, std::size_t> data_line_index_;
    std::size_t data_cursor_{};
    std::array<VariableKind, 26> default_types_{};
    std::unordered_map<int, std::shared_ptr<void>> files_;
    mutable std::size_t current_print_column_{};
    mutable std::string input_char_buffer_;
    bool stop_requested_{false};
    bool key_display_enabled_{true};
    mutable int text_row_{1};
    mutable int text_col_{1};
    int text_width_{80};
    int text_foreground_{15};
    int text_background_{0};
    bool cursor_visible_{true};
    std::array<std::uint8_t, 65536> virtual_memory_{};
    int screen_mode_{0};
    int graphics_width_{320};
    int graphics_height_{200};
    int graphics_cursor_x_{0};
    int graphics_cursor_y_{0};
    int graphics_view_left_{0};
    int graphics_view_top_{0};
    int graphics_view_right_{319};
    int graphics_view_bottom_{199};
    int graphics_color_{15};
    bool window_active_{false};
    bool window_screen_coordinates_{false};
    double window_x1_{0.0};
    double window_y1_{0.0};
    double window_x2_{319.0};
    double window_y2_{199.0};
    mutable std::vector<std::uint8_t> graphics_pixels_;
    std::array<std::uint8_t, 256> palette_map_{};
    std::unordered_map<std::string, std::vector<std::uint8_t>> graphics_blocks_;
    mutable bool graphics_dirty_{true};
    mutable std::size_t graphics_dirty_ops_{0};
};

} // namespace gwbasic
