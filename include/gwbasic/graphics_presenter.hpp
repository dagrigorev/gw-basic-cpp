#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gwbasic::graphics {
class Presenter {
public:
    virtual ~Presenter() = default;
    virtual void present(const std::vector<std::uint8_t>& pixels, int width, int height, const std::array<std::uint8_t, 256>& palette_map) = 0;
    [[nodiscard]] virtual auto poll_key() -> std::optional<std::string> { return std::nullopt; }
    virtual void pump_events() {}
    virtual void wait_until_closed() {}
    [[nodiscard]] virtual bool is_open() const { return true; }
};
[[nodiscard]] auto create_default_presenter() -> std::unique_ptr<Presenter>;
} // namespace gwbasic::graphics
