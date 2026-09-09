#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <sstream>
#include <string>

#include "game/map/map_definition.h"
#include "render/gl/backend/cursed_gold_vein_mesh.h"
#include "render/gl/backend/iron_ore_mesh.h"

namespace {

using Render::GL::BackendPipelines::build_cursed_gold_vein_mesh;
using Render::GL::BackendPipelines::build_iron_ore_mesh;
using Render::GL::BackendPipelines::PropMeshData;

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::path(__FILE__).parent_path();
  while (!path.empty()) {
    if (std::filesystem::exists(path / "CMakeLists.txt") &&
        std::filesystem::exists(path / "assets" / "shaders" /
                                "cursed_gold_vein_instanced.frag")) {
      return path;
    }
    if (!path.has_parent_path() || path.parent_path() == path) {
      break;
    }
    path = path.parent_path();
  }
  return {};
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

auto top_of(const PropMeshData& mesh) -> float {
  float top = 0.0F;
  for (auto const& vertex : mesh.vertices) {
    top = std::max(top, vertex.first.y());
  }
  return top;
}

} // namespace

TEST(RockPropMeshTest, TheVeinShaderAndMeshAgreeOnTheRockCrown) {
  auto const root = find_repo_root();
  ASSERT_FALSE(root.empty());
  auto const source =
      read_text(root / "assets" / "shaders" / "cursed_gold_vein_instanced.frag");
  ASSERT_FALSE(source.empty());

  std::smatch match;
  std::regex const pattern(R"(const\s+float\s+k_rock_crown\s*=\s*([0-9.]+)\s*;)");
  ASSERT_TRUE(std::regex_search(source, match, pattern))
      << "cursed_gold_vein_instanced.frag no longer declares k_rock_crown";

  auto const text = match[1].str();
  float crown = 0.0F;
  auto const parsed = std::from_chars(text.data(), text.data() + text.size(), crown);
  ASSERT_EQ(parsed.ec, std::errc{}) << "could not read k_rock_crown from the shader";

  EXPECT_NEAR(
      crown, Render::GL::BackendPipelines::k_cursed_gold_vein_rock_crown, 1.0e-4F)
      << "the shader paints rock and crystal around a different height than the "
         "mesh is built around; rock will turn gold or a shard will read as stone";
}

TEST(RockPropMeshTest, EveryRockPropFillsTheHeightItDeclares) {
  struct Case {
    const char* name;
    Game::Map::WorldProp::Type type;
    PropMeshData mesh;
  };

  std::array cases{
      Case{"cursed_gold_vein",
           Game::Map::WorldProp::Type::CursedGoldVein,
           build_cursed_gold_vein_mesh()},
      Case{"iron_ore", Game::Map::WorldProp::Type::IronOre, build_iron_ore_mesh()},
  };

  for (auto const& entry : cases) {
    float const declared = Game::Map::world_prop_model_height(entry.type);
    float const drawn = top_of(entry.mesh);
    EXPECT_LE(drawn, declared + 1.0e-3F)
        << entry.name << " draws to " << drawn << " but declares " << declared;
    EXPECT_GE(drawn, declared * 0.9F)
        << entry.name << " declares " << declared << " but only draws to " << drawn;
  }
}
