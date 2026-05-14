#include "arrow_system.h"
#include "../../render/geom/arrow.h"
#include "../../render/scene_renderer.h"
#include <algorithm>
#include <cmath>
#include <qvectornd.h>

namespace Game::Systems {
namespace {

constexpr float k_unit_float_divisor = 1.0F / 16777215.0F;

auto mix_seed(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

auto unit_float(std::uint32_t seed) -> float {
  return static_cast<float>(mix_seed(seed) & 0x00ffffffU) *
         k_unit_float_divisor;
}

auto remap_unit(float value, float minimum, float maximum) -> float {
  return minimum + (maximum - minimum) * value;
}

void apply_visual_profile(ArrowInstance &arrow, ArrowVisualStyle style,
                          std::uint32_t sequence) {
  arrow.style = style;

  switch (style) {
  case ArrowVisualStyle::Volley:
    arrow.t = -remap_unit(unit_float(sequence ^ 0x13579bdfU), 0.0F, 0.16F);
    arrow.scale = remap_unit(unit_float(sequence ^ 0x2468ace0U), 0.92F, 1.18F);
    arrow.roll_deg =
        remap_unit(unit_float(sequence ^ 0x55aa55aaU), -32.0F, 32.0F);
    arrow.spin_rate_deg =
        remap_unit(unit_float(sequence ^ 0xa55aa55aU), -280.0F, 280.0F);
    arrow.trail_alpha =
        remap_unit(unit_float(sequence ^ 0x0f1e2d3cU), 0.16F, 0.28F);
    arrow.trail_length =
        remap_unit(unit_float(sequence ^ 0x10293847U), 0.11F, 0.18F);
    arrow.brightness =
        remap_unit(unit_float(sequence ^ 0x56473829U), 0.88F, 1.08F);
    break;
  case ArrowVisualStyle::Marker:
    arrow.t = 0.0F;
    arrow.scale = 1.08F;
    arrow.roll_deg = 0.0F;
    arrow.spin_rate_deg = 0.0F;
    arrow.trail_alpha = 0.0F;
    arrow.trail_length = 0.0F;
    arrow.brightness = 1.05F;
    break;
  case ArrowVisualStyle::Focused:
  default:
    arrow.t = -remap_unit(unit_float(sequence ^ 0x89abcdefU), 0.0F, 0.05F);
    arrow.scale = remap_unit(unit_float(sequence ^ 0x31415926U), 0.97F, 1.08F);
    arrow.roll_deg =
        remap_unit(unit_float(sequence ^ 0x27182818U), -10.0F, 10.0F);
    arrow.spin_rate_deg =
        remap_unit(unit_float(sequence ^ 0x42424242U), -80.0F, 80.0F);
    arrow.trail_alpha =
        remap_unit(unit_float(sequence ^ 0xabcdef01U), 0.08F, 0.15F);
    arrow.trail_length =
        remap_unit(unit_float(sequence ^ 0x10fedcbaU), 0.06F, 0.10F);
    arrow.brightness =
        remap_unit(unit_float(sequence ^ 0xc001d00dU), 0.94F, 1.04F);
    break;
  }
}

} // namespace

ArrowSystem::ArrowSystem() : m_config(GameConfig::instance().arrow()) {}

void ArrowSystem::spawn_arrow(const QVector3D &start, const QVector3D &end,
                              const QVector3D &color, float speed,
                              ArrowVisualStyle style) {
  ArrowInstance a;
  a.start = start;
  a.end = end;
  a.color = color;
  a.speed = speed;
  a.active = true;
  QVector3D const delta = end - start;
  float const dist = delta.length();
  a.arc_height = std::clamp(m_config.arc_height_multiplier * dist,
                            m_config.arc_height_min, m_config.arc_height_max);

  a.inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;
  apply_visual_profile(a, style, ++m_spawn_sequence);
  m_arrows.push_back(a);
}

void ArrowSystem::update(Engine::Core::World *, float delta_time) {
  for (auto &arrow : m_arrows) {
    if (!arrow.active) {
      continue;
    }

    arrow.t += delta_time * arrow.speed * arrow.inv_dist;
    if (arrow.t >= 1.0F) {
      arrow.t = 1.0F;
      arrow.active = false;
    }
  }

  m_arrows.erase(
      std::remove_if(m_arrows.begin(), m_arrows.end(),
                     [](const ArrowInstance &a) { return !a.active; }),
      m_arrows.end());
}

} // namespace Game::Systems
