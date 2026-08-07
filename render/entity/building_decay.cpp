#include "building_decay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL {
namespace {

const QVector3D k_ash{0.19F, 0.175F, 0.16F};
const QVector3D k_rot{0.21F, 0.25F, 0.15F};
const QVector3D k_dust{0.45F, 0.42F, 0.38F};

auto smooth_step(float edge0, float edge1, float x) -> float {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto mix(const QVector3D& a, const QVector3D& b, float t) -> QVector3D {
  return a * (1.0F - t) + b * t;
}

auto luma_of(const QVector3D& c) -> float {
  return (0.299F * c.x()) + (0.587F * c.y()) + (0.114F * c.z());
}

auto clamp01(const QVector3D& c) -> QVector3D {
  return {std::clamp(c.x(), 0.0F, 1.0F),
          std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}

auto hash_at(int seed, int salt) -> float {
  return decay_hash((seed * 733) + (salt * 197) + 17);
}

auto signed_hash(int seed, int salt) -> float {
  return (hash_at(seed, salt) * 2.0F) - 1.0F;
}

} // namespace

auto decay_hash(int seed) -> float {
  const float n = std::sin(static_cast<float>(seed) * 12.9898F) * 43758.5453F;
  return n - std::floor(n);
}

auto part_supports_state(BuildingStateMask mask, BuildingState state) -> bool {
  const auto bit = (state == BuildingState::Normal)    ? BuildingStateMask::Normal
                   : (state == BuildingState::Damaged) ? BuildingStateMask::Damaged
                                                       : BuildingStateMask::Destroyed;
  return (mask & bit) != BuildingStateMask::None;
}

auto building_decay_profile(BuildingState state) -> BuildingDecayProfile {
  switch (state) {
  case BuildingState::Normal:
    return {};
  case BuildingState::Damaged:
    return {.desaturation = 0.34F,
            .darkening = 0.85F,
            .soot = 0.22F,
            .moss = 0.09F,
            .dust = 0.12F,
            .variation = 0.06F};
  case BuildingState::Destroyed:
    return {.desaturation = 0.58F,
            .darkening = 0.74F,
            .soot = 0.36F,
            .moss = 0.20F,
            .dust = 0.30F,
            .variation = 0.10F};
  }
  return {};
}

auto decayed_color(const QVector3D& base, BuildingState state, int seed) -> QVector3D {
  const BuildingDecayProfile profile = building_decay_profile(state);
  if (profile.desaturation <= 0.0F && profile.soot <= 0.0F && profile.moss <= 0.0F &&
      profile.dust <= 0.0F && profile.darkening >= 1.0F) {
    return base;
  }

  const float luma = luma_of(base);
  QVector3D color = mix(base, QVector3D(luma, luma, luma), profile.desaturation);

  const float bright_t = smooth_step(0.10F, 0.68F, luma);
  color *= 1.0F - ((1.0F - profile.darkening) * bright_t);

  const float soot_mask = std::clamp((hash_at(seed, 1) - 0.25F) * 1.7F, 0.0F, 1.0F);
  color = mix(color, k_ash, profile.soot * soot_mask);

  const float rot_mask = std::clamp((hash_at(seed, 2) - 0.55F) * 2.4F, 0.0F, 1.0F);
  color = mix(color, k_rot, profile.moss * rot_mask);

  const float dust_t = smooth_step(0.16F, 0.62F, luma);
  color =
      mix(color, k_dust, profile.dust * dust_t * (1.0F - soot_mask) * hash_at(seed, 4));

  const float jitter = 1.0F + (signed_hash(seed, 3) * profile.variation);
  return clamp01(color * jitter);
}

void add_rubble_field(BuildingArchetypeDesc& desc, const RubbleField& field) {
  for (int i = 0; i < field.count; ++i) {
    const int seed = field.seed + (i * 31);
    const auto spread = [&](float raw) {
      if (field.ring_bias <= 0.0F) {
        return raw;
      }
      const float magnitude =
          field.ring_bias + ((1.0F - field.ring_bias) * std::abs(raw));
      return raw < 0.0F ? -magnitude : magnitude;
    };
    const float x =
        field.center.x() + (spread(signed_hash(seed, 11)) * field.extent.x());
    const float z =
        field.center.z() + (spread(signed_hash(seed, 12)) * field.extent.z());
    const float size = (0.055F + (hash_at(seed, 13) * 0.085F)) * field.chunk_scale;
    const float height = size * (0.55F + (hash_at(seed, 14) * 0.75F));
    const float yaw = hash_at(seed, 15) * 90.0F;
    const float tilt = signed_hash(seed, 16) * 22.0F;
    const QVector3D color = mix(field.stone_dark, field.stone, hash_at(seed, 17)) *
                            (0.86F + (hash_at(seed, 18) * 0.24F));

    desc.add_rotated_box(QVector3D(x, field.ground_y + height, z),
                         QVector3D(size, height, size * 0.82F),
                         QVector3D(tilt, yaw, tilt * 0.6F),
                         clamp01(color),
                         field.states,
                         field.lod);
  }
}

void add_charred_beams(BuildingArchetypeDesc& desc, const CharredBeams& beams) {
  for (int i = 0; i < beams.count; ++i) {
    const int seed = beams.seed + (i * 47);
    const float x = beams.center.x() + (signed_hash(seed, 21) * beams.extent.x());
    const float z = beams.center.z() + (signed_hash(seed, 22) * beams.extent.z());
    const float angle = hash_at(seed, 23) * 6.2831853F;
    const float half = beams.length * (0.5F + (hash_at(seed, 24) * 0.35F));
    const float lift = beams.radius * (1.0F + (hash_at(seed, 25) * 2.6F));
    const QVector3D dir(std::cos(angle) * half,
                        hash_at(seed, 26) * beams.length * 0.22F,
                        std::sin(angle) * half);
    const QVector3D color =
        clamp01(beams.timber * (0.80F + (hash_at(seed, 27) * 0.45F)));

    desc.add_cylinder(QVector3D(x, beams.ground_y + lift, z) - dir,
                      QVector3D(x, beams.ground_y + lift, z) + dir,
                      beams.radius * (0.75F + (hash_at(seed, 28) * 0.6F)),
                      color,
                      beams.states,
                      beams.lod);
  }
}

void add_scorch_patch(BuildingArchetypeDesc& desc, const ScorchPatch& patch) {
  for (int i = 0; i < patch.count; ++i) {
    const int seed = patch.seed + (i * 53);
    const float x = patch.center.x() + (signed_hash(seed, 31) * patch.radius * 0.62F);
    const float z = patch.center.z() + (signed_hash(seed, 32) * patch.radius * 0.62F);
    const float extent = patch.radius * (0.10F + (hash_at(seed, 33) * 0.17F));
    const float fade = 0.75F + (hash_at(seed, 34) * 0.75F);

    desc.add_rotated_box(QVector3D(x, patch.ground_y + 0.006F, z),
                         QVector3D(extent, 0.004F, extent * 0.78F),
                         QVector3D(0.0F, hash_at(seed, 35) * 90.0F, 0.0F),
                         clamp01(patch.soot * fade),
                         patch.states,
                         patch.lod);
  }
}

void add_crack_veins(BuildingArchetypeDesc& desc, const CrackVeins& veins) {
  for (int i = 0; i < veins.count; ++i) {
    const int seed = veins.seed + (i * 59);
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(veins.count);
    const float wander = signed_hash(seed, 41) * 0.22F;

    const QVector3D along = veins.span * ((t - 0.5F) + (wander * 0.12F));
    const QVector3D base(
        veins.origin.x() + along.x(), veins.origin.y(), veins.origin.z() + along.z());
    const float run = veins.span.y() * (0.35F + (hash_at(seed, 42) * 0.5F));
    const float lean = signed_hash(seed, 43) * 26.0F;

    const QVector3D half_size = (std::abs(veins.span.x()) >= std::abs(veins.span.z()))
                                    ? QVector3D(veins.width, run, veins.width * 0.6F)
                                    : QVector3D(veins.width * 0.6F, run, veins.width);

    desc.add_rotated_box(base + QVector3D(0.0F, run, 0.0F),
                         half_size,
                         QVector3D(0.0F, 0.0F, lean),
                         clamp01(veins.color),
                         veins.states,
                         veins.lod);
  }
}

void add_broken_rim(BuildingArchetypeDesc& desc, const BrokenRim& rim) {
  for (int i = 0; i < rim.count; ++i) {
    const int seed = rim.seed + (i * 61);
    const float angle =
        (6.2831853F * static_cast<float>(i) / static_cast<float>(rim.count)) +
        (signed_hash(seed, 61) * 0.22F);
    const float radius = rim.radius * (0.94F + (hash_at(seed, 62) * 0.12F));
    const float half = rim.chunk_half * (0.65F + (hash_at(seed, 63) * 0.8F));
    const float rise = rim.rise * (0.3F + (hash_at(seed, 64) * 1.1F));

    desc.add_rotated_box(QVector3D(std::sin(angle) * radius,
                                   rim.center.y() + rise,
                                   std::cos(angle) * radius) +
                             QVector3D(rim.center.x(), 0.0F, rim.center.z()),
                         QVector3D(half, rise, half * 0.8F),
                         QVector3D(signed_hash(seed, 65) * 12.0F,
                                   -angle * 57.2957795F,
                                   signed_hash(seed, 66) * 10.0F),
                         clamp01(rim.color * (0.85F + (hash_at(seed, 67) * 0.28F))),
                         rim.states,
                         rim.lod);
  }
}

void add_ruin_dressing(BuildingArchetypeDesc& desc, const RuinDressing& dressing) {
  const QVector3D apron =
      dressing.apron_extent.isNull() ? dressing.extent * 1.05F : dressing.apron_extent;
  const auto both = static_cast<BuildingStateMask>(
      static_cast<std::uint8_t>(BuildingStateMask::Damaged) |
      static_cast<std::uint8_t>(BuildingStateMask::Destroyed));

  add_scorch_patch(
      desc,
      ScorchPatch{.center = dressing.center,
                  .radius = std::max(dressing.extent.x(), dressing.extent.z()) * 0.95F,
                  .ground_y = dressing.ground_y,
                  .count = 4,
                  .seed = dressing.seed + 601,
                  .states = both});

  add_rubble_field(desc,
                   RubbleField{.center = dressing.center,
                               .extent = apron,
                               .stone = dressing.stone,
                               .stone_dark = dressing.stone_dark,
                               .ground_y = dressing.apron_y,
                               .chunk_scale = dressing.scale * 0.85F,
                               .ring_bias = 0.78F,
                               .count = 8,
                               .seed = dressing.seed + 17,
                               .states = BuildingStateMask::Damaged});

  add_rubble_field(desc,
                   RubbleField{.center = dressing.center,
                               .extent = dressing.extent,
                               .stone = dressing.stone,
                               .stone_dark = dressing.stone_dark,
                               .ground_y = dressing.ground_y,
                               .chunk_scale = dressing.scale * 1.35F,
                               .ring_bias = 0.35F,
                               .count = 18,
                               .seed = dressing.seed + 233,
                               .states = BuildingStateMask::Destroyed});

  add_charred_beams(desc,
                    CharredBeams{.center = dressing.center,
                                 .extent = dressing.extent * 0.58F,
                                 .timber = dressing.timber,
                                 .ground_y = dressing.ground_y,
                                 .length = 0.85F * dressing.scale,
                                 .radius = 0.052F * dressing.scale,
                                 .count = 5,
                                 .seed = dressing.seed + 419,
                                 .states = BuildingStateMask::Destroyed});

  if (!dressing.collapsed_roof) {
    return;
  }

  for (int i = 0; i < 3; ++i) {
    const int seed = dressing.seed + 811 + (i * 37);
    const float x = dressing.center.x() + (signed_hash(seed, 51) * dressing.extent.x());
    const float z = dressing.center.z() + (signed_hash(seed, 52) * dressing.extent.z());
    const float half = (0.18F + (hash_at(seed, 53) * 0.20F)) * dressing.scale;
    const QVector3D slab =
        clamp01(mix(dressing.stone_dark, dressing.timber, hash_at(seed, 54)));

    desc.add_rotated_box(QVector3D(x, dressing.ground_y + (half * 0.32F), z),
                         QVector3D(half, 0.028F * dressing.scale, half * 0.72F),
                         QVector3D(signed_hash(seed, 55) * 34.0F,
                                   hash_at(seed, 56) * 90.0F,
                                   signed_hash(seed, 57) * 30.0F),
                         slab,
                         BuildingStateMask::Destroyed,
                         BuildingLODMask::Full);
  }
}

} // namespace Render::GL
