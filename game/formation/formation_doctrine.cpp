#include "formation_doctrine.h"

#include <QCoreApplication>

#include <algorithm>
#include <utility>

#include "game/util/asset_text.h"

namespace Game::Formation {

namespace {

auto mask(std::initializer_list<RoleTag> tags) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (auto tag : tags) {
    set |= to_mask(tag);
  }
  return set;
}

const RoleTagSet k_any_cavalry = mask({RoleTag::Cavalry, RoleTag::Mounted});
const RoleTagSet k_any_ranged = mask({RoleTag::Ranged});
const RoleTagSet k_any_siege = mask({RoleTag::Siege});
const RoleTagSet k_any_command = mask({RoleTag::Command});
const RoleTagSet k_any_support =
    mask({RoleTag::Support, RoleTag::Worker, RoleTag::Civilian, RoleTag::Caster});
const RoleTagSet k_any_elephant = mask({RoleTag::Elephant});
const RoleTagSet k_any_spear = mask({RoleTag::SpearInfantry});
const RoleTagSet k_any_line =
    mask({RoleTag::LineInfantry, RoleTag::HeavyInfantry, RoleTag::Shielded});
const RoleTagSet k_any_expendable = mask({RoleTag::Expendable});

auto elephant_line(ArmyRole role,
                   float lateral,
                   float depth,
                   float gap) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = role;
  rule.match_any = k_any_elephant;
  rule.max_per_row = 3;
  rule.min_per_row = 1;
  rule.lateral_spacing_scale = lateral;
  rule.depth_spacing_scale = depth;
  rule.line_gap_scale = gap;
  return rule;
}

auto cavalry_flank_line(float lateral,
                        float flank_gap,
                        float right_weight,
                        float echelon,
                        float forward_step) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = ArmyRole::LeftFlank;
  rule.match_any = k_any_cavalry;
  rule.placement = LinePlacement::SplitFlanks;
  rule.max_per_row = 3;
  rule.min_per_row = 1;
  rule.lateral_spacing_scale = lateral;
  rule.flank_gap_scale = flank_gap;
  rule.right_side_weight = right_weight;
  rule.row_echelon_scale = echelon;
  rule.flank_forward_step_scale = forward_step;
  rule.consumes_depth = false;
  return rule;
}

auto ranged_line(int max_per_row,
                 float lateral,
                 float depth,
                 float gap,
                 float stagger,
                 float jitter) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = ArmyRole::Ranged;
  rule.match_any = k_any_ranged;
  rule.exclude = k_any_cavalry | k_any_siege;
  rule.max_per_row = max_per_row;
  rule.min_per_row = 2;
  rule.lateral_spacing_scale = lateral;
  rule.depth_spacing_scale = depth;
  rule.line_gap_scale = gap;
  rule.row_stagger_scale = stagger;
  rule.lateral_jitter_scale = jitter;
  rule.depth_jitter_scale = jitter * 0.75F;
  return rule;
}

auto siege_line(float lateral, float depth, float gap) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = ArmyRole::Siege;
  rule.match_any = k_any_siege;
  rule.max_per_row = 4;
  rule.min_per_row = 1;
  rule.lateral_spacing_scale = lateral;
  rule.depth_spacing_scale = depth;
  rule.line_gap_scale = gap;
  return rule;
}

auto command_line(float gap) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = ArmyRole::Command;
  rule.match_any = k_any_command;
  rule.max_per_row = 4;
  rule.min_per_row = 1;
  rule.line_gap_scale = gap;
  return rule;
}

auto support_line(float gap, float jitter) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = ArmyRole::Reserve;
  rule.match_any = k_any_support;
  rule.exclude = k_any_command;
  rule.max_per_row = 6;
  rule.min_per_row = 1;
  rule.line_gap_scale = gap;
  rule.lateral_jitter_scale = jitter;
  rule.depth_jitter_scale = jitter;
  return rule;
}

auto infantry_line(ArmyRole role,
                   RoleTagSet match_any,
                   RoleTagSet exclude,
                   int max_per_row,
                   float lateral,
                   float depth,
                   float gap,
                   float echelon,
                   float stagger,
                   float jitter) -> DoctrineLineRule {
  DoctrineLineRule rule;
  rule.role = role;
  rule.match_any = match_any;
  rule.exclude = exclude;
  rule.max_per_row = max_per_row;
  rule.min_per_row = 2;
  rule.lateral_spacing_scale = lateral;
  rule.depth_spacing_scale = depth;
  rule.line_gap_scale = gap;
  rule.row_echelon_scale = echelon;
  rule.row_stagger_scale = stagger;
  rule.lateral_jitter_scale = jitter;
  rule.depth_jitter_scale = jitter * 0.7F;
  return rule;
}

auto column_template(ArmyFormationIntent intent,
                     float spacing_scale,
                     float jitter) -> DoctrineIntentTemplate {
  DoctrineIntentTemplate tmpl;
  tmpl.intent = intent;
  tmpl.frontage_scale = 0.35F;
  tmpl.depth_scale = 2.4F;
  tmpl.spacing_scale = spacing_scale;
  tmpl.default_movement = MovementPolicy::MaintainFormation;

  DoctrineLineRule vanguard = infantry_line(
      ArmyRole::Vanguard, k_any_cavalry, 0U, 2, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, jitter);
  vanguard.min_per_row = 1;
  tmpl.lines.push_back(vanguard);
  tmpl.lines.push_back(infantry_line(ArmyRole::Centre,
                                     k_any_line | k_any_spear,
                                     0U,
                                     2,
                                     1.0F,
                                     1.05F,
                                     1.0F,
                                     0.0F,
                                     0.0F,
                                     jitter));
  tmpl.lines.push_back(ranged_line(2, 1.0F, 1.05F, 1.0F, 0.0F, jitter));
  tmpl.lines.push_back(siege_line(1.2F, 1.2F, 1.0F));
  tmpl.lines.push_back(command_line(1.0F));
  tmpl.lines.push_back(support_line(1.0F, jitter));

  DoctrineLineRule catch_all;
  catch_all.role = ArmyRole::Reserve;
  catch_all.max_per_row = 2;
  catch_all.min_per_row = 1;
  tmpl.lines.push_back(catch_all);
  return tmpl;
}

void add_catch_all(DoctrineIntentTemplate& tmpl, ArmyRole role, int max_per_row) {
  DoctrineLineRule rule;
  rule.role = role;
  rule.max_per_row = max_per_row;
  rule.min_per_row = 1;
  rule.line_gap_scale = 0.9F;
  tmpl.lines.push_back(rule);
}

} // namespace

auto DoctrineLineRule::matches(RoleTagSet troop_roles) const -> bool {
  if (exclude != 0U && has_any_role(troop_roles, exclude)) {
    return false;
  }
  if (match_all != 0U && !has_all_roles(troop_roles, match_all)) {
    return false;
  }
  if (match_any != 0U) {
    return has_any_role(troop_roles, match_any);
  }
  return true;
}

auto FormationDoctrine::find_template(ArmyFormationIntent intent) const
    -> const DoctrineIntentTemplate* {
  auto it = intents.find(static_cast<int>(intent));
  return it == intents.end() ? nullptr : &it->second;
}

auto FormationDoctrine::resolve_template(ArmyFormationIntent intent) const
    -> const DoctrineIntentTemplate* {
  if (intent == ArmyFormationIntent::FactionDefault) {
    if (const auto* direct = find_template(ArmyFormationIntent::FactionDefault)) {
      return direct;
    }
    return find_template(default_intent);
  }
  if (const auto* direct = find_template(intent)) {
    return direct;
  }
  if (const auto* fallback = find_template(ArmyFormationIntent::FactionDefault)) {
    return fallback;
  }
  return find_template(default_intent);
}

auto FormationDoctrine::supports(ArmyFormationIntent intent) const -> bool {
  if (intent == ArmyFormationIntent::FactionDefault) {
    return true;
  }
  return find_template(intent) != nullptr;
}

auto make_neutral_doctrine() -> FormationDoctrine {
  FormationDoctrine doctrine;
  doctrine.id = k_neutral_doctrine;
  doctrine.display_name = QT_TRANSLATE_NOOP("Formation", "Neutral");
  doctrine.default_intent = ArmyFormationIntent::Line;

  DoctrineIntentTemplate line;
  line.intent = ArmyFormationIntent::Line;
  line.lines.push_back(infantry_line(ArmyRole::Centre,
                                     k_any_line | k_any_spear,
                                     0U,
                                     6,
                                     1.05F,
                                     1.0F,
                                     0.95F,
                                     0.0F,
                                     0.0F,
                                     0.03F));
  line.lines.push_back(ranged_line(6, 1.0F, 1.0F, 1.0F, 0.15F, 0.05F));
  line.lines.push_back(siege_line(1.4F, 1.15F, 1.1F));
  line.lines.push_back(command_line(0.85F));
  line.lines.push_back(support_line(0.85F, 0.05F));
  line.lines.push_back(cavalry_flank_line(1.1F, 1.8F, 0.5F, 0.0F, 0.0F));
  add_catch_all(line, ArmyRole::Reserve, 6);
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Line)] = line;

  DoctrineIntentTemplate faction_default = line;
  faction_default.intent = ArmyFormationIntent::FactionDefault;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::FactionDefault)] =
      faction_default;

  DoctrineIntentTemplate defensive = line;
  defensive.intent = ArmyFormationIntent::Defensive;
  defensive.frontage_scale = 0.85F;
  defensive.depth_scale = 1.25F;
  defensive.spacing_scale = 0.9F;
  defensive.reserve_rows = 1;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Defensive)] = defensive;

  DoctrineIntentTemplate assault = line;
  assault.intent = ArmyFormationIntent::Assault;
  assault.frontage_scale = 0.9F;
  assault.depth_scale = 1.15F;
  assault.default_ranged = RangedPlacement::Skirmish;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Assault)] = assault;

  doctrine.intents[static_cast<int>(ArmyFormationIntent::Column)] =
      column_template(ArmyFormationIntent::Column, 0.95F, 0.02F);

  return doctrine;
}

auto make_rome_doctrine() -> FormationDoctrine {
  FormationDoctrine doctrine;
  doctrine.id = "rome";
  doctrine.display_name = QT_TRANSLATE_NOOP("Formation", "Roman Republic");
  doctrine.default_intent = ArmyFormationIntent::Line;

  DoctrineIntentTemplate battle_line;
  battle_line.intent = ArmyFormationIntent::FactionDefault;
  battle_line.frontage_scale = 1.0F;
  battle_line.depth_scale = 1.0F;
  battle_line.spacing_scale = 1.0F;
  battle_line.reserve_rows = 1;
  battle_line.default_flank = FlankPreference::Balanced;
  battle_line.default_ranged = RangedPlacement::Rear;

  battle_line.lines.push_back(elephant_line(ArmyRole::Vanguard, 1.60F, 1.20F, 0.80F));
  battle_line.lines.push_back(infantry_line(ArmyRole::Screen,
                                            k_any_spear,
                                            k_any_cavalry,
                                            8,
                                            1.18F,
                                            1.02F,
                                            0.85F,
                                            0.0F,
                                            0.0F,
                                            0.0F));
  battle_line.lines.push_back(infantry_line(ArmyRole::Centre,
                                            k_any_line,
                                            k_any_cavalry | k_any_spear,
                                            8,
                                            1.08F,
                                            1.00F,
                                            0.95F,
                                            0.0F,
                                            0.0F,
                                            0.0F));
  battle_line.lines.push_back(ranged_line(10, 1.02F, 1.05F, 1.05F, 0.20F, 0.0F));
  battle_line.lines.push_back(siege_line(1.45F, 1.15F, 1.15F));
  battle_line.lines.push_back(command_line(0.75F));
  battle_line.lines.push_back(support_line(0.75F, 0.0F));
  battle_line.lines.push_back(cavalry_flank_line(1.08F, 1.90F, 0.50F, 0.0F, 0.0F));
  add_catch_all(battle_line, ArmyRole::Reserve, 6);
  doctrine.intents[static_cast<int>(ArmyFormationIntent::FactionDefault)] = battle_line;

  DoctrineIntentTemplate line = battle_line;
  line.intent = ArmyFormationIntent::Line;
  line.frontage_scale = 1.25F;
  line.depth_scale = 0.85F;
  for (auto& rule : line.lines) {
    if (rule.placement == LinePlacement::CentreBlock) {
      rule.max_per_row = (rule.max_per_row * 3) / 2;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Line)] = line;

  DoctrineIntentTemplate defensive = battle_line;
  defensive.intent = ArmyFormationIntent::Defensive;
  defensive.frontage_scale = 0.85F;
  defensive.depth_scale = 1.30F;
  defensive.spacing_scale = 0.88F;
  defensive.reserve_rows = 2;
  defensive.default_ranged = RangedPlacement::Rear;
  for (auto& rule : defensive.lines) {
    if (rule.role == ArmyRole::Screen) {
      rule.front_offset_scale = 0.35F;
    }
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_gap_scale = 1.35F;
      rule.front_offset_scale = -0.8F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Defensive)] = defensive;

  DoctrineIntentTemplate assault = battle_line;
  assault.intent = ArmyFormationIntent::Assault;
  assault.frontage_scale = 0.95F;
  assault.depth_scale = 1.10F;
  assault.spacing_scale = 0.95F;
  assault.reserve_rows = 1;
  assault.default_ranged = RangedPlacement::Skirmish;
  for (auto& rule : assault.lines) {
    if (rule.role == ArmyRole::Centre) {
      rule.front_offset_scale = 0.5F;
    }
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_forward_step_scale = 0.25F;
      rule.front_offset_scale = 0.6F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Assault)] = assault;

  doctrine.intents[static_cast<int>(ArmyFormationIntent::Column)] =
      column_template(ArmyFormationIntent::Column, 0.92F, 0.0F);

  DoctrineIntentTemplate siege_escort = battle_line;
  siege_escort.intent = ArmyFormationIntent::SiegeEscort;
  siege_escort.frontage_scale = 0.9F;
  siege_escort.depth_scale = 1.35F;
  siege_escort.required_roles = k_any_siege;
  siege_escort.requirement_hint = QT_TRANSLATE_NOOP(
      "Formation", "Requires at least one siege engine in the selection.");
  for (auto& rule : siege_escort.lines) {
    if (rule.role == ArmyRole::Siege) {
      rule.front_offset_scale = -0.2F;
      rule.line_gap_scale = 1.5F;
    }
    if (rule.role == ArmyRole::Centre || rule.role == ArmyRole::Screen) {
      rule.front_offset_scale = 0.8F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::SiegeEscort)] = siege_escort;

  DoctrineIntentTemplate encirclement = battle_line;
  encirclement.intent = ArmyFormationIntent::Encirclement;
  encirclement.frontage_scale = 1.6F;
  encirclement.depth_scale = 0.7F;
  encirclement.default_flank = FlankPreference::Split;
  encirclement.required_roles = k_any_cavalry;
  encirclement.requirement_hint = QT_TRANSLATE_NOOP(
      "Formation", "Requires cavalry or mounted troops to close the encirclement.");
  for (auto& rule : encirclement.lines) {
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_gap_scale = 3.4F;
      rule.flank_forward_step_scale = 0.55F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Encirclement)] = encirclement;

  return doctrine;
}

auto make_carthage_doctrine() -> FormationDoctrine {
  FormationDoctrine doctrine;
  doctrine.id = "carthage";
  doctrine.display_name = QT_TRANSLATE_NOOP("Formation", "Carthage");
  doctrine.default_intent = ArmyFormationIntent::Line;

  DoctrineIntentTemplate battle_line;
  battle_line.intent = ArmyFormationIntent::FactionDefault;
  battle_line.frontage_scale = 1.20F;
  battle_line.depth_scale = 0.90F;
  battle_line.spacing_scale = 1.05F;
  battle_line.reserve_rows = 1;
  battle_line.default_flank = FlankPreference::StrongRight;
  battle_line.default_ranged = RangedPlacement::Skirmish;

  battle_line.lines.push_back(elephant_line(ArmyRole::Vanguard, 1.70F, 1.15F, 0.60F));
  battle_line.lines.push_back(infantry_line(ArmyRole::Screen,
                                            k_any_spear,
                                            k_any_cavalry,
                                            7,
                                            1.16F,
                                            1.00F,
                                            0.80F,
                                            0.30F,
                                            0.18F,
                                            0.05F));
  battle_line.lines.push_back(infantry_line(ArmyRole::Centre,
                                            k_any_line,
                                            k_any_cavalry | k_any_spear,
                                            7,
                                            1.08F,
                                            1.00F,
                                            0.90F,
                                            -0.18F,
                                            0.12F,
                                            0.04F));
  battle_line.lines.push_back(ranged_line(9, 1.05F, 1.00F, 1.00F, 0.32F, 0.08F));
  battle_line.lines.push_back(siege_line(1.55F, 1.20F, 1.15F));
  battle_line.lines.push_back(command_line(0.75F));
  battle_line.lines.push_back(support_line(0.75F, 0.04F));
  battle_line.lines.push_back(cavalry_flank_line(1.12F, 2.10F, 0.60F, 0.25F, 0.35F));
  add_catch_all(battle_line, ArmyRole::Reserve, 6);
  doctrine.intents[static_cast<int>(ArmyFormationIntent::FactionDefault)] = battle_line;

  DoctrineIntentTemplate line = battle_line;
  line.intent = ArmyFormationIntent::Line;
  line.frontage_scale = 1.45F;
  line.depth_scale = 0.75F;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Line)] = line;

  DoctrineIntentTemplate defensive = battle_line;
  defensive.intent = ArmyFormationIntent::Defensive;
  defensive.frontage_scale = 1.05F;
  defensive.depth_scale = 1.20F;
  defensive.spacing_scale = 0.95F;
  defensive.reserve_rows = 2;
  defensive.default_ranged = RangedPlacement::Rear;
  for (auto& rule : defensive.lines) {
    if (rule.role == ArmyRole::Screen || rule.role == ArmyRole::Centre) {
      rule.row_echelon_scale *= 0.5F;
      rule.front_offset_scale = 0.25F;
    }
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_gap_scale = 1.60F;
      rule.flank_forward_step_scale = 0.0F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Defensive)] = defensive;

  DoctrineIntentTemplate assault = battle_line;
  assault.intent = ArmyFormationIntent::Assault;
  assault.frontage_scale = 1.15F;
  assault.depth_scale = 1.05F;
  assault.default_flank = FlankPreference::StrongRight;
  for (auto& rule : assault.lines) {
    if (rule.role == ArmyRole::Vanguard) {
      rule.front_offset_scale = 0.9F;
      rule.lateral_spacing_scale *= 1.15F;
    }
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_forward_step_scale = 0.60F;
      rule.front_offset_scale = 0.9F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Assault)] = assault;

  doctrine.intents[static_cast<int>(ArmyFormationIntent::Column)] =
      column_template(ArmyFormationIntent::Column, 1.05F, 0.05F);

  DoctrineIntentTemplate encirclement = battle_line;
  encirclement.intent = ArmyFormationIntent::Encirclement;
  encirclement.frontage_scale = 1.85F;
  encirclement.depth_scale = 0.65F;
  encirclement.default_flank = FlankPreference::Split;
  encirclement.required_roles = k_any_cavalry;
  encirclement.requirement_hint = QT_TRANSLATE_NOOP(
      "Formation", "Requires cavalry or mounted troops to close the encirclement.");
  for (auto& rule : encirclement.lines) {
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_gap_scale = 3.8F;
      rule.flank_forward_step_scale = 0.75F;
      rule.right_side_weight = 0.5F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Encirclement)] = encirclement;

  DoctrineIntentTemplate siege_escort = battle_line;
  siege_escort.intent = ArmyFormationIntent::SiegeEscort;
  siege_escort.frontage_scale = 1.05F;
  siege_escort.depth_scale = 1.30F;
  siege_escort.required_roles = k_any_siege;
  siege_escort.requirement_hint = QT_TRANSLATE_NOOP(
      "Formation", "Requires at least one siege engine in the selection.");
  for (auto& rule : siege_escort.lines) {
    if (rule.role == ArmyRole::Siege) {
      rule.line_gap_scale = 1.5F;
    }
    if (rule.placement == LinePlacement::SplitFlanks) {
      rule.flank_gap_scale = 1.4F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::SiegeEscort)] = siege_escort;

  return doctrine;
}

auto make_iron_sepulcher_doctrine() -> FormationDoctrine {
  FormationDoctrine doctrine;
  doctrine.id = "iron_sepulcher";
  doctrine.display_name = QT_TRANSLATE_NOOP("Formation", "The Iron Sepulcher");
  doctrine.default_intent = ArmyFormationIntent::Defensive;

  DoctrineIntentTemplate burial_guard;
  burial_guard.intent = ArmyFormationIntent::FactionDefault;
  burial_guard.frontage_scale = 0.80F;
  burial_guard.depth_scale = 1.30F;
  burial_guard.spacing_scale = 0.78F;
  burial_guard.reserve_rows = 1;
  burial_guard.default_flank = FlankPreference::Balanced;
  burial_guard.default_ranged = RangedPlacement::Rear;
  burial_guard.default_movement = MovementPolicy::MaintainFormation;

  burial_guard.lines.push_back(
      infantry_line(ArmyRole::Vanguard,
                    k_any_expendable,
                    k_any_ranged | k_any_cavalry | k_any_command | k_any_support,
                    6,
                    0.92F,
                    0.94F,
                    0.55F,
                    0.0F,
                    0.0F,
                    0.0F));
  burial_guard.lines.push_back(infantry_line(ArmyRole::Centre,
                                             k_any_line | k_any_spear,
                                             k_any_cavalry | k_any_ranged,
                                             6,
                                             0.95F,
                                             0.96F,
                                             0.60F,
                                             0.0F,
                                             0.0F,
                                             0.0F));
  burial_guard.lines.push_back(ranged_line(6, 0.98F, 0.95F, 0.70F, 0.0F, 0.0F));
  burial_guard.lines.push_back(siege_line(1.30F, 1.10F, 0.90F));
  burial_guard.lines.push_back(command_line(0.60F));
  burial_guard.lines.push_back(support_line(0.60F, 0.0F));
  burial_guard.lines.push_back(cavalry_flank_line(0.98F, 1.20F, 0.50F, 0.0F, 0.0F));
  add_catch_all(burial_guard, ArmyRole::Reserve, 6);
  doctrine.intents[static_cast<int>(ArmyFormationIntent::FactionDefault)] =
      burial_guard;

  DoctrineIntentTemplate line = burial_guard;
  line.intent = ArmyFormationIntent::Line;
  line.frontage_scale = 1.00F;
  line.depth_scale = 1.10F;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Line)] = line;

  DoctrineIntentTemplate shrine_defence = burial_guard;
  shrine_defence.intent = ArmyFormationIntent::Defensive;
  shrine_defence.frontage_scale = 0.70F;
  shrine_defence.depth_scale = 1.45F;
  shrine_defence.spacing_scale = 0.70F;
  shrine_defence.reserve_rows = 2;
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Defensive)] = shrine_defence;

  DoctrineIntentTemplate dense_advance = burial_guard;
  dense_advance.intent = ArmyFormationIntent::Assault;
  dense_advance.frontage_scale = 0.75F;
  dense_advance.depth_scale = 1.55F;
  dense_advance.spacing_scale = 0.72F;
  for (auto& rule : dense_advance.lines) {
    if (rule.role == ArmyRole::Vanguard) {
      rule.front_offset_scale = 0.7F;
    }
  }
  doctrine.intents[static_cast<int>(ArmyFormationIntent::Assault)] = dense_advance;

  doctrine.intents[static_cast<int>(ArmyFormationIntent::Column)] =
      column_template(ArmyFormationIntent::Column, 0.75F, 0.0F);

  DoctrineIntentTemplate siege_escort = burial_guard;
  siege_escort.intent = ArmyFormationIntent::SiegeEscort;
  siege_escort.depth_scale = 1.50F;
  siege_escort.required_roles = k_any_siege;
  siege_escort.requirement_hint = QT_TRANSLATE_NOOP(
      "Formation", "Requires at least one siege engine in the selection.");
  doctrine.intents[static_cast<int>(ArmyFormationIntent::SiegeEscort)] = siege_escort;

  return doctrine;
}

DoctrineRegistry::DoctrineRegistry() {
  reset_to_defaults();
}

auto DoctrineRegistry::instance() -> DoctrineRegistry& {
  static DoctrineRegistry registry;
  return registry;
}

void DoctrineRegistry::reset_to_defaults() {
  m_doctrines.clear();
  m_neutral = make_neutral_doctrine();
  register_doctrine(m_neutral);
  register_doctrine(make_rome_doctrine());
  register_doctrine(make_carthage_doctrine());
  register_doctrine(make_iron_sepulcher_doctrine());
}

void DoctrineRegistry::clear() {
  m_doctrines.clear();
  m_neutral = make_neutral_doctrine();
  register_doctrine(m_neutral);
}

void DoctrineRegistry::register_doctrine(FormationDoctrine doctrine) {
  auto const id = doctrine.id;
  m_doctrines[id] = std::move(doctrine);
}

auto DoctrineRegistry::find(const FormationDoctrineId& id) const
    -> const FormationDoctrine* {
  auto it = m_doctrines.find(id);
  return it == m_doctrines.end() ? nullptr : &it->second;
}

auto DoctrineRegistry::get_or_neutral(const FormationDoctrineId& id) const
    -> const FormationDoctrine& {
  if (const auto* found = find(id)) {
    return *found;
  }
  return m_neutral;
}

auto DoctrineRegistry::ids() const -> std::vector<FormationDoctrineId> {
  std::vector<FormationDoctrineId> out;
  out.reserve(m_doctrines.size());
  for (const auto& entry : m_doctrines) {
    out.push_back(entry.first);
  }
  std::sort(out.begin(), out.end());
  return out;
}

auto DoctrineRegistry::availability_reason(const FormationDoctrineId& doctrine_id,
                                           ArmyFormationIntent intent,
                                           RoleTagSet available_roles,
                                           int member_count) const -> std::string {

  if (member_count <= 0) {
    return QCoreApplication::translate("Formation", "No units selected.").toStdString();
  }
  const auto& doctrine = get_or_neutral(doctrine_id);
  const auto* tmpl = doctrine.resolve_template(intent);
  if (tmpl == nullptr) {
    return QCoreApplication::translate("Formation",
                                       "%1 has no template for this intent.")
        .arg(Util::tr_asset(Util::k_formations_context, doctrine.display_name))
        .toStdString();
  }
  if (tmpl->required_roles != 0U &&
      !has_any_role(available_roles, tmpl->required_roles)) {
    return tmpl->requirement_hint.empty()
               ? QCoreApplication::translate(
                     "Formation",
                     "The selection lacks the troop types this formation needs.")
                     .toStdString()
               : Util::tr_asset_std(Util::k_formations_context, tmpl->requirement_hint);
  }
  if (intent == ArmyFormationIntent::Encirclement && member_count < 3) {
    return QCoreApplication::translate("Formation",
                                       "Encirclement needs at least three units.")
        .toStdString();
  }
  return {};
}

} // namespace Game::Formation
