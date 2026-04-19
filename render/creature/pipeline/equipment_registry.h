

#pragma once

#include "unit_visual_spec.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Render::GL {
class IEquipmentRenderer;
class IHorseEquipmentRenderer;
struct SwordRenderConfig;
struct ShieldRenderConfig;
struct SpearRenderConfig;
struct BowRenderConfig;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

auto make_legacy_equipment_record(
    Render::GL::IEquipmentRenderer &renderer,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

auto make_stateless_sword_record(
    const Render::GL::SwordRenderConfig &config,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

auto make_stateless_shield_record(
    const Render::GL::ShieldRenderConfig &config,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

auto make_stateless_spear_record(
    const Render::GL::SpearRenderConfig &config,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

auto make_stateless_bow_record(
    const Render::GL::BowRenderConfig &config,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

template <class Renderer, class PayloadFn>
auto make_payload_record(
    PayloadFn payload_fn,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord {
  EquipmentRecord rec{};
  rec.socket = socket;
  rec.dispatch =
      [payload_fn = std::move(payload_fn)](const EquipmentSubmitContext &sub,
                                           Render::GL::EquipmentBatch &batch) {
        if (sub.ctx == nullptr || sub.frames == nullptr ||
            sub.palette == nullptr || sub.anim == nullptr) {
          return;
        }
        auto config = payload_fn(sub.seed);
        Renderer::submit(config, *sub.ctx, *sub.frames, *sub.palette, *sub.anim,
                         batch);
      };
  return rec;
}

auto make_legacy_horse_equipment_record(
    const Render::GL::IHorseEquipmentRenderer &renderer,
    Render::Creature::SocketIndex socket = Render::Creature::kInvalidSocket)
    -> EquipmentRecord;

class EquipmentRegistry {
public:
  static auto instance() -> EquipmentRegistry &;

  auto register_loadout(std::string_view key,
                        std::span<const EquipmentRecord> records) -> SpecId;

  [[nodiscard]] auto find(std::string_view key) const noexcept -> SpecId;

  [[nodiscard]] auto
  get(SpecId id) const noexcept -> std::span<const EquipmentRecord>;

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_records.size();
  }

  void clear() noexcept {
    m_records.clear();
    m_keys.clear();
    m_by_key.clear();
  }

private:
  EquipmentRegistry() = default;

  EquipmentRegistry(const EquipmentRegistry &) = delete;
  EquipmentRegistry(EquipmentRegistry &&) = delete;
  auto operator=(const EquipmentRegistry &) -> EquipmentRegistry & = delete;
  auto operator=(EquipmentRegistry &&) -> EquipmentRegistry & = delete;

  std::vector<std::vector<EquipmentRecord>> m_records;
  std::vector<std::string> m_keys;
  std::unordered_map<std::string, SpecId> m_by_key;
};

} // namespace Render::Creature::Pipeline
