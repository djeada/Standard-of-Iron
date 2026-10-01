#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "static_building_batch.h"

namespace Render::GL {

inline constexpr std::uint32_t k_static_mesh_pack_magic = 0x4B504D53U;

struct StaticMeshPackEntry {
  std::string name;
  MergedBuildingMesh mesh;
};

[[nodiscard]] auto serialize_static_mesh_pack(
    const std::vector<StaticMeshPackEntry>& entries) -> std::string;

auto parse_static_mesh_pack(const std::uint8_t* data,
                            std::size_t size,
                            std::vector<StaticMeshPackEntry>& out,
                            std::string& error) -> bool;

[[nodiscard]] auto
find_baked_mesh_directory(const std::string& directory) -> std::filesystem::path;

[[nodiscard]] auto next_merged_mesh_id() -> std::uint64_t;

class StaticMeshLibrary {
public:
  static auto instance() -> StaticMeshLibrary&;

  void add(std::string name, MergedBuildingMesh mesh);

  [[nodiscard]] auto
  find(std::string_view name) -> std::shared_ptr<const MergedBuildingMesh>;

  [[nodiscard]] auto size() -> std::size_t;

  void clear();

private:
  void ensure_loaded();
  auto load_all(const std::string& directory) -> std::size_t;

  std::mutex m_mutex;
  bool m_loaded{false};
  std::unordered_map<std::string, std::shared_ptr<const MergedBuildingMesh>> m_meshes;
};

} // namespace Render::GL
