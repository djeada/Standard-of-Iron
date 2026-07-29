

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;

auto find_repo_root() -> fs::path {
  fs::path current = fs::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(current / "README.md") && fs::exists(current / "game")) {
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

auto contains(const std::string& haystack, const std::string& needle) -> bool {
  return haystack.find(needle) != std::string::npos;
}

TEST(DocumentationAccuracy, StatedEntityIdWidthMatchesTheCode) {
  const auto root = find_repo_root();
  const auto entity = read_text(root / "game" / "core" / "entity.h");
  const auto readme = read_text(root / "README.md");
  ASSERT_FALSE(entity.empty());
  ASSERT_FALSE(readme.empty());

  const bool sixty_four_bit = contains(entity, "using EntityID = std::uint64_t");
  ASSERT_TRUE(sixty_four_bit) << "entity id width changed; update the README too";
  EXPECT_TRUE(contains(readme, "64-bit"))
      << "the README no longer states the entity handle width";
  EXPECT_FALSE(contains(readme, "32-bit _entity ID_"))
      << "the README describes a 32-bit entity id that no longer exists";
}

TEST(DocumentationAccuracy, DoesNotClaimComponentsAvoidPolymorphism) {
  const auto root = find_repo_root();
  const auto entity = read_text(root / "game" / "core" / "entity.h");
  const auto readme = read_text(root / "README.md");
  ASSERT_FALSE(entity.empty());

  const bool polymorphic = contains(entity, "virtual ~Component()");
  ASSERT_TRUE(polymorphic);
  EXPECT_FALSE(contains(readme, "Polymorphism is avoided"))
      << "components still share a polymorphic base; the README says otherwise";
}

TEST(DocumentationAccuracy, DoesNotDenyTheSoftwareRenderingBackend) {
  const auto root = find_repo_root();
  const auto readme = read_text(root / "README.md");
  ASSERT_FALSE(readme.empty());

  const bool backend_exists = fs::exists(root / "render" / "software_backend.h") &&
                              fs::exists(root / "render" / "software");
  ASSERT_TRUE(backend_exists);
  EXPECT_FALSE(contains(readme, "Software rendering is not supported"))
      << "a CPU rasteriser backend ships under render/software; describe what it "
         "actually does instead of denying it";
  EXPECT_TRUE(contains(readme, "--force-software"))
      << "the README should say how to select the software backend";
}

TEST(DocumentationAccuracy, ArchitectureDocumentDescribesTheEnforcedBoundaries) {
  const auto root = find_repo_root();
  const auto doc = read_text(root / "docs" / "ARCHITECTURE.md");
  ASSERT_FALSE(doc.empty()) << "docs/ARCHITECTURE.md is missing";

  for (const char* claim :
       {"game_sim", "game_view", "SessionContext", "CommandQueue"}) {
    EXPECT_TRUE(contains(doc, claim))
        << "docs/ARCHITECTURE.md no longer describes " << claim;
  }

  EXPECT_TRUE(contains(doc, "Known limitations"))
      << "docs/ARCHITECTURE.md must keep stating what is still open";
}

TEST(DocumentationAccuracy, ReadmePointsAtTheArchitectureDocument) {
  const auto root = find_repo_root();
  const auto readme = read_text(root / "README.md");
  EXPECT_TRUE(contains(readme, "docs/ARCHITECTURE.md"));
}

} // namespace
