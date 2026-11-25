#include "skirmish_camp_start.h"

#include <QDebug>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_core.h"
#include "../core/component_structures.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/economy/build_site.h"
#include "../systems/economy/recruitment_gate.h"
#include "../systems/movement/body_profile.h"
#include "../systems/nation_registry.h"
#include "../systems/navigation/walkability.h"
#include "../systems/owner_registry.h"
#include "../systems/resource_stockpile.h"
#include "../systems/site_keep_out.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "../units/unit.h"
#include "map_transformer.h"

namespace Game::Map {

namespace {

using Engine::Core::EntityID;

// Homes stand behind the barracks, the farm off to one side, and the gate side
// stays open for the muster and the builders' haul to the yard.
constexpr float k_home_back = 11.0F;
constexpr float k_home_side = 7.5F;
constexpr float k_farm_back = 6.0F;
constexpr float k_farm_side = 19.0F;
constexpr float k_site_search = 18.0F;
constexpr float k_gate_keep_out = 7.0F;
constexpr float k_yard_keep_out = 4.5F;
constexpr float k_troop_search = 12.0F;
constexpr float k_camp_reach = 42.0F;
constexpr float k_lattice_step = 2.0F;

struct Seat {
  int owner_id = 0;
  Engine::Core::Entity* barracks = nullptr;
  int builder_crews = 0;
};

auto seats_with_a_barracks(Engine::Core::World& world) -> std::vector<Seat> {
  const auto& owners = *Game::Session::services_for(world).owners;
  std::map<int, Seat> by_owner;
  std::map<int, int> crews_by_owner;
  // One pass over the units: each seat's lowest-id barracks and its builder
  // crews, so placing a seat's camp never rescans the world.
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(entity->get_id());
    if (unit != nullptr && unit->health > 0 &&
        unit->spawn_type == Game::Units::SpawnType::Builder) {
      ++crews_by_owner[unit->owner_id];
      continue;
    }
    if (unit == nullptr || unit->health <= 0 ||
        unit->spawn_type != Game::Units::SpawnType::Barracks ||
        Game::Core::is_neutral_owner(unit->owner_id) ||
        !(owners.is_player(unit->owner_id) || owners.is_ai(unit->owner_id)) ||
        !world.has<Engine::Core::ProductionComponent>(entity->get_id())) {
      continue;
    }
    auto& seat = by_owner[unit->owner_id];
    if (seat.barracks == nullptr || entity->get_id() < seat.barracks->get_id()) {
      seat.owner_id = unit->owner_id;
      seat.barracks = entity;
    }
  }
  std::vector<Seat> seats;
  seats.reserve(by_owner.size());
  for (auto& [owner, seat] : by_owner) {
    seat.builder_crews = crews_by_owner[owner];
    seats.push_back(seat);
  }
  return seats;
}

auto missile_squad_for(const Game::Systems::Nation* nation) -> Game::Units::SpawnType {
  if (nation != nullptr) {
    if (nation->get_troop(Game::Units::TroopType::Velites) != nullptr) {
      return Game::Units::SpawnType::Velites;
    }
    if (nation->get_troop(Game::Units::TroopType::Slinger) != nullptr) {
      return Game::Units::SpawnType::Slinger;
    }
  }
  return Game::Units::SpawnType::Archer;
}

struct Frame {
  QVector3D centre;
  QVector3D outward;
  QVector3D across;
};

auto frame_of(const Engine::Core::TransformComponent& transform) -> Frame {
  const auto gate = Game::Systems::barracks_gate(transform);
  const QVector3D centre(transform.position.x, 0.0F, transform.position.z);
  const QVector3D across(gate.outward.z(), 0.0F, -gate.outward.x());
  return {.centre = centre, .outward = gate.outward, .across = across};
}

auto spawn(Engine::Core::World& world,
           Game::Units::SpawnType type,
           int owner_id,
           bool ai_controlled,
           Game::Systems::NationID nation,
           const QVector3D& position,
           float rotation_y) -> EntityID {
  auto registry = MapTransformer::get_factory_registry();
  if (!registry) {
    return 0;
  }
  Game::Units::SpawnParams params;
  params.position = position;
  params.rotation_y = rotation_y;
  params.player_id = owner_id;
  params.spawn_type = type;
  params.ai_controlled = ai_controlled;
  params.nation_id = nation;
  params.is_initial_spawn = true;
  auto unit = registry->create(type, world, params);
  return unit ? unit->id() : 0;
}

// The nearest clear ground to `wanted`: first the builders' own ring search
// around it, then a 2 m lattice over the camp (the reach the farmland test
// guarantees room in), nearest to `wanted` first.
auto camp_site(const Engine::Core::World& world,
               const char* type_name,
               const QVector3D& wanted,
               const QVector3D& centre,
               const QVector3D& gate,
               float rotation_y,
               const std::vector<Game::Systems::SiteKeepOut>& keep_out)
    -> std::optional<QVector3D> {
  // The camp's buildings stand where its crews can walk from the gate: a
  // terrace above the barracks is level ground nobody can reach.
  const Game::Systems::SiteAcceptor reachable = [&gate](float x, float z) {
    return Game::Systems::site_access_on_foot(gate.x(), gate.z(), x, z) !=
           Game::Systems::SiteAccess::CutOff;
  };
  if (auto site = Game::Systems::find_clear_site(world,
                                                 type_name,
                                                 wanted,
                                                 k_site_search,
                                                 rotation_y,
                                                 {},
                                                 keep_out,
                                                 true,
                                                 reachable)) {
    return site;
  }
  const int steps = static_cast<int>(k_camp_reach / k_lattice_step);
  std::vector<std::pair<float, QVector3D>> candidates;
  for (int ix = -steps; ix <= steps; ++ix) {
    for (int iz = -steps; iz <= steps; ++iz) {
      const QVector3D site(centre.x() + static_cast<float>(ix) * k_lattice_step,
                           0.0F,
                           centre.z() + static_cast<float>(iz) * k_lattice_step);
      if ((site - centre).lengthSquared() > k_camp_reach * k_camp_reach) {
        continue;
      }
      candidates.emplace_back((site - wanted).lengthSquared(), site);
    }
  }
  std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  for (const auto& [distance, site] : candidates) {
    (void)distance;
    if (auto found = Game::Systems::find_clear_site(
            world, type_name, site, 0.0F, rotation_y, {}, keep_out, true, reachable)) {
      return found;
    }
  }
  return std::nullopt;
}

auto raise_building(Engine::Core::World& world,
                    const Seat& seat,
                    bool ai_controlled,
                    Game::Systems::NationID nation,
                    Game::Units::SpawnType type,
                    const char* type_name,
                    const QVector3D& wanted,
                    const QVector3D& centre,
                    const QVector3D& gate,
                    float rotation_y,
                    const std::vector<Game::Systems::SiteKeepOut>& keep_out)
    -> EntityID {
  const auto site =
      camp_site(world, type_name, wanted, centre, gate, rotation_y, keep_out);
  if (!site.has_value()) {
    qInfo() << "Camp start: no clear ground for a" << type_name << "of owner"
            << seat.owner_id;
    return 0;
  }
  return spawn(world, type, seat.owner_id, ai_controlled, nation, *site, rotation_y);
}

auto place_troop(Engine::Core::World& world,
                 const Seat& seat,
                 bool ai_controlled,
                 Game::Systems::NationID nation,
                 Game::Units::SpawnType type,
                 const QVector3D& wanted,
                 float facing) -> EntityID {
  const auto standing = Game::Systems::Walkability::nearest_standable(
      wanted, Game::Systems::BodyProfile{}, k_troop_search);
  if (!standing.has_value()) {
    qInfo() << "Camp start: no standing room for a"
            << Game::Units::spawn_typeToQString(type) << "of owner" << seat.owner_id;
    return 0;
  }
  return spawn(world, type, seat.owner_id, ai_controlled, nation, *standing, facing);
}

} // namespace

auto apply_skirmish_camp_start(Engine::Core::World& world,
                               const CampStart& camp) -> std::vector<CampStartReport> {
  std::vector<CampStartReport> reports;
  const auto& services = Game::Session::services_for(world);
  if (services.owners == nullptr || services.nations == nullptr) {
    return reports;
  }
  const auto& owners = *services.owners;
  const auto& nations = *services.nations;

  for (const Seat& seat : seats_with_a_barracks(world)) {
    const auto* transform =
        world.try_get<Engine::Core::TransformComponent>(seat.barracks->get_id());
    auto* production =
        world.try_get<Engine::Core::ProductionComponent>(seat.barracks->get_id());
    if (transform == nullptr || production == nullptr) {
      continue;
    }
    CampStartReport report;
    report.owner_id = seat.owner_id;
    report.barracks = seat.barracks->get_id();

    const bool ai_controlled = owners.is_ai(seat.owner_id);
    const auto* nation = nations.get_nation_for_player(seat.owner_id);
    const auto nation_id = nation != nullptr ? nation->id : nations.default_nation_id();

    production->manpower_ceiling =
        std::max(production->manpower_limit(), camp.barracks_reserve);
    production->manpower_available =
        std::max(production->manpower_available, camp.barracks_reserve);

    const Frame frame = frame_of(*transform);
    const float yaw = transform->rotation.y;
    // Buildings that face a quarter turn keep their nav footprint tight.
    const float quarter_turn_yaw = std::round(yaw / 90.0F) * 90.0F;
    const auto gate = Game::Systems::barracks_gate(*transform);
    const auto yard = Game::Systems::stockpile_drop_point(
        transform->position.x, transform->position.z, yaw);
    const std::vector<Game::Systems::SiteKeepOut> keep_out = {
        {.x = gate.position.x() + gate.outward.x() * 3.0F,
         .z = gate.position.z() + gate.outward.z() * 3.0F,
         .radius = k_gate_keep_out},
        {.x = yard.x, .z = yard.z, .radius = k_yard_keep_out},
    };

    const std::array<float, 2> home_sides = {-1.0F, 1.0F};
    for (int i = 0; i < camp.homes; ++i) {
      const float side = home_sides[static_cast<std::size_t>(i) % home_sides.size()];
      const float row = static_cast<float>(i / 2);
      const QVector3D wanted = frame.centre -
                               frame.outward * (k_home_back + row * 7.0F) +
                               frame.across * (side * k_home_side);
      if (const auto id = raise_building(world,
                                         seat,
                                         ai_controlled,
                                         nation_id,
                                         Game::Units::SpawnType::Home,
                                         "home",
                                         wanted,
                                         frame.centre,
                                         gate.position,
                                         quarter_turn_yaw,
                                         keep_out);
          id != 0) {
        report.homes.push_back(id);
      }
    }
    for (int i = 0; i < camp.farms; ++i) {
      const float side = (i % 2 == 0) ? 1.0F : -1.0F;
      const QVector3D wanted = frame.centre - frame.outward * k_farm_back +
                               frame.across * (side * k_farm_side);
      if (const auto id = raise_building(world,
                                         seat,
                                         ai_controlled,
                                         nation_id,
                                         Game::Units::SpawnType::Farm,
                                         "farm",
                                         wanted,
                                         frame.centre,
                                         gate.position,
                                         quarter_turn_yaw,
                                         keep_out);
          id != 0) {
        report.farms.push_back(id);
      }
    }

    const QVector3D muster = gate.position + gate.outward * 5.0F;
    const int missing_crews = camp.builder_crews - seat.builder_crews;
    for (int i = 0; i < missing_crews; ++i) {
      const float side = (i % 2 == 0) ? -1.0F : 1.0F;
      const QVector3D wanted = muster + frame.across * (side * (5.0F + 2.5F * i));
      if (const auto id = place_troop(world,
                                      seat,
                                      ai_controlled,
                                      nation_id,
                                      Game::Units::SpawnType::Builder,
                                      wanted,
                                      yaw);
          id != 0) {
        report.builders.push_back(id);
      }
    }

    const std::array<Game::Units::SpawnType, 2> squads = {
        Game::Units::SpawnType::Swordsman, missile_squad_for(nation)};
    for (std::size_t i = 0; i < squads.size(); ++i) {
      const float side = i == 0 ? -1.0F : 1.0F;
      const QVector3D wanted =
          muster + frame.outward * 4.0F + frame.across * (side * 3.5F);
      if (const auto id = place_troop(
              world, seat, ai_controlled, nation_id, squads[i], wanted, yaw);
          id != 0) {
        report.squads.push_back(id);
      }
    }

    qInfo() << "Camp start: owner" << seat.owner_id << "homes" << report.homes.size()
            << "farms" << report.farms.size() << "new crews" << report.builders.size()
            << "squads" << report.squads.size() << "reserve"
            << production->manpower_available;
    reports.push_back(std::move(report));
  }
  return reports;
}

} // namespace Game::Map
