#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

#include "animation/rig/humanoid_proportions.h"
#include "render/cache/static_attachment_spec.h"
#include "render/creature/humanoid/asset/bind_skeleton.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/runtime/humanoid_math.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/humanoid/runtime/style_palette.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_cache_key.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/oriented_archetype_utils.h"
#include "render/equipment/primitive_archetype_utils.h"
#include "render/equipment/renderer_constants.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/frame/render_archetype.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"
#include "render/math/math_utils.h"

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;
namespace sword_renderer_detail {

enum SwordPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_metal_dark_slot = 1U,
  k_fuller_slot = 2U,
  k_leather_slot = 3U,
};

struct SwordArchetypeKey {
  int sword_length_key{0};
  int sword_width_key{0};
  int guard_half_width_key{0};
  int handle_radius_key{0};
  int pommel_radius_key{0};
  int pommel_length_key{0};
  int blade_ricasso_key{0};
  int blade_taper_bias_key{0};
  int blade_mid_width_scale_key{0};
  int blade_tip_width_scale_key{0};
  int blade_curve_key{0};
  int guard_curve_key{0};
  int guard_spike_length_key{0};
  int blade_back_spike_count{0};
  int blade_back_spike_length_key{0};
  int material_id{0};
  bool has_hooked_pommel{false};
};

auto quantize_sword_value(float value) -> int;

auto oriented_box_between(const QVector3D& start,
                          const QVector3D& end,
                          float half_width,
                          float half_depth,
                          QVector3D right_hint = QVector3D(1.0F,
                                                           0.0F,
                                                           0.0F)) -> QMatrix4x4;

auto oriented_cone_between(const QVector3D& start,
                           const QVector3D& end,
                           float half_width,
                           float half_depth,
                           QVector3D right_hint = QVector3D(1.0F,
                                                            0.0F,
                                                            0.0F)) -> QMatrix4x4;

auto hand_basis_transform(const QMatrix4x4& parent,
                          const AttachmentFrame& hand) -> QMatrix4x4;

auto sword_local_pose(const QVector3D& blade_axis_local) -> QMatrix4x4;

[[nodiscard]] auto
uses_authored_rpg_sword_blade(const HumanoidAnimationContext& anim) noexcept -> bool;

[[nodiscard]] auto authored_sword_blade_dir() noexcept -> QVector3D;

struct SwordTrailWindow {
  float start{0.28F};
  float end{0.68F};
};

[[nodiscard]] auto sword_trail_window(const HumanoidAnimationContext& anim,
                                      bool authored) noexcept -> SwordTrailWindow;

auto sword_archetype(const SwordRenderConfig& config) -> const RenderArchetype&;

enum ScabbardPaletteSlot : std::uint8_t {
  k_scabbard_leather_slot = 0U,
  k_scabbard_metal_slot = 1U,
};

struct ScabbardKey {
  int sheath_r_key{0};
};

auto scabbard_archetype(float sheath_r) -> const RenderArchetype&;

} // namespace sword_renderer_detail

} // namespace Render::GL
