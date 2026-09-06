#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "wildlife_species.h"
#include "wildlife_threats.h"

namespace Engine::Core {
class Entity;
class World;
class WildlifeComponent;
class MovementComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Wildlife {

inline constexpr float k_wolf_bite_range = 1.05F;
inline constexpr float k_wolf_bite_windup_range = 0.80F;

struct SpeciesConfig;

enum class NaturePriority : std::uint8_t {
  Ambient = 0,
  Routine = 20,
  Interest = 40,
  Survival = 80,
};

enum class NatureEvent : std::uint8_t {
  Flee = 0,
  Hunt = 1,
};

struct HerdRuntime {
  float center_x{0.0F};
  float center_z{0.0F};
  float alarm{0.0F};
  int alive{0};
};

struct PreyRef {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  float radius{0.0F};
  bool livestock{false};

  [[nodiscard]] auto valid() const noexcept -> bool { return entity != nullptr; }
};

struct PackSlot {
  int index{0};
  int count{1};
};

struct NatureContext {
  Engine::Core::World* world{nullptr};
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::WildlifeComponent* wildlife{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  const SpeciesConfig* config{nullptr};
  const ThreatField* threats{nullptr};
  HerdRuntime herd{};
  float x{0.0F};
  float z{0.0F};
};

class NatureActions {
public:
  NatureActions() = default;
  virtual ~NatureActions() = default;
  NatureActions(const NatureActions&) = delete;
  NatureActions(NatureActions&&) = delete;
  auto operator=(const NatureActions&) -> NatureActions& = delete;
  auto operator=(NatureActions&&) -> NatureActions& = delete;

  virtual void move_to(const NatureContext& ctx, float world_x, float world_z) = 0;

  [[nodiscard]] virtual auto
  bypass_around_obstacle(const NatureContext& ctx,
                         float prey_x,
                         float prey_z,
                         float standoff) -> std::optional<std::pair<float, float>> = 0;
  virtual void halt(const NatureContext& ctx) = 0;
  virtual void face_toward(const NatureContext& ctx, float world_x, float world_z) = 0;
  virtual void set_travel_speed(const NatureContext& ctx, bool urgent) = 0;
  virtual void alert_herd(std::uint16_t group_id, float duration) = 0;
  virtual void mark_hostile(const NatureContext& ctx,
                            Engine::Core::EntityID foe_id,
                            bool rally_pack) = 0;
  virtual void note(NatureEvent event) = 0;

  [[nodiscard]] virtual auto pick_open_point(std::uint32_t& rng,
                                             float origin_x,
                                             float origin_z,
                                             float min_radius,
                                             float max_radius,
                                             float& out_x,
                                             float& out_z) -> bool = 0;
  [[nodiscard]] virtual auto nearest_prey(const NatureContext& ctx,
                                          float radius) -> PreyRef = 0;
  [[nodiscard]] virtual auto nearest_quarry(const NatureContext& ctx,
                                            float radius) -> PreyRef = 0;
  [[nodiscard]] virtual auto locate(Engine::Core::EntityID entity_id) -> PreyRef = 0;
  [[nodiscard]] virtual auto claim_pack_slot(const NatureContext& ctx,
                                             const PreyRef& prey) -> PackSlot = 0;
  [[nodiscard]] virtual auto
  nearest_pack_hunter(float world_x, float world_z, float radius) -> ThreatQuery = 0;
  [[nodiscard]] virtual auto bite(const NatureContext& ctx,
                                  const PreyRef& prey) -> bool = 0;
};

class NatureBehavior {
public:
  NatureBehavior() = default;
  virtual ~NatureBehavior() = default;
  NatureBehavior(const NatureBehavior&) = delete;
  NatureBehavior(NatureBehavior&&) = delete;
  auto operator=(const NatureBehavior&) -> NatureBehavior& = delete;
  auto operator=(NatureBehavior&&) -> NatureBehavior& = delete;

  [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;
  [[nodiscard]] virtual auto priority() const noexcept -> NaturePriority = 0;

  [[nodiscard]] virtual auto try_run(const NatureContext& ctx,
                                     NatureActions& actions) -> bool = 0;
};

class NatureBrain {
public:
  void add(std::unique_ptr<NatureBehavior> behavior);

  auto tick(const NatureContext& ctx, NatureActions& actions) -> std::string_view;

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_behaviors.size(); }

private:
  std::vector<std::unique_ptr<NatureBehavior>> m_behaviors;
};

[[nodiscard]] auto make_sheep_brain() -> NatureBrain;
[[nodiscard]] auto make_wolf_brain() -> NatureBrain;
[[nodiscard]] auto brain_for(Species species) -> NatureBrain;

} // namespace Game::Wildlife
