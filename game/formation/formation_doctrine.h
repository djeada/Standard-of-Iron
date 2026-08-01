#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "army_formation_types.h"
#include "formation_roles.h"

namespace Game::Formation {

enum class LinePlacement : std::uint8_t {
  CentreBlock,
  SplitFlanks,
  Screen,
  Trailing
};

struct DoctrineLineRule {
  ArmyRole role{ArmyRole::Centre};
  RoleTagSet match_any{0U};
  RoleTagSet match_all{0U};
  RoleTagSet exclude{0U};

  LinePlacement placement{LinePlacement::CentreBlock};
  int max_per_row{6};
  int min_per_row{1};

  float lateral_spacing_scale{1.0F};
  float depth_spacing_scale{1.0F};
  float line_gap_scale{1.0F};
  float row_echelon_scale{0.0F};
  float row_stagger_scale{0.0F};
  float lateral_jitter_scale{0.0F};
  float depth_jitter_scale{0.0F};
  float front_offset_scale{0.0F};

  float flank_gap_scale{1.8F};
  float right_side_weight{0.5F};
  float flank_forward_step_scale{0.0F};

  bool consumes_depth{true};
  bool optional{true};

  [[nodiscard]] auto matches(RoleTagSet troop_roles) const -> bool;
};

struct DoctrineIntentTemplate {
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};
  std::vector<DoctrineLineRule> lines;

  float frontage_scale{1.0F};
  float depth_scale{1.0F};
  float spacing_scale{1.0F};
  int reserve_rows{0};

  FlankPreference default_flank{FlankPreference::Balanced};
  RangedPlacement default_ranged{RangedPlacement::Rear};
  MovementPolicy default_movement{MovementPolicy::ReformAtDestination};

  RoleTagSet required_roles{0U};
  std::string requirement_hint;
};

struct FormationDoctrine {
  FormationDoctrineId id{k_neutral_doctrine};
  std::string display_name{"Neutral"};
  ArmyFormationIntent default_intent{ArmyFormationIntent::Line};
  std::unordered_map<int, DoctrineIntentTemplate> intents;

  [[nodiscard]] auto
  find_template(ArmyFormationIntent intent) const -> const DoctrineIntentTemplate*;

  [[nodiscard]] auto
  resolve_template(ArmyFormationIntent intent) const -> const DoctrineIntentTemplate*;

  [[nodiscard]] auto supports(ArmyFormationIntent intent) const -> bool;
};

class DoctrineRegistry {
public:
  static auto instance() -> DoctrineRegistry&;

  void reset_to_defaults();
  void clear();

  void register_doctrine(FormationDoctrine doctrine);

  [[nodiscard]] auto
  find(const FormationDoctrineId& id) const -> const FormationDoctrine*;

  [[nodiscard]] auto
  get_or_neutral(const FormationDoctrineId& id) const -> const FormationDoctrine&;

  [[nodiscard]] auto ids() const -> std::vector<FormationDoctrineId>;

  [[nodiscard]] auto availability_reason(const FormationDoctrineId& doctrine,
                                         ArmyFormationIntent intent,
                                         RoleTagSet available_roles,
                                         int member_count) const -> std::string;

private:
  DoctrineRegistry();

  std::unordered_map<FormationDoctrineId, FormationDoctrine> m_doctrines;
  FormationDoctrine m_neutral;
};

[[nodiscard]] auto make_neutral_doctrine() -> FormationDoctrine;
[[nodiscard]] auto make_rome_doctrine() -> FormationDoctrine;
[[nodiscard]] auto make_carthage_doctrine() -> FormationDoctrine;
[[nodiscard]] auto make_iron_sepulcher_doctrine() -> FormationDoctrine;

} // namespace Game::Formation
