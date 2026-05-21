#include "bpat_playback.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "../bpat/bpat_format.h"
#include "../bpat/bpat_registry.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_terminal_non_looping_phase = std::nextafter(1.0F, 0.0F);

auto marker_for_name(std::string_view clip_name) noexcept -> ClipMarkerSet {
  ClipMarkerSet markers{};
  if (clip_name.find("attack_sword") != std::string_view::npos ||
      clip_name.find("attack_spear") != std::string_view::npos ||
      clip_name.find("riding_sword") != std::string_view::npos) {
    markers.anticipation_start = 0.10F;
    markers.weapon_release = 0.54F;
    markers.contact = 0.58F;
    markers.recover_unlocked = 0.72F;
    markers.exit_safe = 0.90F;
    return markers;
  }
  if (clip_name.find("attack_bow") != std::string_view::npos ||
      clip_name.find("bow_shot") != std::string_view::npos) {
    markers.anticipation_start = 0.18F;
    markers.weapon_release = 0.56F;
    markers.contact = 0.56F;
    markers.recover_unlocked = 0.66F;
    markers.exit_safe = 0.86F;
    return markers;
  }
  if (clip_name.find("hold") != std::string_view::npos) {
    markers.anticipation_start = 0.0F;
    markers.exit_safe = 0.98F;
    return markers;
  }
  if (clip_name.find("walk") != std::string_view::npos ||
      clip_name.find("run") != std::string_view::npos ||
      clip_name.find("idle") != std::string_view::npos ||
      clip_name.find("riding_idle") != std::string_view::npos ||
      clip_name.find("riding_charge") != std::string_view::npos) {
    markers.exit_safe = 0.98F;
    return markers;
  }
  return markers;
}

} // namespace

auto normalize_bpat_phase(float phase, bool loops) noexcept -> float {
  if (!loops) {
    return std::clamp(phase, 0.0F, k_terminal_non_looping_phase);
  }

  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto resolve_bpat_playback(const Render::Creature::Bpat::BpatBlob* blob,
                           std::uint16_t clip_id,
                           float phase) noexcept -> ResolvedClipPlayback {
  ResolvedClipPlayback resolved{};
  if (blob == nullptr || clip_id == k_invalid_bpat_clip ||
      clip_id >= blob->clip_count()) {
    return resolved;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return resolved;
  }

  float const normalized_phase = normalize_bpat_phase(phase, clip.loops != 0U);
  int frame_idx =
      static_cast<int>(normalized_phase * static_cast<float>(clip.frame_count));
  frame_idx = std::clamp(frame_idx, 0, static_cast<int>(clip.frame_count) - 1);

  resolved.blob = blob;
  resolved.clip_id = clip_id;
  resolved.frame_count = clip.frame_count;
  resolved.loops = clip.loops != 0U;
  resolved.normalized_phase = normalized_phase;
  resolved.frame_in_clip = static_cast<std::uint32_t>(frame_idx);
  resolved.global_frame = clip.frame_offset + resolved.frame_in_clip;
  return resolved;
}

auto resolve_bpat_playback(std::uint32_t species_id,
                           std::uint16_t clip_id,
                           float phase) noexcept -> ResolvedClipPlayback {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  return resolve_bpat_playback(blob, clip_id, phase);
}

auto clip_marker_set(std::uint32_t species_id,
                     std::uint16_t clip_id) noexcept -> ClipMarkerSet {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || clip_id == k_invalid_bpat_clip ||
      clip_id >= blob->clip_count()) {
    return {};
  }
  return marker_for_name(blob->clip(clip_id).name);
}

auto clip_marker_phase(std::uint32_t species_id,
                       std::uint16_t clip_id,
                       ClipMarker marker) noexcept -> std::optional<float> {
  auto const markers = clip_marker_set(species_id, clip_id);
  float phase = -1.0F;
  switch (marker) {
  case ClipMarker::AnticipationStart:
    phase = markers.anticipation_start;
    break;
  case ClipMarker::WeaponRelease:
    phase = markers.weapon_release;
    break;
  case ClipMarker::Contact:
    phase = markers.contact;
    break;
  case ClipMarker::RecoverUnlocked:
    phase = markers.recover_unlocked;
    break;
  case ClipMarker::ExitSafe:
    phase = markers.exit_safe;
    break;
  }
  if (phase < 0.0F) {
    return std::nullopt;
  }
  return phase;
}

auto blend_weight_bucket(float weight) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>(std::clamp(weight, 0.0F, 1.0F) * 15.99F);
}

} // namespace Render::Creature::Pipeline
