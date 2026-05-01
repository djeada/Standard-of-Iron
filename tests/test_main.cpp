#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/snapshot_mesh_registry.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <filesystem>
#include <gtest/gtest.h>

int main(int argc, char **argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);

  namespace fs = std::filesystem;
  const std::array<fs::path, 4> roots{
      fs::current_path() / "assets" / "creatures",
      fs::current_path() / ".." / "assets" / "creatures",
      fs::current_path() / ".." / ".." / "assets" / "creatures",
      fs::path("assets") / "creatures",
  };
  auto &reg = Render::Creature::Bpat::BpatRegistry::instance();
  auto &snapshot_reg =
      Render::Creature::Snapshot::SnapshotMeshRegistry::instance();
  for (const auto &root : roots) {
    if (fs::exists(root / "humanoid.bpat")) {
      reg.load_all(root.string());
      snapshot_reg.load_all(root.string());
      break;
    }
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
