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

struct ProgramLine {
    int number{};
    std::vector<StmtPtr> statements;
    std::string source;
};

class Program {
public:
    void store(ParsedLine line);
    void clear();
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto lines() const -> const std::map<int, ProgramLine>&;
    [[nodiscard]] auto has_line(int line) const -> bool;

private:
    std::map<int, ProgramLine> lines_;
};

struct ForFrame {
    std::string variable;
    double limit{};
    double step{};
    int line_after_for{};
    std::size_t statement_index_after_for{};
};

enum class VariableKind {
    Numeric,
    Integer,
    String
};

struct ArrayValue {
    std::vector<int> dimensions;
    std::vector<Value> elements;
    VariableKind kind{VariableKind::Numeric};
};

struct WhileFrame {
    int while_line{};
    std::size_t while_statement_index{};
    int body_line{};
    std::size_t body_statement_index{};
};

class RuntimeContext {
public:
    using Output = std::function<void(const std::string&)>;
    using Input = std::function<std::string()>;
    using KeyInput = std::function<std::optional<std::string>()>;
    using GraphicsPresenter = std::function<void(const std::vector<std::uint8_t>&, int, int, const std::array<std::uint8_t, 256>&)>;
    using EngineTick = std::function<void()>;

    explicit RuntimeContext(Output output = {}, Input input = {}, KeyInput key_input = {});

    void set_output(Output output);
    void set_input(Input input);
    void set_key_input(KeyInput key_input);
    void set_graphics_presenter(GraphicsPresenter presenter);
    void set_engine_tick(EngineTick tick);

    void print(const std::string& text) const;
    [[nodiscard]] auto read_line() const -> std::string;
    [[nodiscard]] auto read_key() const -> std::string;
    void tick_engine() const;
    void flush_graphics() const;
    void request_stop();
    void clear_stop_request();
    [[nodiscard]] bool stop_requested() const;

    void set_variable(const std::string& name, Value value);
    [[nodiscard]] auto get_variable(const std::string& name) const -> Value;
    void clear_variables();

    void dim_array(const std::string& name, std::vector<int> dimensions);
    void set_array_value(const std::string& name, const std::vector<int>& indices, Value value);
    [[nodiscard]] auto get_array_value(const std::string& name, const std::vector<int>& indices) const -> Value;

    [[nodiscard]] auto variable_kind(const std::string& name) const -> VariableKind;
    [[nodiscard]] auto is_string_variable(const std::string& name) const -> bool;
    void set_default_numeric_range(char start, char end, VariableKind kind);

    void push_return(int line, std::size_t statement_index);
    [[nodiscard]] auto pop_return() -> std::optional<std::pair<int, std::size_t>>;

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
    void remove_directory(const std::string& path);
    void locate_cursor(std::optional<int> row, std::optional<int> column, std::optional<int> cursor = std::nullopt, std::optional<int> start = std::nullopt, std::optional<int> stop = std::nullopt);
    void set_color(std::optional<int> foreground, std::optional<int> background = std::nullopt, std::optional<int> border = std::nullopt);
    void set_screen(std::optional<int> mode, std::optional<int> color_switch = std::nullopt, std::optional<int> active_page = std::nullopt, std::optional<int> visual_page = std::nullopt);
    void set_key_display(bool enabled);
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
    std::vector<std::pair<int, std::size_t>> return_stack_;
    std::vector<ForFrame> for_stack_;
    std::vector<WhileFrame> while_stack_;
    std::vector<Value> data_items_;
    std::unordered_map<int, std::size_t> data_line_index_;
    std::size_t data_cursor_{};
    std::array<VariableKind, 26> default_types_{};
    std::unordered_map<int, std::shared_ptr<void>> files_;
    mutable std::size_t current_print_column_{};
    bool stop_requested_{false};
    bool key_display_enabled_{true};
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
    std::vector<std::uint8_t> graphics_pixels_;
    std::array<std::uint8_t, 256> palette_map_{};
    std::unordered_map<std::string, std::vector<std::uint8_t>> graphics_blocks_;
    mutable bool graphics_dirty_{true};
    mutable std::size_t graphics_dirty_ops_{0};
};

} // namespace gwbasic
