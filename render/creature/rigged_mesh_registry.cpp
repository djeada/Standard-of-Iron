#include "rigged_mesh_registry.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>

namespace Render::Creature::Rigged {

namespace {

constexpr std::array<std::pair<std::uint32_t, std::string_view>, 10> k_species{{
    {Render::Creature::Bpat::k_species_humanoid, "humanoid"},
    {Render::Creature::Bpat::k_species_humanoid_sword, "humanoid"},
    {Render::Creature::Bpat::k_species_humanoid_spear, "humanoid"},
    {Render::Creature::Bpat::k_species_humanoid_skeleton, "skeleton_humanoid"},
    {Render::Creature::Bpat::k_species_humanoid_caster, "humanoid"},
    {Render::Creature::Bpat::k_species_humanoid_stave_caster, "humanoid"},
    {Render::Creature::Bpat::k_species_horse, "horse"},
    {Render::Creature::Bpat::k_species_elephant, "elephant"},
    {Render::Creature::Bpat::k_species_sheep, "sheep"},
    {Render::Creature::Bpat::k_species_wolf, "wolf"},
}};

constexpr std::array<Render::Creature::CreatureLOD, 2> k_lods{
    Render::Creature::CreatureLOD::Full,
    Render::Creature::CreatureLOD::Minimal,
};

auto lod_slot_index(Render::Creature::CreatureLOD lod) -> std::size_t {
  switch (lod) {
  case Render::Creature::CreatureLOD::Full:
    return 0U;
  case Render::Creature::CreatureLOD::Minimal:
    return 1U;
  case Render::Creature::CreatureLOD::Culled:
    break;
  }
  return 2U;
}

auto find_existing_asset_root(const std::string& asset_root) -> std::string {
  namespace fs = std::filesystem;
  const fs::path app_dir =
      QCoreApplication::instance() != nullptr
          ? fs::path{QCoreApplication::applicationDirPath().toStdString()}
          : fs::path{};

  auto const probe =
      asset_file_name(k_species[0].second, Render::Creature::CreatureLOD::Full);
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

  for (const auto& candidate : candidates) {
    if (fs::exists(candidate / probe)) {
      return fs::absolute(candidate).lexically_normal().string();
    }
  }
  return asset_root;
}

} // namespace

auto RiggedMeshRegistry::instance() noexcept -> RiggedMeshRegistry& {
  static RiggedMeshRegistry registry;
  return registry;
}

auto RiggedMeshRegistry::slot(std::uint32_t species_id,
                              Render::Creature::CreatureLOD lod) noexcept
    -> RiggedMeshBlob* {
  auto const lod_slot = lod_slot_index(lod);
  if (species_id >= Render::Creature::Bpat::k_species_count || lod_slot >= 2U) {
    return nullptr;
  }
  return &m_blobs[(species_id * 2U) + lod_slot];
}

auto RiggedMeshRegistry::slot(std::uint32_t species_id,
                              Render::Creature::CreatureLOD lod) const noexcept
    -> const RiggedMeshBlob* {
  auto const lod_slot = lod_slot_index(lod);
  if (species_id >= Render::Creature::Bpat::k_species_count || lod_slot >= 2U) {
    return nullptr;
  }
  return &m_blobs[(species_id * 2U) + lod_slot];
}

auto RiggedMeshRegistry::load_species(std::uint32_t species_id,
                                      Render::Creature::CreatureLOD lod,
                                      const std::string& path) -> bool {
  auto* dst = slot(species_id, lod);
  if (dst == nullptr) {
    m_last_error = "unsupported rigged mesh slot";
    return false;
  }
  auto loaded = RiggedMeshBlob::from_file(path);
  if (!loaded.loaded()) {
    m_last_error = std::string{loaded.last_error()};
    return false;
  }
  if (loaded.lod() != lod) {
    m_last_error = "rigged mesh lod mismatch";
    return false;
  }
  *dst = std::move(loaded);
  m_last_error.clear();
  return true;
}

auto RiggedMeshRegistry::load_all(const std::string& asset_root) -> std::size_t {
  const std::string resolved_root = find_existing_asset_root(asset_root);
  std::size_t loaded = 0U;
  for (auto const& [species_id, species_name] : k_species) {
    for (auto const lod : k_lods) {
      std::string path = resolved_root;
      if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path += '/';
      }
      path.append(asset_file_name(species_name, lod));
      if (load_species(species_id, lod, path)) {
        ++loaded;
      }
    }
  }

  namespace fs = std::filesystem;
  std::error_code ec;
  for (auto const& entry : fs::directory_iterator(resolved_root, ec)) {
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    std::string const file = entry.path().filename().string();
    for (auto const lod : k_lods) {
      std::string const suffix = asset_file_name("", lod);
      if (file.size() <= suffix.size() ||
          file.compare(file.size() - suffix.size(), suffix.size(), suffix) != 0) {
        continue;
      }
      std::string const body_name = file.substr(0, file.size() - suffix.size());
      bool const is_species =
          std::any_of(k_species.begin(), k_species.end(), [&](const auto& species) {
            return species.second == body_name;
          });
      if (!is_species && load_body(body_name, lod, entry.path().string())) {
        ++loaded;
      }
    }
  }
  return loaded;
}

auto RiggedMeshRegistry::load_body(std::string_view body_name,
                                   Render::Creature::CreatureLOD lod,
                                   const std::string& path) -> bool {
  auto const lod_slot = lod_slot_index(lod);
  if (lod_slot >= 2U) {
    m_last_error = "unsupported rigged mesh slot";
    return false;
  }
  auto loaded = RiggedMeshBlob::from_file(path);
  if (!loaded.loaded()) {
    m_last_error = std::string{loaded.last_error()};
    return false;
  }
  if (loaded.lod() != lod) {
    m_last_error = "rigged mesh lod mismatch";
    return false;
  }
  m_bodies[std::string(body_name)][lod_slot] = std::move(loaded);
  m_last_error.clear();
  return true;
}

auto RiggedMeshRegistry::body(std::string_view body_name,
                              Render::Creature::CreatureLOD lod) const noexcept
    -> const RiggedMeshBlob* {
  auto const lod_slot = lod_slot_index(lod);
  auto const it = m_bodies.find(body_name);
  if (lod_slot >= 2U || it == m_bodies.end()) {
    return nullptr;
  }
  const auto& candidate = it->second[lod_slot];
  return candidate.loaded() ? &candidate : nullptr;
}

auto RiggedMeshRegistry::blob(std::uint32_t species_id,
                              Render::Creature::CreatureLOD lod) const noexcept
    -> const RiggedMeshBlob* {
  const auto* candidate = slot(species_id, lod);
  return (candidate != nullptr && candidate->loaded()) ? candidate : nullptr;
}

void RiggedMeshRegistry::clear() {
  for (auto& blob : m_blobs) {
    blob = RiggedMeshBlob{};
  }
  m_bodies.clear();
  m_last_error.clear();
}

} // namespace Render::Creature::Rigged
