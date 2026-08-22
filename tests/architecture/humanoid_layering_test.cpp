

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

auto find_repo_root() -> fs::path {
  fs::path current = fs::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(current / "CMakeLists.txt") && fs::exists(current / "render") &&
        fs::exists(current / "game")) {
      return current;
    }
    if (!current.has_parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return fs::current_path();
}

auto read_text(const fs::path& path) -> std::string {
  std::ifstream stream(path);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

auto sources_under(const fs::path& directory) -> std::vector<fs::path> {
  std::vector<fs::path> files;
  if (!fs::exists(directory)) {
    return files;
  }
  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto extension = entry.path().extension().string();
    if (extension == ".h" || extension == ".cpp") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

auto contains(const std::string& haystack, const std::string& needle) -> bool {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST(HumanoidLayering, SchemaStaysDependencyLight) {
  const auto root = find_repo_root();
  const auto schema = root / "render" / "humanoid" / "schema";
  ASSERT_TRUE(fs::exists(schema)) << schema.string();

  static const std::vector<std::string> forbidden{
      "game/core/entity.h",
      "game/core/world.h",
      "game/core/component.h",
      "render/scene_renderer.h",
      "render/gl/backend.h",
      "render/entity/registry.h",
      "render/submitter.h",
  };

  for (const auto& file : sources_under(schema)) {
    const auto source = read_text(file);
    for (const auto& include : forbidden) {
      EXPECT_FALSE(contains(source, "#include \"" + include + "\""))
          << file.string() << " pulls " << include
          << " into the shared humanoid schema";
    }
  }
}

TEST(HumanoidLayering, RuntimeDoesNotDependOnBaking) {
  const auto root = find_repo_root();
  const auto humanoid = root / "render" / "humanoid";
  ASSERT_TRUE(fs::exists(humanoid)) << humanoid.string();

  for (const auto& file : sources_under(humanoid / "runtime")) {
    const auto source = read_text(file);
    EXPECT_FALSE(contains(source, "_bake.h\""))
        << file.string() << " includes a bake header from the humanoid runtime";
  }

  for (const auto& file : sources_under(humanoid / "asset")) {
    const auto source = read_text(file);
    EXPECT_FALSE(contains(source, "#include \"render/scene_renderer.h\""))
        << file.string() << " pulls the scene renderer into humanoid asset code";
  }
}

TEST(HumanoidLayering, ThereIsNoUmbrellaInternalHeader) {
  const auto root = find_repo_root();
  EXPECT_FALSE(fs::exists(root / "render" / "humanoid" / "prepare_internal.h"));

  for (const auto& file : sources_under(root / "render" / "humanoid")) {
    const auto source = read_text(file);
    EXPECT_FALSE(contains(source, "prepare_internal.h"))
        << file.string() << " still includes the retired umbrella header";
  }
}

TEST(HumanoidLayering, CreaturePipelineKnowsNothingAboutHumanoidBones) {
  const auto root = find_repo_root();
  const auto pipeline =
      root / "render" / "creature" / "pipeline" / "creature_pipeline.cpp";
  const auto source = read_text(pipeline);
  ASSERT_FALSE(source.empty()) << pipeline.string();

  EXPECT_FALSE(contains(source, "Render::Humanoid::"))
      << "the generic creature pipeline names a humanoid type";
  EXPECT_FALSE(contains(source, "HumanoidBone"))
      << "the generic creature pipeline reads humanoid bone semantics";
  EXPECT_FALSE(contains(source, "humanoid/schema/"))
      << "the generic creature pipeline includes the humanoid schema";
  EXPECT_FALSE(contains(source, "humanoid/asset/"))
      << "the generic creature pipeline includes humanoid asset code";

  EXPECT_TRUE(contains(source, "render/humanoid/runtime/frame_control.h"))
      << "if this include is gone, drop the exception rather than widening it";
}

TEST(HumanoidLayering, RendererOwnsNoMutableRuntimeState) {
  const auto root = find_repo_root();
  const auto header = root / "render" / "humanoid" / "runtime" / "humanoid_renderer.h";
  const auto source = read_text(header);
  ASSERT_FALSE(source.empty()) << header.string();

  EXPECT_FALSE(contains(source, "mutable "))
      << "HumanoidRendererBase declares mutable state";
  for (const auto& banned : {"m_visual_spec_baked",
                             "m_visual_spec_cache",
                             "m_proportion_scale_cached",
                             "m_cached_proportion_scale"}) {
    EXPECT_FALSE(contains(source, banned))
        << "HumanoidRendererBase reintroduced " << banned;
  }
}

TEST(CreatureBakeBoundary, NoRendererLazilyBakesItsVisualSpec) {
  const auto root = find_repo_root();
  for (const auto& directory : {"render/horse",
                                "render/elephant",
                                "render/humanoid",
                                "render/entity",
                                "render/wildlife"}) {
    for (const auto& file : sources_under(root / directory)) {
      const auto source = read_text(file);
      for (const auto& banned : {"m_visual_spec_baked", "m_visual_spec_cache"}) {
        EXPECT_FALSE(contains(source, banned))
            << file.string() << " reintroduced " << banned;
      }
    }
  }
}

TEST(CreatureBakeBoundary, NoNamespaceScopeSpeciesStatistics) {
  const auto root = find_repo_root();
  for (const auto& directory : {"render/horse", "render/elephant"}) {
    for (const auto& file : sources_under(root / directory)) {
      const auto source = read_text(file);
      EXPECT_FALSE(contains(source, "static HorseRenderStats"))
          << file.string() << " reintroduced namespace-scope horse statistics";
      EXPECT_FALSE(contains(source, "static ElephantRenderStats"))
          << file.string() << " reintroduced namespace-scope elephant statistics";
    }
  }
}

TEST(HumanoidLayering, NoProcessGlobalFrameOrStatistics) {
  const auto root = find_repo_root();
  for (const auto& file : sources_under(root / "render" / "humanoid")) {
    const auto source = read_text(file);
    EXPECT_FALSE(contains(source, "uint32_t s_current_frame"))
        << file.string() << " reintroduced a namespace-scope frame counter";
    EXPECT_FALSE(contains(source, "HumanoidRenderStats s_render_stats"))
        << file.string() << " reintroduced namespace-scope render statistics";
  }
}

TEST(CreatureBakeBoundary, RuntimeCodeDoesNotIncludeBakeRecipes) {
  const auto root = find_repo_root();

  static const std::vector<std::string> runtime_roots{"render/horse",
                                                      "render/elephant",
                                                      "render/wildlife",
                                                      "render/creature",
                                                      "render/humanoid"};

  for (const auto& relative : runtime_roots) {
    for (const auto& file : sources_under(root / relative)) {
      const auto name = file.filename().string();
      if (name.find("bake_recipe") != std::string::npos) {
        continue;
      }

      if (name == "sheep_manifest.cpp" || name == "sheep_manifest.h" ||
          name == "wolf_manifest.cpp" || name == "wolf_manifest.h" ||
          name == "humanoid_manifest.cpp" || name == "humanoid_manifest.h") {
        continue;
      }
      const auto source = read_text(file);
      EXPECT_FALSE(contains(source, "bake_recipe.h\""))
          << file.string() << " includes a bake recipe from runtime code";
      EXPECT_FALSE(contains(source, "creature_bake_recipe.h\""))
          << file.string() << " includes the bake recipe schema from runtime code";
    }
  }
}

TEST(CreatureBakeBoundary, RuntimeManifestCarriesNoBakeFields) {
  const auto root = find_repo_root();
  const auto header =
      root / "render" / "creature" / "schema" / "creature_runtime_manifest.h";
  const auto source = read_text(header);
  ASSERT_FALSE(source.empty()) << header.string();

  for (const auto& banned : {"BakeClipDescriptor",
                             "BakeSocketDescriptor",
                             "bake_clip_frame",
                             "clip_markers"}) {
    EXPECT_FALSE(contains(source, banned))
        << "CreatureRuntimeManifest regained the bake-only field " << banned;
  }
}
