#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::Creature::Bpat {

inline constexpr std::array<std::uint8_t, 4> k_magic{'B', 'P', 'A', 'T'};
inline constexpr std::uint32_t k_version = 2U;

inline constexpr std::uint32_t k_species_humanoid = 0U;
inline constexpr std::uint32_t k_species_horse = 1U;
inline constexpr std::uint32_t k_species_elephant = 2U;
inline constexpr std::uint32_t k_species_humanoid_sword = 3U;
inline constexpr std::uint32_t k_species_humanoid_spear = 4U;
inline constexpr std::uint32_t k_species_humanoid_skeleton = 5U;
inline constexpr std::uint32_t k_species_count = 6U;
inline constexpr std::uint32_t k_max_species_id = k_species_humanoid_skeleton;

inline constexpr std::size_t k_matrix_floats = 16U;
inline constexpr std::size_t k_socket_matrix_floats = 12U;
inline constexpr std::size_t k_section_alignment = 16U;

struct BpatHeader {
  std::uint8_t magic[4];
  std::uint32_t version;
  std::uint32_t species_id;
  std::uint32_t bone_count;
  std::uint32_t socket_count;
  std::uint32_t clip_count;
  std::uint32_t frame_total;
  std::uint32_t string_table_size;
  std::uint64_t clip_table_offset;
  std::uint64_t socket_table_offset;
  std::uint64_t string_table_offset;
  std::uint64_t palette_data_offset;
};

static_assert(sizeof(BpatHeader) == 64, "BpatHeader must be exactly 64 bytes");
static_assert(alignof(BpatHeader) == 8, "BpatHeader must be 8-byte aligned");

struct BpatClipEntry {
  std::uint32_t name_offset;
  std::uint32_t name_length;
  std::uint32_t frame_count;
  std::uint32_t frame_offset;
  float fps;
  std::uint8_t loops;
  std::uint8_t pad[3];

  float marker_anticipation_start;
  float marker_weapon_release;
  float marker_contact;
  float marker_recover_unlocked;
  float marker_exit_safe;
};

static_assert(sizeof(BpatClipEntry) == 44, "BpatClipEntry must be exactly 44 bytes");

struct BpatSocketEntry {
  std::uint32_t name_offset;
  std::uint32_t name_length;
  std::uint32_t anchor_bone;
  float local_offset[3];
  std::uint32_t reserved[2];
};

static_assert(sizeof(BpatSocketEntry) == 32,
              "BpatSocketEntry must be exactly 32 bytes");

[[nodiscard]] constexpr auto
align_up(std::uint64_t value, std::uint64_t alignment) noexcept -> std::uint64_t {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

} // namespace Render::Creature::Bpat
