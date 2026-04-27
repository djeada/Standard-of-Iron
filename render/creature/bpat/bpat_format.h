#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::Creature::Bpat {

inline constexpr std::array<std::uint8_t, 4> kMagic{'B', 'P', 'A', 'T'};
inline constexpr std::uint32_t kVersion = 1U;

inline constexpr std::uint32_t kSpeciesHumanoid = 0U;
inline constexpr std::uint32_t kSpeciesHorse = 1U;
inline constexpr std::uint32_t kSpeciesElephant = 2U;
inline constexpr std::uint32_t kSpeciesHumanoidSword = 3U;
inline constexpr std::uint32_t kSpeciesCount = 4U;
inline constexpr std::uint32_t kMaxSpeciesId = kSpeciesHumanoidSword;

inline constexpr std::size_t kMatrixFloats = 16U;
inline constexpr std::size_t kSocketMatrixFloats = 12U;
inline constexpr std::size_t kSectionAlignment = 16U;

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
  std::uint32_t reserved[2];
};

static_assert(sizeof(BpatClipEntry) == 32,
              "BpatClipEntry must be exactly 32 bytes");

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
align_up(std::uint64_t value,
         std::uint64_t alignment) noexcept -> std::uint64_t {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

} // namespace Render::Creature::Bpat
