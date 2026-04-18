// Stage 12 — generic keyframe animation clip.
//
// A clip holds a sorted list of keyframes over a scalar-valued bone or
// driver channel. The design deliberately does NOT bake in
// HumanoidPose: a clip's value type is templated, so the same system
// drives float channels (banner sway, aura intensity), QVector3D
// channels (bone offsets, anchor displacements), or any POD that
// supports linear blend.
//
// Why not one big "humanoid clip" with N bones?
//   * Most animations touch 1–5 channels, not all 24. Per-channel clips
//     avoid storing keyframes for bones that don't move in a given anim.
//   * Adding a new channel type never breaks existing clips.
//   * A state-machine blends per channel (see state_machine.h), which is
//     exactly what humanoid idle → walk needs — one channel settling
//     separately from another.
//
// Scalability note: a Clip<T> is an immutable POD after construction.
// Authored clips live in header files (see clips/) and are
// constexpr-initialisable for float scalars.

#pragma once

#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Render::Animation {

template <typename T> struct Keyframe {
  float time{0.0F};
  T value{};
};

// Built-in lerp overloads. Callers can specialise for custom POD types.
inline auto lerp(float a, float b, float t) noexcept -> float {
  return a + (b - a) * t;
}
inline auto lerp(const QVector3D &a, const QVector3D &b, float t) -> QVector3D {
  return QVector3D(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t,
                   a.z() + (b.z() - a.z()) * t);
}

template <typename T> class Clip {
public:
  Clip() = default;
  Clip(std::string_view name, std::vector<Keyframe<T>> keys)
      : m_name(name), m_keys(std::move(keys)) {
    // Sort by time so evaluate() can binary-search. We don't enforce
    // uniqueness of timestamps — duplicate timestamps degenerate the
    // lerp ratio but never divide by zero (we guard below).
    std::sort(m_keys.begin(), m_keys.end(),
              [](const Keyframe<T> &a, const Keyframe<T> &b) {
                return a.time < b.time;
              });
    m_duration = m_keys.empty() ? 0.0F : m_keys.back().time;
  }

  [[nodiscard]] auto name() const noexcept -> std::string_view {
    return m_name;
  }
  [[nodiscard]] auto duration() const noexcept -> float { return m_duration; }
  [[nodiscard]] auto key_count() const noexcept -> std::size_t {
    return m_keys.size();
  }
  [[nodiscard]] auto keys() const noexcept -> const std::vector<Keyframe<T>> & {
    return m_keys;
  }

private:
  std::string_view m_name;
  std::vector<Keyframe<T>> m_keys;
  float m_duration{0.0F};
};

enum class WrapMode : std::uint8_t {
  Clamp = 0, // Values before [0,duration] clamp to endpoints.
  Loop = 1,  // Wrap time into [0,duration].
};

// Sample a clip at `time`. Empty clips return a value-initialised T.
template <typename T>
[[nodiscard]] auto evaluate(const Clip<T> &clip, float time,
                            WrapMode wrap = WrapMode::Clamp) -> T {
  auto const &keys = clip.keys();
  if (keys.empty()) {
    return T{};
  }
  if (keys.size() == 1 || clip.duration() <= 0.0F) {
    return keys.front().value;
  }

  float t = time;
  if (wrap == WrapMode::Loop) {
    float const d = clip.duration();
    t = t - d * std::floor(t / d);
  } else {
    t = std::clamp(t, 0.0F, clip.duration());
  }

  // Linear search is fine for small clips (< 16 keys, the expected
  // range for hand-authored animations). If a clip ever exceeds ~64
  // keys, switch to std::upper_bound here.
  std::size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].time < t) {
    ++i;
  }
  if (i + 1 >= keys.size()) {
    return keys.back().value;
  }
  float const dt = keys[i + 1].time - keys[i].time;
  float const u = dt > 0.0F ? (t - keys[i].time) / dt : 0.0F;
  return lerp(keys[i].value, keys[i + 1].value, u);
}

} // namespace Render::Animation
