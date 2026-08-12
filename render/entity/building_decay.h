#pragma once

#include <QVector3D>

#include <cstdint>

#include "building_archetype_desc.h"
#include "building_state.h"

namespace Render::GL {

struct BuildingDecayProfile {
  float desaturation{0.0F};
  float darkening{1.0F};
  float soot{0.0F};
  float moss{0.0F};
  float dust{0.0F};
  float variation{0.0F};
};

auto building_decay_profile(BuildingState state) -> BuildingDecayProfile;

auto decayed_color(const QVector3D& base, BuildingState state, int seed) -> QVector3D;

auto decay_hash(int seed) -> float;

auto part_supports_state(BuildingStateMask mask, BuildingState state) -> bool;

struct RubbleField {
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D extent{1.0F, 0.0F, 1.0F};
  QVector3D stone{0.60F, 0.57F, 0.51F};
  QVector3D stone_dark{0.38F, 0.35F, 0.31F};
  float ground_y{0.0F};
  float chunk_scale{1.0F};
  float ring_bias{0.0F};
  int count{9};
  int seed{0};
  BuildingStateMask states{BuildingStateMask::Destroyed};
};

void add_rubble_field(BuildingArchetypeDesc& desc, const RubbleField& field);

struct CharredBeams {
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D extent{1.0F, 0.0F, 1.0F};
  QVector3D timber{0.16F, 0.12F, 0.09F};
  float ground_y{0.0F};
  float length{0.9F};
  float radius{0.045F};
  int count{4};
  int seed{0};
  BuildingStateMask states{BuildingStateMask::Destroyed};
};

void add_charred_beams(BuildingArchetypeDesc& desc, const CharredBeams& beams);

struct ScorchPatch {
  QVector3D center{0.0F, 0.0F, 0.0F};
  float radius{0.8F};
  float ground_y{0.0F};
  int count{5};
  int seed{0};
  QVector3D soot{0.11F, 0.10F, 0.09F};
  BuildingStateMask states{BuildingStateMask::Destroyed};
};

void add_scorch_patch(BuildingArchetypeDesc& desc, const ScorchPatch& patch);

struct CrackVeins {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D span{1.0F, 1.0F, 0.0F};
  QVector3D color{0.20F, 0.18F, 0.16F};
  float width{0.014F};
  int count{4};
  int seed{0};
  BuildingStateMask states{BuildingStateMask::Damaged | BuildingStateMask::Destroyed};
};

void add_crack_veins(BuildingArchetypeDesc& desc, const CrackVeins& veins);

struct RuinDressing {
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D extent{1.0F, 0.0F, 1.0F};
  QVector3D apron_extent{0.0F, 0.0F, 0.0F};
  QVector3D stone{0.58F, 0.55F, 0.49F};
  QVector3D stone_dark{0.36F, 0.33F, 0.29F};
  QVector3D timber{0.17F, 0.12F, 0.08F};
  float ground_y{0.0F};
  float apron_y{0.02F};
  float scale{1.0F};
  int seed{0};
  bool collapsed_roof{true};
};

void add_ruin_dressing(BuildingArchetypeDesc& desc, const RuinDressing& dressing);

struct BrokenRim {
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D color{0.55F, 0.52F, 0.47F};
  float radius{1.0F};
  float chunk_half{0.10F};
  float rise{0.16F};
  int count{7};
  int seed{0};
  BuildingStateMask states{BuildingStateMask::Destroyed};
};

void add_broken_rim(BuildingArchetypeDesc& desc, const BrokenRim& rim);

} // namespace Render::GL
