#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "game/save/snapshot_contract.h"
#include "game/systems/save_format.h"

namespace {

using Game::Save::FieldClass;

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::current_path();
  for (int i = 0; i < 8; ++i) {
    if (std::filesystem::exists(path / "game" / "core" / "component.h")) {
      return path;
    }
    path = path.parent_path();
  }
  return std::filesystem::current_path();
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream stream(path);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

auto declared_components() -> std::set<std::string> {
  const auto source = read_text(find_repo_root() / "game" / "core" / "component.h");
  const std::regex pattern(R"(^class ([A-Za-z0-9_]+) : public Component)",
                           std::regex::multiline);

  std::set<std::string> names;
  for (auto it = std::sregex_iterator(source.begin(), source.end(), pattern);
       it != std::sregex_iterator();
       ++it) {
    names.insert((*it)[1].str());
  }
  return names;
}

auto serialized_components() -> std::set<std::string> {
  const auto source =
      read_text(find_repo_root() / "game" / "core" / "serialization.cpp");
  const std::regex pattern(R"((?:get_component|add_component)<([A-Za-z0-9_]+)>)");

  std::set<std::string> names;
  for (auto it = std::sregex_iterator(source.begin(), source.end(), pattern);
       it != std::sregex_iterator();
       ++it) {
    names.insert((*it)[1].str());
  }
  return names;
}

auto contract_components() -> std::set<std::string> {
  std::set<std::string> names;
  for (const auto& spec : Game::Save::fields()) {
    const std::string name = spec.name;
    if (name.find('.') == std::string::npos) {
      names.insert(name);
    }
  }
  return names;
}

TEST(SnapshotContractTest, EveryComponentIsClassified) {

  const auto declared = declared_components();
  ASSERT_FALSE(declared.empty()) << "component.h could not be scanned";

  const auto classified = contract_components();
  for (const auto& name : declared) {
    EXPECT_TRUE(classified.contains(name))
        << name
        << " is not in the snapshot contract. Add it to game/save/"
           "snapshot_contract.cpp and say whether it is authoritative-serialized, "
           "derived-rebuilt or presentation-only.";
  }
}

TEST(SnapshotContractTest, ContractDoesNotNameComponentsThatNoLongerExist) {
  const auto declared = declared_components();
  ASSERT_FALSE(declared.empty());

  for (const auto& name : contract_components()) {
    EXPECT_TRUE(declared.contains(name))
        << name << " is classified in the contract but no longer exists.";
  }
}

TEST(SnapshotContractTest, AuthoritativeComponentsAreActuallySerialized) {
  const auto serialized = serialized_components();
  ASSERT_FALSE(serialized.empty()) << "serialization.cpp could not be scanned";

  for (const auto& spec : Game::Save::fields()) {
    if (spec.classification != FieldClass::AuthoritativeSerialized) {
      continue;
    }
    const std::string name = spec.name;
    if (name.find('.') != std::string::npos) {
      continue;
    }
    EXPECT_TRUE(serialized.contains(name))
        << name
        << " is declared authoritative-serialized but the world serializer "
           "never touches it, so it is silently lost on save.";
  }
}

TEST(SnapshotContractTest, DerivedAndPresentationStateIsNeverWrittenToASave) {

  const auto serialized = serialized_components();
  ASSERT_FALSE(serialized.empty());

  for (const auto& spec : Game::Save::fields()) {
    const std::string name = spec.name;
    if (name.find('.') != std::string::npos) {
      continue;
    }
    if (spec.classification == FieldClass::AuthoritativeSerialized) {
      continue;
    }

    if (name == "ConstructionPreviewComponent") {
      continue;
    }
    EXPECT_FALSE(serialized.contains(name))
        << name << " is " << Game::Save::field_class_name(spec.classification)
        << " but the world serializer writes it.";
  }
}

TEST(SnapshotContractTest, SessionStoresAreAllAccountedFor) {

  for (const char* store : {"session.world",
                            "session.terrain",
                            "session.owners",
                            "session.economy",
                            "session.nations",
                            "session.stats",
                            "session.clock",
                            "session.rng",
                            "session.visibility",
                            "session.troop_counts",
                            "session.building_collision",
                            "session.marketplace",
                            "session.commands"}) {
    EXPECT_NE(Game::Save::find(store), nullptr) << store << " is unclassified";
  }
}

TEST(SnapshotContractTest, ThereIsOnlyOneVersionNumber) {
  EXPECT_EQ(Game::Systems::Save::k_schema_version, Game::Save::k_snapshot_version);
}

TEST(SnapshotContractTest, EveryEntryExplainsItself) {
  for (const auto& spec : Game::Save::fields()) {
    ASSERT_NE(spec.rationale, nullptr) << spec.name;
    EXPECT_GT(std::string(spec.rationale).size(), 10U)
        << spec.name << " needs a rationale someone can act on";
  }
}

} // namespace
