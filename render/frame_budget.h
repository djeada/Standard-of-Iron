#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Render {

struct FrameBudgetConfig {
  float target_frame_ms = 12.0F;
  float hard_deadline_ms = 14.0F;

  bool allow_partial_render = false;
  bool enabled = true;
};

enum class CommandPriority : std::uint8_t {
  Critical = 0,
  High = 1,
  Normal = 2,
  Low = 3
};

class FrameTimeTracker {
public:
  void begin_frame() {
    m_frame_start = std::chrono::steady_clock::now();
    m_commands_executed = 0;
    m_commands_deferred = 0;
    m_frame_completed = false;
  }

  [[nodiscard]] auto elapsed_ms() const -> float {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float, std::milli>(now - m_frame_start)
        .count();
  }

  [[nodiscard]] auto
  should_defer(const FrameBudgetConfig &config) const -> bool {
    if (!config.enabled || !config.allow_partial_render)
      return false;
    return elapsed_ms() >= config.hard_deadline_ms;
  }

  [[nodiscard]] auto
  is_approaching_budget(const FrameBudgetConfig &config) const -> bool {
    if (!config.enabled)
      return false;
    return elapsed_ms() >= config.target_frame_ms;
  }

  void record_executed() { ++m_commands_executed; }
  void record_deferred() { ++m_commands_deferred; }
  void mark_complete() { m_frame_completed = true; }

  [[nodiscard]] auto commands_executed() const -> std::size_t {
    return m_commands_executed;
  }
  [[nodiscard]] auto commands_deferred() const -> std::size_t {
    return m_commands_deferred;
  }
  [[nodiscard]] auto frame_completed() const -> bool {
    return m_frame_completed;
  }
  [[nodiscard]] auto total_frame_ms() const -> float { return elapsed_ms(); }

  void end_frame() {
    float frame_ms = elapsed_ms();
    m_avg_frame_ms = m_avg_frame_ms * 0.9F + frame_ms * 0.1F;
    ++m_total_frames;
    if (m_commands_deferred > 0)
      ++m_deferred_frames;
  }

  [[nodiscard]] auto avg_frame_ms() const -> float { return m_avg_frame_ms; }
  [[nodiscard]] auto deferred_frame_ratio() const -> float {
    return m_total_frames > 0 ? static_cast<float>(m_deferred_frames) /
                                    static_cast<float>(m_total_frames)
                              : 0.0F;
  }

private:
  std::chrono::steady_clock::time_point m_frame_start{};
  std::size_t m_commands_executed{0};
  std::size_t m_commands_deferred{0};
  bool m_frame_completed{false};
  float m_avg_frame_ms{16.0F};
  std::uint64_t m_total_frames{0};
  std::uint64_t m_deferred_frames{0};
};

} // namespace Render
