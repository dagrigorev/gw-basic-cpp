#include "gwbasic/runtime.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <regex>

namespace gwbasic {
namespace {
struct FileHandle {
    FileMode mode{FileMode::Input};
    std::fstream stream;
    std::size_t record_len{0};
    std::vector<std::pair<int, std::string>> field_bindings;
    std::size_t current_record{1};
};

[[nodiscard]] auto ansi_foreground_from_basic(int color) -> int {
    static const int table[16] = {30, 34, 32, 36, 31, 35, 33, 37, 90, 94, 92, 96, 91, 95, 93, 97};
    return table[std::clamp(color, 0, 15)];
}

[[nodiscard]] auto ansi_background_from_basic(int color) -> int {
    static const int table[16] = {40, 44, 42, 46, 41, 45, 43, 47, 100, 104, 102, 106, 101, 105, 103, 107};
    return table[std::clamp(color, 0, 15)];
}
[[nodiscard]] auto in_bounds(int x, int y, int width, int height) -> bool {
    return x >= 0 && y >= 0 && x < width && y < height;
}

[[nodiscard]] auto parse_int_token(const std::string& text, std::size_t& pos, int default_value = 1) -> int {
    bool negative = false;
    if (pos < text.size() && text[pos] == '-') { negative = true; ++pos; }
    int value = 0;
    bool saw_digit = false;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        saw_digit = true;
        value = value * 10 + (text[pos] - '0');
        ++pos;
    }
    if (!saw_digit) value = default_value;
    return negative ? -value : value;
}


auto split_csv_record(const std::string& line) -> std::vector<std::string> {
    std::vector<std::string> out;
    std::string current;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (ch == '"') { in_quotes = !in_quotes; continue; }
        if (ch == ',' && !in_quotes) { out.push_back(current); current.clear(); continue; }
        current.push_back(ch);
    }
    out.push_back(current);
    return out;
}

[[nodiscard]] auto wildcard_to_regex(const std::string& pattern) -> std::regex {
    std::string out = "^";
    for (char ch : pattern) {
        switch (ch) {
            case '*': out += ".*"; break;
            case '?': out += '.'; break;
            case '.': case '\\': case '+': case '^': case '$': case '(': case ')': case '[': case ']': case '{': case '}': case '|':
                out.push_back('\\');
                out.push_back(ch);
                break;
            default:
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                break;
        }
    }
    out += '$';
    return std::regex(out);
}




[[nodiscard]] auto glyph_rows_for(char ch) -> std::array<std::uint8_t, 7> {
    using A = std::array<std::uint8_t,7>;
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    switch (ch) {
        case 'A': return A{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
        case 'B': return A{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
        case 'C': return A{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
        case 'D': return A{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C};
        case 'E': return A{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
        case 'F': return A{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
        case 'G': return A{0x0E,0x11,0x10,0x10,0x13,0x11,0x0E};
        case 'H': return A{0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
        case 'I': return A{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
        case 'J': return A{0x01,0x01,0x01,0x01,0x11,0x11,0x0E};
        case 'K': return A{0x11,0x12,0x14,0x18,0x14,0x12,0x11};
        case 'L': return A{0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
        case 'M': return A{0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
        case 'N': return A{0x11,0x11,0x19,0x15,0x13,0x11,0x11};
        case 'O': return A{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
        case 'P': return A{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
        case 'Q': return A{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
        case 'R': return A{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
        case 'S': return A{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
        case 'T': return A{0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
        case 'U': return A{0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
        case 'V': return A{0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
        case 'W': return A{0x11,0x11,0x11,0x15,0x15,0x15,0x0A};
        case 'X': return A{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
        case 'Y': return A{0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
        case 'Z': return A{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
        case '0': return A{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
        case '1': return A{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
        case '2': return A{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
        case '3': return A{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E};
        case '4': return A{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
        case '5': return A{0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E};
        case '6': return A{0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E};
        case '7': return A{0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
        case '8': return A{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
        case '9': return A{0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E};
        case ':': return A{0x00,0x04,0x04,0x00,0x04,0x04,0x00};
        case '.': return A{0x00,0x00,0x00,0x00,0x00,0x06,0x06};
        case ',': return A{0x00,0x00,0x00,0x00,0x06,0x06,0x04};
        case '-': return A{0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
        case '_': return A{0x00,0x00,0x00,0x00,0x00,0x00,0x1F};
        case '/': return A{0x01,0x02,0x02,0x04,0x08,0x08,0x10};
        case '\\': return A{0x10,0x08,0x08,0x04,0x02,0x02,0x01};
        case '[': return A{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E};
        case ']': return A{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E};
        case '(': return A{0x02,0x04,0x08,0x08,0x08,0x04,0x02};
        case ')': return A{0x08,0x04,0x02,0x02,0x02,0x04,0x08};
        case '+': return A{0x00,0x04,0x04,0x1F,0x04,0x04,0x00};
        case '=': return A{0x00,0x1F,0x00,0x1F,0x00,0x00,0x00};
        case '>': return A{0x10,0x08,0x04,0x02,0x04,0x08,0x10};
        case '<': return A{0x01,0x02,0x04,0x08,0x04,0x02,0x01};
        case '!': return A{0x04,0x04,0x04,0x04,0x04,0x00,0x04};
        case '?': return A{0x0E,0x11,0x01,0x02,0x04,0x00,0x04};
        case ' ': default: return A{0,0,0,0,0,0,0};
    }
}

[[nodiscard]] auto normalize_graphics_put_mode(std::optional<std::string> mode) -> std::string {
    if (!mode.has_value()) {
        return "PSET";
    }
    std::string value = *mode;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}
}


void Program::store(ParsedLine line) {
    if (!line.line_number.has_value()) {
        throw std::runtime_error("Cannot store immediate line in program");
    }
    if (line.statements.empty()) {
        lines_.erase(*line.line_number);
        line_index_.erase(*line.line_number);
        return;
    }
    auto [it, inserted] = lines_.insert_or_assign(*line.line_number, ProgramLine{*line.line_number, std::move(line.statements), std::move(line.original_text)});
    (void)inserted;
    line_index_[*line.line_number] = &it->second;
}

void Program::clear() {
    lines_.clear();
    line_index_.clear();
}
auto Program::empty() const -> bool { return lines_.empty(); }
auto Program::lines() const -> const std::map<int, ProgramLine>& { return lines_; }
auto Program::find_line(int line) const -> const ProgramLine* {
    if (const auto it = line_index_.find(line); it != line_index_.end()) {
        return it->second;
    }
    return nullptr;
}
auto Program::has_line(int line) const -> bool { return line_index_.contains(line); }

RuntimeContext::RuntimeContext(Output output, Input input, KeyInput key_input) : output_(std::move(output)), input_(std::move(input)), key_input_(std::move(key_input)) {
    default_types_.fill(VariableKind::Numeric);
    graphics_pixels_.assign(static_cast<std::size_t>(graphics_width_ * graphics_height_), 0);
    graphics_view_right_ = graphics_width_ - 1;
    graphics_view_bottom_ = graphics_height_ - 1;
    for (std::size_t i = 0; i < palette_map_.size(); ++i) {
        palette_map_[i] = static_cast<std::uint8_t>(i);
    }
}

void RuntimeContext::set_output(Output output) { output_ = std::move(output); }
void RuntimeContext::set_input(Input input) { input_ = std::move(input); }
void RuntimeContext::set_key_input(KeyInput key_input) { key_input_ = std::move(key_input); }
void RuntimeContext::set_graphics_presenter(GraphicsPresenter presenter) { graphics_presenter_ = std::move(presenter); }
void RuntimeContext::set_engine_tick(EngineTick tick) { engine_tick_ = std::move(tick); }
void RuntimeContext::request_stop() { stop_requested_ = true; }
void RuntimeContext::clear_stop_request() { stop_requested_ = false; }
bool RuntimeContext::stop_requested() const { return stop_requested_; }

bool RuntimeContext::graphics_text_mode() const {
    return graphics_presenter_ && screen_mode_ != 0;
}

void RuntimeContext::draw_glyph_cell(int column, int row, char ch, int fg, int bg) const {
    const int px = std::max(0, (column - 1) * 8);
    const int py = std::max(0, (row - 1) * 8);
    const auto glyph = glyph_rows_for(ch);
    const auto fg_color = palette_map_[static_cast<std::size_t>(std::clamp(fg, 0, 255))];
    const auto bg_color = palette_map_[static_cast<std::size_t>(std::clamp(bg, 0, 255))];
    for (int yy = 0; yy < 8; ++yy) {
        for (int xx = 0; xx < 8; ++xx) {
            const int gx = px + xx;
            const int gy = py + yy;
            if (!in_bounds(gx, gy, graphics_width_, graphics_height_)) continue;
            std::uint8_t color = bg_color;
            if (yy >= 1 && yy <= 7 && xx >= 1 && xx <= 5) {
                const auto rowbits = glyph[static_cast<std::size_t>(yy - 1)];
                const auto mask = static_cast<std::uint8_t>(1u << (5 - xx));
                if ((rowbits & mask) != 0) color = fg_color;
            }
            graphics_pixels_[static_cast<std::size_t>(gy * graphics_width_ + gx)] = color;
        }
    }
}

void RuntimeContext::render_text(const std::string& text) const {
    const int cols = std::max(1, std::min(text_width_, graphics_width_ / 8));
    const int rows = std::max(1, graphics_height_ / 8);
    for (char ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            ++text_row_;
            text_col_ = 1;
            current_print_column_ = 0;
            continue;
        }
        if (text_col_ > cols) {
            ++text_row_;
            text_col_ = 1;
        }
        if (text_row_ > rows) text_row_ = rows;
        draw_glyph_cell(text_col_, text_row_, ch, text_foreground_, text_background_);
        ++text_col_;
        ++current_print_column_;
    }
    mark_graphics_dirty(false);
}

void RuntimeContext::cls() {
    if (graphics_text_mode()) {
        std::fill(graphics_pixels_.begin(), graphics_pixels_.end(), palette_map_[static_cast<std::size_t>(std::clamp(text_background_, 0, 255))]);
        text_row_ = 1;
        text_col_ = 1;
        current_print_column_ = 0;
        graphics_cursor_x_ = 0;
        graphics_cursor_y_ = 0;
        mark_graphics_dirty(true);
        return;
    }
    print("\x1b[0m\x1b[2J\x1b[H");
}

void RuntimeContext::print(const std::string& text) const {
    if (graphics_text_mode()) {
        render_text(text);
        return;
    }
    for (char ch : text) {
        if (ch == '\n' || ch == '\r') {
            current_print_column_ = 0;
        } else {
            ++current_print_column_;
        }
    }
    if (output_) {
        output_(text);
    }
}

auto RuntimeContext::read_line() const -> std::string {
    return input_ ? input_() : std::string{};
}

auto RuntimeContext::read_chars(std::size_t count) const -> std::string {
    std::string out;
    out.reserve(count);
    while (out.size() < count) {
        if (input_char_buffer_.empty()) {
            input_char_buffer_ = read_line();
            if (input_char_buffer_.empty()) {
                break;
            }
        }
        const auto take = std::min(count - out.size(), input_char_buffer_.size());
        out.append(input_char_buffer_, 0, take);
        input_char_buffer_.erase(0, take);
    }
    return out;
}

void RuntimeContext::tick_engine() const {
    flush_graphics();
    if (engine_tick_) {
        engine_tick_();
    }
}

void RuntimeContext::flush_graphics() const {
    if (graphics_dirty_ && screen_mode_ != 0) {
        present_graphics();
    } else if (screen_mode_ == 0) {
        graphics_dirty_ = false;
        graphics_dirty_ops_ = 0;
    }
}

auto RuntimeContext::read_key() const -> std::string {
    if (key_input_) {
        if (const auto value = key_input_(); value.has_value()) {
            return *value;
        }
        return std::string{};
    }
    const auto line = read_line();
    if (line.empty()) {
        return std::string{};
    }
    return std::string(1, line.front());
}

void RuntimeContext::mark_graphics_dirty(bool immediate) const {
    graphics_dirty_ = true;
    if (!graphics_presenter_ || screen_mode_ == 0) {
        return;
    }
    if (immediate || ++graphics_dirty_ops_ >= 2048U) {
        present_graphics();
    }
}

void RuntimeContext::present_graphics() const {
    if (graphics_presenter_ && screen_mode_ != 0) {
        graphics_presenter_(graphics_pixels_, graphics_width_, graphics_height_, palette_map_);
    }
    graphics_dirty_ = false;
    graphics_dirty_ops_ = 0;
}

auto RuntimeContext::variable_kind(const std::string& name) const -> VariableKind {
    if (name.ends_with('$')) {
        return VariableKind::String;
    }
    const auto it = std::find_if(name.begin(), name.end(), [](unsigned char ch) { return std::isalpha(ch); });
    if (it == name.end()) {
        return VariableKind::Numeric;
    }
    const auto index = static_cast<std::size_t>(std::toupper(static_cast<unsigned char>(*it)) - 'A');
    if (index >= default_types_.size()) {
        return VariableKind::Numeric;
    }
    return default_types_[index];
}

auto RuntimeContext::is_string_variable(const std::string& name) const -> bool {
    return variable_kind(name) == VariableKind::String;
}

void RuntimeContext::set_default_numeric_range(char start, char end, VariableKind kind) {
    auto normalize = [](char ch) -> std::size_t {
        const auto up = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(ch)));
        if (up < 'A' || up > 'Z') {
            throw std::runtime_error("DEF type range must use A-Z");
        }
        return static_cast<std::size_t>(up - 'A');
    };

    auto lhs = normalize(start);
    auto rhs = normalize(end);
    if (lhs > rhs) {
        std::swap(lhs, rhs);
    }
    for (std::size_t i = lhs; i <= rhs; ++i) {
        default_types_[i] = kind;
    }
}

auto RuntimeContext::default_value_for_kind(VariableKind kind) const -> Value {
    switch (kind) {
        case VariableKind::String: return Value{std::string{}};
        case VariableKind::Integer: return Value{0.0};
        case VariableKind::Numeric: return Value{0.0};
    }
    return Value{0.0};
}

auto RuntimeContext::normalize_value_for_kind(VariableKind kind, Value value) const -> Value {
    switch (kind) {
        case VariableKind::String:
            return Value{value.as_string()};
        case VariableKind::Integer:
            return Value{static_cast<double>(std::llround(std::trunc(value.is_string() ? std::stod(value.as_string()) : value.as_number())))};
        case VariableKind::Numeric:
            if (value.is_string()) {
                return Value{std::stod(value.as_string())};
            }
            return value;
    }
    return value;
}

void RuntimeContext::set_variable(const std::string& name, Value value) {
    variables_[name] = normalize_value_for_kind(variable_kind(name), std::move(value));
}

auto RuntimeContext::get_variable(const std::string& name) const -> Value {
    if (const auto it = variables_.find(name); it != variables_.end()) {
        return it->second;
    }
    return default_value_for_kind(variable_kind(name));
}

void RuntimeContext::clear_variables() {
    variables_.clear();
    arrays_.clear();
    option_base_ = 0;
    return_stack_.clear();
    for_stack_.clear();
    while_stack_.clear();
    current_print_column_ = 0;
    input_char_buffer_.clear();
    std::fill(graphics_pixels_.begin(), graphics_pixels_.end(), std::uint8_t{0});
    graphics_cursor_x_ = 0;
    graphics_cursor_y_ = 0;
    graphics_color_ = 15;
    graphics_view_left_ = 0;
    graphics_view_top_ = 0;
    graphics_view_right_ = graphics_width_ - 1;
    graphics_view_bottom_ = graphics_height_ - 1;
    text_row_ = 1;
    text_col_ = 1;
    text_width_ = 80;
    text_foreground_ = 15;
    text_background_ = 0;
    cursor_visible_ = true;
    virtual_memory_.fill(std::uint8_t{0});
    mark_graphics_dirty(true);
}

void RuntimeContext::dim_array(const std::string& name, std::vector<int> dimensions) {
    if (dimensions.empty()) {
        throw std::runtime_error("DIM requires at least one dimension");
    }
    std::size_t count = 1;
    std::vector<int> lower_bounds(dimensions.size(), option_base_);
    for (int& dim : dimensions) {
        if (dim < option_base_) {
            throw std::runtime_error("DIM upper bound is below OPTION BASE");
        }
        if (dim == std::numeric_limits<int>::max()) {
            throw std::runtime_error("DIM dimension is too large");
        }
        const auto extent = static_cast<std::size_t>(dim - option_base_ + 1);
        if (count > std::numeric_limits<std::size_t>::max() / extent) {
            throw std::runtime_error("DIM dimensions are too large");
        }
        count *= extent;
        dim = static_cast<int>(extent);
    }
    ArrayValue array;
    array.lower_bounds = std::move(lower_bounds);
    array.dimensions = std::move(dimensions);
    array.kind = variable_kind(name);
    array.elements.assign(count, default_value_for_kind(array.kind));
    arrays_[name] = std::move(array);
}

void RuntimeContext::erase_array(const std::string& name) {
    arrays_.erase(name);
}

void RuntimeContext::set_option_base(int base) {
    if (base != 0 && base != 1) {
        throw std::runtime_error("OPTION BASE must be 0 or 1");
    }
    option_base_ = base;
}

auto RuntimeContext::compute_flat_index(const ArrayValue& array, const std::vector<int>& indices) const -> std::size_t {
    if (indices.size() != array.dimensions.size()) {
        throw std::runtime_error("Wrong number of array indices");
    }
    std::size_t flat = 0;
    std::size_t stride = 1;
    for (std::size_t rev = indices.size(); rev-- > 0;) {
        const int lower = rev < array.lower_bounds.size() ? array.lower_bounds[rev] : 0;
        const int idx = indices[rev] - lower;
        const int dim = array.dimensions[rev];
        if (idx < 0 || idx >= dim) {
            throw std::runtime_error("Array index out of bounds");
        }
        flat += static_cast<std::size_t>(idx) * stride;
        const auto extent = static_cast<std::size_t>(dim);
        if (extent != 0 && stride > std::numeric_limits<std::size_t>::max() / extent) {
            throw std::runtime_error("Array index overflow");
        }
        stride *= extent;
    }
    return flat;
}

void RuntimeContext::set_array_value(const std::string& name, const std::vector<int>& indices, Value value) {
    auto it = arrays_.find(name);
    if (it == arrays_.end()) {
        std::vector<int> dims;
        dims.reserve(indices.size());
        for (int index : indices) {
            if (index < option_base_) {
                throw std::runtime_error("Array index is below OPTION BASE");
            }
            dims.push_back(index);
        }
        dim_array(name, std::move(dims));
        it = arrays_.find(name);
    }
    auto& array = it->second;
    const auto flat = compute_flat_index(array, indices);
    array.elements[flat] = normalize_value_for_kind(array.kind, std::move(value));
}

auto RuntimeContext::get_array_value(const std::string& name, const std::vector<int>& indices) const -> Value {
    if (const auto it = arrays_.find(name); it != arrays_.end()) {
        return it->second.elements[compute_flat_index(it->second, indices)];
    }
    return default_value_for_kind(variable_kind(name));
}

void RuntimeContext::push_return(int line, std::size_t statement_index) {
    return_stack_.emplace_back(line, statement_index);
}

auto RuntimeContext::pop_return() -> std::optional<std::pair<int, std::size_t>> {
    if (return_stack_.empty()) {
        return std::nullopt;
    }
    auto value = return_stack_.back();
    return_stack_.pop_back();
    return value;
}

void RuntimeContext::clear_control_stacks() {
    return_stack_.clear();
    for_stack_.clear();
    while_stack_.clear();
}

void RuntimeContext::push_for(ForFrame frame) { for_stack_.push_back(std::move(frame)); }
auto RuntimeContext::top_for() -> ForFrame* { return for_stack_.empty() ? nullptr : &for_stack_.back(); }
void RuntimeContext::pop_for() { if (!for_stack_.empty()) { for_stack_.pop_back(); } }

void RuntimeContext::push_while(WhileFrame frame) { while_stack_.push_back(std::move(frame)); }
auto RuntimeContext::top_while() -> WhileFrame* { return while_stack_.empty() ? nullptr : &while_stack_.back(); }
void RuntimeContext::pop_while() { if (!while_stack_.empty()) { while_stack_.pop_back(); } }
void RuntimeContext::clear_while_frames_for_line(int line, std::size_t statement_index) {
    while (!while_stack_.empty()) {
        const auto& frame = while_stack_.back();
        if (frame.while_line == line && frame.while_statement_index == statement_index) {
            while_stack_.pop_back();
            continue;
        }
        break;
    }
}

void RuntimeContext::set_data(std::vector<Value> values, std::unordered_map<int, std::size_t> line_index) {
    data_items_ = std::move(values);
    data_line_index_ = std::move(line_index);
    data_cursor_ = 0;
}

auto RuntimeContext::read_data() -> Value {
    if (data_cursor_ >= data_items_.size()) {
        throw std::runtime_error("Out of DATA");
    }
    return data_items_[data_cursor_++];
}

void RuntimeContext::restore_data(std::optional<int> line) {
    if (!line.has_value()) {
        data_cursor_ = 0;
        return;
    }
    if (const auto it = data_line_index_.find(*line); it != data_line_index_.end()) {
        data_cursor_ = it->second;
        return;
    }
    throw std::runtime_error("RESTORE target has no DATA line");
}

auto RuntimeContext::current_print_column() const -> std::size_t { return current_print_column_; }


void RuntimeContext::open_file(int file_number, const std::string& path, FileMode mode, std::optional<int> record_len) {
    close_file(file_number);
    auto handle = std::make_shared<FileHandle>();
    handle->mode = mode;
    std::ios::openmode open_mode = std::ios::binary;
    if (mode == FileMode::Input) open_mode |= std::ios::in;
    else if (mode == FileMode::Output) open_mode |= std::ios::out | std::ios::trunc;
    else if (mode == FileMode::Append) open_mode |= std::ios::out | std::ios::app;
    else open_mode |= std::ios::in | std::ios::out;

    if (mode == FileMode::Random) {
        handle->record_len = static_cast<std::size_t>(record_len.value_or(128));
        handle->stream.open(path, open_mode);
        if (!handle->stream.is_open()) {
            std::ofstream create(path, std::ios::binary | std::ios::app);
            create.close();
            handle->stream.open(path, open_mode);
        }
    } else {
        handle->stream.open(path, open_mode);
    }
    if (!handle->stream.is_open()) throw std::runtime_error("Failed to open file: " + path);
    files_[file_number] = std::static_pointer_cast<void>(handle);
}

void RuntimeContext::close_file(std::optional<int> file_number) {
    if (!file_number.has_value()) {
        for (auto& [num, opaque] : files_) {
            auto handle = std::static_pointer_cast<FileHandle>(opaque);
            if (handle->stream.is_open()) handle->stream.close();
        }
        files_.clear();
        return;
    }
    auto it = files_.find(*file_number);
    if (it != files_.end()) {
        auto handle = std::static_pointer_cast<FileHandle>(it->second);
        if (handle->stream.is_open()) handle->stream.close();
        files_.erase(it);
    }
}

void RuntimeContext::write_file(int file_number, const std::string& text) {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode == FileMode::Input) throw std::runtime_error("File not open for output");
    handle->stream << text;
    handle->stream.flush();
}

void RuntimeContext::list_files(const std::string& pattern) const {
    const auto matcher = wildcard_to_regex(pattern.empty() ? "*" : pattern);
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path(ec), ec)) {
        if (ec) {
            throw std::runtime_error("Failed to list files");
        }
        const auto name = entry.path().filename().string();
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        if (std::regex_match(upper, matcher)) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        print(name + "\n");
    }
}

void RuntimeContext::set_field(int file_number, std::vector<std::pair<int, std::string>> bindings) {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode != FileMode::Random) throw std::runtime_error("FIELD requires a RANDOM file");
    std::size_t total = 0;
    for (const auto& [w, _] : bindings) {
        if (w <= 0) throw std::runtime_error("FIELD width must be positive");
        total += static_cast<std::size_t>(w);
    }
    if (total > handle->record_len) throw std::runtime_error("FIELD definitions exceed record length");
    handle->field_bindings = std::move(bindings);
    for (const auto& [width, variable] : handle->field_bindings) {
        set_variable(variable, Value{std::string(static_cast<std::size_t>(width), ' ')});
    }
}

void RuntimeContext::set_record_field(const std::string& variable, const std::string& value, bool right_align) {
    for (auto& [_, opaque] : files_) {
        auto handle = std::static_pointer_cast<FileHandle>(opaque);
        if (handle->mode != FileMode::Random) continue;
        for (const auto& [width, name] : handle->field_bindings) {
            if (name == variable) {
                std::string text = value;
                if (text.size() > static_cast<std::size_t>(width)) {
                    text = right_align ? text.substr(text.size() - static_cast<std::size_t>(width)) : text.substr(0, static_cast<std::size_t>(width));
                }
                if (text.size() < static_cast<std::size_t>(width)) {
                    const std::size_t pad = static_cast<std::size_t>(width) - text.size();
                    if (right_align) text = std::string(pad, ' ') + text;
                    else text += std::string(pad, ' ');
                }
                variables_[variable] = Value{text};
                return;
            }
        }
    }
    throw std::runtime_error("LSET/RSET target is not bound by FIELD: " + variable);
}

void RuntimeContext::put_record(int file_number, std::optional<int> record_number) {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode != FileMode::Random) throw std::runtime_error("PUT requires a RANDOM file");
    const std::size_t record = static_cast<std::size_t>(record_number.value_or(static_cast<int>(handle->current_record)));
    if (record < 1) throw std::runtime_error("PUT record number must be >= 1");
    std::string buffer(handle->record_len, ' ');
    std::size_t offset = 0;
    for (const auto& [width, variable] : handle->field_bindings) {
        auto field = get_variable(variable).as_string();
        if (field.size() < static_cast<std::size_t>(width)) field += std::string(static_cast<std::size_t>(width) - field.size(), ' ');
        else if (field.size() > static_cast<std::size_t>(width)) field = field.substr(0, static_cast<std::size_t>(width));
        buffer.replace(offset, static_cast<std::size_t>(width), field);
        offset += static_cast<std::size_t>(width);
    }
    handle->stream.clear();
    handle->stream.seekp(static_cast<std::streamoff>((record - 1) * handle->record_len), std::ios::beg);
    handle->stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    handle->stream.flush();
    handle->current_record = record + 1;
}

void RuntimeContext::get_record(int file_number, std::optional<int> record_number) {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode != FileMode::Random) throw std::runtime_error("GET requires a RANDOM file");
    const std::size_t record = static_cast<std::size_t>(record_number.value_or(static_cast<int>(handle->current_record)));
    if (record < 1) throw std::runtime_error("GET record number must be >= 1");
    std::string buffer(handle->record_len, ' ');
    handle->stream.clear();
    handle->stream.seekg(static_cast<std::streamoff>((record - 1) * handle->record_len), std::ios::beg);
    handle->stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    auto bytes = static_cast<std::size_t>(handle->stream.gcount());
    if (bytes == 0) throw std::runtime_error("GET past end of file");
    if (bytes < buffer.size()) std::fill(buffer.begin() + static_cast<std::ptrdiff_t>(bytes), buffer.end(), ' ');
    std::size_t offset = 0;
    for (const auto& [width, variable] : handle->field_bindings) {
        variables_[variable] = Value{buffer.substr(offset, static_cast<std::size_t>(width))};
        offset += static_cast<std::size_t>(width);
    }
    handle->current_record = record + 1;
}

auto RuntimeContext::file_length(int file_number) -> std::size_t {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    handle->stream.clear();
    const auto current = handle->stream.tellg();
    handle->stream.seekg(0, std::ios::end);
    const auto end = handle->stream.tellg();
    if (current != std::streampos(-1)) handle->stream.seekg(current, std::ios::beg);
    return end == std::streampos(-1) ? 0u : static_cast<std::size_t>(end);
}

auto RuntimeContext::file_loc(int file_number) -> std::size_t {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode == FileMode::Random) return handle->current_record;
    const auto pos = handle->stream.tellg();
    if (pos == std::streampos(-1)) return 0;
    return static_cast<std::size_t>(pos);
}

auto RuntimeContext::read_file_record(int file_number) -> std::vector<std::string> {
    return split_csv_record(read_file_line(file_number));
}

auto RuntimeContext::read_file_line(int file_number) -> std::string {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode != FileMode::Input) throw std::runtime_error("File not open for input");
    std::string line;
    if (!std::getline(handle->stream, line)) throw std::runtime_error("End of file");
    return line;
}

auto RuntimeContext::eof_file(int file_number) -> bool {
    auto it = files_.find(file_number);
    if (it == files_.end()) throw std::runtime_error("File not open: #" + std::to_string(file_number));
    auto handle = std::static_pointer_cast<FileHandle>(it->second);
    if (handle->mode != FileMode::Input) throw std::runtime_error("File not open for input");
    return handle->stream.peek() == std::char_traits<char>::eof();
}

void RuntimeContext::delete_file(const std::string& path) {
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) throw std::runtime_error("Failed to delete file: " + path);
}

void RuntimeContext::rename_file(const std::string& old_path, const std::string& new_path) {
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    if (ec) throw std::runtime_error("Failed to rename file");
}

void RuntimeContext::create_directory(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::create_directory(path, ec) && ec) throw std::runtime_error("Failed to create directory: " + path);
}

void RuntimeContext::change_directory(const std::string& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (ec) throw std::runtime_error("Failed to change directory: " + path);
}

void RuntimeContext::remove_directory(const std::string& path) {
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) throw std::runtime_error("Failed to remove directory: " + path);
}

void RuntimeContext::locate_cursor(std::optional<int> row, std::optional<int> column, std::optional<int> cursor, std::optional<int> start, std::optional<int> stop) {
    (void)start;
    (void)stop;
    if (graphics_text_mode()) {
        if (row.has_value()) text_row_ = std::max(1, *row);
        if (column.has_value()) text_col_ = std::max(1, *column);
        if (cursor.has_value()) cursor_visible_ = (*cursor != 0);
        current_print_column_ = static_cast<std::size_t>(std::max(0, text_col_ - 1));
        return;
    }
    if (row.has_value() || column.has_value()) {
        const int ansi_row = std::max(1, row.value_or(1));
        const int ansi_col = std::max(1, column.value_or(1));
        print("[" + std::to_string(ansi_row) + ";" + std::to_string(ansi_col) + "H");
    }
    if (cursor.has_value()) {
        print(cursor.value() == 0 ? "[?25l" : "[?25h");
    }
}

void RuntimeContext::set_text_width(int columns) {
    if (columns <= 0) {
        throw std::runtime_error("WIDTH must be positive");
    }
    text_width_ = std::clamp(columns, 1, 255);
}

void RuntimeContext::set_color(std::optional<int> foreground, std::optional<int> background, std::optional<int> border) {
    (void)border;
    if (graphics_text_mode()) {
        if (!foreground.has_value() && !background.has_value()) {
            text_foreground_ = 15;
            text_background_ = 0;
            return;
        }
        if (foreground.has_value()) text_foreground_ = std::clamp(*foreground, 0, 255);
        if (background.has_value()) text_background_ = std::clamp(*background, 0, 255);
        return;
    }
    if (!foreground.has_value() && !background.has_value()) {
        print("[0m");
        return;
    }
    std::string sequence = "[";
    bool first = true;
    if (foreground.has_value()) {
        sequence += std::to_string(ansi_foreground_from_basic(*foreground));
        first = false;
    }
    if (background.has_value()) {
        if (!first) sequence += ';';
        sequence += std::to_string(ansi_background_from_basic(*background));
    }
    sequence += 'm';
    print(sequence);
}

void RuntimeContext::set_screen(std::optional<int> mode, std::optional<int> color_switch, std::optional<int> active_page, std::optional<int> visual_page) {
    (void)color_switch;
    (void)active_page;
    (void)visual_page;
    screen_mode_ = mode.value_or(screen_mode_);
    switch (screen_mode_) {
        case 1: graphics_width_ = 320; graphics_height_ = 200; break;
        case 2: graphics_width_ = 640; graphics_height_ = 200; break;
        default: graphics_width_ = 320; graphics_height_ = 200; break;
    }
    graphics_pixels_.assign(static_cast<std::size_t>(graphics_width_ * graphics_height_), 0);
    graphics_cursor_x_ = 0;
    graphics_cursor_y_ = 0;
    graphics_color_ = 15;
    graphics_view_left_ = 0;
    graphics_view_top_ = 0;
    graphics_view_right_ = graphics_width_ - 1;
    graphics_view_bottom_ = graphics_height_ - 1;
    window_active_ = false;
    window_screen_coordinates_ = false;
    window_x1_ = 0.0;
    window_y1_ = 0.0;
    window_x2_ = static_cast<double>(graphics_width_ - 1);
    window_y2_ = static_cast<double>(graphics_height_ - 1);
    for (std::size_t i = 0; i < palette_map_.size(); ++i) { palette_map_[i] = static_cast<std::uint8_t>(i); }
    text_row_ = 1;
    text_col_ = 1;
    text_width_ = 80;
    text_foreground_ = 15;
    text_background_ = 0;
    cursor_visible_ = true;
    cls();
    mark_graphics_dirty(true);
}

void RuntimeContext::set_key_display(bool enabled) {
    key_display_enabled_ = enabled;
}

void RuntimeContext::poke(int address, int value) {
    if (address < 0 || address >= static_cast<int>(virtual_memory_.size())) {
        throw std::runtime_error("POKE address out of range");
    }
    virtual_memory_[static_cast<std::size_t>(address)] = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

auto RuntimeContext::peek(int address) const -> double {
    if (address < 0 || address >= static_cast<int>(virtual_memory_.size())) {
        throw std::runtime_error("PEEK address out of range");
    }
    return static_cast<double>(virtual_memory_[static_cast<std::size_t>(address)]);
}

void RuntimeContext::sound(std::optional<double> frequency, std::optional<double> duration) {
    (void)frequency;
    (void)duration;
    print("");
}

void RuntimeContext::play(const std::string& sequence) {
    std::size_t notes = 0;
    for (char ch : sequence) {
        switch (std::toupper(static_cast<unsigned char>(ch))) {
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
            case 'P':
                ++notes;
                break;
            default:
                break;
        }
    }
    if (notes == 0) {
        print("");
        return;
    }
    print(std::string(notes, ''));
}


void RuntimeContext::pset(int x, int y, std::optional<int> color) {
    if (!in_bounds(x, y, graphics_width_, graphics_height_)) {
        return;
    }
    if (x < graphics_view_left_ || x > graphics_view_right_ || y < graphics_view_top_ || y > graphics_view_bottom_) {
        return;
    }
    const int requested = std::clamp(color.value_or(graphics_color_), 0, 255);
    graphics_color_ = requested;
    const auto resolved = palette_map_[static_cast<std::size_t>(requested)];
    graphics_cursor_x_ = x;
    graphics_cursor_y_ = y;
    graphics_pixels_[static_cast<std::size_t>(y * graphics_width_ + x)] = resolved;
    mark_graphics_dirty(false);
}

void RuntimeContext::draw_line(int x1, int y1, int x2, int y2, std::optional<int> color, bool box, bool fill) {
    const int c = std::clamp(color.value_or(15), 0, 255);
    auto draw_segment = [&](int ax, int ay, int bx, int by) {
        int dx = std::abs(bx - ax);
        int sx = ax < bx ? 1 : -1;
        int dy = -std::abs(by - ay);
        int sy = ay < by ? 1 : -1;
        int err = dx + dy;
        while (true) {
            pset(ax, ay, c);
            if (ax == bx && ay == by) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; ax += sx; }
            if (e2 <= dx) { err += dx; ay += sy; }
        }
    };

    if (box || fill) {
        const int left = std::min(x1, x2);
        const int right = std::max(x1, x2);
        const int top = std::min(y1, y2);
        const int bottom = std::max(y1, y2);
        if (fill) {
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    pset(x, y, c);
                }
            }
        } else {
            draw_segment(left, top, right, top);
            draw_segment(right, top, right, bottom);
            draw_segment(right, bottom, left, bottom);
            draw_segment(left, bottom, left, top);
        }
        return;
    }

    draw_segment(x1, y1, x2, y2);
}

void RuntimeContext::draw_circle(int x, int y, int radius, std::optional<int> color) {
    if (radius < 0) {
        return;
    }
    const int c = std::clamp(color.value_or(15), 0, 255);
    int cx = radius;
    int cy = 0;
    int err = 0;
    while (cx >= cy) {
        pset(x + cx, y + cy, c);
        pset(x + cy, y + cx, c);
        pset(x - cy, y + cx, c);
        pset(x - cx, y + cy, c);
        pset(x - cx, y - cy, c);
        pset(x - cy, y - cx, c);
        pset(x + cy, y - cx, c);
        pset(x + cx, y - cy, c);
        ++cy;
        if (err <= 0) {
            err += 2 * cy + 1;
        }
        if (err > 0) {
            --cx;
            err -= 2 * cx + 1;
        }
    }
}


void RuntimeContext::paint(int x, int y, std::optional<int> color, std::optional<int> border) {
    if (!in_bounds(x, y, graphics_width_, graphics_height_)) {
        return;
    }
    if (x < graphics_view_left_ || x > graphics_view_right_ || y < graphics_view_top_ || y > graphics_view_bottom_) {
        return;
    }
    const auto target = graphics_pixels_[static_cast<std::size_t>(y * graphics_width_ + x)];
    const auto fill_index = std::clamp(color.value_or(graphics_color_), 0, 255);
    const auto fill = palette_map_[static_cast<std::size_t>(fill_index)];
    const auto border_index = std::clamp(border.value_or(fill_index), 0, 255);
    const auto border_color = palette_map_[static_cast<std::size_t>(border_index)];
    if (target == fill || target == border_color) {
        return;
    }
    std::vector<std::pair<int,int>> stack{{x,y}};
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        if (!in_bounds(cx, cy, graphics_width_, graphics_height_)) continue;
        if (cx < graphics_view_left_ || cx > graphics_view_right_ || cy < graphics_view_top_ || cy > graphics_view_bottom_) continue;
        auto &pixel = graphics_pixels_[static_cast<std::size_t>(cy * graphics_width_ + cx)];
        if (pixel != target || pixel == border_color) continue;
        pixel = fill;
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
    graphics_color_ = fill;
    graphics_cursor_x_ = x;
    graphics_cursor_y_ = y;
    mark_graphics_dirty(false);
}

void RuntimeContext::draw_commands(const std::string& commands) {
    int x = graphics_cursor_x_;
    int y = graphics_cursor_y_;
    bool pen_down = true;
    std::size_t i = 0;
    while (i < commands.size()) {
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(commands[i])));
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == ';' || ch == ',') { ++i; continue; }
        ++i;
        switch (ch) {
            case 'B': pen_down = false; continue;
            case 'N': pen_down = true; continue;
            case 'C': {
                graphics_color_ = std::clamp(parse_int_token(commands, i, graphics_color_), 0, 255);
                continue;
            }
            case 'M': {
                int nx = parse_int_token(commands, i, x);
                if (i < commands.size() && commands[i] == ',') ++i;
                int ny = parse_int_token(commands, i, y);
                if (pen_down) draw_line(x, y, nx, ny, graphics_color_);
                x = nx; y = ny;
                graphics_cursor_x_ = x; graphics_cursor_y_ = y;
                pen_down = true;
                continue;
            }
            default: break;
        }
        int length = parse_int_token(commands, i, 1);
        int dx = 0, dy = 0;
        switch (ch) {
            case 'U': dy = -length; break;
            case 'D': dy = length; break;
            case 'L': dx = -length; break;
            case 'R': dx = length; break;
            case 'E': dx = length; dy = -length; break;
            case 'F': dx = length; dy = length; break;
            case 'G': dx = -length; dy = length; break;
            case 'H': dx = -length; dy = -length; break;
            default: continue;
        }
        const int nx = x + dx;
        const int ny = y + dy;
        if (pen_down) draw_line(x, y, nx, ny, graphics_color_);
        x = nx; y = ny;
        graphics_cursor_x_ = x; graphics_cursor_y_ = y;
        pen_down = true;
    }
}

void RuntimeContext::get_graphics_block(const std::string& name, int x1, int y1, int x2, int y2) {
    const int left = std::max(0, std::min(x1, x2));
    const int top = std::max(0, std::min(y1, y2));
    const int right = std::min(graphics_width_ - 1, std::max(x1, x2));
    const int bottom = std::min(graphics_height_ - 1, std::max(y1, y2));
    if (left > right || top > bottom) {
        graphics_blocks_[name] = {};
        return;
    }
    const int width = right - left + 1;
    const int height = bottom - top + 1;
    std::vector<std::uint8_t> block;
    block.reserve(static_cast<std::size_t>(2 + width * height));
    block.push_back(static_cast<std::uint8_t>(std::clamp(width, 0, 255)));
    block.push_back(static_cast<std::uint8_t>(std::clamp(height, 0, 255)));
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            block.push_back(graphics_pixels_[static_cast<std::size_t>(y * graphics_width_ + x)]);
        }
    }
    graphics_blocks_[name] = std::move(block);
}

void RuntimeContext::put_graphics_block(const std::string& name, int x, int y, std::optional<std::string> mode) {
    const auto it = graphics_blocks_.find(name);
    if (it == graphics_blocks_.end() || it->second.size() < 2) {
        throw std::runtime_error("Graphics block not found: " + name);
    }
    const auto& block = it->second;
    const int width = static_cast<int>(block[0]);
    const int height = static_cast<int>(block[1]);
    const std::string put_mode = normalize_graphics_put_mode(std::move(mode));
    std::size_t index = 2;
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            if (index >= block.size()) {
                return;
            }
            const int px = x + dx;
            const int py = y + dy;
            const auto src = block[index++];
            if (!in_bounds(px, py, graphics_width_, graphics_height_)) {
                continue;
            }
            if (px < graphics_view_left_ || px > graphics_view_right_ || py < graphics_view_top_ || py > graphics_view_bottom_) {
                continue;
            }
            auto& dest = graphics_pixels_[static_cast<std::size_t>(py * graphics_width_ + px)];
            if (put_mode == "PSET") {
                dest = src;
            } else if (put_mode == "PRESET") {
                if (src != 0) {
                    dest = 0;
                }
            } else if (put_mode == "AND") {
                dest = static_cast<std::uint8_t>(dest & src);
            } else if (put_mode == "OR") {
                dest = static_cast<std::uint8_t>(dest | src);
            } else if (put_mode == "XOR") {
                dest = static_cast<std::uint8_t>(dest ^ src);
            } else {
                throw std::runtime_error("Unsupported graphics PUT mode: " + put_mode);
            }
        }
    }
    mark_graphics_dirty(true);
}

void RuntimeContext::set_window(bool screen_coordinates, std::optional<double> x1, std::optional<double> y1, std::optional<double> x2, std::optional<double> y2) {
    if (!x1.has_value() || !y1.has_value() || !x2.has_value() || !y2.has_value()) {
        window_active_ = false;
        window_screen_coordinates_ = false;
        window_x1_ = 0.0;
        window_y1_ = 0.0;
        window_x2_ = static_cast<double>(graphics_width_ - 1);
        window_y2_ = static_cast<double>(graphics_height_ - 1);
        mark_graphics_dirty(true);
        return;
    }
    window_active_ = true;
    window_screen_coordinates_ = screen_coordinates;
    window_x1_ = *x1;
    window_y1_ = *y1;
    window_x2_ = *x2;
    window_y2_ = *y2;
    mark_graphics_dirty(true);
}

void RuntimeContext::set_palette(std::optional<int> attribute, std::optional<int> color) {
    if (!attribute.has_value()) {
        for (std::size_t i = 0; i < palette_map_.size(); ++i) palette_map_[i] = static_cast<std::uint8_t>(i);
        mark_graphics_dirty(true);
        return;
    }
    const auto index = static_cast<std::size_t>(std::clamp(*attribute, 0, 255));
    palette_map_[index] = static_cast<std::uint8_t>(std::clamp(color.value_or(*attribute), 0, 255));
    mark_graphics_dirty(true);
}

void RuntimeContext::set_palette_using(const VariableRef& source) {
    if (!source.indices.empty()) {
        throw std::runtime_error("PALETTE USING expects an array name, not indexed access");
    }
    auto it = arrays_.find(source.name);
    if (it == arrays_.end()) {
        throw std::runtime_error("Array not DIMed: " + source.name);
    }
    const auto& array = it->second;
    if (array.kind == VariableKind::String) {
        throw std::runtime_error("PALETTE USING requires a numeric array");
    }
    if (array.dimensions.empty()) {
        throw std::runtime_error("PALETTE USING requires a one-dimensional numeric array");
    }
    if (array.dimensions.size() != 1) {
        throw std::runtime_error("PALETTE USING currently supports one-dimensional arrays only");
    }
    const auto limit = std::min<std::size_t>(palette_map_.size(), array.elements.size());
    for (std::size_t i = 0; i < limit; ++i) {
        palette_map_[i] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(array.elements[i].as_number())), 0, 255));
    }
    for (std::size_t i = limit; i < palette_map_.size(); ++i) {
        palette_map_[i] = static_cast<std::uint8_t>(i);
    }
    mark_graphics_dirty(true);
}

static auto map_axis(double value, double src1, double src2, int dst1, int dst2) -> int {
    if (std::fabs(src2 - src1) < 1e-12) {
        return dst1;
    }
    const double t = (value - src1) / (src2 - src1);
    return static_cast<int>(std::lround(static_cast<double>(dst1) + t * static_cast<double>(dst2 - dst1)));
}

auto RuntimeContext::map_window_x(double x) const -> int {
    if (!window_active_) return static_cast<int>(std::lround(x));
    return map_axis(x, window_x1_, window_x2_, graphics_view_left_, graphics_view_right_);
}

auto RuntimeContext::map_window_y(double y) const -> int {
    if (!window_active_) return static_cast<int>(std::lround(y));
    if (window_screen_coordinates_) {
        return map_axis(y, window_y1_, window_y2_, graphics_view_top_, graphics_view_bottom_);
    }
    return map_axis(y, window_y1_, window_y2_, graphics_view_bottom_, graphics_view_top_);
}

auto RuntimeContext::pmap(double coordinate, int mode) const -> double {
    switch (mode) {
        case 0: return static_cast<double>(map_window_x(coordinate));
        case 1: return static_cast<double>(map_window_y(coordinate));
        case 2: {
            if (!window_active_) return coordinate;
            if (graphics_view_right_ == graphics_view_left_) return window_x1_;
            const double t = (coordinate - graphics_view_left_) / static_cast<double>(graphics_view_right_ - graphics_view_left_);
            return window_x1_ + t * (window_x2_ - window_x1_);
        }
        case 3: {
            if (!window_active_) return coordinate;
            if (graphics_view_bottom_ == graphics_view_top_) return window_y1_;
            if (window_screen_coordinates_) {
                const double t = (coordinate - graphics_view_top_) / static_cast<double>(graphics_view_bottom_ - graphics_view_top_);
                return window_y1_ + t * (window_y2_ - window_y1_);
            }
            const double t = (coordinate - graphics_view_bottom_) / static_cast<double>(graphics_view_top_ - graphics_view_bottom_);
            return window_y1_ + t * (window_y2_ - window_y1_);
        }
        default: return 0.0;
    }
}

void RuntimeContext::set_view(std::optional<int> x1, std::optional<int> y1, std::optional<int> x2, std::optional<int> y2) {
    if (!x1.has_value() || !y1.has_value() || !x2.has_value() || !y2.has_value()) {
        graphics_view_left_ = 0;
        graphics_view_top_ = 0;
        graphics_view_right_ = graphics_width_ - 1;
        graphics_view_bottom_ = graphics_height_ - 1;
        mark_graphics_dirty(true);
        return;
    }
    graphics_view_left_ = std::clamp(std::min(*x1, *x2), 0, graphics_width_ - 1);
    graphics_view_top_ = std::clamp(std::min(*y1, *y2), 0, graphics_height_ - 1);
    graphics_view_right_ = std::clamp(std::max(*x1, *x2), 0, graphics_width_ - 1);
    graphics_view_bottom_ = std::clamp(std::max(*y1, *y2), 0, graphics_height_ - 1);
    mark_graphics_dirty(true);
}

auto RuntimeContext::point(int x, int y) const -> double {
    if (x < 0 || y < 0 || x >= graphics_width_ || y >= graphics_height_) {
        return 0.0;
    }
    return static_cast<double>(graphics_pixels_[static_cast<std::size_t>(y * graphics_width_ + x)]);
}

} // namespace gwbasic
