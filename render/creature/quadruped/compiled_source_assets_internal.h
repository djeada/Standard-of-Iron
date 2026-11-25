#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(Q_OS_MACOS)
#include <mach-o/dyld.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

#include "animation/elephant_gait_manifest.h"
#include "animation/elephant_motion_manifest.h"
#include "animation/rig/horse_attachment_frames.h"
#include "render/creature/elephant/elephant_source_asset.h"
#include "render/creature/elephant/elephant_spec.h"
#include "render/creature/elephant/schema/attachment_schema.h"
#include "render/creature/horse/horse_source_asset.h"
#include "render/creature/horse/horse_spec.h"
#include "render/creature/horse/schema/mounted_sockets.h"
#include "render/creature/quadruped/attachment_resolver.h"

namespace Render::Creature::Quadruped::compiled_source_detail {

using namespace Render::Horse;

using namespace Render::Elephant;

using Render::Creature::BoneDef;

using Render::Creature::BoneIndex;

using Render::Creature::Quadruped::CustomMeshNode;

using Render::Creature::Quadruped::MeshNode;

using Render::GL::Vertex;

constexpr std::string_view k_resource_path = ":/assets/creatures/horse/horse.cmesh";

constexpr std::string_view k_relative_path = "assets/creatures/horse/horse.cmesh";

constexpr std::array<std::string_view, k_horse_source_bone_count> k_bone_names{{
    "Body",
    "Back",
    "Torso",
    "Torso2",
    "Torso3",
    "Neck1",
    "Neck2",
    "Neck3",
    "Head",
    "Ear1.L",
    "Ear2.L",
    "Ear3.L",
    "Ear4.L",
    "Ear1.R",
    "Ear2.R",
    "Ear3.R",
    "Ear4.R",
    "FrontShoulder.L",
    "FrontUpperLeg.L",
    "FrontLowerLeg.L",
    "FrontShoulder.R",
    "FrontUpperLeg.R",
    "FrontLowerLeg.R",
    "BackShoulder.L",
    "BackLeg.L",
    "BackUpperLeg.L",
    "BackLowerLeg.L",
    "BackShoulder.R",
    "BackLeg.R",
    "BackUpperLeg.R",
    "BackLowerLeg.R",
    "Tail1",
    "Tail2",
    "Tail3",
    "Tail4",
    "Tail5",
    "Tail6",
    "Tail7",
    "PoleTargetBack.L",
    "PoleTarget.L",
    "PoleTargetBack.R",
    "PoleTarget.R",
    "IKBackLeg.L",
    "FFB.L",
    "IKFrontLeg.L",
    "FF.L",
    "IKBackLeg.R",
    "FFB.R",
    "IKFrontLeg.R",
    "FF.R",
}};

struct MaterialBinding {
  std::string_view exported_name;
  std::string_view node_name;
  std::uint8_t role;
};

constexpr std::array<MaterialBinding, 8> k_horse_materials{{
    {"horse_production_material_0", "horse.production.coat", 1U},
    {"horse_production_material_1", "horse.production.coat_dark", 2U},
    {"horse_production_material_2", "horse.production.coat_light", 3U},
    {"horse_production_material_3", "horse.production.hooves", 4U},
    {"horse_production_material_4", "horse.production.hair", 5U},
    {"horse_production_material_5", "horse.production.muzzle", 7U},
    {"horse_production_material_6", "horse.production.eye_white", 3U},
    {"horse_production_material_7", "horse.production.eye_black", 8U},
}};

constexpr std::array<std::string_view, Render::Elephant::k_elephant_source_bone_count>
    k_elephant_bone_names{{
        "Bone",
        "Bone.001",
        "Bone.002",
        "Bone.003",
        "Bone.012",
        "Bone.013",
        "Bone.014",
        "Bone.004",
        "Bone.005",
        "UpperLegFront.L",
        "LowerLegFront.L",
        "FootFront.L",
        "UpperLegFront.L.001",
        "LowerLegFront.L.001",
        "FootFront.L.001",
        "UpperLegBack.L",
        "LowerLegBack.L",
        "FootBack.L",
        "UpperLegBack.L.001",
        "LowerLegBack.L.001",
        "FootBack.L.001",
        "Tail",
        "KneeBack.L",
        "KneeFront.L",
        "Bone.016",
        "Bone.017",
        "Bone.018",
        "TrunkIK",
        "KneeBack.L.001",
        "KneeFront.L.001",
        "Bone.019",
        "Bone.020",
    }};

constexpr std::array<MaterialBinding, 3> k_elephant_materials{{
    {"elephant_production_material_0",
     "elephant.production.skin",
     Render::Elephant::k_elephant_role_skin},
    {"elephant_production_material_1",
     "elephant.production.tusks",
     Render::Elephant::k_elephant_role_tusk},
    {"elephant_production_material_2",
     "elephant.production.eyes",
     Render::Elephant::k_elephant_role_eye},
}};

struct SourceConfig {
  std::string_view resource_path;
  std::string_view relative_path;
  std::span<const std::string_view> bone_names;
  std::span<const MaterialBinding> materials;
  std::size_t expected_vertices;
  std::size_t expected_triangles;
  std::size_t expected_clips;
  float scale_x;
  float scale_y;
  float scale_z;
  float ground_y;
  std::string_view sha256;
};

constexpr SourceConfig k_horse_config{
    k_resource_path,
    k_relative_path,
    k_bone_names,
    k_horse_materials,
    4400U,
    2182U,
    13U,
    k_horse_mesh_scale_x,
    k_horse_mesh_scale_y,
    k_horse_mesh_scale_z,
    k_horse_mesh_ground_y,
    "f17f71f44344b9b1be1b53a2f26bf6a0d08f7a5ef15218077363abb7a05db993"};

constexpr SourceConfig k_elephant_config{
    ":/assets/creatures/elephant/elephant.cmesh",
    "assets/creatures/elephant/elephant.cmesh",
    k_elephant_bone_names,
    k_elephant_materials,
    1474U,
    772U,
    5U,
    Render::Elephant::k_elephant_mesh_scale_x,
    Render::Elephant::k_elephant_mesh_scale_y,
    Render::Elephant::k_elephant_mesh_scale_z,
    Render::Elephant::k_elephant_mesh_ground_y,
    "34b00dea1c0f3fdafef08231d73c7c28b3310d341b08058432354da002721ccf"};

struct NodePose {
  QVector3D translation;
  QQuaternion rotation;
  QVector3D scale{1.0F, 1.0F, 1.0F};
};

enum class ChannelPath : std::uint8_t {
  Translation,
  Rotation,
  Scale
};

struct AnimationChannel {
  int node{-1};
  ChannelPath path{ChannelPath::Translation};
  std::vector<float> times;
  std::vector<float> values;
  int components{3};
  bool step{false};
};

struct SourceClip {
  std::string name;
  float duration{0.0F};
  std::vector<AnimationChannel> channels;
};

struct AccessorView {
  const char* data{nullptr};
  std::size_t count{0U};
  std::size_t stride{0U};
  int component_type{0};
  int components{0};
};

struct SourceAsset {
  QByteArray buffer;
  QJsonObject document;
  std::vector<NodePose> default_poses;
  std::vector<int> parents;
  std::vector<int> joint_nodes;
  std::vector<MeshNode> mesh_nodes;
  std::vector<QMatrix4x4> bind_palette;
  std::vector<BoneDef> bone_defs;
  std::vector<SourceClip> clips;
  Render::Creature::CompiledCreatureAssetStatus status;
};

auto executable_directory() -> QString;

auto read_compiled_asset(SourceConfig const& config) -> QByteArray;

auto component_count(QString const& type) noexcept -> int;

auto component_size(int type) noexcept -> std::size_t;

auto accessor_view(SourceAsset const& asset, int accessor_index) -> AccessorView;

template <typename T>
auto read_component(const char* data) noexcept -> T {
  T result{};
  std::memcpy(&result, data, sizeof(T));
  return result;
}

auto accessor_float(AccessorView const& view,
                    std::size_t index,
                    int component) noexcept -> float;

auto accessor_uint(AccessorView const& view,
                   std::size_t index,
                   int component = 0) noexcept -> std::uint32_t;

auto json_vec3(QJsonValue const& value, QVector3D fallback) -> QVector3D;

auto json_quaternion(QJsonValue const& value) -> QQuaternion;

auto local_matrix(NodePose const& pose) -> QMatrix4x4;

void evaluate_world_matrices(std::span<const NodePose> poses,
                             std::span<const int> parents,
                             std::vector<QMatrix4x4>& out);

auto normalize_world_matrix(QMatrix4x4 matrix,
                            SourceConfig const& config) noexcept -> QMatrix4x4;

auto parse_source(SourceConfig const& config) -> SourceAsset;

auto source_asset() -> const SourceAsset&;

auto elephant_asset() -> const SourceAsset&;

void sample_channel(AnimationChannel const& channel, float time, NodePose& pose);

auto sample_source_clip(SourceAsset const& asset,
                        SourceConfig const& config,
                        std::string_view source_clip,
                        float normalized_phase,
                        std::span<QMatrix4x4> out) noexcept -> bool;

auto sample_source_slots(SourceAsset const& asset,
                         SourceConfig const& config,
                         std::string_view source_clip,
                         float normalized_phase,
                         std::span<const std::size_t> joint_slots,
                         std::span<QMatrix4x4> out) noexcept -> bool;

void rotate_elephant_subtree_about(std::span<QMatrix4x4> palette,
                                   std::span<const BoneDef> bones,
                                   std::size_t root,
                                   float degrees,
                                   const QVector3D& axis) noexcept;

void transform_elephant_palette(std::span<QMatrix4x4> palette,
                                std::size_t bone_count,
                                const QVector3D& pivot,
                                float degrees,
                                const QVector3D& axis) noexcept;

namespace ElephantRig {
constexpr std::size_t k_body = 1U;
constexpr std::size_t k_head = 3U;
constexpr std::array<std::size_t, 3> k_trunk{{4U, 5U, 6U}};
constexpr std::size_t k_ear_left = 7U;
constexpr std::size_t k_ear_right = 8U;
constexpr std::array<std::size_t, 4> k_upper{{9U, 12U, 15U, 18U}};
constexpr std::array<std::size_t, 4> k_lower{{10U, 13U, 16U, 19U}};
constexpr std::array<std::size_t, 4> k_feet{{11U, 14U, 17U, 20U}};
constexpr std::size_t k_tail = 21U;
const QVector3D k_pitch_axis{1.0F, 0.0F, 0.0F};
const QVector3D k_yaw_axis{0.0F, 1.0F, 0.0F};
const QVector3D k_roll_axis{0.0F, 0.0F, 1.0F};
} // namespace ElephantRig

void apply_elephant_secondary(SourceAsset const& asset,
                              const Animation::ElephantSecondaryMotion& motion,
                              std::span<QMatrix4x4> out) noexcept;

auto synthesise_elephant_locomotion(SourceAsset const& asset,
                                    float phase,
                                    bool fast,
                                    std::span<QMatrix4x4> out) noexcept -> bool;

auto synthesise_elephant_idle(SourceAsset const& asset,
                              float phase,
                              std::span<QMatrix4x4> out) noexcept -> bool;

auto synthesise_elephant_stomp(SourceAsset const& asset,
                               float phase,
                               std::span<QMatrix4x4> out) noexcept -> bool;

auto sample_elephant_locomotion(SourceAsset const& asset,
                                float phase,
                                bool fast,
                                std::span<QMatrix4x4> out) noexcept -> bool;

} // namespace Render::Creature::Quadruped::compiled_source_detail
