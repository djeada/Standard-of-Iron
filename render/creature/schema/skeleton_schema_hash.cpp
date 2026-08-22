#include "skeleton_schema_hash.h"

#include <QDebug>

#include <mutex>
#include <string>
#include <unordered_set>

namespace Render::Creature {

namespace {

constexpr std::uint64_t k_fnv_offset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t k_fnv_prime = 0x100000001b3ULL;

auto mix_bytes(std::uint64_t hash,
               const void* data,
               std::size_t size) noexcept -> std::uint64_t {
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= k_fnv_prime;
  }
  return hash;
}

auto mix_float(std::uint64_t hash, float value) noexcept -> std::uint64_t {

  const auto quantized = static_cast<std::int64_t>(value * 100000.0F);
  return mix_bytes(hash, &quantized, sizeof(quantized));
}

auto mix_vector(std::uint64_t hash, const QVector3D& value) noexcept -> std::uint64_t {
  hash = mix_float(hash, value.x());
  hash = mix_float(hash, value.y());
  return mix_float(hash, value.z());
}

} // namespace

auto skeleton_schema_hash(const SkeletonTopology& topology) noexcept
    -> SkeletonSchemaHash {
  std::uint64_t hash = k_fnv_offset;
  for (const auto& bone : topology.bones) {
    hash = mix_bytes(hash, bone.name.data(), bone.name.size());
    hash = mix_bytes(hash, &bone.parent, sizeof(bone.parent));
  }
  for (const auto& socket : topology.sockets) {
    hash = mix_bytes(hash, socket.name.data(), socket.name.size());
    hash = mix_bytes(hash, &socket.bone, sizeof(socket.bone));
    hash = mix_vector(hash, socket.local_offset);
    hash = mix_vector(hash, socket.local_right);
    hash = mix_vector(hash, socket.local_up);
    hash = mix_vector(hash, socket.local_forward);
  }
  return hash;
}

auto bone_parents_match(const SkeletonTopology& topology,
                        std::span<const std::uint8_t> bone_parents,
                        std::uint32_t bone_count) noexcept -> bool {
  if (bone_count != topology.bones.size()) {
    return false;
  }
  if (bone_parents.size() < topology.bones.size()) {
    return false;
  }
  constexpr std::uint8_t k_no_parent = 0xFFU;
  for (std::size_t i = 0; i < topology.bones.size(); ++i) {
    const auto expected = topology.bones[i].parent == k_invalid_bone
                              ? k_no_parent
                              : static_cast<std::uint8_t>(topology.bones[i].parent);
    if (bone_parents[i] != expected) {
      return false;
    }
  }
  return true;
}

void report_skeleton_schema_mismatch(std::string_view species,
                                     const SkeletonTopology& topology,
                                     std::span<const std::uint8_t> bone_parents,
                                     std::uint32_t bone_count) {
  static std::mutex mutex;
  static std::unordered_set<std::string> reported;
  const std::string key(species);
  {
    std::lock_guard<std::mutex> const lock(mutex);
    if (!reported.emplace(key).second) {
      return;
    }
  }
  qCritical().noquote() << "Baked animation does not match the runtime skeleton for"
                        << QString::fromStdString(key) << ": runtime has"
                        << static_cast<int>(topology.bones.size())
                        << "bones (schema hash"
                        << QString::number(skeleton_schema_hash(topology))
                        << "), the BPAT has" << static_cast<int>(bone_count) << "with"
                        << static_cast<int>(bone_parents.size())
                        << "parent entries. Re-bake the creature assets.";
}

} // namespace Render::Creature
