

#include <algorithm>
#include <cstdlib>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "tests/core/campaign_playthrough_harness.h"

namespace {

using namespace Playthrough;

constexpr char k_campaign[] = "second_punic_war";

auto cap_seconds() -> float {
  const char* value = std::getenv("SOI_PLAYTHROUGH_CAP");
  return value != nullptr && std::atof(value) > 0.0
             ? static_cast<float>(std::atof(value))
             : 25.0F * 60.0F;
}

auto default_plan() -> Plan;

class CampaignPlaythroughTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto play(const char* mission_id, Plan plan) -> Report {

    if (const char* which = std::getenv("SOI_PT_PLAN");
        which != nullptr && std::string(which) == "default") {
      plan = default_plan();
    }
    Mission mission;
    if (!mission.load(QString::fromLatin1(k_campaign),
                      QString::fromLatin1(mission_id))) {
      ADD_FAILURE() << mission_id << " did not load: " << mission.error().toStdString();
      return {};
    }
    if (narrating()) {
      std::printf("== %s\n", mission_id);
    }
    Report report = mission.run(std::move(plan), cap_seconds());
    std::printf("[playthrough] %s %s\n", mission_id, report.line().c_str());
    std::printf("[playthrough] %s objectives: %s\n",
                mission_id,
                report.objectives.join(QStringLiteral(", ")).toStdString().c_str());
    return report;
  }

  struct Progress {
    int commanders_killed = 0;
    int camps_captured = 0;
    int stages_complete = 0;
    float survives_seconds = 0.0F;
  };
  static void expect_progress(const Report& report, const Progress& floor) {
    EXPECT_GE(report.commanders_killed, floor.commanders_killed) << report.line();
    EXPECT_GE(report.camps_captured, floor.camps_captured) << report.line();
    EXPECT_GE(report.stages_complete, floor.stages_complete) << report.line();
    EXPECT_GE(report.seconds, floor.survives_seconds) << report.line();
  }

  static void expect_victory(const Report& report) {
    EXPECT_EQ(report.outcome.toStdString(), "victory")
        << report.line() << "\n defeat: " << report.defeat_description.toStdString();
    EXPECT_TRUE(report.defeat_rule.isEmpty()) << report.defeat_rule.toStdString();
  }
};

auto army_of(Mission& m) -> Ids {
  return m.army();
}

auto knob(const char* name, float fallback) -> float {
  const char* value = std::getenv((std::string("SOI_PT_") + name).c_str());
  return value != nullptr ? static_cast<float>(std::atof(value)) : fallback;
}

struct Decapitate {
  std::function<Ids(Mission&)> who = army_of;
  std::optional<G> home;
  int garrison = static_cast<int>(knob("GARRISON", 3));
  float defend_radius = knob("DEFEND", 45.0F);
  float strike_after = knob("STRIKE_AFTER", 0.0F);

  float strike_radius = knob("STRIKE_RADIUS", 35.0F);

  float breach_after = knob("BREACH_AFTER", 25.0F);

  float engaged_radius = knob("ENGAGED_RADIUS", 10.0F);

  std::vector<int> order;

  struct Ambush {
    G at;
    float pounce = 60.0F;
  };
  std::map<int, Ambush> ambush;

  float regroup_beyond = knob("REGROUP_BEYOND", 0.0F);
  float regroup_short = knob("REGROUP_SHORT", 60.0F);
  float every = 5.0F;
};

auto recruiting(std::vector<Game::Units::TroopType> mix = {
                    Game::Units::TroopType::Spearman,
                    Game::Units::TroopType::Swordsman,
                    Game::Units::TroopType::Archer}) -> Step {
  auto next = std::make_shared<std::size_t>(0);
  return {"keep the camps recruiting",
          always(),
          [mix = std::move(mix), next](Mission& m) {
            if (mix.empty()) {
              return;
            }
            if (m.recruit(mix[*next % mix.size()]) > 0) {
              ++*next;
            }
          },
          3.0F};
}

auto fight_men_not_stones(float reach = knob("MICRO_REACH", 14.0F)) -> Step {
  return {"turn companies from towers to the men beside them",
          always(),
          [reach](Mission& m) {
            const Ids foes = m.enemy_troops();
            for (const auto id : m.army()) {
              auto* entity = m.world().get_entity(id);
              const auto* attack =
                  entity == nullptr
                      ? nullptr
                      : entity->get_component<Engine::Core::AttackTargetComponent>();
              if (attack == nullptr || attack->target_id == 0 ||
                  !m.alive(attack->target_id)) {
                continue;
              }
              const auto* target = m.unit(attack->target_id);
              if (target == nullptr ||
                  !Game::Units::is_building_spawn(target->spawn_type) ||
                  target->spawn_type == SpawnType::WallGate) {
                continue;
              }
              if (const auto foe =
                      m.nearest(m.near(foes, m.position(id), reach), m.position(id))) {
                m.attack({id}, *foe);
              }
            }
          },
          2.0F};
}

auto not_fighting(Mission& m, const Ids& ids) -> Ids {
  Ids out;
  for (const auto id : ids) {
    auto* entity = m.world().get_entity(id);
    const auto* attack =
        entity == nullptr
            ? nullptr
            : entity->get_component<Engine::Core::AttackTargetComponent>();
    if (attack == nullptr || attack->target_id == 0 || !m.alive(attack->target_id)) {
      out.push_back(id);
    }
  }
  return out;
}

auto decapitate(Decapitate spec) -> Step {
  struct State {
    bool garrisoned = false;
    std::optional<G> home;
    QString mode;
    EntityID quarry = 0;
    G aim;
    std::optional<G> rally;
    float rally_until = 0.0F;
    float close_since = -1.0F;
    EntityID gate = 0;
  };
  auto state = std::make_shared<State>();
  return {
      "cut off the head: garrison home, break raids, hunt commanders, occupy camps",
      always(),
      [spec = std::move(spec), state](Mission& m) {
        if (!state->garrisoned) {
          state->garrisoned = true;
          state->home = spec.home;
          if (!state->home) {
            for (const auto id : m.barracks()) {
              if (m.unit(id)->owner_id == k_local_owner) {
                state->home = m.position(id);
                break;
              }
            }
          }
          if (state->home && spec.garrison > 0) {
            Ids foot = m.except_types(spec.who(m),
                                      {SpawnType::MountedSwordsman,
                                       SpawnType::HorseArcher,
                                       SpawnType::HorseSpearman,
                                       SpawnType::Elephant,
                                       SpawnType::Catapult,
                                       SpawnType::Ballista,
                                       SpawnType::Ram,
                                       SpawnType::SiegeTower});
            std::sort(foot.begin(), foot.end(), [&](EntityID a, EntityID b) {
              return Mission::distance(m.position(a), *state->home) <
                     Mission::distance(m.position(b), *state->home);
            });
            foot.resize(std::min<std::size_t>(foot.size(), spec.garrison));
            m.tag("garrison", foot);
            m.move(foot, *state->home);
          }
        }
        const Ids garrison = m.group("garrison");
        if (!garrison.empty() && state->home) {
          const Ids strayed = m.idle(garrison);
          Ids away;
          for (const auto id : strayed) {
            if (Mission::distance(m.position(id), *state->home) > 7.0F) {
              away.push_back(id);
            }
          }
          m.move(away, *state->home);
        }
        const Ids troops = m.all_but(spec.who(m), garrison);
        if (troops.empty()) {
          return;
        }

        auto set_mode = [&](const QString& mode, EntityID quarry) -> bool {
          const QString kind = mode.section(QLatin1Char(' '), 0, 1);
          const bool changed = kind != state->mode.section(QLatin1Char(' '), 0, 1) ||
                               quarry != state->quarry;
          state->mode = mode;
          state->quarry = quarry;
          if (changed && narrating()) {
            std::printf(
                "  [%5.0fs] plan mode: %s\n", m.seconds(), mode.toStdString().c_str());
          }
          return changed;
        };
        if (state->home) {
          const Ids threats =
              m.near(m.enemy_troops(), *state->home, spec.defend_radius);
          if (!threats.empty()) {
            const bool changed = set_mode(QStringLiteral("defend home"), 0);
            m.attack_move(changed ? not_fighting(m, troops) : m.idle(troops),
                          m.centroid(threats));
            return;
          }
        }
        if (m.seconds() < spec.strike_after) {
          if (set_mode(QStringLiteral("wait at home"), 0) && state->home) {
            m.attack_move(troops, *state->home);
          }
          return;
        }
        const G from = m.centroid(troops);
        const Ids commanders = m.enemy_commanders();
        std::optional<EntityID> quarry;
        std::vector<int> order = spec.order;
        if (const char* env = std::getenv("SOI_PT_ORDER")) {
          order.clear();
          for (const auto& part : QString::fromLatin1(env).split(QLatin1Char(','))) {
            order.push_back(part.toInt());
          }
        }
        for (const int owner : order) {
          for (const auto id : commanders) {
            if (m.unit(id)->owner_id == owner) {
              quarry = id;
              break;
            }
          }
          if (quarry) {
            break;
          }
        }
        if (!quarry) {
          quarry = m.nearest(commanders, from);
        }
        if (quarry) {
          const G at = m.position(*quarry);
          const auto lie_in_wait = spec.ambush.find(m.unit(*quarry)->owner_id);
          if (lie_in_wait != spec.ambush.end() &&
              Mission::distance(at, lie_in_wait->second.at) >
                  lie_in_wait->second.pounce) {
            const bool changed = set_mode(
                QStringLiteral("lie in wait at (%1,%2) for the commander of owner %3")
                    .arg(lie_in_wait->second.at.x, 0, 'f', 0)
                    .arg(lie_in_wait->second.at.z, 0, 'f', 0)
                    .arg(m.unit(*quarry)->owner_id),
                0);

            const Ids passing = m.near(
                m.enemy_troops(), lie_in_wait->second.at, lie_in_wait->second.pounce);
            if (!passing.empty()) {
              m.attack_move(not_fighting(m, troops), m.centroid(passing));
              return;
            }
            Ids away;
            for (const auto id : changed ? not_fighting(m, troops) : m.idle(troops)) {
              if (Mission::distance(m.position(id), lie_in_wait->second.at) > 15.0F) {
                away.push_back(id);
              }
            }
            m.attack_move(away, lie_in_wait->second.at);
            return;
          }
          const bool changed =
              set_mode(QStringLiteral("hunt commander of owner %1 at (%2,%3)")
                           .arg(m.unit(*quarry)->owner_id)
                           .arg(at.x, 0, 'f', 0)
                           .arg(at.z, 0, 'f', 0),
                       *quarry);

          if (changed && spec.regroup_beyond > 0.0F &&
              Mission::distance(from, at) > spec.regroup_beyond) {
            const float d = Mission::distance(from, at);
            const float k = (d - spec.regroup_short) / d;
            state->rally =
                G{from.x + (at.x - from.x) * k, from.z + (at.z - from.z) * k};
            state->rally_until = m.seconds() + 90.0F;
            m.attack_move(not_fighting(m, troops), *state->rally);
            if (narrating()) {
              std::printf("  [%5.0fs] plan: assemble at (%.0f,%.0f)\n",
                          m.seconds(),
                          state->rally->x,
                          state->rally->z);
            }
            return;
          }
          if (state->rally) {
            const auto there = m.near(troops, *state->rally, 25.0F);
            if (m.seconds() < state->rally_until &&
                there.size() * 10 < troops.size() * 7) {
              m.attack_move(m.idle(m.all_but(troops, there)), *state->rally);
              return;
            }
            state->rally.reset();
            state->aim = G{-1000.0F, -1000.0F};
          }

          const Ids close = m.near(troops, at, spec.strike_radius);
          if (changed) {
            state->gate = 0;
          }
          if (changed || close.empty()) {
            state->close_since = -1.0F;
          } else if (state->close_since < 0.0F) {
            state->close_since = m.seconds();
          }

          if (state->gate != 0 && !m.alive(state->gate)) {
            state->gate = 0;
            state->close_since = m.seconds();
          }
          if (state->gate == 0 && state->close_since >= 0.0F &&
              m.seconds() - state->close_since > spec.breach_after) {
            const Ids gates =
                m.collect([&](EntityID, const Engine::Core::UnitComponent& u) {
                  return u.spawn_type == SpawnType::WallGate &&
                         m.is_enemy_owner(u.owner_id);
                });
            if (const auto gate =
                    m.nearest(m.near(gates, at, 45.0F), m.centroid(close))) {
              state->gate = *gate;
            }
          }
          if (state->gate != 0) {
            if (const std::optional<EntityID> gate = state->gate) {
              Ids breachers;
              const Ids foes = m.enemy_troops();
              for (const auto id : close) {
                if (!m.near(foes, m.position(id), spec.engaged_radius).empty()) {
                  continue;
                }
                auto* entity = m.world().get_entity(id);
                const auto* attack =
                    entity == nullptr
                        ? nullptr
                        : entity->get_component<Engine::Core::AttackTargetComponent>();
                if (attack == nullptr || attack->target_id != *gate) {
                  breachers.push_back(id);
                }
              }
              if (narrating() && !breachers.empty()) {
                const G g = m.position(*gate);
                std::printf("  [%5.0fs] plan: breach the gate at (%.0f,%.0f), hp %d, "
                            "%zu companies\n",
                            m.seconds(),
                            g.x,
                            g.z,
                            m.unit(*gate)->health,
                            breachers.size());
              }
              m.attack(breachers, *gate);
              return;
            }
          }
          if (!close.empty()) {

            const Ids foes = m.all_but(m.enemy_troops(), {*quarry});
            Ids retarget;
            for (const auto id : close) {
              if (!m.near(foes, m.position(id), spec.engaged_radius).empty()) {
                continue;
              }
              auto* entity = m.world().get_entity(id);
              const auto* attack =
                  entity == nullptr
                      ? nullptr
                      : entity->get_component<Engine::Core::AttackTargetComponent>();
              if (attack == nullptr || attack->target_id != *quarry) {
                retarget.push_back(id);
              }
            }
            m.attack(retarget, *quarry);
          }

          const bool moved = Mission::distance(at, state->aim) > 12.0F;
          if (changed || moved) {
            state->aim = at;
          }
          const Ids far = m.all_but(troops, close);
          m.attack_move(changed || moved ? not_fighting(m, far) : m.idle(far), at);
          return;
        }

        Ids camps;
        for (const auto id : m.enemy_barracks()) {
          if (m.unit(id)->nation_id != Game::Systems::NationID::IronSepulcher) {
            camps.push_back(id);
          }
        }
        std::sort(camps.begin(), camps.end(), [&](EntityID a, EntityID b) {
          return Mission::distance(m.position(a), from) <
                 Mission::distance(m.position(b), from);
        });
        if (camps.size() > 2) {
          camps.resize(2);
        }
        if (camps.empty()) {
          set_mode(QStringLiteral("mop up"), 0);
          if (const auto straggler = m.nearest(m.enemy_troops(), from)) {
            m.attack_move(m.idle(troops), m.position(*straggler));
          }
          return;
        }
        set_mode(QStringLiteral("occupy the camps"), 0);
        const Ids idle = m.idle(troops);
        for (std::size_t i = 0; i < idle.size(); ++i) {
          m.move({idle[i]}, m.position(camps[i % camps.size()]));
        }
      },
      spec.every};
}

auto default_plan() -> Plan {
  return {recruiting(), fight_men_not_stones(), decapitate({})};
}

} // namespace

auto ferry(std::string label,
           std::function<bool(Mission&)> when,
           std::function<Ids(Mission&)> who,
           G bank,
           std::function<bool(Mission&, EntityID)> across) -> Step {
  auto called = std::make_shared<std::set<EntityID>>();
  return {std::move(label),
          std::move(when),
          [who = std::move(who), bank, across = std::move(across), called](Mission& m) {
            Ids walk;
            Ids board;
            for (const auto id : who(m)) {
              if (across(m, id) || called->count(id) > 0) {
                continue;
              }
              (Mission::distance(m.position(id), bank) <= 10.0F ? board : walk)
                  .push_back(id);
            }
            m.move(m.idle(walk), bank);
            if (!board.empty() && m.cross_by_raft(board)) {
              called->insert(board.begin(), board.end());
            }
          },
          5.0F};
}

auto occupy(std::string label,
            std::function<bool(Mission&)> when,
            std::function<Ids(Mission&)> who,
            G camp) -> Step {
  return {std::move(label),
          std::move(when),
          [who = std::move(who), camp](Mission& m) {
            Ids away;
            for (const auto id : m.idle(who(m))) {
              if (Mission::distance(m.position(id), camp) > 4.0F) {
                away.push_back(id);
              }
            }
            m.move(away, camp);
          },
          5.0F};
}

TEST_F(CampaignPlaythroughTest, CrossingTheRhoneKeepsItsProgress) {
  static constexpr G k_hill_fort{376.0F, 38.0F};
  static constexpr G k_branch_bank{450.0F, 222.0F};
  static constexpr G k_island_muster{452.0F, 266.0F};
  const auto on_island = [](Mission& m, EntityID id) {
    const G p = m.position(id);
    return p.x > 256.0F && p.z > 252.0F;
  };
  const auto fort_falls = [](Mission& m) {
    return !m.commander_alive(2);
  };
  const auto fort_party = [](Mission& m) {
    if (m.group("fort").empty() && !m.commander_alive(2)) {
      Ids party = m.local_of_type({SpawnType::Elephant});
      Ids foot =
          m.except_types(m.army(), {SpawnType::Elephant, SpawnType::MountedSwordsman});
      std::sort(foot.begin(), foot.end(), [&](EntityID a, EntityID b) {
        return Mission::distance(m.position(a), k_hill_fort) <
               Mission::distance(m.position(b), k_hill_fort);
      });
      foot.resize(std::min<std::size_t>(foot.size(), 2));
      party.insert(party.end(), foot.begin(), foot.end());
      m.tag("fort", party);
    }
    return m.group("fort");
  };
  const auto field_army = [fort_party](Mission& m) {
    return m.all_but(m.army(), fort_party(m));
  };
  expect_progress(
      play("crossing_the_rhone",
           {decapitate({.who =
                            [field_army, on_island](Mission& m) {
                              if (m.commander_alive(2)) {
                                return m.army();
                              }

                              const Ids field = field_army(m);
                              Ids landed;
                              for (const auto id : field) {
                                if (on_island(m, id)) {
                                  landed.push_back(id);
                                }
                              }
                              if (m.group("landed").empty() &&
                                  landed.size() * 10 >= field.size() * 8 &&
                                  !landed.empty()) {
                                m.tag("landed", landed);
                              }
                              return m.group("landed").empty() ? Ids{} : landed;
                            },
                        .garrison = 0,
                        .defend_radius = 0.0F,
                        .order = {2},
                        .regroup_beyond = 80.0F,
                        .regroup_short = 50.0F}),
            {"assemble on the island by the landing",
             [](Mission& m) {
               return !m.commander_alive(2) && m.group("landed").empty();
             },
             [on_island, field_army](Mission& m) {
               Ids waiting;
               for (const auto id : m.idle(field_army(m))) {
                 if (on_island(m, id) &&
                     Mission::distance(m.position(id), k_island_muster) > 12.0F) {
                   waiting.push_back(id);
                 }
               }
               m.move(waiting, k_island_muster);
             },
             5.0F},
            occupy("take the fallen hill fort", fort_falls, fort_party, k_hill_fort),
            ferry("raft the army over the eastern branch",
                  fort_falls,
                  field_army,
                  k_branch_bank,
                  on_island),
            recruiting(),
            fight_men_not_stones()}),
      Progress{.commanders_killed = 1,
               .camps_captured = 1,
               .stages_complete = 1,
               .survives_seconds = 1490.0F});
}

auto gather_quota(int wood, int stone, int iron) -> Step {
  auto assigned = std::make_shared<std::map<EntityID, std::string>>();
  return {
      "builders gather the road stores",
      always(),
      [wood, stone, iron, assigned](Mission& m) {
        using Game::Systems::ResourceType;
        struct Need {
          const char* product;
          double behind;
        };
        std::vector<Need> needs = {
            {"cut_tree", 1.0 - m.harvested(ResourceType::Wood) / double(wood)},
            {"collect_stone", 1.0 - m.harvested(ResourceType::Stone) / double(stone)},
            {"collect_iron_ore", 1.0 - m.harvested(ResourceType::Iron) / double(iron)}};
        std::stable_sort(needs.begin(), needs.end(), [](const Need& a, const Need& b) {
          return a.behind > b.behind;
        });
        const Ids builders = m.local_of_type({SpawnType::Builder});
        for (std::size_t i = 0; i < builders.size(); ++i) {

          const std::size_t pick = i < 2 ? 0 : std::min<std::size_t>(i - 1, 2);
          auto& current = (*assigned)[builders[i]];
          if (needs[pick].behind > 0.0 && current != needs[pick].product) {
            current = needs[pick].product;
            m.auto_gather({builders[i]}, needs[pick].product);
          }
        }
      },
      45.0F};
}

TEST_F(CampaignPlaythroughTest, CrossingTheAlpsKeepsItsProgress) {
  expect_progress(play("crossing_the_alps",
                       {gather_quota(600, 350, 300),
                        recruiting(),
                        fight_men_not_stones(),
                        decapitate({})}),
                  Progress{.stages_complete = 1, .survives_seconds = 1150.0F});
}

auto horse(Mission& m) -> Ids {
  return m.local_of_type(
      {SpawnType::MountedSwordsman, SpawnType::HorseArcher, SpawnType::HorseSpearman});
}

TEST_F(CampaignPlaythroughTest, BattleOfTicinoIsWinnable) {
  expect_victory(play("battle_of_ticino",
                      {recruiting(),
                       fight_men_not_stones(),
                       occupy(
                           "the foot hold the camp yard",
                           always(),
                           [](Mission& m) {
                             return m.except_types(m.army(),
                                                   {SpawnType::MountedSwordsman,
                                                    SpawnType::HorseArcher,
                                                    SpawnType::HorseSpearman});
                           },
                           G{140.0F, 293.0F}),
                       decapitate({.who = horse,
                                   .garrison = 0,
                                   .defend_radius = 0.0F,
                                   .order = {3, 2},
                                   .regroup_beyond = knob("TIC_REGROUP", 0.0F)})}));
}

TEST_F(CampaignPlaythroughTest, BattleOfTrebiaIsWinnable) {
  expect_victory(play(
      "battle_of_trebia",
      {recruiting(),
       decapitate({.garrison = 0, .defend_radius = 0.0F, .engaged_radius = 0.0F})}));
}

TEST_F(CampaignPlaythroughTest, BattleOfTrasimeneKeepsItsProgress) {
  expect_progress(play("battle_of_trasimene",
                       {recruiting(), fight_men_not_stones(), decapitate({})}),
                  Progress{.survives_seconds = 520.0F});
}

TEST_F(CampaignPlaythroughTest, BattleOfCannaeKeepsItsProgress) {
  expect_progress(
      play("battle_of_cannae",
           {recruiting(),
            fight_men_not_stones(),
            decapitate({.garrison = static_cast<int>(knob("CAN_GARRISON", 8)),
                        .defend_radius = knob("CAN_DEFEND", 0.0F),
                        .order = {2,
                                  static_cast<int>(knob("CAN_SECOND", 3)),
                                  static_cast<int>(knob("CAN_THIRD", 4))},
                        .regroup_beyond = knob("CAN_REGROUP", 120.0F)})}),
      Progress{.commanders_killed = 2, .survives_seconds = 560.0F});
}

TEST_F(CampaignPlaythroughTest, CampaniaCampaignKeepsItsProgress) {
  expect_progress(
      play("campania_campaign", {recruiting(), fight_men_not_stones(), decapitate({})}),
      Progress{.survives_seconds = 380.0F});
}

TEST_F(CampaignPlaythroughTest, BattleOfZamaKeepsItsProgress) {
  expect_progress(
      play("battle_of_zama", {recruiting(), fight_men_not_stones(), decapitate({})}),
      Progress{.survives_seconds = 360.0F});
}

namespace {

constexpr int k_victumulae_garrison = 2;

struct GarrisonWatch {
  struct Track {
    G last;
    float last_dx = 0.0F;
    float last_dz = 0.0F;
    bool has_last = false;
    bool has_step = false;
    G goal;
    G previous_goal;
    float goal_since = -1.0F;
    bool has_goal = false;
    bool has_previous_goal = false;
  };
  std::map<EntityID, Track> tracks;
  int reversals = 0;
  int window_reversals = 0;
  int archer_reversals = 0;
  int window_archer_reversals = 0;
  int order_flips = 0;
  int window_order_flips = 0;
  int peak_troops = 0;
  int recruited = 0;
  std::set<EntityID> seen;
  bool first_sample = true;
};

auto is_garrison_foot(const Engine::Core::UnitComponent& u) -> bool {
  return u.owner_id == k_victumulae_garrison && u.health > 0 &&
         (u.spawn_type == SpawnType::Swordsman || u.spawn_type == SpawnType::Spearman);
}

auto watch_the_garrison(std::shared_ptr<GarrisonWatch> watch) -> Step {
  Step step{
      "watch the garrison", always(), [watch](Mission& m) {
        constexpr float k_step = 0.6F;
        int troops = 0;
        for (auto [id, u] : m.world().view<const Engine::Core::UnitComponent>()) {
          if (u.owner_id != k_victumulae_garrison || u.health <= 0 ||
              !Game::Units::is_troop_spawn(u.spawn_type)) {
            continue;
          }
          ++troops;
          const bool soldier = u.spawn_type != SpawnType::Civilian &&
                               u.spawn_type != SpawnType::Builder &&
                               !m.world().has<Engine::Core::AssaultWaveComponent>(id);
          if (soldier && watch->seen.insert(id).second && !watch->first_sample) {
            ++watch->recruited;
          }
          const bool archer = u.spawn_type == SpawnType::Archer;
          if (!is_garrison_foot(u) && !archer) {
            continue;
          }
          auto& track = watch->tracks[id];
          const G at = m.position(id);
          if (track.has_last) {
            const float dx = at.x - track.last.x;
            const float dz = at.z - track.last.z;
            const float len = std::hypot(dx, dz);
            if (len >= k_step) {
              if (track.has_step) {
                const float prev = std::hypot(track.last_dx, track.last_dz);
                if ((dx * track.last_dx + dz * track.last_dz) < -0.5F * len * prev) {
                  ++(archer ? watch->archer_reversals : watch->reversals);
                  ++(archer ? watch->window_archer_reversals : watch->window_reversals);
                }
              }
              track.last_dx = dx;
              track.last_dz = dz;
              track.has_step = true;
            }
          }
          track.last = at;
          track.has_last = true;

          constexpr float k_same_goal = 2.0F;
          constexpr float k_flip_window = 10.0F;
          if (auto const* mv = m.world().try_get<Engine::Core::MovementComponent>(id);
              mv != nullptr && mv->get_has_target()) {
            const G goal =
                m.to_grid(QVector3D(mv->get_goal_x(), 0.0F, mv->get_goal_y()));
            if (!track.has_goal || Mission::distance(goal, track.goal) > k_same_goal) {
              if (track.has_previous_goal &&
                  Mission::distance(goal, track.previous_goal) <= k_same_goal &&
                  m.seconds() - track.goal_since <= k_flip_window) {
                ++watch->order_flips;
                ++watch->window_order_flips;
                if (narrating()) {
                  auto const* target =
                      m.world().try_get<Engine::Core::AttackTargetComponent>(id);
                  std::printf(
                      "  [%5.0fs] flip o2 %s %llu at (%.0f,%.0f): "
                      "(%.0f,%.0f) -> (%.0f,%.0f) -> back, target %s\n",
                      m.seconds(),
                      std::string(Game::Units::spawn_type_name(u.spawn_type)).c_str(),
                      static_cast<unsigned long long>(id),
                      at.x,
                      at.z,
                      track.previous_goal.x,
                      track.previous_goal.z,
                      track.goal.x,
                      track.goal.z,
                      target != nullptr && target->target_id != 0 &&
                              m.alive(target->target_id)
                          ? std::string(Game::Units::spawn_type_name(
                                            m.unit(target->target_id)->spawn_type))
                                .c_str()
                          : "-");
                }
              }
              track.previous_goal = track.goal;
              track.has_previous_goal = track.has_goal;
              track.goal = goal;
              track.goal_since = m.seconds();
              track.has_goal = true;
            }
          }
        }
        watch->first_sample = false;
        watch->peak_troops = std::max(watch->peak_troops, troops);
      }};
  step.every = 1.0F;
  return step;
}

auto report_the_garrison(std::shared_ptr<GarrisonWatch> watch) -> Step {
  Step step{
      "report the garrison", always(), [watch](Mission& m) {
        if (!narrating()) {
          watch->window_reversals = 0;
          watch->window_archer_reversals = 0;
          watch->window_order_flips = 0;
          return;
        }
        int troops = 0;
        int foot = 0;
        int fighting = 0;
        for (auto [id, u] : m.world().view<const Engine::Core::UnitComponent>()) {
          if (u.owner_id != k_victumulae_garrison || u.health <= 0 ||
              !Game::Units::is_troop_spawn(u.spawn_type)) {
            continue;
          }
          ++troops;
          foot += is_garrison_foot(u) ? 1 : 0;
          if (auto const* target =
                  m.world().try_get<Engine::Core::AttackTargetComponent>(id);
              target != nullptr && target->target_id != 0) {
            ++fighting;
          }
        }
        QString production = QStringLiteral("-");
        for (auto [id, u, prod] :
             m.world()
                 .view<const Engine::Core::UnitComponent,
                       const Engine::Core::ProductionComponent>()) {
          if (u.owner_id == k_victumulae_garrison && u.health > 0 &&
              u.spawn_type == SpawnType::Barracks) {
            production = QStringLiteral("%1 %2 q%3 made%4 manpower%5/%6")
                             .arg(QString::fromStdString(std::string(
                                 Game::Units::spawn_type_name(u.spawn_type))))
                             .arg(prod.in_progress ? "busy" : "idle")
                             .arg(prod.production_queue.size())
                             .arg(prod.produced_count)
                             .arg(prod.manpower_available)
                             .arg(prod.manpower_ceiling);
          }
        }
        QString ai_line = QStringLiteral("-");
        if (auto* ai = m.world().get_system<Game::Systems::AISystem>()) {
          if (auto const* ctx = ai->plan_for(k_victumulae_garrison)) {
            ai_line = QStringLiteral("state%1 builders%2 headroom%3 recruitable%4")
                          .arg(static_cast<int>(ctx->state))
                          .arg(ctx->builder_count)
                          .arg(ctx->population_headroom())
                          .arg(ctx->recruitment_manpower_available);
          }
        }
        const auto& purse = m.session().economy();
        std::printf(
            "  [%5.0fs] garrison troops=%d foot=%d fighting=%d "
            "reversals/30s=%d archer_reversals/30s=%d flips/30s=%d recruited=%d | %s | "
            "%s | gold%d food%d wood%d iron%d\n",
            m.seconds(),
            troops,
            foot,
            fighting,
            watch->window_reversals,
            watch->window_archer_reversals,
            watch->window_order_flips,
            watch->recruited,
            production.toStdString().c_str(),
            ai_line.toStdString().c_str(),
            purse.get(k_victumulae_garrison, Game::Systems::ResourceType::Gold),
            purse.get(k_victumulae_garrison, Game::Systems::ResourceType::Food),
            purse.get(k_victumulae_garrison, Game::Systems::ResourceType::Wood),
            purse.get(k_victumulae_garrison, Game::Systems::ResourceType::Iron));
        watch->window_reversals = 0;
        watch->window_archer_reversals = 0;
        watch->window_order_flips = 0;
      }};
  step.every = 30.0F;
  return step;
}

auto victumulae_gate(Mission& m, float min_z) -> std::optional<EntityID> {
  std::optional<EntityID> best;
  float best_z = -1.0F;
  for (auto [id, u] : m.world().view<const Engine::Core::UnitComponent>()) {
    if (u.owner_id != k_victumulae_garrison || u.health <= 0 ||
        u.spawn_type != SpawnType::WallGate) {
      continue;
    }
    const float z = m.position(id).z;
    if (z >= min_z && z > best_z) {
      best_z = z;
      best = id;
    }
  }
  return best;
}

auto storming_party(Mission& m) -> Ids {
  return m.except_types(
      m.army(),
      {SpawnType::Ram, SpawnType::SiegeTower, SpawnType::Healer, SpawnType::Builder});
}

auto victumulae_assault() -> Plan {
  return {
      {"raise a ram",
       at(1.0F),
       [](Mission& m) {
         const Ids builders = m.local_of_type({SpawnType::Builder});
         m.build(Ids(builders.begin(),
                     builders.begin() + std::min<std::size_t>(2, builders.size())),
                 "ram",
                 {86.0F, 124.0F});
       }},
      {"raise a siege tower beside it",
       at(3.0F),
       [](Mission& m) {
         const Ids builders = m.local_of_type({SpawnType::Builder});
         if (builders.size() > 2) {
           m.build(Ids(builders.begin() + 2, builders.end()),
                   "siege_tower",
                   {72.0F, 124.0F});
         }
       }},
      {"roll the engines at the south ring",
       [](Mission& m) {
         return !m.local_of_type({SpawnType::Ram}).empty() &&
                !m.local_of_type({SpawnType::SiegeTower}).empty();
       },
       [](Mission& m) {
         if (auto gate = victumulae_gate(m, 70.0F)) {
           m.attack(m.local_of_type({SpawnType::Ram}), *gate);
         }
         m.move(m.local_of_type({SpawnType::SiegeTower}), {72.0F, 84.3F});
         m.attack_move(m.except_types(storming_party(m), {SpawnType::Archer}),
                       {80.0F, 92.0F});
         m.attack_move(m.local_of_type({SpawnType::Archer}), {80.0F, 98.0F});
       }},
      {"pour through the south ring",
       [](Mission& m) {
         return m.seconds() > 60.0F && !victumulae_gate(m, 70.0F).has_value();
       },
       [](Mission& m) { m.attack_move(storming_party(m), {80.0F, 66.0F}); },
       10.0F},
      {"break the citadel gate",
       [](Mission& m) {
         return m.seconds() > 60.0F && !victumulae_gate(m, 70.0F).has_value() &&
                victumulae_gate(m, 40.0F).has_value();
       },
       [](Mission& m) {
         if (auto gate = victumulae_gate(m, 40.0F)) {
           m.attack(m.local_of_type({SpawnType::Ram}), *gate);
         }
       },
       15.0F},
      {"storm the citadel yard",
       [](Mission& m) {
         return m.seconds() > 60.0F && !victumulae_gate(m, 40.0F).has_value();
       },
       [](Mission& m) { m.attack_move(storming_party(m), {80.0F, 38.0F}); },
       10.0F},
  };
}

} // namespace

TEST_F(CampaignPlaythroughTest,
       VictumulaeGarrisonDefendsWithoutDitheringAndKeepsRecruiting) {
  auto watch = std::make_shared<GarrisonWatch>();
  Plan plan = victumulae_assault();
  plan.push_back(watch_the_garrison(watch));
  plan.push_back(report_the_garrison(watch));
  const Report report = play("storming_of_victumulae", std::move(plan));
  std::printf("[playthrough] storming_of_victumulae garrison reversals=%d "
              "archer_reversals=%d order_flips=%d recruited=%d peak_troops=%d\n",
              watch->reversals,
              watch->archer_reversals,
              watch->order_flips,
              watch->recruited,
              watch->peak_troops);

  EXPECT_GE(report.stages_complete, 2) << report.line();
  EXPECT_GE(watch->recruited, 5)
      << "the garrison stood idle in its barracks while the town was stormed";
  EXPECT_LE(watch->order_flips, 15)
      << "companies were sent back and forth between two posts";
}
