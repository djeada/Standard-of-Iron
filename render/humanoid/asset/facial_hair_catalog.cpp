#include "render/humanoid/asset/facial_hair_catalog.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "render/humanoid/asset/humanoid_beard_mesh.h"

namespace Render::Humanoid {

namespace {

struct BodyVariantKey {
  Render::Creature::ArchetypeId base_archetype{Render::Creature::k_invalid_archetype};
  HumanoidBodyVariant variant{HumanoidBodyVariant::Clean};

  auto operator==(const BodyVariantKey& other) const noexcept -> bool = default;
};

struct BodyVariantKeyHash {
  auto operator()(const BodyVariantKey& key) const noexcept -> std::size_t {
    return (static_cast<std::size_t>(key.base_archetype) << 8U) ^
           static_cast<std::size_t>(key.variant);
  }
};

auto body_variant_cache() -> std::unordered_map<BodyVariantKey,
                                                Render::Creature::ArchetypeId,
                                                BodyVariantKeyHash>& {
  static std::
      unordered_map<BodyVariantKey, Render::Creature::ArchetypeId, BodyVariantKeyHash>
          cache;
  return cache;
}

auto body_variant_cache_mutex() -> std::mutex& {
  static std::mutex mutex;
  return mutex;
}

} // namespace

auto facial_hair_body_archetype(Render::Creature::ArchetypeId base_archetype,
                                Render::GL::FacialHairStyle style)
    -> Render::Creature::ArchetypeId {
  HumanoidBodyVariant const variant = body_variant_for_facial_hair(style);
  if (variant == HumanoidBodyVariant::Clean) {
    return base_archetype;
  }
  if (base_archetype == Render::Creature::k_invalid_archetype) {
    base_archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  }

  BodyVariantKey const key{base_archetype, variant};
  std::lock_guard<std::mutex> lock(body_variant_cache_mutex());
  if (auto it = body_variant_cache().find(key); it != body_variant_cache().end()) {
    return it->second;
  }

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const* base_desc = registry.get(base_archetype);
  if (base_desc == nullptr ||
      base_desc->body_variant == static_cast<std::uint8_t>(variant)) {
    return base_archetype;
  }
  Render::Creature::ArchetypeDescriptor desc = *base_desc;
  desc.debug_name =
      base_desc->debug_name + std::string(body_variant_name_suffix(variant));
  desc.body_variant = static_cast<std::uint8_t>(variant);
  auto const id = registry.register_archetype(desc);
  if (id == Render::Creature::k_invalid_archetype) {
    return base_archetype;
  }
  body_variant_cache().emplace(key, id);
  return id;
}

auto resolve_facial_hair_archetype(Render::Creature::ArchetypeId base_archetype,
                                   const Render::GL::HumanoidVariant& variant)
    -> Render::Creature::ArchetypeId {
  if (variant.facial_hair.coverage < 0.01F) {
    return base_archetype;
  }
  return facial_hair_body_archetype(base_archetype, variant.facial_hair.style);
}

} // namespace Render::Humanoid
