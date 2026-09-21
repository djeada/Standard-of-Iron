#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

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

  for (const char* relative : {"render/horse/prepare.cpp",
                               "render/entity/mounted_humanoid_renderer_base.cpp",
                               "render/equipment/weapons/quiver_renderer.cpp"}) {
    const auto source = read_text(root / relative);
    ASSERT_FALSE(source.empty()) << relative;
    EXPECT_EQ(source.find("reinterpret_cast<uintptr_t>(ctx.entity)"), std::string::npos)
        << relative << " seeds a creature from its entity address";
    EXPECT_EQ(source.find("reinterpret_cast<std::uintptr_t>(ctx.entity)"),
              std::string::npos)
        << relative << " seeds a creature from its entity address";
  }
}

TEST(SceneWalkEntityHandleTest, StableEntitySeedIsAFunctionOfTheHandleAlone) {
  EXPECT_EQ(Render::Creature::stable_entity_seed(42U),
            Render::Creature::stable_entity_seed(42U));
  EXPECT_NE(Render::Creature::stable_entity_seed(42U),
            Render::Creature::stable_entity_seed(43U));

  EXPECT_NE(Render::Creature::stable_entity_seed(1U),
            Render::Creature::stable_entity_seed((1ULL << 32U) | 1ULL));
}
