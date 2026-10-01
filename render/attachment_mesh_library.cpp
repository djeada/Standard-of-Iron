#include "attachment_mesh_library.h"

#include <QDebug>

#include <cstdio>
#include <filesystem>
#include <utility>

#include "animation/bpat/asset_compression.h"
#include "creature/spec.h"
#include "mesh_pack_format.h"
#include "static_mesh_pack.h"

namespace Render::GL {

auto attachment_mesh_key(const Render::Creature::CreatureSpec& spec,
                         Render::Creature::CreatureLOD lod,
                         std::uint32_t skin_species_id,
                         std::string_view attachment_set_name) -> std::string {
  std::array<char, 32> middle{};
  std::snprintf(middle.data(),
                middle.size(),
                "/%u/%u/",
                static_cast<unsigned>(lod),
                skin_species_id);
  std::string key(spec.species_name);
  key += middle.data();
  key += attachment_set_name;
  return key;
}

auto serialize_attachment_mesh_pack(const std::vector<AttachmentMeshEntry>& entries)
    -> std::string {
  std::vector<MeshPack::Entry<RiggedVertex>> packed;
  packed.reserve(entries.size());
  for (const AttachmentMeshEntry& entry : entries) {
    packed.push_back(MeshPack::Entry<RiggedVertex>{
        .name = entry.key, .vertices = entry.vertices, .indices = entry.indices});
  }
  return MeshPack::serialize(k_attachment_mesh_pack_magic, packed);
}

auto parse_attachment_mesh_pack(const std::uint8_t* data,
                                std::size_t size,
                                std::vector<AttachmentMeshEntry>& out,
                                std::string& error) -> bool {
  std::vector<MeshPack::Entry<RiggedVertex>> packed;
  if (!MeshPack::parse(k_attachment_mesh_pack_magic, data, size, packed, error)) {
    return false;
  }
  out.clear();
  out.reserve(packed.size());
  for (auto& entry : packed) {
    out.push_back(AttachmentMeshEntry{.key = std::move(entry.name),
                                      .vertices = std::move(entry.vertices),
                                      .indices = std::move(entry.indices)});
  }
  return true;
}

auto AttachmentMeshLibrary::instance() -> AttachmentMeshLibrary& {
  static AttachmentMeshLibrary library;
  return library;
}

auto AttachmentMeshLibrary::find(std::string_view key)
    -> std::shared_ptr<const AttachmentMeshEntry> {
  const std::lock_guard<std::mutex> lock(m_mutex);
  ensure_loaded();
  const auto it = m_meshes.find(std::string(key));
  return it != m_meshes.end() ? it->second : nullptr;
}

auto AttachmentMeshLibrary::size() -> std::size_t {
  const std::lock_guard<std::mutex> lock(m_mutex);
  ensure_loaded();
  return m_meshes.size();
}

void AttachmentMeshLibrary::clear() {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_meshes.clear();
  m_loaded = false;
}

void AttachmentMeshLibrary::start_empty() {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_meshes.clear();
  m_loaded = true;
}

void AttachmentMeshLibrary::ensure_loaded() {
  if (m_loaded) {
    return;
  }
  m_loaded = true;
  const auto path =
      find_baked_mesh_directory("assets/meshes") / std::string(k_attachment_mesh_pack);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return;
  }
  std::vector<std::uint8_t> bytes;
  std::string error;
  std::vector<AttachmentMeshEntry> entries;
  if (!Render::Creature::Bpat::read_asset_file(path.string(), bytes, error) ||
      !parse_attachment_mesh_pack(bytes.data(), bytes.size(), entries, error)) {
    qWarning() << "AttachmentMeshLibrary:" << QString::fromStdString(path.string())
               << QString::fromStdString(error);
    return;
  }
  for (AttachmentMeshEntry& entry : entries) {
    std::string key = entry.key;
    m_meshes[std::move(key)] =
        std::make_shared<const AttachmentMeshEntry>(std::move(entry));
  }
}

} // namespace Render::GL
