#include "static_attachment_spec.h"

#include <array>
#include <cstring>

namespace Render::Creature {

namespace {

constexpr std::uint64_t k_fnv_offset = 1469598103934665603ULL;
constexpr std::uint64_t k_fnv_prime = 1099511628211ULL;

inline auto
mix_bytes(std::uint64_t h, const void* bytes, std::size_t n) noexcept -> std::uint64_t {
  const auto* p = static_cast<const unsigned char*>(bytes);
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<std::uint64_t>(p[i]);
    h *= k_fnv_prime;
  }
  return h;
}

} // namespace

auto static_attachment_hash(const StaticAttachmentSpec& spec) noexcept
    -> std::uint64_t {
  std::uint64_t h = k_fnv_offset;
  const auto arch_bits = reinterpret_cast<std::uintptr_t>(spec.archetype);
  h = mix_bytes(h, &arch_bits, sizeof(arch_bits));
  h = mix_bytes(h, &spec.socket_bone_index, sizeof(spec.socket_bone_index));

  h = mix_bytes(h, spec.local_offset.constData(), sizeof(float) * 16);
  h = mix_bytes(h, spec.palette_role_remap.data(), spec.palette_role_remap.size());
  h = mix_bytes(h, &spec.override_color_role, sizeof(spec.override_color_role));
  h = mix_bytes(h, &spec.uniform_scale, sizeof(spec.uniform_scale));
  h = mix_bytes(h, &spec.material_id, sizeof(spec.material_id));
  if (spec.drape.enabled) {
    const auto& d = spec.drape;
    h = mix_bytes(h, &d.pelvis_bone, sizeof(d.pelvis_bone));
    h = mix_bytes(h, &d.leg_l_bone, sizeof(d.leg_l_bone));
    h = mix_bytes(h, &d.leg_r_bone, sizeof(d.leg_r_bone));
    const std::array<float, 4> params{
        d.top_y, d.bottom_y, d.leg_share, d.leg_crossfade_half_width};
    h = mix_bytes(h, params.data(), sizeof(float) * params.size());
  }
  return h;
}

auto static_attachments_hash(const StaticAttachmentSpec* attachments,
                             std::size_t count) noexcept -> std::uint64_t {
  std::uint64_t h = k_fnv_offset ^ (count * 0x9E3779B97F4A7C15ULL);
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint64_t entry = static_attachment_hash(attachments[i]);
    h = mix_bytes(h, &entry, sizeof(entry));
  }
  return h;
}

auto static_attachment_equal(const StaticAttachmentSpec& a,
                             const StaticAttachmentSpec& b) noexcept -> bool {
  if (a.archetype != b.archetype || a.socket_bone_index != b.socket_bone_index ||
      a.override_color_role != b.override_color_role ||
      a.uniform_scale != b.uniform_scale || a.material_id != b.material_id ||
      a.drape.enabled != b.drape.enabled) {
    return false;
  }
  if (std::memcmp(a.palette_role_remap.data(),
                  b.palette_role_remap.data(),
                  a.palette_role_remap.size()) != 0) {
    return false;
  }
  if (a.drape.enabled) {
    const auto& x = a.drape;
    const auto& y = b.drape;
    if (x.pelvis_bone != y.pelvis_bone || x.leg_l_bone != y.leg_l_bone ||
        x.leg_r_bone != y.leg_r_bone || x.top_y != y.top_y ||
        x.bottom_y != y.bottom_y || x.leg_share != y.leg_share ||
        x.leg_crossfade_half_width != y.leg_crossfade_half_width) {
      return false;
    }
  }
  return std::memcmp(a.local_offset.constData(),
                     b.local_offset.constData(),
                     sizeof(float) * 16) == 0;
}

} // namespace Render::Creature
