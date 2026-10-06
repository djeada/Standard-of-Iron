#pragma once

#include <QVector3D>

#include <string_view>

#include "animation/siege_wreck_manifest.h"
#include "registry.h"
#include "siege_crew.h"

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
  float travelled{0.0F};
  bool initialized{false};
  SiegeCrewState crew{};
};

struct SiegeMotion {
  float left_roll{0.0F};
  float right_roll{0.0F};
  float movement{0.0F};
  float recoil{0.0F};

  float jolt{0.0F};
};

[[nodiscard]] auto siege_motion(const DrawContext& ctx,
                                SiegeTravelState& state,
                                float wheel_radius,
                                float half_track) -> SiegeMotion;
[[nodiscard]] auto siege_body_model(const DrawContext& ctx,
                                    const SiegeMotion& motion) -> QMatrix4x4;
struct SiegeWreckState {
  bool destroyed{false};
  float elapsed{0.0F};
  float sink{0.0F};
  Animation::SiegeWreckPose pose{};
};

[[nodiscard]] auto
resolve_siege_wreck(const DrawContext& ctx,
                    Animation::SiegeWreckKind kind) -> SiegeWreckState;
void apply_siege_wreck(QMatrix4x4& model,
                       const SiegeWreckState& wreck,
                       float half_width,
                       float sink_depth);
[[nodiscard]] auto siege_charred(const QVector3D& color,
                                 const SiegeWreckState& wreck) -> QVector3D;

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
