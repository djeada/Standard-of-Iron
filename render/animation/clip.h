

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
  Clamp = 0,
  Loop = 1,
};

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
