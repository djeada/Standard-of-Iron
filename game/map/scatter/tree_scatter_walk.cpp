#include "tree_scatter_walk.h"

#include <array>
#include <cstddef>

namespace Render::Ground {
namespace {

using Game::Map::TreeSpecies;
using Game::Map::WorldProp;
using Channel = SceneChannel;

auto make_pine_profile() -> TreeScatterProfile {
  TreeScatterProfile profile;
  profile.species = TreeSpecies::Pine;
  profile.rule_species = ScatterRuleSpecies::Pine;
  profile.prop_type = WorldProp::Type::PineTree;
  profile.random_order = TreeRandomOrder::ScaleFirst;

  profile.cell_slope_limit = 0.75F;

  profile.cell_salt = 0xAB12CD34U;
  profile.cell_scene_salt = 0x5E2C4B81U;
  profile.sample_salt = 0x92C3B17FU;
  profile.leader_salt = 0x07E84CD3U;
  profile.macro_salt = 0x4F2E9A7BU;
  profile.mid_salt = 0xB3C71E4DU;
  profile.prop_salt = 0x4A7F2C9EU;

  profile.hill_density = 1.2F;
  profile.mountain_density = 0.4F;
  profile.forest_density = 2.25F;

  profile.cluster_base = 0.45F;
  profile.cluster_gain = 1.75F;
  profile.cluster_bias_gain = 1.10F;

  profile.base_tint = {QVector3D(0.095F, 0.200F, 0.150F),
                       {Channel::Shelter, Channel::Shelter, Channel::Shelter},
                       QVector3D(0.03F, 0.08F, 0.04F)};
  profile.var_tint = {QVector3D(0.278F, 0.452F, 0.232F),
                      {Channel::ClusterBias, Channel::Shelter, Channel::ClusterBias},
                      QVector3D(0.05F, 0.08F, 0.04F)};
  profile.blend = {QVector3D(0.34F, 0.31F, 0.23F),
                   0.03F,
                   Channel::Dryness,
                   0.05F,
                   0.10F,
                   Channel::Rockiness,
                   0.06F};
  profile.value_min = 0.78F;
  profile.value_max = 1.24F;

  profile.prop_base_color = QVector3D(0.092F, 0.198F, 0.122F);
  profile.prop_var_color = QVector3D(0.262F, 0.428F, 0.222F);
  profile.prop_value_min = 0.80F;
  profile.prop_value_max = 1.22F;
  return profile;
}

auto make_olive_profile() -> TreeScatterProfile {
  TreeScatterProfile profile;
  profile.species = TreeSpecies::Olive;
  profile.rule_species = ScatterRuleSpecies::Olive;
  profile.prop_type = WorldProp::Type::OliveTree;
  profile.random_order = TreeRandomOrder::ScaleLast;

  profile.cell_slope_limit = 0.65F;
  profile.spawn_max_slope = 0.65F;

  profile.cell_salt = 0xCD34EF56U;
  profile.cell_scene_salt = 0x62D1E7AFU;
  profile.sample_salt = 0x16C92A4FU;
  profile.leader_salt = 0x0E63A1B2U;
  profile.macro_salt = 0xC7E4F1A3U;
  profile.mid_salt = 0xA2B5D8E6U;
  profile.prop_salt = 0x7B3E5F1CU;

  profile.hill_density = 1.15F;
  profile.mountain_density = 0.5F;
  profile.forest_density = 0.45F;

  profile.cluster_base = 0.40F;
  profile.cluster_gain = 1.65F;
  profile.cluster_bias_gain = 1.05F;

  profile.base_tint = {QVector3D(0.160F, 0.245F, 0.170F),
                       {Channel::Dryness, Channel::Rockiness, Channel::Dryness},
                       QVector3D(0.05F, 0.04F, 0.03F)};
  profile.var_tint = {QVector3D(0.495F, 0.605F, 0.355F),
                      {Channel::ClusterBias, Channel::Rockiness, Channel::ClusterBias},
                      QVector3D(0.05F, 0.05F, 0.04F)};
  profile.blend = {QVector3D(0.530F, 0.560F, 0.500F),
                   0.08F,
                   Channel::Rockiness,
                   0.04F,
                   0.18F,
                   Channel::Dryness,
                   0.08F};
  profile.value_min = 0.80F;
  profile.value_max = 1.22F;

  profile.prop_base_color = QVector3D(0.215F, 0.290F, 0.185F);
  profile.prop_var_color = QVector3D(0.480F, 0.570F, 0.340F);
  profile.prop_value_min = 0.82F;
  profile.prop_value_max = 1.20F;
  return profile;
}

auto make_cypress_profile() -> TreeScatterProfile {
  TreeScatterProfile profile;
  profile.species = TreeSpecies::Cypress;
  profile.rule_species = ScatterRuleSpecies::Cypress;
  profile.prop_type = WorldProp::Type::CypressTree;
  profile.random_order = TreeRandomOrder::ScaleFirst;

  profile.cell_slope_limit = 0.62F;
  profile.spawn_max_slope = 0.55F;

  profile.cell_salt = 0x3D91C7E2U;
  profile.cell_scene_salt = 0x18F4A63BU;
  profile.sample_salt = 0x6B27D95AU;
  profile.leader_salt = 0xE4130C87U;
  profile.macro_salt = 0x2A76BE41U;
  profile.mid_salt = 0x95C0D3F8U;
  profile.prop_salt = 0x51B8E20DU;

  profile.hill_density = 1.05F;
  profile.mountain_density = 0.25F;
  profile.forest_density = 0.60F;

  profile.cluster_base = 0.50F;
  profile.cluster_gain = 1.55F;
  profile.cluster_bias_gain = 1.00F;

  profile.base_tint = {QVector3D(0.072F, 0.146F, 0.104F),
                       {Channel::Shelter, Channel::Shelter, Channel::Shelter},
                       QVector3D(0.02F, 0.05F, 0.03F)};
  profile.var_tint = {QVector3D(0.196F, 0.322F, 0.184F),
                      {Channel::ClusterBias, Channel::Shelter, Channel::ClusterBias},
                      QVector3D(0.04F, 0.06F, 0.03F)};
  profile.blend = {QVector3D(0.30F, 0.29F, 0.22F),
                   0.03F,
                   Channel::Dryness,
                   0.05F,
                   0.09F,
                   Channel::Rockiness,
                   0.05F};
  profile.value_min = 0.82F;
  profile.value_max = 1.18F;

  profile.prop_base_color = QVector3D(0.070F, 0.150F, 0.100F);
  profile.prop_var_color = QVector3D(0.198F, 0.330F, 0.186F);
  profile.prop_value_min = 0.84F;
  profile.prop_value_max = 1.16F;
  return profile;
}

auto make_palm_profile() -> TreeScatterProfile {
  TreeScatterProfile profile;
  profile.species = TreeSpecies::Palm;
  profile.rule_species = ScatterRuleSpecies::Palm;
  profile.prop_type = WorldProp::Type::PalmTree;
  profile.random_order = TreeRandomOrder::ScaleLast;

  profile.cell_slope_limit = 0.45F;
  profile.spawn_max_slope = 0.40F;

  profile.cell_salt = 0x7E2A4B90U;
  profile.cell_scene_salt = 0xC3096DA5U;
  profile.sample_salt = 0x4F81B27EU;
  profile.leader_salt = 0xA6D53814U;
  profile.macro_salt = 0x0B9E76C2U;
  profile.mid_salt = 0xD842F35BU;
  profile.prop_salt = 0x39C7A1E6U;

  profile.hill_density = 0.45F;
  profile.mountain_density = 0.05F;
  profile.forest_density = 0.30F;

  profile.cluster_base = 0.42F;
  profile.cluster_gain = 1.70F;
  profile.cluster_bias_gain = 1.15F;

  profile.base_tint = {QVector3D(0.118F, 0.196F, 0.110F),
                       {Channel::Dryness, Channel::RiverInfluence, Channel::Dryness},
                       QVector3D(0.04F, 0.05F, 0.03F)};
  profile.var_tint = {
      QVector3D(0.296F, 0.410F, 0.198F),
      {Channel::ClusterBias, Channel::RiverInfluence, Channel::ClusterBias},
      QVector3D(0.05F, 0.06F, 0.04F)};
  profile.blend = {QVector3D(0.470F, 0.500F, 0.400F),
                   0.05F,
                   Channel::Rockiness,
                   0.04F,
                   0.14F,
                   Channel::Dryness,
                   0.08F};
  profile.value_min = 0.84F;
  profile.value_max = 1.14F;

  profile.prop_base_color = QVector3D(0.120F, 0.200F, 0.108F);
  profile.prop_var_color = QVector3D(0.292F, 0.404F, 0.196F);
  profile.prop_value_min = 0.86F;
  profile.prop_value_max = 1.12F;
  return profile;
}

auto make_profiles()
    -> std::array<TreeScatterProfile, Game::Map::k_tree_species_count> {
  return {make_pine_profile(),
          make_olive_profile(),
          make_cypress_profile(),
          make_palm_profile()};
}

} // namespace

auto tree_scatter_profile(Game::Map::TreeSpecies species) -> const TreeScatterProfile& {
  static const auto profiles = make_profiles();
  return profiles[static_cast<std::size_t>(species)];
}

} // namespace Render::Ground
