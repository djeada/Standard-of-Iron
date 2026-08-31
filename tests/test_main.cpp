#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <filesystem>
#include <gtest/gtest.h>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_registry.h"
#include "game/session/session_context.h"
#include "render/creature/snapshot_mesh_registry.h"

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  QTemporaryDir settings_sandbox;
  if (!settings_sandbox.isValid()) {
    qCritical("could not create a throwaway settings profile for the tests");
    return 1;
  }
  QSettings::setPath(
      QSettings::IniFormat, QSettings::UserScope, settings_sandbox.path());

  namespace fs = std::filesystem;
  const fs::path app_dir = fs::path(app.applicationDirPath().toStdString());
  const std::array<fs::path, 8> roots{
      fs::current_path() / "assets" / "creatures",
      fs::current_path() / ".." / "assets" / "creatures",
      fs::current_path() / ".." / ".." / "assets" / "creatures",
      app_dir / "assets" / "creatures",
      app_dir / ".." / "assets" / "creatures",
      app_dir / ".." / ".." / "assets" / "creatures",
      fs::path("build") / "bin" / "assets" / "creatures",
      fs::path("assets") / "creatures",
  };
  auto& reg = Render::Creature::Bpat::BpatRegistry::instance();
  auto& snapshot_reg = Render::Creature::Snapshot::SnapshotMeshRegistry::instance();
  for (const auto& root : roots) {
    if (fs::exists(root / "humanoid.bpat")) {
      reg.load_all(root.string());
      snapshot_reg.load_all(root.string());
      break;
    }
  }

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
