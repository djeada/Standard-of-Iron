#include <QCoreApplication>

#include <array>
#include <filesystem>
#include <gtest/gtest.h>

#include "animation/bpat/bpat_registry.h"
#include "game/session/session_context.h"

namespace {

void load_baked_clips(const QCoreApplication& app) {
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
  for (const auto& root : roots) {
    if (fs::exists(root / "humanoid.bpat")) {
      Render::Creature::Bpat::BpatRegistry::instance().load_all(root.string());
      return;
    }
  }
}

} // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  load_baked_clips(app);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
