#pragma once

#include <QVector3D>

#include <cstdint>

namespace App::Core {

struct CommanderFinalePose {
  QVector3D eye{};
  QVector3D target{};
};

class CommanderFinale {
public:
  enum class Kind : std::uint8_t {
    None,

    Fallen,

    Victory,

    Defeat
  };

  static constexpr float k_duration_seconds = 3.4F;

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

class CommanderEntryDive {
public:
  static constexpr float k_duration_seconds = 0.55F;

  void begin(const QVector3D& eye, const QVector3D& target);
  void advance(float real_seconds);
  void end() { m_active = false; }

  [[nodiscard]] auto active() const -> bool { return m_active; }

  [[nodiscard]] auto
  blend(const CommanderFinalePose& chase) const -> CommanderFinalePose;

private:
  bool m_active{false};
  float m_elapsed{0.0F};
  QVector3D m_eye{};
  QVector3D m_target{};
};

} // namespace App::Core
