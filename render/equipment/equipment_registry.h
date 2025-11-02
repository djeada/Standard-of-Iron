#pragma once

#include "i_equipment_renderer.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Render::GL {

/**
 * @brief Equipment category types
 */
enum class EquipmentCategory { Helmet, Armor, Weapon };

/**
 * @brief Singleton registry for equipment renderers
 *
 * The registry manages equipment renderers organized by category (helmets,
 * armor, weapons). Equipment pieces are registered with unique IDs and can be
 * retrieved for rendering on humanoid units.
 */
class EquipmentRegistry {
public:
  /**
   * @brief Get the singleton instance
   */
  static auto instance() -> EquipmentRegistry &;

  /**
   * @brief Register an equipment renderer
   *
   * @param category Equipment category (helmet, armor, weapon)
   * @param id Unique identifier for this equipment piece
   * @param renderer Shared pointer to the equipment renderer
   */
  void registerEquipment(EquipmentCategory category, const std::string &id,
                         std::shared_ptr<IEquipmentRenderer> renderer);

  /**
   * @brief Get an equipment renderer by category and ID
   *
   * @param category Equipment category
   * @param id Equipment identifier
   * @return Shared pointer to the renderer, or nullptr if not found
   */
  auto get(EquipmentCategory category,
           const std::string &id) const -> std::shared_ptr<IEquipmentRenderer>;

  /**
   * @brief Check if an equipment piece exists
   *
   * @param category Equipment category
   * @param id Equipment identifier
   * @return true if the equipment is registered, false otherwise
   */
  auto has(EquipmentCategory category, const std::string &id) const -> bool;

private:
  // Private constructor for singleton
  EquipmentRegistry() = default;

  // Delete copy/move constructors and assignment operators
  EquipmentRegistry(const EquipmentRegistry &) = delete;
  EquipmentRegistry(EquipmentRegistry &&) = delete;
  auto operator=(const EquipmentRegistry &) -> EquipmentRegistry & = delete;
  auto operator=(EquipmentRegistry &&) -> EquipmentRegistry & = delete;

  // Storage: category -> (id -> renderer)
  std::unordered_map<EquipmentCategory,
                     std::unordered_map<std::string,
                                        std::shared_ptr<IEquipmentRenderer>>>
      m_renderers;
};

} // namespace Render::GL
