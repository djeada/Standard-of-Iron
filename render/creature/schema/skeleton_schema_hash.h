#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "render/creature/skeleton.h"

namespace Render::Creature {

using SkeletonSchemaHash = std::uint64_t;

[[nodiscard]] auto
skeleton_schema_hash(const SkeletonTopology& topology) noexcept -> SkeletonSchemaHash;

[[nodiscard]] auto bone_parents_match(const SkeletonTopology& topology,
                                      std::span<const std::uint8_t> bone_parents,
                                      std::uint32_t bone_count) noexcept -> bool;

void report_skeleton_schema_mismatch(std::string_view species,
                                     const SkeletonTopology& topology,
                                     std::span<const std::uint8_t> bone_parents,
                                     std::uint32_t bone_count);

} // namespace Render::Creature
