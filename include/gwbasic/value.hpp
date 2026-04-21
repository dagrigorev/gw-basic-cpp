#pragma once

#include <string>
#include <variant>

namespace gwbasic {

class Value {
public:
    using Storage = std::variant<double, std::string>;

    Value();
    explicit Value(double number);
    explicit Value(std::string text);

    [[nodiscard]] auto is_number() const -> bool;
    [[nodiscard]] auto is_string() const -> bool;
    [[nodiscard]] auto as_number() const -> double;
    [[nodiscard]] auto as_string() const -> std::string;
    [[nodiscard]] auto truthy() const -> bool;

private:
    Storage storage_;
};

} // namespace gwbasic
