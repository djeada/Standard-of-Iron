

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
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

auto defines_them(const fs::path& file) -> bool {
  const auto name = file.filename().string();
  return name == "horse_spec.cpp" || name == "horse_spec.h" ||
         name == "elephant_spec.cpp" || name == "elephant_spec.h";
}

} // namespace

TEST(CreatureProceduralPose, HasNoProductionCallers) {
  const auto root = find_repo_root();

  static const std::vector<std::string> procedural{
      "evaluate_horse_skeleton",
      "make_horse_spec_pose",
      "compute_horse_bone_palette",
      "evaluate_elephant_skeleton",
      "make_elephant_spec_pose",
      "compute_elephant_bone_palette",
  };

  for (const auto& directory : {"render", "game", "app", "tools"}) {
    for (const auto& file : sources_under(root / directory)) {
      if (defines_them(file)) {
        continue;
      }
      const auto source = read_text(file);
      for (const auto& symbol : procedural) {
        EXPECT_EQ(source.find(symbol), std::string::npos)
            << file.string() << " calls " << symbol
            << ", which is procedural fallback anatomy rather than the production "
               "authored-asset path";
      }
    }
  }
}

TEST(CreatureProceduralPose, BindPalettesComeFromTheCompiledAsset) {
  const auto root = find_repo_root();

  const auto horse = read_text(root / "render" / "horse" / "horse_spec.cpp");
  ASSERT_FALSE(horse.empty());
  EXPECT_NE(horse.find("return horse_source_bind_palette();"), std::string::npos)
      << "the horse bind palette no longer comes from the compiled asset";

  const auto elephant = read_text(root / "render" / "elephant" / "elephant_spec.cpp");
  ASSERT_FALSE(elephant.empty());
  EXPECT_NE(elephant.find("elephant_source_bind_palette()"), std::string::npos)
      << "the elephant bind palette no longer comes from the compiled asset";
}
