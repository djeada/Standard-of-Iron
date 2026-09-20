#pragma once

#include <QVector3D>

#include <string_view>

#include "registry.h"

namespace Render::GL {
class Mesh;
class Texture;

struct SiegeTravelState {
  QVector3D position;
  float yaw{0.0F};
  float time{0.0F};
  float left_roll{0.0F};
  float right_roll{0.0F};
  float movement{0.0F};
  bool initialized{false};
};

struct SiegeMotion {
  float left_roll{0.0F};
  float right_roll{0.0F};
  float movement{0.0F};
  float recoil{0.0F};
};

[[nodiscard]] auto siege_motion(const DrawContext& ctx,
                                SiegeTravelState& state,
                                float wheel_radius,
                                float half_track) -> SiegeMotion;
[[nodiscard]] auto siege_body_model(const DrawContext& ctx,
                                    const SiegeMotion& motion) -> QMatrix4x4;
[[nodiscard]] auto siege_winding(float progress) -> float;
[[nodiscard]] auto siege_release(float progress) -> float;

void draw_siege_wheel(ISubmitter& out,
                      Texture* white,
                      const QMatrix4x4& model,
                      const QVector3D& hub,
                      float radius,
                      float roll,
                      const QVector3D& wood,
                      const QVector3D& iron,
                      const QVector3D& bronze);
void draw_siege_regalia(const DrawContext& ctx,
                        ISubmitter& out,
                        Mesh* cube,
                        Texture* white,
                        const QVector3D& team,
                        const QVector3D& bronze,
                        bool ballista);

using SiegeBodyDrawer = void (*)(const DrawContext&,
                                 ISubmitter&,
                                 Mesh*,
                                 Texture*,
                                 const QVector3D&,
                                 const SiegeMotion&);

struct SiegeRendererConfig {
  std::string_view renderer_key;
  QVector3D default_team;
  SiegeBodyDrawer draw_body;
};

void register_siege_renderer_variant(EntityRendererRegistry& registry,
                                     const SiegeRendererConfig& config);

} // namespace Render::GL
