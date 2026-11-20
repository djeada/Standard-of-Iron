#pragma once

#include <QVector3D>

#include <cstdint>

namespace App::Core {

struct CommanderFinalePose {
  QVector3D eye{};
  QVector3D target{};
};

// The last look. A battle that ends while the player is the commander used to
// cut to the verdict panel on the frame the blow landed: the kill, or the
// commander's own fall, was never seen. This holds the commander's camera for
// a few seconds instead, turning slowly round the spot and drawing back, and
// only then hands the screen to the result.
//
// It owns no world state. It is given the view the player had and the ground
// the commander stood on, and it answers with where the lens should be for a
// given amount of elapsed real time, so the same sequence plays under the
// film clock, in a test and at any frame rate.
class CommanderFinale {
public:
  enum class Kind : std::uint8_t {
    None,
    // The controlled commander was killed.
    Fallen,
    // The battle was won.
    Victory,
    // The battle was lost with the commander still standing.
    Defeat
  };

  static constexpr float k_duration_seconds = 3.4F;
  // The simulation runs at this fraction of its speed while the lens turns.
  static constexpr float k_time_scale = 0.35F;

  void begin(Kind kind,
             const QVector3D& ground,
             const QVector3D& eye,
             const QVector3D& target);
  void advance(float real_seconds);
  void end();

  [[nodiscard]] auto active() const -> bool { return m_kind != Kind::None; }
  [[nodiscard]] auto kind() const -> Kind { return m_kind; }
  [[nodiscard]] auto elapsed() const -> float { return m_elapsed; }
  [[nodiscard]] auto progress() const -> float;
  [[nodiscard]] auto expired() const -> bool {
    return active() && m_elapsed >= k_duration_seconds;
  }
  [[nodiscard]] auto pose() const -> CommanderFinalePose;

  // Claims the one hand-back to the result screen. True exactly once per
  // sequence, after it has run its course.
  [[nodiscard]] auto take_hand_back() -> bool;

private:
  Kind m_kind{Kind::None};
  float m_elapsed{0.0F};
  bool m_handed_back{false};
  QVector3D m_focus{};
  QVector3D m_start_target{};
  float m_start_angle{0.0F};
  float m_start_radius{0.0F};
  float m_start_height{0.0F};
};

} // namespace App::Core
