#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "game/core/component.h"
#include "render/creature/assets/creature_lod_geometry.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/creature/wildlife/wildlife_gait.h"
#include "render/creature/wildlife/wolf_manifest.h"
#include "render/creature/wildlife/wolf_spec.h"

namespace Render::Wildlife {

namespace wolf_spec_detail {

using Render::Creature::Quadruped::ConeNode;

using Render::Creature::Quadruped::EllipsoidNode;

using Render::Creature::Quadruped::MeshNode;

using Render::Creature::Quadruped::SnoutNode;

using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;

constexpr float k_fore_half_width = 0.098F;

constexpr float k_hind_half_width = 0.104F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

constexpr std::array<BodyRing, 14> k_body_rings{{
    {0.392F, 0.468F, 0.034F, 0.050F, 0.054F},
    {0.366F, 0.472F, 0.070F, 0.094F, 0.104F},
    {0.318F, 0.486F, 0.110F, 0.130F, 0.158F},
    {0.256F, 0.496F, 0.132F, 0.158F, 0.192F},
    {0.186F, 0.502F, 0.138F, 0.158F, 0.204F},
    {0.108F, 0.506F, 0.134F, 0.148F, 0.192F},
    {0.026F, 0.510F, 0.120F, 0.138F, 0.162F},
    {-0.056F, 0.514F, 0.108F, 0.132F, 0.132F},
    {-0.136F, 0.516F, 0.106F, 0.130F, 0.116F},
    {-0.214F, 0.518F, 0.122F, 0.134F, 0.120F},
    {-0.288F, 0.516F, 0.130F, 0.132F, 0.130F},
    {-0.350F, 0.508F, 0.110F, 0.114F, 0.112F},
    {-0.398F, 0.498F, 0.060F, 0.074F, 0.066F},
    {-0.420F, 0.492F, 0.028F, 0.038F, 0.034F},
}};

constexpr std::array<float, k_body_rings.size()> k_saddle_reach{{0.72F,
                                                                 0.58F,
                                                                 0.38F,
                                                                 0.22F,
                                                                 0.18F,
                                                                 0.28F,
                                                                 0.38F,
                                                                 0.44F,
                                                                 0.44F,
                                                                 0.42F,
                                                                 0.40F,
                                                                 0.46F,
                                                                 0.62F,
                                                                 0.76F}};

constexpr float k_saddle_relief = 0.005F;

struct LegPlan {
  float x;
  float phase_offset;
  bool hind;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-k_fore_half_width, 0.0F, false},
    {k_fore_half_width, 0.5F, false},
    {-k_hind_half_width, 0.5F, true},
    {k_hind_half_width, 0.0F, true},
}};

struct LegRestPlan {
  float knee_y;
  float knee_z;
  float ankle_y;
  float ankle_z;
  float toe_y;
  float toe_z;
  float hip_y;
  float hip_z;
};

constexpr LegRestPlan k_fore_rest{
    0.244F, 0.272F, 0.108F, 0.216F, 0.024F, 0.236F, 0.402F, 0.222F};

constexpr LegRestPlan k_hind_rest{
    0.286F, -0.140F, 0.134F, -0.256F, 0.024F, -0.220F, 0.438F, -0.222F};

constexpr float k_walk_bob = 0.016F;

constexpr float k_stalk_bob = 0.005F;

constexpr float k_walk_sway = 0.012F;

constexpr float k_stalk_sway = 0.013F;

constexpr float k_run_rock = 0.035F;

constexpr float k_run_flight = 0.019F;

constexpr float k_run_carriage = 0.50F;

constexpr QVector3D k_neck_root(0.0F, 0.575F, 0.300F);

constexpr QVector3D k_poll_high(0.0F, 0.715F, 0.455F);

constexpr QVector3D k_poll_low(0.0F, 0.505F, 0.485F);

constexpr QVector3D k_neck_bend_high(0.0F, 0.740F, 0.360F);

constexpr QVector3D k_neck_bend_low(0.0F, 0.550F, 0.398F);

constexpr QVector3D k_tail_base(0.0F, 0.540F, -0.392F);

constexpr QVector3D k_tail_mid(0.0F, 0.462F, -0.560F);

constexpr QVector3D k_tail_tip(0.0F, 0.296F, -0.672F);

constexpr std::array<float, k_leg_count> k_walk_leg_phase{{0.25F, 0.75F, 0.0F, 0.5F}};

constexpr std::array<float, k_leg_count> k_run_leg_phase{{0.0F, 0.12F, 0.48F, 0.60F}};

auto leg_phase_offset(WolfGait gait, std::size_t leg) -> float;

constexpr GaitPlan k_gait_stalk{0.240F, 0.70F, 0.034F, 0.048F, 0.95F};

constexpr GaitPlan k_gait_walk{0.400F, 0.62F, 0.052F, 0.092F, 0.70F};

constexpr GaitPlan k_gait_run{0.383F, 0.28F, 0.110F, 0.140F, 1.05F};

auto gait_plan(WolfGait gait) noexcept -> GaitPlan;

auto leg_rests() noexcept -> const std::array<LegRest, k_leg_count>&;

auto oval_width_fraction(float height) -> float;

auto saddle_rings() -> std::vector<Render::Creature::Quadruped::BarrelRing>;

auto lerp(const QVector3D& a, const QVector3D& b, float t) -> QVector3D;

auto bezier(const QVector3D& p0,
            const QVector3D& p1,
            const QVector3D& p2,
            float t) -> QVector3D;

struct BodyMotion {
  float fore{0.0F};
  float hind{0.0F};
  float sway{0.0F};
  float fore_mid{0.0F};
  float hind_mid{0.0F};
};

auto body_motion(const WolfDrive& drive) -> BodyMotion;

void fill_legs(RigPose& pose, const WolfDrive& drive, const BodyMotion& motion);

auto head_chain(RigPose& pose) -> std::array<QVector3D*, 8>;

void fill_head(RigPose& pose, const WolfDrive& drive);

auto rotate_about_x(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D;

auto rotate_about_y(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D;

auto rotate_about_z(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D;

void apply_rear(RigPose& pose, const WolfDrive& drive);

void apply_lunge(RigPose& pose, const WolfDrive& drive);

struct HeadAttachment {
  float neck_length{0.0F};
  float muzzle{0.0F};
  float jaw_hinge{0.0F};
  float jaw{0.0F};
  float ear_base_l{0.0F};
  float ear_l{0.0F};
  float ear_base_r{0.0F};
  float ear_r{0.0F};
};

auto capture_head_attachment(const RigPose& pose) -> HeadAttachment;

void reattach_head(RigPose& pose, const HeadAttachment& held);

void apply_collapse(RigPose& pose, const WolfDrive& drive);

void apply_idle_motion(RigPose& pose, const WolfDrive& drive);

auto make_pose(const WolfDrive& drive) -> RigPose;

auto ellipsoid(std::string_view name,
               Bone bone,
               std::uint8_t role,
               const QVector3D& center,
               const QVector3D& radii,
               std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode;

auto barrel(std::string_view name,
            Bone bone,
            std::uint8_t role,
            std::vector<Render::Creature::Quadruped::BarrelRing> rings) -> MeshNode;

auto tube(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& start,
          const QVector3D& end,
          float start_radius,
          float end_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode;

auto cone(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& base_center,
          const QVector3D& tip,
          float base_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode;

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode>;

} // namespace wolf_spec_detail

} // namespace Render::Wildlife
