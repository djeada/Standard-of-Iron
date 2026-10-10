// Scripted playthroughs of every mission of "The Barcid Road".
//
// Each test loads a campaign mission headless (see campaign_playthrough_harness.h),
// plays a short, readable plan through the UI's own order paths, and asserts that the
// mission is won within the cap. SOI_PLAYTHROUGH_NARRATE=1 prints the start-of-mission
// layout, each plan step, each stage change and a one-line status every game minute.

#include <algorithm>
#include <cstdlib>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "tests/core/campaign_playthrough_harness.h"

namespace {

using namespace Playthrough;

constexpr char k_campaign[] = "second_punic_war";
// Twenty-five game minutes; SOI_PLAYTHROUGH_CAP=<seconds> shortens it while
// calibrating.
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
    // SOI_PT_PLAN=default swaps the mission's plan for the generic one (calibration).
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

  // For missions no scripted plan wins yet: the new content must keep reaching the
  // progress it reached when this was measured (2026-10-10), so a regression trips.
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

// ---- shared plan pieces ----------------------------------------------------------

auto army_of(Mission& m) -> Ids {
  return m.army();
}

// The default campaign plan, "cut off the head":
//  - `garrison` companies stand on hold in the yard of the home camp (a camp is lost
//    to whoever outnumbers its yard three to one, so an empty yard falls to a raid);
//  - every other company answers any enemy inside `defend_radius` of home first;
//  - from `strike_after` on, the field army marches as one body on the next living
//  enemy
//    commander (a nation dies with its commander: its camp falls neutral, its troops
//    leave the field and its queued waves stand down);
//  - with no enemy commander left, it walks into every foreign camp still standing.
// Calibration knobs: SOI_PT_<NAME>=<value> overrides a plan default without a rebuild.
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
  // Companies this close to the quarry are ordered to attack him directly.
  float strike_radius = knob("STRIKE_RADIUS", 35.0F);
  // Seconds within strike radius of a living quarry before breaking his camp's gate.
  float breach_after = knob("BREACH_AFTER", 25.0F);
  // A company with another enemy this close fights it instead of reaching for him.
  float engaged_radius = knob("ENGAGED_RADIUS", 10.0F);
  // Commander owners in the order to hunt them; owners left out follow, nearest first.
  // SOI_PT_ORDER="3,2" overrides it while calibrating.
  std::vector<int> order;
  // Commanders who sit behind their camp walls: wait for them at `at`, out of tower
  // range, and strike once one comes within `pounce` of it.
  struct Ambush {
    G at;
    float pounce = 60.0F;
  };
  std::map<int, Ambush> ambush;
  // Before marching on a quarry further than this, the field army first assembles
  // `regroup_short` short of him (0 disables).
  float regroup_beyond = knob("REGROUP_BEYOND", 0.0F);
  float regroup_short = knob("REGROUP_SHORT", 60.0F);
  float every = 5.0F;
};

// Keep every camp recruiting, cycling through `mix`, as a player clicking the HUD's
// recruit buttons would; ProductionService refuses what the treasury cannot pay for.
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

// Micro a player does without thinking: a company hacking at a tower or a wall while
// enemy soldiers stand within reach turns on the soldiers.
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
        // Returns true when the plan switches to a new mode or a new quarry; the
        // whole field army is re-ordered then, otherwise only its idle companies.
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
            // Anything that walks into the ambush is fought where it stands.
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
          // A long march: assemble short of him first so the horse does not arrive
          // alone, then go in together.
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
          // Close enough to see him: order the attack on the man himself, or the
          // companies stand under the camp's towers trading blows with escorts.
          const Ids close = m.near(troops, at, spec.strike_radius);
          if (changed) {
            state->gate = 0;
          }
          if (changed || close.empty()) {
            state->close_since = -1.0F;
          } else if (state->close_since < 0.0F) {
            state->close_since = m.seconds();
          }
          // Still alive after a long look: he is behind his camp's wall. Break the
          // nearest gate, as a player would, and go in.
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
            // Only companies with no other enemy at their throat go for him; the
            // rest keep fighting whoever is fighting them.
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
          // Follow a quarry that moves: re-aim everyone not already fighting.
          const bool moved = Mission::distance(at, state->aim) > 12.0F;
          if (changed || moved) {
            state->aim = at;
          }
          const Ids far = m.all_but(troops, close);
          m.attack_move(changed || moved ? not_fighting(m, far) : m.idle(far), at);
          return;
        }
        // Camps worth taking: foreign, not the Sepulcher's (those stay warded),
        // nearest first.
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

// Ferry `who` over a raft: walk to `bank` (within reach of the raft's near landing),
// then join the raft's line, one call per company. `across` says who has landed.
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

// Hold `who` in the yard of the camp at `camp` until it is ours.
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

// The Rhone: the Volcae and Rome's hill fort hold the far bank at the bridges; the
// river town stands south of the eastern branch, where no bridge reaches. The army
// crosses at the bridges while Hanno's Iberians come down on the Volcae from upstream
// (the default hunt sends both at the consul), then a few companies and the elephants
// take the fallen hill fort while everyone else is rafted over the branch to finish
// Rome's second commander and walk into the river town.
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
                              // On the island the army waits until (nearly) everyone
                              // has landed, then goes in as one body.
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

// Keep the builders on auto-gather, each on whichever of wood, stone and iron is
// furthest behind the mission's quota.
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
          // Two on the scarcest store, then one each down the list.
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

// Ticino is a cavalry fight. The reserve's officer waits at the bridge below his camp
// at the start and shelters behind its wall and gate once threatened, so the horse
// rides for him first, at once; the foot hold the home camp's yard. Scipio, in the open
// by the forest sanctuary, is ridden down after, and the fallen camps are walked into.
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
      Progress{.survives_seconds = 400.0F});
}
