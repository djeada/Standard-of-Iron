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
    if (fs::exists(current / "CMakeLists.txt") &&
        fs::exists(current / ".github" / "workflows" / "release.yml")) {
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
  std::ostringstream contents;
  contents << stream.rdbuf();
  return contents.str();
}

TEST(ReleaseContract, PublishKeepsDownloadedPackagesInTheWorkspace) {
  const auto workflow =
      read_text(find_repo_root() / ".github" / "workflows" / "release.yml");
  ASSERT_FALSE(workflow.empty());

  const auto publish = workflow.find("  publish:");
  ASSERT_NE(publish, std::string::npos);

  const auto checkout = workflow.find("actions/checkout@", publish);
  const auto download = workflow.find("actions/download-artifact@", publish);
  ASSERT_NE(checkout, std::string::npos);
  ASSERT_NE(download, std::string::npos);
  EXPECT_LT(checkout, download)
      << "checkout cleans packages/ when it runs after artifact download";

  EXPECT_NE(workflow.find(
                "softprops/action-gh-release@b4309332981a82ec1c5618f44dd2e27cc8bfbfda",
                publish),
            std::string::npos)
      << "release publishing must use the pinned Node 24 action";
  EXPECT_NE(workflow.find("fail_on_unmatched_files: true", publish), std::string::npos);
}

TEST(ReleaseContract, ProjectVersionIsVisibleInTheRunningGame) {
  const auto root = find_repo_root();
  const auto cmake = read_text(root / "CMakeLists.txt");
  const auto main_cpp = read_text(root / "main.cpp");
  const auto main_menu = read_text(root / "ui" / "qml" / "MainMenu.qml");
  const auto settings = read_text(root / "ui" / "qml" / "SettingsPanel.qml");

  EXPECT_NE(cmake.find("SOI_VERSION=\"${PROJECT_VERSION}\""), std::string::npos);
  EXPECT_NE(main_cpp.find("setApplicationVersion(QStringLiteral(SOI_VERSION))"),
            std::string::npos);
  EXPECT_NE(main_menu.find("objectName: \"mainMenuVersionLabel\""), std::string::npos);
  EXPECT_NE(main_menu.find("Qt.application.version"), std::string::npos);
  EXPECT_NE(settings.find("Qt.application.version"), std::string::npos);
}

TEST(ReleaseContract, TagPreflightRunsTheCompiledTestWorkflow) {
  const auto root = find_repo_root();
  const auto release = read_text(root / ".github" / "workflows" / "release.yml");
  const auto pull_request = read_text(root / ".github" / "workflows" / "pr.yml");

  EXPECT_NE(pull_request.find("workflow_call:"), std::string::npos);
  EXPECT_NE(release.find("uses: ./.github/workflows/pr.yml"), std::string::npos);
  EXPECT_NE(release.find("gh release delete"), std::string::npos);
}

TEST(ReleaseContract, EveryPackagedGameRunsTheFullReleaseSelfTest) {
  const auto workflows = find_repo_root() / ".github" / "workflows";
  for (const char* name : {"build-linux.yml", "build-macos.yml", "build-windows.yml"}) {
    const auto workflow = read_text(workflows / name);
    SCOPED_TRACE(name);
    EXPECT_NE(workflow.find("-DBUILD_TESTING=OFF"), std::string::npos)
        << "package builds must contain only the game, not test executables";
    EXPECT_NE(workflow.find("--target standard_of_iron"), std::string::npos)
        << "package workflows must not link unrelated developer tools";
    EXPECT_NE(workflow.find("-DENABLE_GENERATED_CAMPAIGN_MAP_ASSETS=ON"),
              std::string::npos);
    EXPECT_NE(workflow.find("python scripts/generate-campaign-map.py"),
              std::string::npos);
    EXPECT_NE(workflow.find("--release-self-test"), std::string::npos);
    for (const char* marker : {"SOI_GRAPHICS_DEFAULT_SELF_TEST: PASS",
                               "SOI_CAMPAIGN_MAP_SELF_TEST: PASS",
                               "SOI_CREATURE_ASSET_SELF_TEST: PASS",
                               "SOI_MISSION_SELF_TEST: PASS",
                               "SOI_RENDERER_SELF_TEST: PASS"}) {
      EXPECT_NE(workflow.find(marker), std::string::npos) << marker;
    }
  }
}

TEST(ReleaseContract, CampaignMapRuntimeAssetsAreMandatory) {
  const auto cmake = read_text(find_repo_root() / "CMakeLists.txt");

  EXPECT_NE(cmake.find("ENABLE_GENERATED_CAMPAIGN_MAP_ASSETS\n"
                       "    \"Embed the campaign map assets required by the shipped "
                       "campaign UI\"\n"
                       "    ON"),
            std::string::npos);
  EXPECT_NE(cmake.find("assets/campaign_map/terrain_height.png"), std::string::npos);
  EXPECT_NE(cmake.find("Required campaign map runtime asset is missing"),
            std::string::npos);
}

TEST(ReleaseContract, FreshProfileDefaultAndPackagedCreatureLookupAreExplicit) {
  const auto root = find_repo_root();
  const auto graphics = read_text(root / "render" / "graphics_settings.h");
  const auto creature_assets =
      read_text(root / "render" / "creature" / "compiled_creature_assets.cpp");

  EXPECT_NE(graphics.find("k_default_graphics_quality = GraphicsQuality::Ultra"),
            std::string::npos);
  EXPECT_NE(creature_assets.find("executable_directory"), std::string::npos);
}

} // namespace
