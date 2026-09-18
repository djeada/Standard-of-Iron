#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <functional>
#include <memory>

#include "app/world/world_feedback.h"

class QImage;

namespace Arena::Promo {

class RpgHudProjectorBridge : public QObject {
  Q_OBJECT

public:
  using Projection = std::function<bool(const QVector3D&, QPointF&)>;

  explicit RpgHudProjectorBridge(QObject* parent = nullptr);

  void set_projection(Projection projection);

  Q_INVOKABLE [[nodiscard]] QVariantMap
  project_world(double x, double y, double z) const;

private:
  Projection m_projection;
};

class RpgHudFeedbackSource : public QObject {
  Q_OBJECT

public:
  explicit RpgHudFeedbackSource(QObject* parent = nullptr);

  void push(const App::Core::WorldFeedbackTick& tick);
  void update(float dt);

  Q_INVOKABLE [[nodiscard]] QVariantList pop_feedback_ticks();

private:
  App::Core::WorldFeedbackStore m_store;
};

class RpgHud {
public:
  RpgHud();
  ~RpgHud();
  RpgHud(const RpgHud&) = delete;
  auto operator=(const RpgHud&) -> RpgHud& = delete;

  static void select_software_scene_graph();

  [[nodiscard]] auto ready() const -> bool;
  [[nodiscard]] auto error() const -> const QString&;

  void set_projection(RpgHudProjectorBridge::Projection projection);

  void push_feedback(const App::Core::WorldFeedbackTick& tick);

  void
  paint(QImage& frame, const QVariantMap& status, double capture_seconds, float dt);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace Arena::Promo
