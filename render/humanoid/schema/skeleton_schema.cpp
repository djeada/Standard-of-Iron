#include "render/humanoid/schema/skeleton_schema.h"

#include <QVector3D>

#include <array>
#include <span>

namespace Render::Humanoid {

namespace {

namespace Creature = Render::Creature;

constexpr std::array<std::string_view, k_bone_count> k_bone_names = {
    "Root",      "Pelvis",   "Spine", "Chest",     "Neck",      "Head",     "ShoulderL",
    "UpperArmL", "ForearmL", "HandL", "ShoulderR", "UpperArmR", "ForearmR", "HandR",
    "HipL",      "KneeL",    "FootL", "HipR",      "KneeR",     "FootR",
};

const std::array<SocketDef, k_socket_count> k_socket_defs = {{
    {"Head",
     static_cast<Creature::BoneIndex>(HumanoidBone::Head),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"HandR",
     static_cast<Creature::BoneIndex>(HumanoidBone::HandR),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"HandL",
     static_cast<Creature::BoneIndex>(HumanoidBone::HandL),
     QVector3D(0.0F, 0.00F, 0.0F)},
    {"GripR",
     static_cast<Creature::BoneIndex>(HumanoidBone::HandR),
     QVector3D(0.0F, 0.00F, 0.0F),
     QVector3D(1.0F, 0.0F, 0.0F),
     QVector3D(0.0F, 1.0F, 0.0F),
     QVector3D(0.0F, 0.0F, 1.0F)},
    {"GripL",
     static_cast<Creature::BoneIndex>(HumanoidBone::HandL),
     QVector3D(0.0F, 0.00F, 0.0F),
     QVector3D(-1.0F, 0.0F, 0.0F),
     QVector3D(0.0F, 1.0F, 0.0F),
     QVector3D(0.0F, 0.0F, -1.0F)},
    {"Back",
     static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.10F, -0.12F)},
    {"HipL",
     static_cast<Creature::BoneIndex>(HumanoidBone::HipL),
     QVector3D(0.0F, -0.02F, 0.0F)},
    {"HipR",
     static_cast<Creature::BoneIndex>(HumanoidBone::HipR),
     QVector3D(0.0F, -0.02F, 0.0F)},
    {"ChestFront",
     static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.05F, 0.15F)},
    {"ChestBack",
     static_cast<Creature::BoneIndex>(HumanoidBone::Chest),
     QVector3D(0.0F, 0.05F, -0.15F)},
    {"FootL",
     static_cast<Creature::BoneIndex>(HumanoidBone::FootL),
     QVector3D(0.0F, 0.0F, 0.0F)},
    {"FootR",
     static_cast<Creature::BoneIndex>(HumanoidBone::FootR),
     QVector3D(0.0F, 0.0F, 0.0F)},
}};

constexpr std::uint64_t k_fnv_offset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t k_fnv_prime = 0x100000001b3ULL;

constexpr auto mix_bytes(std::uint64_t hash,
                         const unsigned char* data,
                         std::size_t size) noexcept -> std::uint64_t {
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= data[i];
    hash *= k_fnv_prime;
  }
  return hash;
}

auto mix_float(std::uint64_t hash, float value) noexcept -> std::uint64_t {

  const auto quantized = static_cast<std::int64_t>(value * 100000.0F);
  const auto bytes = static_cast<std::uint64_t>(quantized);
  for (int i = 0; i < 8; ++i) {
    hash ^= (bytes >> (i * 8)) & 0xFFULL;
    hash *= k_fnv_prime;
  }
  return hash;
}

auto mix_vector(std::uint64_t hash, const QVector3D& value) noexcept -> std::uint64_t {
  hash = mix_float(hash, value.x());
  hash = mix_float(hash, value.y());
  return mix_float(hash, value.z());
}

} // namespace

auto humanoid_topology() noexcept -> const Creature::SkeletonTopology& {
  static const std::array<Creature::BoneDef, k_bone_count> bones = [] {
    std::array<Creature::BoneDef, k_bone_count> out{};
    for (std::size_t i = 0; i < k_bone_count; ++i) {
      out[i].name = k_bone_names[i];
      out[i].parent = (k_bone_parents[i] == k_invalid_bone)
                          ? Creature::k_invalid_bone
                          : static_cast<Creature::BoneIndex>(k_bone_parents[i]);
    }
    return out;
  }();

  static const Creature::SkeletonTopology topo{
      std::span<const Creature::BoneDef>(bones.data(), bones.size()),
      std::span<const Creature::SocketDef>(k_socket_defs.data(), k_socket_defs.size()),
  };
  return topo;
}

auto bone_name(HumanoidBone bone) noexcept -> std::string_view {
  auto const i = static_cast<std::size_t>(bone);
  return i < k_bone_count ? k_bone_names[i] : std::string_view{"<invalid>"};
}

auto parent_of(HumanoidBone bone) noexcept -> std::uint8_t {
  auto const i = static_cast<std::size_t>(bone);
  return i < k_bone_count ? k_bone_parents[i] : k_invalid_bone;
}

auto socket_def(HumanoidSocket socket) noexcept -> const SocketDef& {
  auto const i = static_cast<std::size_t>(socket);
  static const SocketDef k_default{
      "<invalid>", static_cast<Creature::BoneIndex>(HumanoidBone::Root), QVector3D()};
  if (i >= k_socket_count) {
    return k_default;
  }
  return k_socket_defs[i];
}

auto socket_bone(HumanoidSocket socket) noexcept -> HumanoidBone {
  return static_cast<HumanoidBone>(socket_def(socket).bone);
}

auto humanoid_skeleton_schema_hash() noexcept -> HumanoidSkeletonSchemaHash {
  static const HumanoidSkeletonSchemaHash hash = [] {
    std::uint64_t value = k_fnv_offset;
    for (std::size_t i = 0; i < k_bone_count; ++i) {
      const auto name = k_bone_names[i];
      value = mix_bytes(
          value, reinterpret_cast<const unsigned char*>(name.data()), name.size());
      const unsigned char parent = k_bone_parents[i];
      value = mix_bytes(value, &parent, 1);
    }
    for (const auto& def : k_socket_defs) {
      value = mix_bytes(value,
                        reinterpret_cast<const unsigned char*>(def.name.data()),
                        def.name.size());
      const auto bone = static_cast<std::uint64_t>(def.bone);
      for (int i = 0; i < 2; ++i) {
        const auto byte = static_cast<unsigned char>((bone >> (i * 8)) & 0xFFULL);
        value = mix_bytes(value, &byte, 1);
      }
      value = mix_vector(value, def.local_offset);
      value = mix_vector(value, def.local_right);
      value = mix_vector(value, def.local_up);
      value = mix_vector(value, def.local_forward);
    }
    return value;
  }();
  return hash;
}

} // namespace Render::Humanoid
