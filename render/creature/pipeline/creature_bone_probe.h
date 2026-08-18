#pragma once

#include <QMatrix4x4>

#include <cstdint>

namespace Render::Creature::Pipeline {

struct BoneProbe {

  std::uint32_t entity_id{0};
  std::uint16_t instance_index{0};

  std::uint16_t bone_index{0};

  bool resolved{false};
  QMatrix4x4 world{};
};

void set_active_bone_probe(BoneProbe* probe) noexcept;

[[nodiscard]] auto active_bone_probe() noexcept -> BoneProbe*;

class ScopedBoneProbe {
public:
  explicit ScopedBoneProbe(BoneProbe* probe) noexcept
      : m_previous(active_bone_probe()) {
    set_active_bone_probe(probe);
  }

  ~ScopedBoneProbe() { set_active_bone_probe(m_previous); }

  ScopedBoneProbe(const ScopedBoneProbe&) = delete;
  auto operator=(const ScopedBoneProbe&) -> ScopedBoneProbe& = delete;
  ScopedBoneProbe(ScopedBoneProbe&&) = delete;
  auto operator=(ScopedBoneProbe&&) -> ScopedBoneProbe& = delete;

private:
  BoneProbe* m_previous{nullptr};
};

} // namespace Render::Creature::Pipeline
