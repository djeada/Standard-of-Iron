#include "equipment_registry.h"

#include "../../equipment/equipment_submit.h"
#include "../../equipment/horse/i_horse_equipment_renderer.h"
#include "../../equipment/i_equipment_renderer.h"
#include "../../equipment/weapons/bow_renderer.h"
#include "../../equipment/weapons/shield_renderer.h"
#include "../../equipment/weapons/spear_renderer.h"
#include "../../equipment/weapons/sword_renderer.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../skeleton.h"

#include <algorithm>

namespace Render::Creature::Pipeline {

namespace {

void invoke_legacy_equipment(void *user, const LegacyEquipmentSubmitArgs &args,
                             Render::GL::ISubmitter &out) {
  if (user == nullptr || args.ctx == nullptr || args.pose == nullptr ||
      args.variant == nullptr || args.anim == nullptr) {
    return;
  }
  auto *renderer = static_cast<Render::GL::IEquipmentRenderer *>(user);
  Render::GL::EquipmentBatch batch;
  renderer->render(*args.ctx, args.pose->body_frames, args.variant->palette,
                   *args.anim, batch);
  Render::GL::submit_equipment_batch(batch, out);
}

void invoke_legacy_horse_equipment(void *user,
                                   const LegacyEquipmentSubmitArgs &args,
                                   Render::GL::ISubmitter &out) {
  if (user == nullptr || args.ctx == nullptr || args.horse_frames == nullptr ||
      args.horse_variant == nullptr || args.horse_anim == nullptr) {
    return;
  }
  const auto *renderer =
      static_cast<const Render::GL::IHorseEquipmentRenderer *>(user);
  Render::GL::EquipmentBatch batch;
  renderer->render(*args.ctx, *args.horse_frames, *args.horse_variant,
                   *args.horse_anim, batch);
  Render::GL::submit_equipment_batch(batch, out);
}

template <class Renderer, class Config>
void invoke_stateless_weapon(void *user, const LegacyEquipmentSubmitArgs &args,
                             Render::GL::ISubmitter &out) {
  if (user == nullptr || args.ctx == nullptr || args.pose == nullptr ||
      args.variant == nullptr || args.anim == nullptr) {
    return;
  }
  const auto *config = static_cast<const Config *>(user);
  Render::GL::EquipmentBatch batch;
  Renderer::submit(*config, *args.ctx, args.pose->body_frames,
                   args.variant->palette, *args.anim, batch);
  Render::GL::submit_equipment_batch(batch, out);
}

template <class Renderer, class Config>
auto make_stateless_record(const Config &config,
                           Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  EquipmentRecord rec{};
  rec.socket = socket;
  rec.legacy_submit = &invoke_stateless_weapon<Renderer, Config>;

  rec.legacy_user = const_cast<void *>(static_cast<const void *>(&config));
  return rec;
}

} // namespace

auto make_legacy_equipment_record(Render::GL::IEquipmentRenderer &renderer,
                                  Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  EquipmentRecord rec{};
  rec.socket = socket;
  rec.legacy_submit = &invoke_legacy_equipment;
  rec.legacy_user = static_cast<void *>(&renderer);
  return rec;
}

auto make_stateless_sword_record(const Render::GL::SwordRenderConfig &config,
                                 Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  return make_stateless_record<Render::GL::SwordRenderer,
                               Render::GL::SwordRenderConfig>(config, socket);
}

auto make_stateless_shield_record(const Render::GL::ShieldRenderConfig &config,
                                  Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  return make_stateless_record<Render::GL::ShieldRenderer,
                               Render::GL::ShieldRenderConfig>(config, socket);
}

auto make_stateless_spear_record(const Render::GL::SpearRenderConfig &config,
                                 Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  return make_stateless_record<Render::GL::SpearRenderer,
                               Render::GL::SpearRenderConfig>(config, socket);
}

auto make_stateless_bow_record(const Render::GL::BowRenderConfig &config,
                               Render::Creature::SocketIndex socket)
    -> EquipmentRecord {
  return make_stateless_record<Render::GL::BowRenderer,
                               Render::GL::BowRenderConfig>(config, socket);
}

auto make_legacy_horse_equipment_record(
    const Render::GL::IHorseEquipmentRenderer &renderer,
    Render::Creature::SocketIndex socket) -> EquipmentRecord {
  EquipmentRecord rec{};
  rec.socket = socket;
  rec.legacy_submit = &invoke_legacy_horse_equipment;

  rec.legacy_user = const_cast<void *>(static_cast<const void *>(&renderer));
  return rec;
}

auto EquipmentRegistry::instance() -> EquipmentRegistry & {
  static EquipmentRegistry s_instance;
  return s_instance;
}

auto EquipmentRegistry::register_loadout(
    std::string_view key, std::span<const EquipmentRecord> records) -> SpecId {
  const std::string key_str{key};
  if (auto it = m_by_key.find(key_str); it != m_by_key.end()) {
    auto &slot = m_records[it->second];
    slot.assign(records.begin(), records.end());
    return it->second;
  }

  const auto id = static_cast<SpecId>(m_records.size());
  m_records.emplace_back(records.begin(), records.end());
  m_keys.emplace_back(key_str);
  m_by_key.emplace(key_str, id);
  return id;
}

auto EquipmentRegistry::find(std::string_view key) const noexcept -> SpecId {

  const std::string key_str{key};
  if (auto it = m_by_key.find(key_str); it != m_by_key.end()) {
    return it->second;
  }
  return kInvalidSpec;
}

auto EquipmentRegistry::get(SpecId id) const noexcept
    -> std::span<const EquipmentRecord> {
  if (id == kInvalidSpec || id >= m_records.size()) {
    return {};
  }
  return {m_records[id].data(), m_records[id].size()};
}

} // namespace Render::Creature::Pipeline
