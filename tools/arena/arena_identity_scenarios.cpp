#include "arena_identity_scenarios.h"

#include <span>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

struct HistoricalCommanderEntry {
  const char* group_name{};
  const char* commander_id{};
  Nation nation;
  int owner{};
};

// The ten historical cameo commanders of issue #1522: Rome and Masinissa in the
// front rank, the Barcid officers behind. Two owners only, because cameos serve
// side by side under one owner the way the battle scripts field them.
constexpr HistoricalCommanderEntry k_historical_front_rank[] = {
    {"sempronius", "roman_sempronius_longus", Nation::RomanRepublic, 1},
    {"flaminius", "roman_gaius_flaminius", Nation::RomanRepublic, 1},
    {"varro", "roman_terentius_varro", Nation::RomanRepublic, 1},
    {"paullus", "roman_aemilius_paullus", Nation::RomanRepublic, 1},
    {"scipio_218", "roman_scipio_consul_218", Nation::RomanRepublic, 1},
    {"masinissa", "numidian_masinissa", Nation::RomanRepublic, 1},
};
constexpr HistoricalCommanderEntry k_historical_back_rank[] = {
    {"mago", "carthage_mago_barca", Nation::Carthage, 2},
    {"maharbal", "carthage_maharbal", Nation::Carthage, 2},
    {"hanno_bomilcar", "carthage_hanno_bomilcar", Nation::Carthage, 2},
    {"hasdrubal_cavalry", "carthage_hasdrubal_cavalry", Nation::Carthage, 2},
};

void add_historical_commander_ranks(ArenaScenarioDefinition& s,
                                    float spacing,
                                    float rank_depth,
                                    bool expect_existence) {
  auto add_rank = [&](std::span<const HistoricalCommanderEntry> rank, float z) {
    float const first_x = -0.5F * spacing * static_cast<float>(rank.size() - 1U);
    for (std::size_t index = 0; index < rank.size(); ++index) {
      auto const& entry = rank[index];
      QVector3D const position(first_x + spacing * static_cast<float>(index), 0.0F, z);
      auto commander = group(QString::fromLatin1(entry.group_name),
                             Troop::RomanVeteranConsul,
                             entry.owner,
                             1,
                             position,
                             1);
      commander.nation_id = entry.nation;
      commander.commander_id = QString::fromLatin1(entry.commander_id);
      s.groups.push_back(std::move(commander));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      if (expect_existence) {
        s.expectations.push_back(
            expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      }
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
  };
  add_rank(k_historical_front_rank, rank_depth);
  add_rank(k_historical_back_rank, -rank_depth);
}

} // namespace

auto build_identity_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_historical_commander_lineup_id),
        QStringLiteral("Historical Commander Lineup"),
        QStringLiteral("The ten non-playable historical commanders (consuls, Barcid "
                       "officers and Masinissa) without escorts, for close-shot "
                       "review of helmet, plume, cloak and weapon identity."),
        12.0F,
        {15.5F, 18.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.camera_focus = QVector3D(0.0F, 1.15F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 1}};
    add_historical_commander_ranks(s, 3.0F, 1.7F, true);
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }
  {
    auto s = definition(
        QString::fromLatin1(k_historical_commander_helmet_review_id),
        QStringLiteral("Historical Commander Helmet Review"),
        QStringLiteral("Head-height close-up of the ten historical commanders so "
                       "crest, plume, diadem and cloak colour can be told apart at "
                       "the size a documentary close shot reads them."),
        6.0F,
        {10.5F, 9.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 1}};
    s.camera_focus = QVector3D(0.0F, 1.10F, 0.0F);
    add_historical_commander_ranks(s, 1.9F, 1.3F, false);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_commander_identity_lineup_id),
        QStringLiteral("Commander Identity Lineup"),
        QStringLiteral("Displays all six commanders without bodyguards or supporting "
                       "units for direct silhouette, weapon, scale, color, and "
                       "ancient-dark-fantasy identity review."),
        12.0F,
        {11.2F, 21.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.camera_focus = QVector3D(0.75F, 1.15F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.owner_teams = {{.owner_id = 1, .team_id = 1},
                     {.owner_id = 2, .team_id = 1},
                     {.owner_id = 3, .team_id = 1},
                     {.owner_id = 4, .team_id = 1},
                     {.owner_id = 5, .team_id = 1},
                     {.owner_id = 6, .team_id = 1}};
    struct CommanderLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      QVector3D position;
      float facing{};
    };
    const CommanderLineupEntry entries[] = {
        {"scipio",
         Troop::RomanVeteranConsul,
         Nation::RomanRepublic,
         1,
         {-3.0F, 0.0F, 1.7F},
         0.0F},
        {"fabius",
         Troop::RomanLegionOrganizer,
         Nation::RomanRepublic,
         2,
         {0.0F, 0.0F, 1.7F},
         0.0F},
        {"marcellus",
         Troop::RomanFieldCommander,
         Nation::RomanRepublic,
         3,
         {3.0F, 0.0F, 1.7F},
         0.0F},
        {"hannibal",
         Troop::CarthageSwordCommander,
         Nation::Carthage,
         4,
         {-1.5F, 0.0F, -1.7F},
         0.0F},
        {"hanno",
         Troop::CarthageSpearCommander,
         Nation::Carthage,
         5,
         {1.5F, 0.0F, -1.7F},
         0.0F},
        {"hasdrubal",
         Troop::CarthageBowCommander,
         Nation::Carthage,
         6,
         {4.5F, 0.0F, -1.7F},
         0.0F},
    };
    for (auto const& entry : entries) {
      auto commander = group(QString::fromLatin1(entry.group_name),
                             entry.troop,
                             entry.owner,
                             1,
                             entry.position,
                             1);
      commander.nation_id = entry.nation;
      commander.facing_degrees = entry.facing;
      s.groups.push_back(std::move(commander));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_healer_identity_lineup_id),
        QStringLiteral("Healer Identity Lineup"),
        QStringLiteral("Displays the Roman senator-physician, the Carthaginian dark "
                       "mage, and the Iron Sepulcher grave priest from the front and "
                       "from behind for direct robe, silhouette, and faction-identity "
                       "review."),
        12.0F,
        {13.0F, 12.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.5F);
    struct HealerLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float facing{};
    };

    s.owner_teams = {{.owner_id = 1, .team_id = 1},
                     {.owner_id = 2, .team_id = 1},
                     {.owner_id = 3, .team_id = 1}};
    const HealerLineupEntry entries[] = {
        {"roman_front", Troop::Healer, Nation::RomanRepublic, 1, -7.5F, 180.0F},
        {"roman_back", Troop::Healer, Nation::RomanRepublic, 1, -4.5F, 0.0F},
        {"carthage_front", Troop::Healer, Nation::Carthage, 2, -1.5F, 180.0F},
        {"carthage_back", Troop::Healer, Nation::Carthage, 2, 1.5F, 0.0F},
        {"priest_front", Troop::GravePriest, Nation::IronSepulcher, 3, 4.5F, 180.0F},
        {"priest_back", Troop::GravePriest, Nation::IronSepulcher, 3, 7.5F, 0.0F},
    };
    for (auto const& entry : entries) {
      auto healer = group(QString::fromLatin1(entry.group_name),
                          entry.troop,
                          entry.owner,
                          1,
                          {entry.x, 0.0F, 0.0F},
                          1);
      healer.nation_id = entry.nation;
      healer.facing_degrees = entry.facing;
      s.groups.push_back(std::move(healer));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_troop_identity_lineup_id),
        QStringLiteral("Troop Identity Lineup"),
        QStringLiteral("Both nations' infantry, support and worker roles side by "
                       "side so silhouettes, equipment and palettes can be compared "
                       "directly. Rome occupies the near row, Carthage the far one."),
        12.0F,
        {15.0F, 17.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    struct TroopLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
    };
    const TroopLineupEntry entries[] = {
        {"rome_archer", Troop::Archer, Nation::RomanRepublic, 1, -7.5F, -3.0F},
        {"rome_spearman", Troop::Spearman, Nation::RomanRepublic, 1, -4.5F, -3.0F},
        {"rome_swordsman", Troop::Swordsman, Nation::RomanRepublic, 1, -1.5F, -3.0F},
        {"rome_healer", Troop::Healer, Nation::RomanRepublic, 1, 1.5F, -3.0F},
        {"rome_builder", Troop::Builder, Nation::RomanRepublic, 1, 4.5F, -3.0F},
        {"rome_civilian", Troop::Civilian, Nation::RomanRepublic, 1, 7.5F, -3.0F},
        {"carthage_archer", Troop::Archer, Nation::Carthage, 2, -7.5F, 3.0F},
        {"carthage_spearman", Troop::Spearman, Nation::Carthage, 2, -4.5F, 3.0F},
        {"carthage_swordsman", Troop::Swordsman, Nation::Carthage, 2, -1.5F, 3.0F},
        {"carthage_healer", Troop::Healer, Nation::Carthage, 2, 1.5F, 3.0F},
        {"carthage_builder", Troop::Builder, Nation::Carthage, 2, 4.5F, 3.0F},
        {"carthage_civilian", Troop::Civilian, Nation::Carthage, 2, 7.5F, 3.0F},
    };
    for (auto const& entry : entries) {
      auto troop = group(QString::fromLatin1(entry.group_name),
                         entry.troop,
                         entry.owner,
                         1,
                         {entry.x, 0.0F, entry.z},
                         1);
      troop.nation_id = entry.nation;
      troop.facing_degrees = 180.0F;
      s.groups.push_back(std::move(troop));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_commander_helmet_review_id),
                   QStringLiteral("Commander Helmet Review"),
                   QStringLiteral("Head-height close-up of all six commander helmets, "
                                  "three per rank, so crest, shell profile, cheek "
                                  "pieces and neck guard can be judged at the size a "
                                  "player actually reads them."),
                   6.0F,
                   {7.5F, 11.5F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.owner_teams = {{.owner_id = 1, .team_id = 1},
                     {.owner_id = 2, .team_id = 1},
                     {.owner_id = 3, .team_id = 1},
                     {.owner_id = 4, .team_id = 1},
                     {.owner_id = 5, .team_id = 1},
                     {.owner_id = 6, .team_id = 1}};
    s.camera_focus = QVector3D(0.45F, 1.10F, 0.0F);
    struct CommanderHelmetEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      QVector3D position;
      float facing{};
    };
    const CommanderHelmetEntry entries[] = {
        {"fabius",
         Troop::RomanLegionOrganizer,
         Nation::RomanRepublic,
         1,
         {-1.9F, 0.0F, 1.3F},
         0.0F},
        {"scipio",
         Troop::RomanVeteranConsul,
         Nation::RomanRepublic,
         2,
         {0.0F, 0.0F, 1.3F},
         0.0F},
        {"marcellus",
         Troop::RomanFieldCommander,
         Nation::RomanRepublic,
         3,
         {1.9F, 0.0F, 1.3F},
         0.0F},
        {"hanno",
         Troop::CarthageSpearCommander,
         Nation::Carthage,
         4,
         {-0.95F, 0.0F, -1.3F},
         0.0F},
        {"hasdrubal",
         Troop::CarthageBowCommander,
         Nation::Carthage,
         5,
         {0.95F, 0.0F, -1.3F},
         0.0F},
        {"hannibal",
         Troop::CarthageSwordCommander,
         Nation::Carthage,
         6,
         {2.85F, 0.0F, -1.3F},
         0.0F},
    };
    for (auto const& entry : entries) {
      auto commander = group(QString::fromLatin1(entry.group_name),
                             entry.troop,
                             entry.owner,
                             1,
                             entry.position,
                             1);
      commander.nation_id = entry.nation;
      commander.facing_degrees = entry.facing;
      s.groups.push_back(std::move(commander));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_helmet_identity_review_id),
        QStringLiteral("Helmet Identity Review"),
        QStringLiteral("Head-height close-up of the helmets both nations issue, from "
                       "the front and in profile, so bowl shape, brow, cheek guards, "
                       "neck guard and crest can be judged at the size a player "
                       "actually reads them."),
        6.0F,
        {3.2F, 6.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 1.15F, 0.0F);
    struct HelmetReviewEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
      float facing{};
    };
    const HelmetReviewEntry entries[] = {
        {"line_front", Troop::Archer, Nation::RomanRepublic, 1, -1.8F, 0.0F, 180.0F},
        {"line_profile", Troop::Archer, Nation::RomanRepublic, 1, -0.6F, 0.0F, 270.0F},
        {"work_front", Troop::Builder, Nation::RomanRepublic, 1, 0.6F, 0.0F, 180.0F},
        {"work_profile", Troop::Builder, Nation::RomanRepublic, 1, 1.8F, 0.0F, 270.0F},
    };
    for (auto const& entry : entries) {
      auto unit = group(QString::fromLatin1(entry.group_name),
                        entry.troop,
                        entry.owner,
                        1,
                        {entry.x, 0.0F, entry.z},
                        1);
      unit.nation_id = entry.nation;
      unit.facing_degrees = entry.facing;
      s.groups.push_back(std::move(unit));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_worker_identity_lineup_id),
        QStringLiteral("Worker Identity Lineup"),
        QStringLiteral("Builders and civilians of both nations from the front and "
                       "from behind. Builders carry tools, aprons and arm guards; "
                       "civilians carry household goods and no work gear."),
        12.0F,
        {12.0F, 14.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.5F);
    struct WorkerLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
      float facing{};
    };
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 1}};
    const WorkerLineupEntry entries[] = {
        {"rome_builder_front",
         Troop::Builder,
         Nation::RomanRepublic,
         1,
         -4.5F,
         -3.0F,
         180.0F},
        {"rome_builder_back",
         Troop::Builder,
         Nation::RomanRepublic,
         1,
         -1.5F,
         -3.0F,
         0.0F},
        {"rome_settler_front",
         Troop::Civilian,
         Nation::RomanRepublic,
         1,
         1.5F,
         -3.0F,
         180.0F},
        {"rome_settler_back",
         Troop::Civilian,
         Nation::RomanRepublic,
         1,
         4.5F,
         -3.0F,
         0.0F},
        {"punic_builder_front",
         Troop::Builder,
         Nation::Carthage,
         2,
         -4.5F,
         3.0F,
         180.0F},
        {"punic_builder_back", Troop::Builder, Nation::Carthage, 2, -1.5F, 3.0F, 0.0F},
        {"punic_settler_front",
         Troop::Civilian,
         Nation::Carthage,
         2,
         1.5F,
         3.0F,
         180.0F},
        {"punic_settler_back", Troop::Civilian, Nation::Carthage, 2, 4.5F, 3.0F, 0.0F},
    };
    for (auto const& entry : entries) {
      auto worker = group(QString::fromLatin1(entry.group_name),
                          entry.troop,
                          entry.owner,
                          1,
                          {entry.x, 0.0F, entry.z},
                          1);
      worker.nation_id = entry.nation;
      worker.facing_degrees = entry.facing;
      s.groups.push_back(std::move(worker));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    struct SettlementWorksSpec {
      const char* id;
      const char* label;
      const char* description;
      Nation nation;
      const char* builder_group;
      const char* civilian_group;
      const char* home_group;
      const char* market_group;
      const char* barracks_group;
      const char* tree_prop;
    };
    const SettlementWorksSpec specs[] = {
        {k_roman_settlement_works_id,
         "Roman Settlement Works",
         "Roman builders raise a home and a marketplace and work the tree line "
         "while civilians carry their household goods in to settle. Shows the "
         "worker roles apart: tools and aprons on the builders, bedroll and "
         "pannier on the civilians.",
         Nation::RomanRepublic,
         "roman_builders",
         "roman_settlers",
         "roman_works_home",
         "roman_works_market",
         "roman_works_barracks",
         "olive_tree"},
        {k_carthage_settlement_works_id,
         "Carthaginian Settlement Works",
         "Carthaginian builders raise a home and a marketplace and work the tree "
         "line while civilians carry their household goods in to settle. Shows "
         "the worker roles apart: tools and aprons on the builders, amphora and "
         "satchel on the civilians.",
         Nation::Carthage,
         "punic_builders",
         "punic_settlers",
         "punic_works_home",
         "punic_works_market",
         "punic_works_barracks",
         "olive_tree"},
    };
    for (auto const& spec : specs) {
      auto s = definition(QString::fromLatin1(spec.id),
                          QString::fromLatin1(spec.label),
                          QString::fromLatin1(spec.description),
                          60.0F,
                          {24.0F, 32.0F, 20.0F});
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

      auto builders = group(QString::fromLatin1(spec.builder_group),
                            Troop::Builder,
                            2,
                            3,
                            {-6.0F, 0.0F, 2.0F},
                            1,
                            {3.0F, 0.0F, 0.0F});
      builders.nation_id = spec.nation;
      builders.ai_controlled = true;

      auto settlers = group(QString::fromLatin1(spec.civilian_group),
                            Troop::Civilian,
                            2,
                            3,
                            {-6.0F, 0.0F, 8.0F},
                            1,
                            {3.0F, 0.0F, 0.0F});
      settlers.nation_id = spec.nation;
      settlers.facing_degrees = 180.0F;

      s.groups = {building(QString::fromLatin1(spec.home_group),
                           Game::Units::SpawnType::Home,
                           spec.nation,
                           2,
                           1,
                           {-9.0F, 0.0F, -6.0F}),
                  building(QString::fromLatin1(spec.market_group),
                           Game::Units::SpawnType::Marketplace,
                           spec.nation,
                           2,
                           1,
                           {2.0F, 0.0F, -7.0F}),
                  building(QString::fromLatin1(spec.barracks_group),
                           Game::Units::SpawnType::Barracks,
                           spec.nation,
                           2,
                           1,
                           {12.0F, 0.0F, -8.0F}),
                  std::move(builders),
                  std::move(settlers)};
      for (auto& works_group : s.groups) {
        works_group.ai_controlled = true;
      }

      s.resource_patches = {
          {QString::fromLatin1(spec.tree_prop),
           6,
           {12.0F, 0.0F, -2.0F},
           {0.0F, 0.0F, 2.4F},
           1.15F},
          {QStringLiteral("boulder"), 4, {10.0F, 0.0F, 9.0F}, {2.2F, 0.0F, 0.0F}, 1.1F},
      };

      s.expectations.push_back(expectation(Expect::GroupIsRendered,
                                           QString::fromLatin1(spec.builder_group)));
      s.expectations.push_back(expectation(Expect::GroupIsRendered,
                                           QString::fromLatin1(spec.civilian_group)));
      s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                           QString::fromLatin1(spec.builder_group),
                                           {},
                                           1.0F));
      s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                           QString::fromLatin1(spec.builder_group)));
      s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(
        QString::fromLatin1(k_healer_lod_probe_id),
        QStringLiteral("Healer LOD Probe"),
        QStringLiteral("Healers and a swordsman at gameplay camera distance with "
                       "production level-of-detail selection, so support units are "
                       "checked for the same reduced-detail coverage as line troops."),
        6.0F,
        {45.0F, 40.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = false;
    s.camera_focus = QVector3D(0.5F, 0.0F, 0.5F);
    struct LodProbeEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      QVector3D position;
    };
    const LodProbeEntry entries[] = {
        {"roman_healer", Troop::Healer, Nation::RomanRepublic, {-4.0F, 0.0F, 0.0F}},
        {"carthage_healer", Troop::Healer, Nation::Carthage, {0.0F, 0.0F, 0.0F}},
        {"roman_swordsman",
         Troop::Swordsman,
         Nation::RomanRepublic,
         {4.0F, 0.0F, 0.0F}},
    };
    for (auto const& entry : entries) {
      auto probe = group(
          QString::fromLatin1(entry.group_name), entry.troop, 1, 1, entry.position, 1);
      probe.nation_id = entry.nation;
      s.groups.push_back(std::move(probe));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
