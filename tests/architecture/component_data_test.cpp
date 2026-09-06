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
    if (fs::exists(current / "CMakeLists.txt") && fs::exists(current / "game")) {
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

auto component_headers() -> std::vector<fs::path> {
  const auto root = find_repo_root();
  std::vector<fs::path> headers;
  for (const char* name : {"component_core.h",
                           "component_combat.h",
                           "component_structures.h",
                           "component_commander.h",
                           "component_gameplay.h",
                           "component_economy.h",
                           "component_presentation.h"}) {
    headers.push_back(root / "game" / "core" / name);
  }
  headers.push_back(root / "render" / "creature" / "animation_state_components.h");
  headers.push_back(root / "render" / "humanoid" / "runtime" / "instance_state.h");
  return headers;
}

} // namespace

TEST(ComponentDataTest, NoComponentDerivesFromABase) {
  for (const auto& header : component_headers()) {
    const auto source = read_text(header);
    ASSERT_FALSE(source.empty()) << header.string();
    const std::regex derived(
        R"((?:class|struct) [A-Za-z0-9_]+Component\s*:\s*(?:public )?[A-Za-z])");
    EXPECT_FALSE(std::regex_search(source, derived))
        << header.string()
        << " declares a component with a base class; components "
           "are plain data";
  }
}

TEST(ComponentDataTest, NoComponentRunsPerTickBehaviour) {
  const std::regex per_tick(
      R"(\n  (?:\[\[nodiscard\]\] )?(?:auto|void|static) [A-Za-z0-9_]+\([^)]*float (?:delta_time|dt|delta)\b)");

  for (const auto& header : component_headers()) {
    const auto source = read_text(header);
    ASSERT_FALSE(source.empty()) << header.string();

    std::smatch match;
    const bool found = std::regex_search(source, match, per_tick);
    EXPECT_FALSE(found) << header.string() << " gives a component a per-tick method ("
                        << (found ? match.str() : "")
                        << "); stepping state over time is a system's job";
  }
}

TEST(ComponentDataTest, NoComponentDeclaresAVirtualMember) {
  for (const auto& header : component_headers()) {
    const auto source = read_text(header);
    ASSERT_FALSE(source.empty()) << header.string();
    EXPECT_EQ(source.find("virtual "), std::string::npos)
        << header.string() << " declares a virtual member; components are plain data";
  }
}
