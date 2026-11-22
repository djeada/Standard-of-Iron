#pragma once

// Shared by the sheep_spec*.cpp translation units; not a public header.

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "render/creature/assets/creature_lod_geometry.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/creature/wildlife/sheep_manifest.h"
#include "render/creature/wildlife/sheep_spec.h"
#include "render/creature/wildlife/wildlife_gait.h"

namespace Render::Wildlife {

namespace sheep_spec_detail {

using Render::Creature::Quadruped::EllipsoidNode;

using Render::Creature::Quadruped::MeshNode;

using Render::Creature::Quadruped::SnoutNode;

using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;

constexpr float k_hip_y = 0.300F;

constexpr float k_knee_y = 0.164F;

constexpr float k_fetlock_y = 0.052F;

constexpr float k_fore_hip_z = 0.180F;

constexpr float k_hind_hip_z = -0.212F;

constexpr QVector3D k_withers(0.0F, 0.452F, 0.250F);

constexpr QVector3D k_poll_up(0.0F, 0.616F, 0.366F);

constexpr QVector3D k_graze_dir(0.0F, -0.974F, 0.226F);

constexpr float k_head_length = 0.170F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

constexpr std::array<BodyRing, 19> k_body_rings{{
    {-0.343F, 0.395F, 0.067F, 0.095F, 0.095F},
    {-0.318F, 0.398F, 0.141F, 0.144F, 0.144F},
    {-0.288F, 0.413F, 0.165F, 0.156F, 0.156F},
    {-0.257F, 0.424F, 0.164F, 0.160F, 0.160F},
    {-0.226F, 0.433F, 0.158F, 0.161F, 0.161F},
    {-0.190F, 0.442F, 0.152F, 0.160F, 0.160F},
    {-0.153F, 0.452F, 0.144F, 0.157F, 0.157F},
    {-0.113F, 0.456F, 0.138F, 0.156F, 0.156F},
    {-0.073F, 0.446F, 0.135F, 0.165F, 0.165F},
    {-0.034F, 0.437F, 0.133F, 0.173F, 0.173F},
    {0.006F, 0.432F, 0.133F, 0.177F, 0.177F},
    {0.046F, 0.425F, 0.135F, 0.176F, 0.176F},
    {0.086F, 0.422F, 0.140F, 0.168F, 0.168F},
    {0.126F, 0.427F, 0.147F, 0.158F, 0.158F},
    {0.165F, 0.438F, 0.151F, 0.155F, 0.155F},
    {0.205F, 0.450F, 0.141F, 0.156F, 0.156F},
    {0.245F, 0.460F, 0.113F, 0.152F, 0.152F},
    {0.275F, 0.470F, 0.080F, 0.139F, 0.139F},
    {0.300F, 0.480F, 0.040F, 0.120F, 0.120F},
}};

struct LegPlan {
  float x;
  float z;
  float phase_offset;
  float knee_bias;
  float foot_bias;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-0.104F, k_fore_hip_z, 0.0F, 0.034F, -0.008F},
    {0.104F, k_fore_hip_z, 0.5F, 0.034F, -0.008F},
    {-0.114F, k_hind_hip_z, 0.5F, -0.056F, 0.006F},
    {0.114F, k_hind_hip_z, 0.0F, -0.056F, 0.006F},
}};

constexpr float k_walk_bob = 0.013F;

constexpr float k_walk_sway = 0.010F;

constexpr float k_run_rock = 0.024F;

constexpr float k_run_flight = 0.026F;

constexpr std::array<float, k_leg_count> k_walk_leg_phase{{0.25F, 0.75F, 0.0F, 0.5F}};

constexpr std::array<float, k_leg_count> k_run_leg_phase{{0.0F, 0.12F, 0.48F, 0.60F}};

constexpr QVector3D k_tail_base(0.0F, 0.512F, -0.316F);

constexpr QVector3D k_tail_mid(0.0F, 0.440F, -0.356F);

constexpr QVector3D k_tail_tip(0.0F, 0.370F, -0.364F);

constexpr GaitPlan k_gait_walk{0.320F, 0.64F, 0.042F, 0.092F, 0.50F};

constexpr GaitPlan k_gait_run{0.385F, 0.32F, 0.115F, 0.145F, 0.90F};

auto gait_plan(SheepGait gait) noexcept -> GaitPlan;

auto leg_rests() noexcept -> const std::array<LegRest, k_leg_count>&;

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

auto body_motion(const SheepDrive& drive) -> BodyMotion;

void fill_legs(RigPose& pose, const SheepDrive& drive, const BodyMotion& motion);

void fill_head(RigPose& pose, const SheepDrive& drive);

auto head_chain(RigPose& pose) -> std::array<QVector3D*, 8>;

struct HeadAttachment {
  float neck_length{0.0F};
  QVector3D muzzle{};
  QVector3D jaw_hinge{};
  QVector3D jaw_tip{};
  QVector3D ear_base_l{};
  QVector3D ear_tip_l{};
  QVector3D ear_base_r{};
  QVector3D ear_tip_r{};
};

auto capture_head_attachment(const RigPose& pose) -> HeadAttachment;

void reattach_head(RigPose& pose, const HeadAttachment& held);

auto rotate_about_x(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D;

auto rotate_about_y(const QVector3D& point,
                    const QVector3D& pivot,
                    float radians) -> QVector3D;

void apply_idle_motion(RigPose& pose, const SheepDrive& drive);

void apply_startle(RigPose& pose, const SheepDrive& drive);

struct CartoonKeel {
  float hop{0.0F};
  float roll{0.0F};
  float splay{0.0F};
  float boing{0.0F};
  float bounce{0.0F};
  float shiver{0.0F};
};

auto cartoon_keel(float phase) -> CartoonKeel;

auto apply_collapse(RigPose& pose, const SheepDrive& drive) -> float;

auto rolled_head(const HeadAttachment& held, float roll) -> HeadAttachment;

auto make_pose(const SheepDrive& drive) -> RigPose;

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

auto ear_flap(std::string_view name,
              Bone bone,
              std::uint8_t role,
              const QVector3D& base,
              const QVector3D& tip,
              const QVector3D& face_normal,
              float half_width) -> MeshNode;

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode>;

} // namespace sheep_spec_detail

} // namespace Render::Wildlife
