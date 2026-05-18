#include "visibility_coordinator.h"

#include "game/core/world.h"
#include "minimap_manager.h"
#include "render/ground/fog_renderer.h"

void VisibilityCoordinator::FogPresenterAdapter::bind(Render::GL::FogRenderer* fog) {
  m_fog = fog;
}

void VisibilityCoordinator::FogPresenterAdapter::apply_visibility_frame(
    const Game::Map::VisibilityService::Snapshot& snapshot) {
  if (m_fog != nullptr) {
    m_fog->update_mask(
        snapshot.width, snapshot.height, snapshot.tile_size, snapshot.cells);
  }
}

void VisibilityCoordinator::FogPresenterAdapter::clear_visibility_frame() {
  if (m_fog != nullptr) {
    m_fog->update_mask(0, 0, 1.0F, {});
  }
}

void VisibilityCoordinator::MinimapPresenterAdapter::bind(MinimapManager* minimap) {
  m_minimap = minimap;
}

void VisibilityCoordinator::MinimapPresenterAdapter::apply_visibility_frame(
    const Game::Map::VisibilityService::Snapshot& snapshot) {
  if (m_minimap != nullptr) {
    m_minimap->update_fog(snapshot);
  }
}

void VisibilityCoordinator::MinimapPresenterAdapter::clear_visibility_frame() {
  if (m_minimap != nullptr) {
    m_minimap->clear_fog();
  }
}

void VisibilityCoordinator::set_presenters(Render::GL::FogRenderer* fog,
                                           MinimapManager* minimap) {
  m_fog = fog;
  m_fog_presenter_adapter.bind(fog);
  m_minimap_presenter_adapter.bind(minimap);
  set_frame_presenters(fog != nullptr ? &m_fog_presenter_adapter : nullptr,
                       minimap != nullptr ? &m_minimap_presenter_adapter : nullptr);
}

void VisibilityCoordinator::set_frame_presenters(VisibilityFramePresenter* first,
                                                 VisibilityFramePresenter* second) {
  m_presenters[0] = first;
  m_presenters[1] = second;
}

void VisibilityCoordinator::initialize_for_world(Engine::Core::World& world,
                                                 int local_owner_id,
                                                 int width,
                                                 int height,
                                                 float tile_size,
                                                 bool spectator_mode) {
  auto& visibility = Game::Map::VisibilityService::instance();
  visibility.initialize(width, height, tile_size);
  m_update_accumulator = 0.0F;
  m_presenters_hidden = spectator_mode;

  if (m_fog != nullptr) {
    m_fog->set_enabled(!spectator_mode);
  }

  if (spectator_mode) {
    visibility.reveal_all();
    clear_presenters();
    return;
  }

  visibility.compute_immediate(world, local_owner_id);
  publish_current_frame(true);
}

void VisibilityCoordinator::update(Engine::Core::World& world,
                                   int local_owner_id,
                                   float dt,
                                   float update_interval,
                                   bool spectator_mode) {
  auto& visibility = Game::Map::VisibilityService::instance();
  if (!visibility.is_initialized()) {
    clear_presenters();
    return;
  }

  if (spectator_mode) {
    if (!m_presenters_hidden) {
      m_presenters_hidden = true;
      if (m_fog != nullptr) {
        m_fog->set_enabled(false);
      }
      clear_presenters();
    }
    return;
  }

  if (m_presenters_hidden) {
    m_presenters_hidden = false;
    if (m_fog != nullptr) {
      m_fog->set_enabled(true);
    }
    publish_current_frame(true);
  }

  m_update_accumulator += dt;
  if (m_update_accumulator >= update_interval) {
    m_update_accumulator = 0.0F;
    visibility.update(world, local_owner_id);
  }

  publish_current_frame(false);
}

void VisibilityCoordinator::publish_current_frame(bool force) {
  if (m_presenters_hidden) {
    clear_presenters();
    return;
  }
  publish_snapshot(current_snapshot(), force);
}

void VisibilityCoordinator::clear_presenters() {
  for (auto* presenter : m_presenters) {
    if (presenter != nullptr) {
      presenter->clear_visibility_frame();
    }
  }
  m_last_published_version = 0;
}

auto VisibilityCoordinator::current_snapshot() const
    -> Game::Map::VisibilityService::SnapshotPtr {
  return Game::Map::VisibilityService::instance().snapshot_ptr();
}

void VisibilityCoordinator::publish_snapshot(
    const Game::Map::VisibilityService::SnapshotPtr& snapshot, bool force) {
  if (snapshot == nullptr || !snapshot->initialized) {
    clear_presenters();
    return;
  }

  if (!force && snapshot->version == m_last_published_version) {
    return;
  }

  for (auto* presenter : m_presenters) {
    if (presenter != nullptr) {
      presenter->apply_visibility_frame(*snapshot);
    }
  }
  m_last_published_version = snapshot->version;
}
