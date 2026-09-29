#pragma once

#include <QString>
#include <QVariantMap>

#include <cstdint>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SelectionService;
}

namespace App::Controllers {

struct FormationOptionsReadout {
  QString intent;
  QString doctrine;
  QString doctrine_display_name;
  bool doctrine_locked = false;
  float frontage_scale = 1.0F;
  float depth_scale = 1.0F;
  float spacing_scale = 1.0F;
  int reserve_rows = -1;
  bool preserve_member_order = false;
  QString flank;
  QString ranged;
  QString movement;
  bool movement_from_doctrine = true;
  int effective_movement_index = 0;
  QString mixed;
  float frontage = 0.0F;
  int blocked_slots = 0;
  int adjusted_slots = 0;
  QString warning;
  int frontage_index = 0;
  int depth_index = 0;
  int spacing_index = 0;
  int flank_index = 0;
  int ranged_index = 0;
  int reserve_index = 0;
  int movement_index = 0;
  int mixed_index = 0;
  int preserve_index = 0;
  QString intent_display_name;
  int unit_count = 0;
  bool single_unit = false;
  QString unit_label;
  QString gesture;
  float facing_degrees = 0.0F;
  bool facing_explicit = false;
  float aim_distance = 0.0F;
  int placed_count = 0;
  int slot_count = 0;
  int ranks = 0;
  int files = 0;
  float plan_frontage = 0.0F;
  float plan_depth = 0.0F;
  bool plan_valid = false;
};

struct SelectedFormationStatus {
  bool active = false;
  QString intent;
  QString intent_display_name;
  QString doctrine_display_name;
  float cohesion = 0.0F;
  QString phase;
  int member_count = 0;
  int selected_in_group = 0;
  bool mixed_groups = false;
  int blocked_slots = 0;
  QString movement;
  bool compressed = false;
};

[[nodiscard]] auto
to_variant_map(const FormationOptionsReadout& readout) -> QVariantMap;
[[nodiscard]] auto to_variant_map(const SelectedFormationStatus& status) -> QVariantMap;

[[nodiscard]] auto read_selected_formation(Engine::Core::World& world,
                                           const std::vector<std::uint64_t>& selected)
    -> SelectedFormationStatus;

[[nodiscard]] auto
single_unit_label(Engine::Core::World* world,
                  const std::vector<std::uint64_t>& units) -> QString;

} // namespace App::Controllers
