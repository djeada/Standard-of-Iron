#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "creature/part_graph.h"
#include "rigged_mesh.h"

namespace Render::Creature {
struct CreatureSpec;
}

namespace Render::GL {

inline constexpr std::uint32_t k_attachment_mesh_pack_magic = 0x4B504D52U;
inline constexpr std::string_view k_attachment_mesh_pack = "attachments.rmpk";

[[nodiscard]] auto
attachment_mesh_key(const Render::Creature::CreatureSpec& spec,
                    Render::Creature::CreatureLOD lod,
                    std::uint32_t skin_species_id,
                    std::string_view attachment_set_name) -> std::string;

struct AttachmentMeshEntry {
  std::string key;
  std::vector<RiggedVertex> vertices;
  std::vector<std::uint32_t> indices;
};

[[nodiscard]] auto serialize_attachment_mesh_pack(
    const std::vector<AttachmentMeshEntry>& entries) -> std::string;

auto parse_attachment_mesh_pack(const std::uint8_t* data,
                                std::size_t size,
                                std::vector<AttachmentMeshEntry>& out,
                                std::string& error) -> bool;

class AttachmentMeshLibrary {
public:
  static auto instance() -> AttachmentMeshLibrary&;

  [[nodiscard]] auto
  find(std::string_view key) -> std::shared_ptr<const AttachmentMeshEntry>;

  [[nodiscard]] auto size() -> std::size_t;

  void clear();

  void start_empty();

private:
  void ensure_loaded();

  std::mutex m_mutex;
  bool m_loaded{false};
  std::unordered_map<std::string, std::shared_ptr<const AttachmentMeshEntry>> m_meshes;
};

} // namespace Render::GL
