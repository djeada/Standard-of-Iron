#pragma once

#include "i_equipment_renderer.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render::GL {

class IHorseEquipmentRenderer;

enum class EquipmentCategory {
  Helmet,
  Armor,
  Weapon,
  HorseTack,
  HorseArmor,
  HorseDecoration
};
using EquipmentHandle = std::uint32_t;
inline constexpr EquipmentHandle k_invalid_equipment_handle = 0;

class EquipmentRegistry {
public:
  static auto instance() -> EquipmentRegistry &;

  void register_equipment(EquipmentCategory category, const std::string &id,
                          std::shared_ptr<IEquipmentRenderer> renderer);

  void register_placeholder_equipment(EquipmentCategory category,
                                      const std::string &id);

  void
  register_horse_equipment(EquipmentCategory category, const std::string &id,
                           std::shared_ptr<IHorseEquipmentRenderer> renderer);

  auto get(EquipmentCategory category,
           const std::string &id) const -> std::shared_ptr<IEquipmentRenderer>;

  auto get_horse(EquipmentCategory category, const std::string &id) const
      -> std::shared_ptr<IHorseEquipmentRenderer>;

  auto has(EquipmentCategory category, const std::string &id) const -> bool;

  auto resolve_handle(EquipmentCategory category,
                      const std::string &id) const -> EquipmentHandle;

  auto get(EquipmentHandle handle) const -> std::shared_ptr<IEquipmentRenderer>;

  auto get_horse(EquipmentHandle handle) const
      -> std::shared_ptr<IHorseEquipmentRenderer>;

  auto has(EquipmentHandle handle) const -> bool;

private:
  EquipmentRegistry() = default;
  void register_equipment_entry(EquipmentCategory category,
                                const std::string &id,
                                std::shared_ptr<IEquipmentRenderer> renderer);

  EquipmentRegistry(const EquipmentRegistry &) = delete;
  EquipmentRegistry(EquipmentRegistry &&) = delete;
  auto operator=(const EquipmentRegistry &) -> EquipmentRegistry & = delete;
  auto operator=(EquipmentRegistry &&) -> EquipmentRegistry & = delete;

  std::unordered_map<
      EquipmentCategory,
      std::unordered_map<std::string, std::shared_ptr<IEquipmentRenderer>>>
      m_renderers;

  struct HandleKey {
    EquipmentCategory category{EquipmentCategory::Helmet};
    std::string id{};

    auto operator==(const HandleKey &other) const -> bool {
      return category == other.category && id == other.id;
    }
  };

  struct HandleKeyHash {
    auto operator()(const HandleKey &key) const noexcept -> std::size_t {
      return (static_cast<std::size_t>(key.category) << 24U) ^
             std::hash<std::string>{}(key.id);
    }
  };

  std::unordered_map<HandleKey, EquipmentHandle, HandleKeyHash> m_handles{};
  std::vector<std::shared_ptr<IEquipmentRenderer>> m_renderers_by_handle{};
};

void register_built_in_equipment();

} // namespace Render::GL
