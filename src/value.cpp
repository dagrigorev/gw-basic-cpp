#include "gwbasic/value.hpp"

#include <cmath>
#include <stdexcept>

namespace gwbasic {

Value::Value() : storage_(0.0) {}
Value::Value(double number) : storage_(number) {}
Value::Value(std::string text) : storage_(std::move(text)) {}

auto Value::is_number() const -> bool { return std::holds_alternative<double>(storage_); }
auto Value::is_string() const -> bool { return std::holds_alternative<std::string>(storage_); }

auto Value::as_number() const -> double {
    if (const auto* value = std::get_if<double>(&storage_)) {
        return *value;
    }
    throw std::runtime_error("Expected numeric value");
}

auto Value::as_string() const -> std::string {
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return *value;
    }
    const auto number = std::get<double>(storage_);
    if (std::fabs(number - std::round(number)) < 1e-9) {
        return std::to_string(static_cast<long long>(std::llround(number)));
    }
    return std::to_string(number);
}

auto Value::truthy() const -> bool {
    if (is_number()) {
        return std::fabs(as_number()) > 1e-12;
    }
    return !std::get<std::string>(storage_).empty();
}

} // namespace gwbasic
