#include <atomic>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <thread>

#include "app/core/commander_overlay_pass.h"
#include "game/core/component.h"
#include "game/core/render_phase.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/units/spawn_type.h"
#include "render/scene/scene_renderer.h"

namespace {

namespace RenderPhase = Engine::Core::RenderPhase;

using Engine::Core::EntityID;
using Engine::Core::World;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

auto spawn_formation(World& world, float x) -> EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  entity->add_component<Engine::Core::RenderableComponent>("unit", "unit");
  entity->add_component<Engine::Core::CombatStateComponent>();
  entity->add_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  return entity->get_id();
}

void resize_formation(World& world, EntityID id, std::size_t soldiers) {
  auto* traversal =
      world.get_entity(id)
          ->get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  traversal->slot_states.clear();
  traversal->slot_states.shrink_to_fit();
  for (std::size_t index = 0; index < soldiers; ++index) {
    Engine::Core::UnitTraversalSlotState slot;
    slot.slot_index = static_cast<std::uint16_t>(index);
    slot.alive = true;
    traversal->slot_states.push_back(slot);
  }
  ++traversal->slot_states_revision;
}

auto locked_overlay(EntityID commander, EntityID target)
    -> std::optional<App::Core::CommanderOverlayFrame> {
  return App::Core::CommanderOverlayFrame{
      .commander_id = commander, .locked_target_id = target, .session = 1};
}

TEST(RenderThreadIsolation, RenderPhaseReportsAccessToTheLiveWorld) {
  SessionContext session;
  const ScopedSession scope(session);
  World world;
  EntityID const id = spawn_formation(world, 0.0F);

  auto const before = RenderPhase::violation_count();
  (void)world.get_entity(id);
  EXPECT_EQ(RenderPhase::violation_count(), before);

  const RenderPhase::Scope render_phase;
  (void)world.get_entity(id);
  EXPECT_EQ(RenderPhase::violation_count(), before + 1U);
}

TEST(RenderThreadIsolation, RenderPhaseMayTakeAndReadTheSnapshot) {
  SessionContext session;
  const ScopedSession scope(session);
  World world;
  EntityID const id = spawn_formation(world, 0.0F);
  auto const before = RenderPhase::violation_count();

  const RenderPhase::Scope render_phase;
  world.ensure_render_snapshot();
  auto const snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);
  EXPECT_NE(snapshot->get_entity(id), nullptr);
  EXPECT_EQ(RenderPhase::violation_count(), before);
}

TEST(RenderThreadIsolation, CommanderOverlayDrawsOnlyFromTheSnapshot) {
  SessionContext session;
  const ScopedSession scope(session);
  World world;
  EntityID const commander = spawn_formation(world, 0.0F);
  EntityID const target = spawn_formation(world, 2.0F);
  resize_formation(world, target, 24U);
  world.ensure_render_snapshot();
  auto const snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);

  Render::GL::Renderer renderer{Render::ShaderQuality::None};
  App::Core::CommanderOverlayPass pass;
  auto const before = RenderPhase::violation_count();

  const RenderPhase::Scope render_phase;
  pass.render(locked_overlay(commander, target), *snapshot, renderer, 0.0F);
  EXPECT_EQ(RenderPhase::violation_count(), before);

  pass.render(locked_overlay(commander, target), world, renderer, 0.0F);
  EXPECT_EQ(RenderPhase::violation_count(), before)
      << "the pass must refuse a live world without touching it";
}

TEST(RenderThreadIsolation, OverlaySurvivesFormationsResizingUnderneathIt) {
  SessionContext session;
  const ScopedSession scope(session);
  World world;
  EntityID const commander = spawn_formation(world, 0.0F);
  EntityID const target = spawn_formation(world, 2.0F);
  resize_formation(world, target, 24U);
  world.ensure_render_snapshot();

  auto const before = RenderPhase::violation_count();
  std::atomic<bool> done{false};
  std::thread simulation([&]() {
    const ScopedSession simulation_scope(session);
    for (std::size_t tick = 0; tick < 600U; ++tick) {
      world.service_stopped_frame(
          [&]() { resize_formation(world, target, 1U + (tick * 7U) % 96U); });
    }
    done.store(true);
  });

  Render::GL::Renderer renderer{Render::ShaderQuality::None};
  App::Core::CommanderOverlayPass pass;
  std::size_t frames = 0;
  while (!done.load() || frames == 0U) {
    const RenderPhase::Scope render_phase;
    auto const snapshot = world.acquire_render_snapshot();
    ASSERT_NE(snapshot, nullptr);
    pass.render(locked_overlay(commander, target), *snapshot, renderer, 0.0F);
    ++frames;
  }
  simulation.join();

  EXPECT_EQ(RenderPhase::violation_count(), before);
}

} // namespace
