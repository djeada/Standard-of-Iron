#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <set>
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

auto sources_under(const fs::path& root) -> std::vector<fs::path> {
  std::vector<fs::path> result;
  if (!fs::exists(root)) {
    return result;
  }
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext == ".h" || ext == ".cpp") {
      result.push_back(entry.path());
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

auto includes_renderer(const fs::path& file) -> bool {
  std::ifstream stream(file);
  std::string line;
  while (std::getline(stream, line)) {
    const auto include_pos = line.find("#include");
    if (include_pos == std::string::npos) {
      continue;
    }
    const auto quote = line.find('"', include_pos);
    if (quote == std::string::npos) {
      continue;
    }
    const auto end = line.find('"', quote + 1);
    if (end == std::string::npos) {
      continue;
    }
    std::string path = line.substr(quote + 1, end - quote - 1);

    while (path.rfind("../", 0) == 0) {
      path.erase(0, 3);
    }
    if (path.rfind("render/", 0) == 0) {
      return true;
    }
  }
  return false;
}

const std::set<std::string>& known_inversions() {
  static const std::set<std::string> entries{};
  return entries;
}

} // namespace

TEST(ArchitectureLayering, GameDoesNotDependOnTheRendererBeyondKnownInversions) {
  const auto root = find_repo_root();
  ASSERT_TRUE(fs::exists(root / "game")) << "repo root not found from " << root;

  std::vector<std::string> unexpected;
  for (const auto& file : sources_under(root / "game")) {
    if (!includes_renderer(file)) {
      continue;
    }
    const auto relative = fs::relative(file, root).generic_string();
    if (!known_inversions().contains(relative)) {
      unexpected.push_back(relative);
    }
  }

  EXPECT_TRUE(unexpected.empty())
      << "game/ gained a new dependency on render/. Put shared types in scene/ or "
         "animation/, or invert the call:\n  "
      << [&] {
           std::string joined;
           for (const auto& entry : unexpected) {
             joined += entry + "\n  ";
           }
           return joined;
         }();
}

TEST(ArchitectureLayering, KnownInversionListStaysHonest) {
  const auto root = find_repo_root();
  ASSERT_TRUE(fs::exists(root / "game"));

  std::vector<std::string> stale;
  for (const auto& entry : known_inversions()) {
    const fs::path file = root / entry;
    if (!fs::exists(file) || !includes_renderer(file)) {
      stale.push_back(entry);
    }
  }

  EXPECT_TRUE(stale.empty()) << "these no longer depend on render/; remove them from "
                                "known_inversions():\n  "
                             << [&] {
                                  std::string joined;
                                  for (const auto& entry : stale) {
                                    joined += entry + "\n  ";
                                  }
                                  return joined;
                                }();
}

TEST(ArchitectureLayering, SharedSceneAndAnimationLayersStayLeaves) {
  const auto root = find_repo_root();

  for (const char* layer : {"scene", "animation"}) {
    for (const auto& file : sources_under(root / layer)) {
      const auto relative = fs::relative(file, root).generic_string();
      EXPECT_FALSE(includes_renderer(file)) << relative << " must not include render/";

      std::ifstream stream(file);
      std::string line;
      while (std::getline(stream, line)) {
        const bool game_include = line.find("#include") != std::string::npos &&
                                  line.find("game/") != std::string::npos;
        EXPECT_FALSE(game_include) << relative << " must not include game/: " << line;
      }
    }
  }
}
