#include "static_mesh_pack.h"

#include <QCoreApplication>
#include <QDebug>

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <type_traits>
#include <utility>

#include "animation/bpat/asset_compression.h"
#include "mesh_pack_format.h"

namespace Render::GL {
static_assert(sizeof(MergedBuildingVertex) == 16U * sizeof(float));

auto find_baked_mesh_directory(const std::string& directory) -> std::filesystem::path {
  namespace fs = std::filesystem;
  const fs::path app_dir =
      QCoreApplication::instance() != nullptr
          ? fs::path{QCoreApplication::applicationDirPath().toStdString()}
          : fs::path{};
  const std::array<fs::path, 6> candidates{
      app_dir / directory,
      fs::path{directory},
      fs::current_path() / directory,
      app_dir / "../" / directory,
      fs::path{"../"} / directory,
      fs::path{"../../"} / directory,
  };
  std::error_code ec;
  for (const auto& candidate : candidates) {
    if (fs::is_directory(candidate, ec)) {
      return candidate;
    }
  }
  return fs::path{directory};
}

auto next_merged_mesh_id() -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{0};
  return ++counter;
}

auto serialize_static_mesh_pack(const std::vector<StaticMeshPackEntry>& entries)
    -> std::string {
  std::vector<MeshPack::Entry<MergedBuildingVertex>> packed;
  packed.reserve(entries.size());
  for (const StaticMeshPackEntry& entry : entries) {
    MeshPack::Entry<MergedBuildingVertex> out{
        .name = entry.name,
        .vertices = entry.mesh.vertices,
        .indices = entry.mesh.indices,
        .bounds = {entry.mesh.bounds_center.x(),
                   entry.mesh.bounds_center.y(),
                   entry.mesh.bounds_center.z(),
                   entry.mesh.bounds_radius},
    };
    for (const MergedBuildingRange& range : entry.mesh.ranges) {
      out.ranges.push_back({range.first_index, range.index_count});
    }
    packed.push_back(std::move(out));
  }
  return MeshPack::serialize(k_static_mesh_pack_magic, packed);
}

auto parse_static_mesh_pack(const std::uint8_t* data,
                            std::size_t size,
                            std::vector<StaticMeshPackEntry>& out,
                            std::string& error) -> bool {
  std::vector<MeshPack::Entry<MergedBuildingVertex>> packed;
  if (!MeshPack::parse(k_static_mesh_pack_magic, data, size, packed, error)) {
    return false;
  }
  out.clear();
  out.reserve(packed.size());
  for (auto& entry : packed) {
    StaticMeshPackEntry converted;
    converted.name = std::move(entry.name);
    converted.mesh.vertices = std::move(entry.vertices);
    converted.mesh.indices = std::move(entry.indices);
    for (const MeshPack::Range& range : entry.ranges) {
      converted.mesh.ranges.push_back(MergedBuildingRange{
          .first_index = range.first_index, .index_count = range.index_count});
    }
    converted.mesh.bounds_center =
        QVector3D(entry.bounds[0], entry.bounds[1], entry.bounds[2]);
    converted.mesh.bounds_radius = entry.bounds[3];
    out.push_back(std::move(converted));
  }
  return true;
}

auto StaticMeshLibrary::instance() -> StaticMeshLibrary& {
  static StaticMeshLibrary library;
  return library;
}

auto StaticMeshLibrary::load_all(const std::string& directory) -> std::size_t {
  namespace fs = std::filesystem;
  const fs::path root = find_baked_mesh_directory(directory);
  std::error_code ec;
  std::size_t loaded = 0;
  for (const auto& file : fs::directory_iterator(root, ec)) {
    if (file.path().extension() != ".smpk") {
      continue;
    }
    std::vector<std::uint8_t> bytes;
    std::string error;
    std::vector<StaticMeshPackEntry> entries;
    if (!Render::Creature::Bpat::read_asset_file(file.path().string(), bytes, error) ||
        !parse_static_mesh_pack(bytes.data(), bytes.size(), entries, error)) {
      qWarning() << "StaticMeshLibrary:" << QString::fromStdString(file.path().string())
                 << QString::fromStdString(error);
      continue;
    }
    for (StaticMeshPackEntry& entry : entries) {
      entry.mesh.id = next_merged_mesh_id();
      m_meshes[std::move(entry.name)] =
          std::make_shared<const MergedBuildingMesh>(std::move(entry.mesh));
      ++loaded;
    }
  }
  if (ec) {
    qWarning() << "StaticMeshLibrary: cannot read"
               << QString::fromStdString(root.string());
  }
  return loaded;
}

void StaticMeshLibrary::add(std::string name, MergedBuildingMesh mesh) {
  const std::lock_guard<std::mutex> lock(m_mutex);
  ensure_loaded();
  mesh.id = next_merged_mesh_id();
  m_meshes[std::move(name)] =
      std::make_shared<const MergedBuildingMesh>(std::move(mesh));
}

auto StaticMeshLibrary::find(std::string_view name)
    -> std::shared_ptr<const MergedBuildingMesh> {
  const std::lock_guard<std::mutex> lock(m_mutex);
  ensure_loaded();
  const auto it = m_meshes.find(std::string(name));
  return it != m_meshes.end() ? it->second : nullptr;
}

auto StaticMeshLibrary::size() -> std::size_t {
  const std::lock_guard<std::mutex> lock(m_mutex);
  ensure_loaded();
  return m_meshes.size();
}

void StaticMeshLibrary::clear() {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_meshes.clear();
  m_loaded = false;
}

void StaticMeshLibrary::ensure_loaded() {
  if (m_loaded) {
    return;
  }
  m_loaded = true;
  (void)load_all("assets/meshes");
}

} // namespace Render::GL
