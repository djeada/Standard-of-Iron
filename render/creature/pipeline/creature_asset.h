#pragma once

#include "../../creature/spec.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace Render::Creature::Pipeline {

inline constexpr std::size_t kMaxCreatureBones = 24;

using BindPaletteFn = std::span<const QMatrix4x4> (*)() noexcept;
using FillRoleColorsFn = std::uint32_t (*)(const void *variant, QVector3D *out,
                                           std::size_t max_roles);

struct CreatureAsset {
  CreatureAssetId id{kInvalidCreatureAsset};
  std::string_view debug_name{};
  CreatureKind kind{CreatureKind::Humanoid};
  const Render::Creature::CreatureSpec *spec{nullptr};
  const Render::Creature::SkeletonTopology *topology{nullptr};
  std::uint8_t role_count{0};
  std::uint8_t max_bones{0};
  BindPaletteFn bind_palette{nullptr};
  FillRoleColorsFn fill_role_colors{nullptr};
};

class CreatureAssetRegistry {
public:
  [[nodiscard]] static auto
  instance() noexcept -> const CreatureAssetRegistry &;

  [[nodiscard]] auto
  get(CreatureAssetId id) const noexcept -> const CreatureAsset *;
  [[nodiscard]] auto
  resolve(const UnitVisualSpec &spec) const noexcept -> const CreatureAsset *;

  [[nodiscard]] auto
  for_species(CreatureKind kind) const noexcept -> const CreatureAsset *;

private:
  CreatureAssetRegistry();

  CreatureAsset m_humanoid{};
  CreatureAsset m_horse{};
  CreatureAsset m_elephant{};
};

} // namespace Render::Creature::Pipeline
