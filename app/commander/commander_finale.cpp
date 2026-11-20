#include "app/commander/commander_finale.h"

#include <algorithm>
#include <cmath>

namespace App::Core {

namespace {

constexpr float k_degrees_to_radians = 0.017453292519943295F;

// How far round the commander the lens travels, and how far it ends up
// standing off, relative to where the player's own camera was.
constexpr float k_sweep_degrees = 34.0F;
constexpr float k_minimum_start_radius = 2.4F;
constexpr float k_minimum_end_radius = 5.2F;
constexpr float k_radius_growth = 1.45F;

// The lens always finishes above head height: at the commander's shoulder it
// ends the sequence staring into whoever is standing next to him.
constexpr float k_minimum_end_height = 2.6F;
constexpr float k_fallen_rise = 1.5F;
constexpr float k_standing_rise = 0.5F;

// A man on the ground is looked at lower than a man on his feet.
constexpr float k_fallen_focus_height = 0.45F;
constexpr float k_standing_focus_height = 1.25F;

// The look point leaves the player's own aim for the commander in the first
// part of the sequence, so the take-over reads as a move and not as a cut.
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
  // A lens standing straight above the commander has no bearing of its own;
  // any fixed one will do.
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
  float const end_height =
      std::max(m_start_height +
                   (m_kind == Kind::Fallen ? k_fallen_rise : k_standing_rise),
               k_minimum_end_height);
  float const height = std::lerp(m_start_height, end_height, t);

  CommanderFinalePose pose;
  pose.eye = m_focus +
             QVector3D(std::sin(angle) * radius, height, std::cos(angle) * radius);
  float const refocus = smoother(progress() / k_refocus_share);
  pose.target = m_start_target + ((m_focus - m_start_target) * refocus);
  return pose;
}

} // namespace App::Core
