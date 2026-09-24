#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/creature/anatomy_bake.h"
#include "render/elephant/dimensions.h"
#include "render/entity/nations/carthage/elephant_renderer.h"
#include "render/math/creature_math_utils.h"

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "render" / "scene_walk.cpp");
  };

  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_repo_markers(path)) {
        return path;
      }
      const auto parent = path.parent_path();
      if (parent == path) {
        break;
      }
      path = parent;
    }
    return {};
  };

  if (const auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
      !from_file.empty()) {
    return from_file;
  }
  return walk_up(std::filesystem::current_path());
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

} // namespace

TEST(SceneWalkEntityHandleTest, WorldWalkIteratesFullWidthEntityHandles) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const auto source = read_text(root / "render" / "scene_walk.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_EQ(source.find("for (std::uint32_t const entity_id :"), std::string::npos)
      << "scene_walk.cpp truncates an EntityID to 32 bits while walking the world";
  EXPECT_EQ(source.find("[&](std::uint32_t entity_id,"), std::string::npos)
      << "scene_walk.cpp truncates an EntityID to 32 bits while collecting entries";
  EXPECT_EQ(source.find("std::span<const std::uint32_t>"), std::string::npos)
      << "scene_walk.cpp walks a 32-bit id span";

  EXPECT_NE(source.find("for (Engine::Core::EntityID const entity_id : entity_ids)"),
            std::string::npos)
      << "expected the collectors to walk full-width handles";

  for (const char* collector : {"collect_unit_entries", "collect_non_unit_entries"}) {
    const auto at = source.find(std::string("void Renderer::") + collector + "(");
    ASSERT_NE(at, std::string::npos) << collector << " is missing from scene_walk.cpp";
    const auto body = source.find('{', at);
    ASSERT_NE(body, std::string::npos) << collector;
    EXPECT_NE(source.find("std::span<const Engine::Core::EntityID> entity_ids", at),
              std::string::npos)
        << collector << " does not take a full-width handle span";
    EXPECT_LT(source.find("std::span<const Engine::Core::EntityID> entity_ids", at),
              body)
        << collector << " does not take a full-width handle span";
  }
}

TEST(SceneWalkEntityHandleTest, RenderEntriesCarryFullWidthEntityHandles) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const auto source = read_text(root / "render" / "scene_walk.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_EQ(source.find("uint32_t entity_id{0};"), std::string::npos)
      << "a render entry stores a truncated entity handle";

  for (const char* entry : {"struct UnitRenderEntry", "struct RenderEntry"}) {
    const auto at = source.find(entry);
    ASSERT_NE(at, std::string::npos) << entry << " is missing from scene_walk.cpp";
    const auto end = source.find("\n};", at);
    ASSERT_NE(end, std::string::npos) << entry;
    const auto field = source.find("Engine::Core::EntityID entity_id{0};", at);
    EXPECT_NE(field, std::string::npos) << entry << " has no full-width handle field";
    EXPECT_LT(field, end) << entry << " has no full-width handle field of its own";
  }

  const auto optimizer = read_text(root / "render" / "battle_render_optimizer.h");
  ASSERT_FALSE(optimizer.empty());
  EXPECT_EQ(optimizer.find("should_update_animation(std::uint32_t entity_id"),
            std::string::npos)
      << "the animation budget keys off a truncated entity handle";
}

TEST(SceneWalkEntityHandleTest, MissingBodyWarningsAreOwnedByTheRendererNotTheProcess) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const auto source = read_text(root / "render" / "scene_walk.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_EQ(source.find("static std::unordered_set<std::string> warned_units"),
            std::string::npos)
      << "render warnings are suppressed by a process-lifetime static";
  EXPECT_NE(source.find("note_missing_body_warning"), std::string::npos)
      << "expected the renderer to own its warning suppression";

  const auto header = read_text(root / "render" / "scene_renderer.h");
  ASSERT_FALSE(header.empty());
  EXPECT_NE(header.find("clear_missing_body_warnings"), std::string::npos);
  const auto clear_at = header.find("void clear_entity_render_caches()");
  ASSERT_NE(clear_at, std::string::npos);
  const auto clear_end = header.find('}', clear_at);
  ASSERT_NE(clear_end, std::string::npos);
  EXPECT_LT(header.find("clear_missing_body_warnings();", clear_at), clear_end)
      << "a map change must drop the warnings the last match accumulated";
}

TEST(SceneWalkEntityHandleTest, CreatureSeedsDeriveFromTheUnitNotItsAddress) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());

  for (const char* relative :
       {"render/horse/prepare.cpp",
        "render/entity/mounted_humanoid_renderer_base.cpp",
        "render/equipment/weapons/quiver_renderer.cpp",
        "render/entity/nations/carthage/elephant_renderer.cpp"}) {
    const auto source = read_text(root / relative);
    ASSERT_FALSE(source.empty()) << relative;
    for (const char* spelling : {"reinterpret_cast<uintptr_t>(ctx.entity)",
                                 "reinterpret_cast<std::uintptr_t>(ctx.entity)",
                                 "reinterpret_cast<uintptr_t>(p.entity)",
                                 "reinterpret_cast<std::uintptr_t>(p.entity)"}) {
      EXPECT_EQ(source.find(spelling), std::string::npos)
          << relative << " seeds a creature from its entity address";
    }
  }
}

TEST(SceneWalkEntityHandleTest, ElephantAnatomySeedIsEntityIdStable) {
  Engine::Core::World first;
  Engine::Core::World second;
  auto* in_first = first.create_entity_with_id(77U);
  auto* in_second = second.create_entity_with_id(77U);
  ASSERT_NE(in_first, nullptr);
  ASSERT_NE(in_second, nullptr);
  ASSERT_NE(in_first, in_second);

  const auto seed = Render::GL::Carthage::elephant_anatomy_seed(*in_first);
  EXPECT_EQ(seed, Render::GL::Carthage::elephant_anatomy_seed(*in_second))
      << "the same unit in two snapshot buffers must seed the same elephant";

  const QVector3D fabric(0.45F, 0.18F, 0.55F);
  const QVector3D metal(0.70F, 0.50F, 0.28F);
  const auto& a =
      Render::Creature::get_or_bake_elephant_anatomy(in_first, seed, fabric, metal);
  const auto& b = Render::Creature::get_or_bake_elephant_anatomy(
      in_second,
      Render::GL::Carthage::elephant_anatomy_seed(*in_second),
      fabric,
      metal);
  EXPECT_EQ(a.seed, b.seed);
  EXPECT_FLOAT_EQ(a.profile.dims.body_length, b.profile.dims.body_length);
  EXPECT_FLOAT_EQ(a.profile.dims.trunk_length, b.profile.dims.trunk_length);
  EXPECT_FLOAT_EQ(a.profile.dims.ear_width, b.profile.dims.ear_width);

  auto* other = second.create_entity_with_id(78U);
  ASSERT_NE(other, nullptr);
  EXPECT_NE(seed, Render::GL::Carthage::elephant_anatomy_seed(*other));
}

TEST(SceneWalkEntityHandleTest, StableEntitySeedIsAFunctionOfTheHandleAlone) {
  EXPECT_EQ(Render::Creature::stable_entity_seed(42U),
            Render::Creature::stable_entity_seed(42U));
  EXPECT_NE(Render::Creature::stable_entity_seed(42U),
            Render::Creature::stable_entity_seed(43U));

  EXPECT_NE(Render::Creature::stable_entity_seed(1U),
            Render::Creature::stable_entity_seed((1ULL << 32U) | 1ULL));
}

TEST(SceneWalkEntityHandleTest, AFrozenRegistryAcceptsReadsOfExistingComponents) {
  Engine::Core::World world;
  auto* entity = world.create_entity();
  ASSERT_NE(entity, nullptr);
  Engine::Core::get_or_add_component<Engine::Core::TransformComponent>(entity);

  {
    const Engine::Core::Registry::StructureFreeze freeze(world.registry());
    EXPECT_TRUE(world.registry().structure_frozen());
    EXPECT_NE(
        Engine::Core::get_or_add_component<Engine::Core::TransformComponent>(entity),
        nullptr);
  }
  EXPECT_FALSE(world.registry().structure_frozen());
  EXPECT_EQ(world.registry().structure_violations(), 0U);
}

#ifdef NDEBUG
TEST(SceneWalkEntityHandleTest, AddingAComponentDuringParallelPrepareIsCounted) {
  Engine::Core::World world;
  auto* entity = world.create_entity();
  ASSERT_NE(entity, nullptr);
  {
    const Engine::Core::Registry::StructureFreeze freeze(world.registry());
    Engine::Core::get_or_add_component<Engine::Core::TransformComponent>(entity);
  }
  EXPECT_GT(world.registry().structure_violations(), 0U)
      << "a preparer inserted a component while workers were reading the registry";
}
#else
TEST(SceneWalkEntityHandleTest, AddingAComponentDuringParallelPrepareAsserts) {
  EXPECT_DEATH(
      {
        Engine::Core::World world;
        auto* entity = world.create_entity();
        const Engine::Core::Registry::StructureFreeze freeze(world.registry());
        Engine::Core::get_or_add_component<Engine::Core::TransformComponent>(entity);
      },
      "frozen");
}
#endif
