#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <string_view>

#include "building_archetype_desc.h"
#include "building_render_common.h"
#include "building_state.h"
#include "game/core/component_economy.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

inline constexpr int k_farm_render_stage_count = 5;
static_assert(k_farm_render_stage_count == Engine::Core::k_farm_growth_stage_count);

using FarmArchetypeResolver = const RenderArchetype& (*)(BuildingState, int stage);

struct FarmRendererConfig {
  std::string_view nation_slug;
  FarmArchetypeResolver archetype;
  BuildingSelectionStyle selection;
};

void register_farm_renderer_variant(EntityRendererRegistry& registry,
                                    const FarmRendererConfig& config);

struct FarmFieldPalette {
  QVector3D soil{0.36F, 0.25F, 0.15F};
  QVector3D soil_dark{0.24F, 0.16F, 0.10F};
  QVector3D soil_light{0.47F, 0.34F, 0.21F};
  QVector3D sprout{0.42F, 0.66F, 0.24F};
  QVector3D leaf{0.34F, 0.58F, 0.20F};
  QVector3D leaf_dark{0.24F, 0.42F, 0.15F};
  QVector3D stalk_green{0.55F, 0.66F, 0.26F};
  QVector3D stalk_gold{0.87F, 0.80F, 0.51F};
  QVector3D stalk_gold_dark{0.65F, 0.57F, 0.32F};
  QVector3D head_green{0.58F, 0.66F, 0.29F};
  QVector3D head_gold{0.91F, 0.75F, 0.36F};
  QVector3D head_gold_light{0.98F, 0.88F, 0.56F};
  QVector3D head_gold_dark{0.69F, 0.54F, 0.26F};
  QVector3D awn{0.94F, 0.87F, 0.62F};
  QVector3D stubble{0.74F, 0.62F, 0.34F};
  QVector3D ash{0.16F, 0.14F, 0.12F};
};

struct FarmFieldSpec {
  QVector3D center{0.0F, 0.0F, 0.0F};
  float half_x{0.90F};
  float half_z{0.62F};
  float ground_y{0.0F};
  int rows{7};
  int stalks_per_row{15};
  int seed{7};
  bool rows_along_x{true};
};

void add_farm_field(BuildingArchetypeDesc& desc,
                    const FarmFieldSpec& spec,
                    const FarmFieldPalette& palette,
                    int stage);

void add_farm_scarecrow(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        const QVector3D& timber,
                        const QVector3D& cloth,
                        const QVector3D& straw);

template <typename Builder>
auto build_farm_archetype_table(Builder&& builder)
    -> std::array<std::array<RenderArchetype, 3>, k_farm_render_stage_count> {
  std::array<std::array<RenderArchetype, 3>, k_farm_render_stage_count> table{};
  for (int stage = 0; stage < k_farm_render_stage_count; ++stage) {
    table[static_cast<std::size_t>(stage)] = {
        builder(BuildingState::Normal, stage),
        builder(BuildingState::Damaged, stage),
        builder(BuildingState::Destroyed, stage),
    };
  }
  return table;
}

auto farm_archetype_from_table(
    const std::array<std::array<RenderArchetype, 3>, k_farm_render_stage_count>& table,
    BuildingState state,
    int stage) -> const RenderArchetype&;

} // namespace Render::GL
