#include "bpat_registry.h"

#include <QCoreApplication>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace Render::Creature::Bpat {

namespace {

inline auto
matrix_from_row_major(std::span<const float> row_major) -> QMatrix4x4 {
  return QMatrix4x4(row_major.data());
}

constexpr std::array<std::string_view, kSpeciesCount> kSpeciesAssetName{
    "humanoid.bpat",
    "horse.bpat",
    "elephant.bpat",
    "humanoid_sword.bpat",
};

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
    if (fs::exists(candidate / kSpeciesAssetName[0])) {
      return fs::absolute(candidate).lexically_normal().string();
    }
  }
  return asset_root;
}

} // namespace

auto BpatRegistry::instance() noexcept -> BpatRegistry & {
  static BpatRegistry registry;
  return registry;
}

auto BpatRegistry::species_slot(std::uint32_t id) noexcept -> BpatBlob * {
  return id < m_blobs.size() ? &m_blobs[id] : nullptr;
}

auto BpatRegistry::species_slot(std::uint32_t id) const noexcept
    -> const BpatBlob * {
  return id < m_blobs.size() ? &m_blobs[id] : nullptr;
}

auto BpatRegistry::load_species(std::uint32_t species_id,
                                const std::string &path) -> bool {
  auto *slot = species_slot(species_id);
  if (slot == nullptr) {
    m_last_error = "unknown species id";
    return false;
  }
  auto loaded = BpatBlob::from_file(path);
  if (!loaded.loaded()) {
    m_last_error = std::string{loaded.last_error()};
    return false;
  }
  if (loaded.species_id() != species_id) {
    m_last_error = "bpat species id mismatch";
    return false;
  }
  *slot = std::move(loaded);
  m_last_error.clear();
  return true;
}

auto BpatRegistry::load_all(const std::string &asset_root) -> std::size_t {
  const std::string resolved_root = find_existing_asset_root(asset_root);
  std::size_t loaded = 0U;
  for (std::uint32_t i = 0; i < kSpeciesAssetName.size(); ++i) {
    std::string path = resolved_root;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
      path += '/';
    }
    path.append(kSpeciesAssetName[i]);
    if (load_species(i, path)) {
      ++loaded;
    }
  }
  return loaded;
}

auto BpatRegistry::blob(std::uint32_t species_id) const noexcept
    -> const BpatBlob * {
  const auto *slot = species_slot(species_id);
  return (slot != nullptr && slot->loaded()) ? slot : nullptr;
}

auto BpatRegistry::has_species(std::uint32_t species_id) const noexcept
    -> bool {
  return blob(species_id) != nullptr;
}

auto BpatRegistry::find_clip(std::uint32_t species_id,
                             std::string_view name) const -> std::uint32_t {
  const auto *b = blob(species_id);
  if (b == nullptr) {
    return kInvalidClip;
  }
  for (std::uint32_t i = 0; i < b->clip_count(); ++i) {
    if (b->clip(i).name == name) {
      return i;
    }
  }
  return kInvalidClip;
}

auto BpatRegistry::sample_palette(
    std::uint32_t species_id, std::uint32_t clip_index,
    std::uint32_t frame_in_clip,
    std::span<QMatrix4x4> out) const -> std::uint32_t {
  const auto *b = blob(species_id);
  if (b == nullptr || clip_index >= b->clip_count()) {
    return 0U;
  }
  auto const c = b->clip(clip_index);
  if (c.frame_count == 0U) {
    return 0U;
  }
  std::uint32_t const wrapped = frame_in_clip % c.frame_count;
  std::uint32_t const global_frame = c.frame_offset + wrapped;
  std::uint32_t const bones = b->bone_count();
  std::uint32_t const n =
      std::min<std::uint32_t>(bones, static_cast<std::uint32_t>(out.size()));
  for (std::uint32_t bone = 0; bone < n; ++bone) {
    auto row = b->palette_matrix(global_frame, bone);
    if (row.size() < kMatrixFloats) {
      return bone;
    }
    out[bone] = matrix_from_row_major(row);
  }
  return n;
}

auto BpatRegistry::sample_socket(std::uint32_t species_id,
                                 std::uint32_t clip_index,
                                 std::uint32_t frame_in_clip,
                                 std::uint32_t socket_index,
                                 QMatrix4x4 &out) const -> bool {
  const auto *b = blob(species_id);
  if (b == nullptr || clip_index >= b->clip_count() ||
      socket_index >= b->socket_count()) {
    return false;
  }
  auto const c = b->clip(clip_index);
  if (c.frame_count == 0U) {
    return false;
  }
  std::uint32_t const wrapped = frame_in_clip % c.frame_count;
  std::uint32_t const global_frame = c.frame_offset + wrapped;
  auto row = b->socket_matrix(global_frame, socket_index);
  if (row.size() < kSocketMatrixFloats) {
    return false;
  }

  std::array<float, 16> full{};
  for (int row_idx = 0; row_idx < 3; ++row_idx) {
    for (int col = 0; col < 4; ++col) {
      full[(row_idx * 4) + col] = row[(row_idx * 4) + col];
    }
  }
  full[12] = 0.0F;
  full[13] = 0.0F;
  full[14] = 0.0F;
  full[15] = 1.0F;
  out = QMatrix4x4(full.data());
  return true;
}

} // namespace Render::Creature::Bpat
