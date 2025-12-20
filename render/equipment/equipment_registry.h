#pragma once

#include "i_equipment_renderer.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Render::GL {

enum class EquipmentCategory { Helmet, Armor, Weapon };

class EquipmentRegistry {
public:
  static auto instance() -> EquipmentRegistry &;

  void register_equipment(EquipmentCategory category, const std::string &id,
                          std::shared_ptr<IEquipmentRenderer> renderer);

  auto get(EquipmentCategory category,
           const std::string &id) const -> std::shared_ptr<IEquipmentRenderer>;

  auto has(EquipmentCategory category, const std::string &id) const -> bool;

private:
  EquipmentRegistry() = default;

  EquipmentRegistry(const EquipmentRegistry &) = delete;
  EquipmentRegistry(EquipmentRegistry &&) = delete;
  auto operator=(const EquipmentRegistry &) -> EquipmentRegistry & = delete;
  auto operator=(EquipmentRegistry &&) -> EquipmentRegistry & = delete;

  std::unordered_map<
      EquipmentCategory,
      std::unordered_map<std::string, std::shared_ptr<IEquipmentRenderer>>>
      m_renderers;
};

void register_built_in_equipment();

} // namespace Render::GL
