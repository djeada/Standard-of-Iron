#pragma once

#include <QCoreApplication>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace TestAssets {

inline auto
find_creature_assets_dir(std::string_view marker_file) -> std::string {
  namespace fs = std::filesystem;

  fs::path const app_dir =
      QCoreApplication::instance() != nullptr
          ? fs::path{QCoreApplication::applicationDirPath().toStdString()}
          : fs::path{};

  std::array<fs::path, 8> const candidates{
      fs::current_path() / "assets" / "creatures",
      fs::current_path() / ".." / "assets" / "creatures",
      fs::current_path() / ".." / ".." / "assets" / "creatures",
      app_dir / "assets" / "creatures",
      app_dir / ".." / "assets" / "creatures",
      app_dir / ".." / ".." / "assets" / "creatures",
      fs::path{"build"} / "bin" / "assets" / "creatures",
      fs::path{"assets"} / "creatures",
  };

  for (auto const &candidate : candidates) {
    if (fs::exists(candidate / marker_file)) {
      return fs::absolute(candidate).lexically_normal().string();
    }
  }
  return {};
}

} // namespace TestAssets
