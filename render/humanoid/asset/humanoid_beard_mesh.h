#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "render/creature/part_graph.h"
#include "render/gl/humanoid/humanoid_types.h"

namespace Render::GL {
class Mesh;
}

namespace Render::Humanoid {

enum class HumanoidBodyVariant : std::uint8_t {
  Clean = 0,
  ShortBeard,
  FullBeard,
  LongGoatee,
  MustacheBeard,
  Count
};

inline constexpr std::size_t k_humanoid_body_variant_count =
    static_cast<std::size_t>(HumanoidBodyVariant::Count);

inline constexpr float k_beard_head_silhouette_radius = 0.168F;

[[nodiscard]] auto body_variant_for_facial_hair(
    Render::GL::FacialHairStyle style) noexcept -> HumanoidBodyVariant;

[[nodiscard]] auto
body_variant_name_suffix(HumanoidBodyVariant variant) noexcept -> std::string_view;

[[nodiscard]] auto build_humanoid_beard_mesh(HumanoidBodyVariant variant,
                                             Render::Creature::CreatureLOD lod)
    -> std::unique_ptr<Render::GL::Mesh>;

} // namespace Render::Humanoid
