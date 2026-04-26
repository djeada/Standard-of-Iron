#include "humanoid_renderer_base.h"

namespace Render::GL {

void HumanoidRendererBase::draw_armor_overlay(
    const DrawContext &, const HumanoidVariant &, const HumanoidPose &, float,
    float, float, float, const QVector3D &, ISubmitter &) const {}

void HumanoidRendererBase::draw_shoulder_decorations(
    const DrawContext &, const HumanoidVariant &, const HumanoidPose &, float,
    float, const QVector3D &, ISubmitter &) const {}

} // namespace Render::GL
