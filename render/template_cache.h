#pragma once

#include "gl/humanoid/humanoid_types.h"
#include "scene_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render::GL {

inline constexpr std::uint8_t k_anim_frame_count = 16;
inline constexpr std::uint8_t k_template_variant_count = 8;

class Mesh;
class Texture;
class Shader;
struct AnimationInputs;

enum class AnimState : std::uint8_t {
  Idle = 0,
  Move = 1,
  Run = 2,
  AttackMelee = 3,
  AttackRanged = 4,
  Construct = 5,
  Heal = 6,
  Hit = 7
};

struct AnimKey {
  AnimState state{AnimState::Idle};
  CombatAnimPhase combat_phase{CombatAnimPhase::Idle};
  std::uint8_t frame{0};
  std::uint8_t attack_variant{0};
};

struct TemplateKey {
  std::string renderer_id;
  std::uint32_t owner_id{0};
  std::uint8_t lod{0};
  std::uint8_t mount_lod{0};
  std::uint8_t variant{0};
  std::uint8_t attack_variant{0};
  AnimState state{AnimState::Idle};
  CombatAnimPhase combat_phase{CombatAnimPhase::Idle};
  std::uint8_t frame{0};

  bool operator==(const TemplateKey &other) const {
    return renderer_id == other.renderer_id && owner_id == other.owner_id &&
           lod == other.lod && mount_lod == other.mount_lod &&
           variant == other.variant && attack_variant == other.attack_variant &&
           state == other.state && combat_phase == other.combat_phase &&
           frame == other.frame;
  }
};

struct TemplateKeyHash {
  std::size_t operator()(const TemplateKey &key) const noexcept;
};

struct RecordedMeshCmd {
  Mesh *mesh{nullptr};
  Texture *texture{nullptr};
  Shader *shader{nullptr};
  QMatrix4x4 local_model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
  int material_id{0};
};

struct PoseTemplate {
  std::vector<RecordedMeshCmd> commands;
};

class TemplateRecorder : public Renderer {
public:
  TemplateRecorder() = default;

  void reset(std::size_t reserve_hint = 0);

  auto take_commands() -> std::vector<RecordedMeshCmd> {
    return std::move(m_commands);
  }

  [[nodiscard]] auto commands() const -> const std::vector<RecordedMeshCmd> & {
    return m_commands;
  }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *texture = nullptr, float alpha = 1.0F,
            int material_id = 0) override;
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}

private:
  std::vector<RecordedMeshCmd> m_commands;
};

class TemplateCache {
public:
  static constexpr std::size_t k_default_max_entries = 500'000;
  static constexpr std::size_t k_dense_attack_variant_slots = 8;
  static constexpr std::size_t k_dense_variant_slots = k_template_variant_count;
  static constexpr std::size_t k_dense_anim_state_slots = 305;
  static constexpr std::size_t k_dense_anim_slot_count =
      k_dense_variant_slots * k_dense_attack_variant_slots *
      k_dense_anim_state_slots;

  struct DenseDomainHandle {
    static constexpr std::size_t k_invalid =
        std::numeric_limits<std::size_t>::max();
    std::size_t value{k_invalid};
    [[nodiscard]] auto is_valid() const -> bool { return value != k_invalid; }
  };

  static auto instance() noexcept -> TemplateCache & {
    static TemplateCache inst;
    return inst;
  }

  auto get_or_build(const TemplateKey &key,
                    const std::function<PoseTemplate()> &builder)
      -> const PoseTemplate *;

  auto get_dense_domain_handle(const std::string &renderer_id,
                               std::uint32_t owner_id, std::uint8_t lod,
                               std::uint8_t mount_lod) -> DenseDomainHandle;

  auto get_or_build_dense(
      DenseDomainHandle domain, std::size_t dense_slot, const TemplateKey &key,
      const std::function<PoseTemplate()> &builder) -> const PoseTemplate *;

  static auto dense_slot_index(std::uint8_t variant,
                               const AnimKey &anim_key) -> std::size_t;

  void clear();

  void set_max_entries(std::size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_entries = max;
  }

  [[nodiscard]] auto size() const -> std::size_t {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
  }

private:
  TemplateCache() = default;

  void evict_lru();
  void clear_dense_slot_for_key(const TemplateKey &key);
  auto set_dense_slot(DenseDomainHandle domain, std::size_t dense_slot,
                      const PoseTemplate *tpl) -> void;

  using LruList = std::list<TemplateKey>;
  struct CacheEntry {
    PoseTemplate tpl;
    LruList::iterator lru_it;
  };

  struct DenseDomainKey {
    std::string renderer_id;
    std::uint32_t owner_id{0};
    std::uint8_t lod{0};
    std::uint8_t mount_lod{0};

    bool operator==(const DenseDomainKey &other) const {
      return renderer_id == other.renderer_id && owner_id == other.owner_id &&
             lod == other.lod && mount_lod == other.mount_lod;
    }
  };

  struct DenseDomainKeyHash {
    std::size_t operator()(const DenseDomainKey &key) const noexcept;
  };

  struct DenseDomainEntry {
    DenseDomainKey key;
    std::vector<const PoseTemplate *> template_slots;
  };

  std::unordered_map<TemplateKey, CacheEntry, TemplateKeyHash> m_cache;
  LruList m_lru;
  std::unordered_map<DenseDomainKey, std::size_t, DenseDomainKeyHash>
      m_dense_domain_lookup;
  std::vector<DenseDomainEntry> m_dense_domains;
  std::size_t m_max_entries{k_default_max_entries};
  mutable std::mutex m_mutex;
};

auto make_anim_key(const AnimationInputs &anim, float phase_offset,
                   std::uint8_t attack_variant) -> AnimKey;
auto make_animation_inputs(const AnimKey &key) -> AnimationInputs;

} // namespace Render::GL
