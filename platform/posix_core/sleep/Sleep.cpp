#include "Sleep.hpp"

#include <chrono>
#include <thread>

namespace PlatformCore {
void Sleep::do_sleep_ms(std::uint32_t ms_sleep) const {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_sleep));
}
}  // namespace PlatformCore