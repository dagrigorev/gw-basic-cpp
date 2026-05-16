#pragma once

#include <string>
#include <variant>

namespace gwbasic {

/**
 * Runtime scalar value used by the interpreter.
 *
 * GW-BASIC values are represented as either a numeric double or a string. Type
 * conversion is deliberately explicit at API boundaries so callers see BASIC
 * type errors as exceptions instead of silent C++ conversions.
 */
class Value {
public:
    using Storage = std::variant<double, std::string>;

    /** Construct numeric zero. */
    Value();
    /** Construct a numeric BASIC value. */
    explicit Value(double number);
    /** Construct a string BASIC value. */
    explicit Value(std::string text);

    [[nodiscard]] auto is_number() const -> bool;
    [[nodiscard]] auto is_string() const -> bool;
    /** Return the number or throw if this is a string value. */
    [[nodiscard]] auto as_number() const -> double;
    /** Return the string, formatting numbers with BASIC-like compactness. */
    [[nodiscard]] auto as_string() const -> std::string;
    /** BASIC truthiness: non-zero numbers and non-empty strings are true. */
    [[nodiscard]] auto truthy() const -> bool;

private:
    Storage storage_;
};

} // namespace gwbasic
