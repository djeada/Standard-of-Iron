#pragma once

#include <cstdint>
#include <optional>

namespace Render::Creature::Bpat {
class BpatBlob;
}

namespace Render::Creature::Pipeline {

inline constexpr std::uint16_t k_invalid_bpat_clip = 0xFFFFu;

struct BpatPlayback {
  std::uint16_t clip_id{0U};
  std::uint16_t frame_in_clip{0U};
};

enum class ClipMarker : std::uint8_t {
  AnticipationStart = 0,
  WeaponRelease,
  Contact,
  RecoverUnlocked,
  ExitSafe,
};

struct ClipMarkerSet {
  float anticipation_start{-1.0F};
  float weapon_release{-1.0F};
  float contact{-1.0F};
  float recover_unlocked{-1.0F};
  float exit_safe{-1.0F};
};

struct ResolvedClipPlayback {
  const Render::Creature::Bpat::BpatBlob* blob{nullptr};
  std::uint16_t clip_id{0xFFFFu};
  std::uint32_t global_frame{0U};
  std::uint32_t frame_in_clip{0U};
  std::uint32_t next_global_frame{0U};
  std::uint32_t next_frame_in_clip{0U};
  std::uint32_t frame_count{0U};
  float normalized_phase{0.0F};
  float frame_lerp{0.0F};
  bool loops{false};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return blob != nullptr && clip_id != k_invalid_bpat_clip && frame_count > 0U;
  }
};

[[nodiscard]] auto normalize_bpat_phase(float phase, bool loops) noexcept -> float;

[[nodiscard]] auto resolve_bpat_playback(const Render::Creature::Bpat::BpatBlob* blob,
                                         std::uint16_t clip_id,
                                         float phase) noexcept -> ResolvedClipPlayback;

[[nodiscard]] auto resolve_bpat_playback(std::uint32_t species_id,
                                         std::uint16_t clip_id,
                                         float phase) noexcept -> ResolvedClipPlayback;

[[nodiscard]] auto clip_marker_set(std::uint32_t species_id,
                                   std::uint16_t clip_id) noexcept -> ClipMarkerSet;

[[nodiscard]] auto
clip_marker_phase(std::uint32_t species_id,
                  std::uint16_t clip_id,
                  ClipMarker marker) noexcept -> std::optional<float>;

[[nodiscard]] auto blend_weight_bucket(float weight) noexcept -> std::uint8_t;

} // namespace Render::Creature::Pipeline
