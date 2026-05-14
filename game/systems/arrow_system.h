#pragma once
#include "../core/system.h"
#include "../core/world.h"
#include "../game_config.h"
#include <QVector3D>
#include <cstdint>
#include <vector>

namespace Game::Systems {

enum class ArrowVisualStyle : std::uint8_t {
  Focused,
  Volley,
  Marker,
};

struct ArrowInstance {
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float t{};
  float speed{};
  bool active{};
  float arc_height{};
  float inv_dist{};
  float scale{1.0F};
  float roll_deg{};
  float spin_rate_deg{};
  float trail_alpha{};
  float trail_length{};
  float brightness{1.0F};
  ArrowVisualStyle style{ArrowVisualStyle::Focused};
};

class ArrowSystem : public Engine::Core::System {
public:
  ArrowSystem();
  void update(Engine::Core::World *world, float delta_time) override;
  void spawn_arrow(const QVector3D &start, const QVector3D &end,
                   const QVector3D &color, float speed = 8.0F,
                   ArrowVisualStyle style = ArrowVisualStyle::Focused);
  [[nodiscard]] auto arrows() const -> const std::vector<ArrowInstance> & {
    return m_arrows;
  }

private:
  std::vector<ArrowInstance> m_arrows;
  ArrowConfig m_config;
  std::uint32_t m_spawn_sequence = 0;
};

} // namespace Game::Systems
