#pragma once

#include <cstdint>

#include "game/map/visibility_service.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FogRenderer;
}

class MinimapManager;

class VisibilityFramePresenter {
public:
  virtual ~VisibilityFramePresenter() = default;
  virtual void
  apply_visibility_frame(const Game::Map::VisibilityService::Snapshot& snapshot) = 0;
  virtual void clear_visibility_frame() = 0;
};

class VisibilityCoordinator {
public:
  class FogPresenterAdapter final : public VisibilityFramePresenter {
  public:
    void bind(Render::GL::FogRenderer* fog);
    void apply_visibility_frame(
        const Game::Map::VisibilityService::Snapshot& snapshot) override;
    void clear_visibility_frame() override;

  private:
    Render::GL::FogRenderer* m_fog = nullptr;
  };

  class MinimapPresenterAdapter final : public VisibilityFramePresenter {
  public:
    void bind(MinimapManager* minimap);
    void apply_visibility_frame(
        const Game::Map::VisibilityService::Snapshot& snapshot) override;
    void clear_visibility_frame() override;

  private:
    MinimapManager* m_minimap = nullptr;
  };

  void set_presenters(Render::GL::FogRenderer* fog, MinimapManager* minimap);
  void set_frame_presenters(VisibilityFramePresenter* first,
                            VisibilityFramePresenter* second);

  void initialize_for_world(Engine::Core::World& world,
                            int local_owner_id,
                            int width,
                            int height,
                            float tile_size,
                            bool spectator_mode);

  void update(Engine::Core::World& world,
              int local_owner_id,
              float dt,
              float update_interval,
              bool spectator_mode);

  void publish_current_frame(bool force = false);
  void clear_presenters();

  [[nodiscard]] auto
  current_snapshot() const -> Game::Map::VisibilityService::SnapshotPtr;
  [[nodiscard]] auto last_published_version() const -> std::uint64_t {
    return m_last_published_version;
  }

private:
  void publish_snapshot(const Game::Map::VisibilityService::SnapshotPtr& snapshot,
                        bool force);

  Render::GL::FogRenderer* m_fog = nullptr;
  FogPresenterAdapter m_fog_presenter_adapter;
  MinimapPresenterAdapter m_minimap_presenter_adapter;
  VisibilityFramePresenter* m_presenters[2]{nullptr, nullptr};
  std::uint64_t m_last_published_version = 0;
  float m_update_accumulator = 0.0F;
  bool m_presenters_hidden = false;
};
