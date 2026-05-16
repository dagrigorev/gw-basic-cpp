#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gwbasic::graphics {
/** Platform presentation hook for the portable indexed-color graphics canvas. */
class Presenter {
public:
    virtual ~Presenter() = default;
    /** Present an indexed pixel buffer using the supplied palette map. */
    virtual void present(const std::vector<std::uint8_t>& pixels, int width, int height, const std::array<std::uint8_t, 256>& palette_map) = 0;
    /** Poll one keyboard input item if the presenter owns a window. */
    [[nodiscard]] virtual auto poll_key() -> std::optional<std::string> { return std::nullopt; }
    /** Pump nonblocking presenter/window events. */
    virtual void pump_events() {}
    /** Block until the presenter window closes, if one exists. */
    virtual void wait_until_closed() {}
    /** Return whether the presenter window is still available. */
    [[nodiscard]] virtual bool is_open() const { return true; }
};
/** Create the best platform presenter, or a null presenter where unavailable. */
[[nodiscard]] auto create_default_presenter() -> std::unique_ptr<Presenter>;
} // namespace gwbasic::graphics
