#include "app/commander/commander_finale.h"

#include <algorithm>
#include <cmath>

namespace App::Core {

namespace {

constexpr float k_degrees_to_radians = 0.017453292519943295F;

constexpr float k_sweep_degrees = 34.0F;
constexpr float k_minimum_start_radius = 2.4F;
constexpr float k_minimum_end_radius = 5.2F;
constexpr float k_radius_growth = 1.45F;

constexpr float k_minimum_end_height = 2.6F;
constexpr float k_fallen_rise = 1.5F;
constexpr float k_standing_rise = 0.5F;

constexpr float k_fallen_focus_height = 0.45F;
constexpr float k_standing_focus_height = 1.25F;

constexpr float k_refocus_share = 0.35F;

auto smoother(float t) -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * t * (t * ((t * 6.0F) - 15.0F) + 10.0F);
}

} // namespace

void CommanderFinale::begin(Kind kind,
                            const QVector3D& ground,
                            const QVector3D& eye,
                            const QVector3D& target) {
  if (kind == Kind::None) {
    end();
    return;
  }
  m_kind = kind;
  m_elapsed = 0.0F;
  m_handed_back = false;
  m_focus = ground + QVector3D(0.0F,
                               kind == Kind::Fallen ? k_fallen_focus_height
                                                    : k_standing_focus_height,
                               0.0F);
  m_start_target = target;

  QVector3D const offset = eye - m_focus;
  m_start_radius = std::max(std::hypot(offset.x(), offset.z()), k_minimum_start_radius);
  m_start_height = offset.y();

  m_start_angle = std::hypot(offset.x(), offset.z()) > 1.0e-3F
                      ? std::atan2(offset.x(), offset.z())
                      : 0.0F;
}

void CommanderFinale::advance(float real_seconds) {
  if (!active()) {
    return;
  }
  m_elapsed = std::min(m_elapsed + std::max(real_seconds, 0.0F), k_duration_seconds);
}

void CommanderFinale::end() {
  m_kind = Kind::None;
  m_elapsed = 0.0F;
  m_handed_back = false;
}

auto CommanderFinale::progress() const -> float {
  return active() ? std::clamp(m_elapsed / k_duration_seconds, 0.0F, 1.0F) : 0.0F;
}

auto CommanderFinale::take_hand_back() -> bool {
  if (!expired() || m_handed_back) {
    return false;
  }
  m_handed_back = true;
  return true;
}

auto CommanderFinale::pose() const -> CommanderFinalePose {
  float const t = smoother(progress());
  float const angle = m_start_angle + (k_sweep_degrees * k_degrees_to_radians * t);
  float const end_radius =
      std::max(m_start_radius * k_radius_growth, k_minimum_end_radius);
  float const radius = std::lerp(m_start_radius, end_radius, t);
  float const end_height = std::max(
      m_start_height + (m_kind == Kind::Fallen ? k_fallen_rise : k_standing_rise),
      k_minimum_end_height);
  float const height = std::lerp(m_start_height, end_height, t);

  CommanderFinalePose pose;
  pose.eye =
      m_focus + QVector3D(std::sin(angle) * radius, height, std::cos(angle) * radius);
  float const refocus = smoother(progress() / k_refocus_share);
  pose.target = m_start_target + ((m_focus - m_start_target) * refocus);
  return pose;
}

void CommanderEntryDive::begin(const QVector3D& eye, const QVector3D& target) {
  m_active = true;
  m_elapsed = 0.0F;
  m_eye = eye;
  m_target = target;
}

void CommanderEntryDive::advance(float real_seconds) {
  if (!m_active) {
    return;
  }
  m_elapsed += std::max(real_seconds, 0.0F);
  if (m_elapsed >= k_duration_seconds) {
    m_active = false;
  }
}

auto CommanderEntryDive::blend(const CommanderFinalePose& chase) const
    -> CommanderFinalePose {
  if (!m_active) {
    return chase;
  }

  float const t = std::clamp(m_elapsed / k_duration_seconds, 0.0F, 1.0F);
  float const k = 1.0F - ((1.0F - t) * (1.0F - t) * (1.0F - t));
  return {.eye = m_eye + ((chase.eye - m_eye) * k),
          .target = m_target + ((chase.target - m_target) * k)};
}

} // namespace App::Core
