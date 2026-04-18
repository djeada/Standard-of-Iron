#include "roman_light_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../rig_dsl/defs/roman_light_helmet_rig.h"
#include "../../rig_dsl/rig_interpreter.h"
#include "../equipment_submit.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

namespace RLH = Render::RigDSL::RomanLightHelmet;

// Pre-computes all helmet anchor positions once per render call. The rig
// interpreter then just reads them out by AnchorId in O(1). This keeps the
// anchor resolver branchless and matches the imperative version's anchor
// values bit-for-bit.
class HelmetAnchors : public Render::RigDSL::AnchorResolver {
public:
  HelmetAnchors(const AttachmentFrame &head, const QVector3D &apex_pos,
                const QVector3D &crest_base, const QVector3D &crest_mid,
                const QVector3D &crest_top) {
    auto head_point = [&](const QVector3D &n) {
      return HumanoidRendererBase::frame_local_position(head, n);
    };

    QVector3D const helmet_top = head_point({0.0F, 1.28F, 0.0F});
    QVector3D const helmet_bot = head_point({0.0F, 0.08F, 0.0F});
    float const half_h = head.radius * 0.018F * 0.5F;

    auto ring = [&](float y_offset) -> std::pair<QVector3D, QVector3D> {
      QVector3D const centre = head_point({0.0F, y_offset, 0.0F});
      return {centre + head.up * half_h, centre - head.up * half_h};
    };
    auto const low_ring = ring(0.35F);
    auto const high_ring = ring(0.95F);

    m_positions[RLH::Helmet_Top] = helmet_top;
    m_positions[RLH::Helmet_Bot] = helmet_bot;
    m_positions[RLH::Apex] = apex_pos;
    m_positions[RLH::Ring_Low_Top] = low_ring.first;
    m_positions[RLH::Ring_Low_Bot] = low_ring.second;
    m_positions[RLH::Ring_High_Top] = high_ring.first;
    m_positions[RLH::Ring_High_Bot] = high_ring.second;
    m_positions[RLH::Neck_Guard_Top] = head_point({0.0F, 0.03F, -0.85F});
    m_positions[RLH::Neck_Guard_Bot] = head_point({0.0F, -0.32F, -0.92F});
    m_positions[RLH::Crest_Base] = crest_base;
    m_positions[RLH::Crest_Mid] = crest_mid;
    m_positions[RLH::Crest_Top] = crest_top;
  }

  [[nodiscard]] auto resolve(Render::RigDSL::AnchorId id) const
      -> QVector3D override {
    return id < m_positions.size() ? m_positions[id] : QVector3D{};
  }

private:
  std::array<QVector3D, 12> m_positions{};
};

class HelmetPalette : public Render::RigDSL::PaletteResolver {
public:
  HelmetPalette(const QVector3D &base, const QVector3D &accent)
      : m_base(base), m_accent(accent) {}

  [[nodiscard]] auto resolve(Render::RigDSL::PaletteSlot slot) const
      -> QVector3D override {
    switch (slot) {
    case Render::RigDSL::PaletteSlot::Metal:
      return m_base;
    case Render::RigDSL::PaletteSlot::Metal_Accent:
      return m_accent;
    default:
      return m_base;
    }
  }

private:
  QVector3D m_base;
  QVector3D m_accent;
};

class HelmetScalars : public Render::RigDSL::ScalarResolver {
public:
  explicit HelmetScalars(float helmet_r) : m_helmet_r(helmet_r) {}
  [[nodiscard]] auto resolve(Render::RigDSL::ScalarId id) const
      -> float override {
    switch (id) {
    case RLH::S_Helmet_R:
      return m_helmet_r;
    case RLH::S_Helmet_R_Apex:
      return m_helmet_r * 0.97F;
    case RLH::S_Helmet_R_Ring_Low:
      return m_helmet_r * 1.06F;
    case RLH::S_Helmet_R_Ring_Hi:
      return m_helmet_r * 1.02F;
    case RLH::S_Helmet_R_Neck:
      return m_helmet_r * 0.86F;
    default:
      return 1.0F;
    }
  }

private:
  float m_helmet_r;
};

} // namespace

void RomanLightHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      EquipmentBatch &batch) {
  (void)anim;

  AttachmentFrame head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  // Helmet-local derived anchors that depend on head.up / apex_pos → computed
  // in one place, fed to the resolver. Keeps the rig def purely declarative.
  auto head_point = [&](const QVector3D &n) {
    return HumanoidRendererBase::frame_local_position(head, n);
  };
  QVector3D const apex_pos = head_point({0.0F, 1.48F, 0.0F});
  QVector3D const crest_base = apex_pos;
  QVector3D const crest_mid = crest_base + head.up * 0.09F;
  QVector3D const crest_top = crest_mid + head.up * 0.12F;

  QVector3D const helmet_color =
      saturate_color(palette.metal * QVector3D(1.15F, 0.92F, 0.68F));
  QVector3D const helmet_accent = helmet_color * 1.14F;
  float const helmet_r = head_r * 1.08F;

  HelmetAnchors const anchors(head, apex_pos, crest_base, crest_mid, crest_top);
  HelmetPalette const pal(helmet_color, helmet_accent);
  HelmetScalars const scalars(helmet_r);

  Render::RigDSL::InterpretContext ictx;
  ictx.model = ctx.model;
  ictx.anchors = &anchors;
  ictx.palette = &pal;
  ictx.scalars = &scalars;
  BatchSubmitterAdapter adapter(batch);
  Render::RigDSL::render_rig(RLH::kRig, ictx, adapter);
}

} // namespace Render::GL
