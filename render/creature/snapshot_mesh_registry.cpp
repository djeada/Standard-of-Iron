#include "snapshot_mesh_registry.h"

#include <QCoreApplication>

#include <array>
#include <filesystem>

namespace Render::Creature::Snapshot {

namespace {

constexpr std::array<std::pair<std::uint32_t, std::string_view>, 2> kAssets{{
    {Render::Creature::Bpat::kSpeciesHorse, "horse_minimal.bpsm"},
    {Render::Creature::Bpat::kSpeciesElephant, "elephant_minimal.bpsm"},
}};

auto lod_slot_index(Render::Creature::CreatureLOD lod) -> std::size_t {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    return 0U;
  case Render::Creature::CreatureLOD::Minimal:
    return 1U;
  case Render::Creature::CreatureLOD::Billboard:
    break;
  }
  return 2U;
}

auto find_existing_asset_root(const std::string &asset_root) -> std::string {
  namespace fs = std::filesystem;
  const fs::path app_dir =
      QCoreApplication::instance() != nullptr
          ? fs::path{QCoreApplication::applicationDirPath().toStdString()}
          : fs::path{};

  std::array<fs::path, 8> candidates{
      fs::path{asset_root},
      fs::path{"../"} / asset_root,
      fs::path{"../../"} / asset_root,
      fs::path{"../../../"} / asset_root,
      fs::current_path() / asset_root,
      app_dir / asset_root,
      app_dir / "../" / asset_root,
      app_dir / "../../" / asset_root,
  };

  for (const auto &candidate : candidates) {
    if (fs::exists(candidate / kAssets[0].second)) {
      return fs::absolute(candidate).lexically_normal().string();
    }
  }
  return asset_root;
}

} // namespace

auto SnapshotMeshRegistry::instance() noexcept -> SnapshotMeshRegistry & {
  static SnapshotMeshRegistry registry;
  return registry;
}

auto SnapshotMeshRegistry::slot(std::uint32_t species_id,
                                Render::Creature::CreatureLOD lod) noexcept
    -> SnapshotMeshBlob * {
  auto const lod_slot = lod_slot_index(lod);
  if (species_id >= Render::Creature::Bpat::kSpeciesCount || lod_slot >= 2U) {
    return nullptr;
  }
  return &m_blobs[(species_id * 2U) + lod_slot];
}

auto SnapshotMeshRegistry::slot(std::uint32_t species_id,
                                Render::Creature::CreatureLOD lod)
    const noexcept -> const SnapshotMeshBlob * {
  auto const lod_slot = lod_slot_index(lod);
  if (species_id >= Render::Creature::Bpat::kSpeciesCount || lod_slot >= 2U) {
    return nullptr;
  }
  return &m_blobs[(species_id * 2U) + lod_slot];
}

auto SnapshotMeshRegistry::load_species(std::uint32_t species_id,
                                        Render::Creature::CreatureLOD lod,
                                        const std::string &path) -> bool {
  auto *dst = slot(species_id, lod);
  if (dst == nullptr) {
    m_last_error = "unsupported snapshot mesh slot";
    return false;
  }
  auto loaded = SnapshotMeshBlob::from_file(path);
  if (!loaded.loaded()) {
    m_last_error = std::string{loaded.last_error()};
    return false;
  }
  if (loaded.species_id() != species_id || loaded.lod() != lod) {
    m_last_error = "snapshot mesh species/lod mismatch";
    return false;
  }
  *dst = std::move(loaded);
  m_last_error.clear();
  return true;
}

auto SnapshotMeshRegistry::load_all(const std::string &asset_root)
    -> std::size_t {
  const std::string resolved_root = find_existing_asset_root(asset_root);
  std::size_t loaded = 0U;
  for (auto const &[species_id, file_name] : kAssets) {
    std::string path = resolved_root;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
      path += '/';
    }
    path.append(file_name);
    if (load_species(species_id, Render::Creature::CreatureLOD::Minimal,
                     path)) {
      ++loaded;
    }
  }
  return loaded;
}

auto SnapshotMeshRegistry::blob(std::uint32_t species_id,
                                Render::Creature::CreatureLOD lod)
    const noexcept -> const SnapshotMeshBlob * {
  const auto *candidate = slot(species_id, lod);
  return (candidate != nullptr && candidate->loaded()) ? candidate : nullptr;
}

void SnapshotMeshRegistry::clear() {
  for (auto &blob : m_blobs) {
    blob = SnapshotMeshBlob{};
  }
  m_last_error.clear();
}

} // namespace Render::Creature::Snapshot
