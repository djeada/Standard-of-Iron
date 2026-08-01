#include "army_formation_types.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>

namespace Game::Formation {

namespace {

template <typename EnumT, std::size_t N>
struct NameTable {
  std::array<std::pair<EnumT, const char*>, N> entries;

  [[nodiscard]] auto to_string(EnumT value) const -> const char* {
    for (const auto& entry : entries) {
      if (entry.first == value) {
        return entry.second;
      }
    }
    return entries.front().second;
  }

  [[nodiscard]] auto parse(const QString& value) const -> std::optional<EnumT> {
    const QString lowered = value.trimmed().toLower();
    for (const auto& entry : entries) {
      if (lowered == QLatin1String(entry.second)) {
        return entry.first;
      }
    }
    return std::nullopt;
  }
};

constexpr NameTable<ArmyFormationIntent, 7> k_intents{
    {{{ArmyFormationIntent::FactionDefault, "faction_default"},
      {ArmyFormationIntent::Line, "line"},
      {ArmyFormationIntent::Column, "column"},
      {ArmyFormationIntent::Defensive, "defensive"},
      {ArmyFormationIntent::Assault, "assault"},
      {ArmyFormationIntent::Encirclement, "encirclement"},
      {ArmyFormationIntent::SiegeEscort, "siege_escort"}}}};

constexpr NameTable<FlankPreference, 4> k_flanks{
    {{{FlankPreference::Balanced, "balanced"},
      {FlankPreference::StrongLeft, "left"},
      {FlankPreference::StrongRight, "right"},
      {FlankPreference::Split, "split"}}}};

constexpr NameTable<MovementPolicy, 2> k_movement{
    {{{MovementPolicy::ReformAtDestination, "reform_at_destination"},
      {MovementPolicy::MaintainFormation, "maintain_formation"}}}};

constexpr NameTable<RangedPlacement, 3> k_ranged{
    {{{RangedPlacement::Rear, "rear"},
      {RangedPlacement::Front, "front"},
      {RangedPlacement::Skirmish, "skirmish"}}}};

constexpr NameTable<MixedDoctrinePolicy, 4> k_mixed{
    {{{MixedDoctrinePolicy::MajorityDoctrine, "majority_doctrine"},
      {MixedDoctrinePolicy::CompositeByRole, "composite_by_role"},
      {MixedDoctrinePolicy::SeparateContingents, "separate_contingents"},
      {MixedDoctrinePolicy::CommanderDoctrine, "commander_doctrine"}}}};

} // namespace

auto intent_to_string(ArmyFormationIntent intent) -> const char* {
  return k_intents.to_string(intent);
}

auto intent_display_name(ArmyFormationIntent intent) -> QString {
  switch (intent) {
  case ArmyFormationIntent::FactionDefault:
    return QCoreApplication::translate("Formation", "Faction Default");
  case ArmyFormationIntent::Line:
    return QCoreApplication::translate("Formation", "Line");
  case ArmyFormationIntent::Column:
    return QCoreApplication::translate("Formation", "Column");
  case ArmyFormationIntent::Defensive:
    return QCoreApplication::translate("Formation", "Defensive");
  case ArmyFormationIntent::Assault:
    return QCoreApplication::translate("Formation", "Assault");
  case ArmyFormationIntent::Encirclement:
    return QCoreApplication::translate("Formation", "Encirclement");
  case ArmyFormationIntent::SiegeEscort:
    return QCoreApplication::translate("Formation", "Siege Escort");
  }
  return QCoreApplication::translate("Formation", "Faction Default");
}

auto try_parse_intent(const QString& value) -> std::optional<ArmyFormationIntent> {
  return k_intents.parse(value);
}

auto all_intents() -> std::vector<ArmyFormationIntent> {
  return {ArmyFormationIntent::FactionDefault,
          ArmyFormationIntent::Line,
          ArmyFormationIntent::Column,
          ArmyFormationIntent::Defensive,
          ArmyFormationIntent::Assault,
          ArmyFormationIntent::Encirclement,
          ArmyFormationIntent::SiegeEscort};
}

auto flank_preference_to_string(FlankPreference pref) -> const char* {
  return k_flanks.to_string(pref);
}

auto try_parse_flank_preference(const QString& value)
    -> std::optional<FlankPreference> {
  return k_flanks.parse(value);
}

auto movement_policy_to_string(MovementPolicy policy) -> const char* {
  return k_movement.to_string(policy);
}

auto try_parse_movement_policy(const QString& value) -> std::optional<MovementPolicy> {
  return k_movement.parse(value);
}

auto ranged_placement_to_string(RangedPlacement placement) -> const char* {
  return k_ranged.to_string(placement);
}

auto try_parse_ranged_placement(const QString& value)
    -> std::optional<RangedPlacement> {
  return k_ranged.parse(value);
}

auto mixed_policy_to_string(MixedDoctrinePolicy policy) -> const char* {
  return k_mixed.to_string(policy);
}

auto try_parse_mixed_policy(const QString& value)
    -> std::optional<MixedDoctrinePolicy> {
  return k_mixed.parse(value);
}

auto ArmyFormation::find_slot(int slot_id) const -> const FormationSlot* {
  auto it = std::find_if(slot_list.begin(),
                         slot_list.end(),
                         [slot_id](const auto& slot) { return slot.id == slot_id; });
  return it == slot_list.end() ? nullptr : &(*it);
}

auto ArmyFormation::find_slot_for(EntityID entity) const -> const FormationSlot* {
  auto it =
      std::find_if(slot_list.begin(), slot_list.end(), [entity](const auto& slot) {
        return slot.occupant == entity;
      });
  return it == slot_list.end() ? nullptr : &(*it);
}

auto ArmyFormation::has_member(EntityID entity) const -> bool {
  return std::find(members.begin(), members.end(), entity) != members.end();
}

auto ArmyFormation::blocked_slot_count() const -> int {
  return static_cast<int>(
      std::count_if(slot_list.begin(), slot_list.end(), [](const auto& s) {
        return s.status == SlotStatus::Blocked;
      }));
}

} // namespace Game::Formation
