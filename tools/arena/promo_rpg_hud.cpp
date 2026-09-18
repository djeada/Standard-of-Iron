#include "promo_rpg_hud.h"

#include <QAnimationDriver>
#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QMetaObject>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <utility>

static void init_rpg_hud_resources() {
  Q_INIT_RESOURCE(promo_rpg_hud);
  Q_INIT_RESOURCE(design_resources);
}

namespace Arena::Promo {
namespace {

constexpr const char* k_host_url = "qrc:/arena_rpg_hud/PromoRpgHudHost.qml";

const QByteArray k_host_source = QByteArrayLiteral(R"(
import QtQuick 2.15

Item {
    id: host

    property var status: ({})
    property QtObject camera: null
    property QtObject feedback: null

    function advance_projection() {
        projector.tick++;
    }

    WorldProjector {
        id: projector
        anchors.fill: parent
        camera: host.camera
        active: false
    }

    RpgFpvOverlay {
        anchors.fill: parent
        status: host.status
        projector: projector
    }

    FloatingNumbers {
        anchors.fill: parent
        source: host.feedback
        projector: projector
    }
}
)");

class CaptureAnimationDriver : public QAnimationDriver {
public:
  void advance_to(qint64 milliseconds) {

    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    m_elapsed = std::max(m_elapsed, milliseconds);
    advance();
  }

  [[nodiscard]] auto elapsed() const -> qint64 override { return m_elapsed; }

private:
  qint64 m_elapsed = 0;
};

auto grab_with_alpha(QQuickWindow& window) -> QImage {
  window.setColor(Qt::black);
  const QImage over_black = window.grabWindow().convertToFormat(QImage::Format_RGB32);
  window.setColor(Qt::white);
  const QImage over_white = window.grabWindow().convertToFormat(QImage::Format_RGB32);
  if (over_black.isNull() || over_white.isNull() ||
      over_black.size() != over_white.size()) {
    return {};
  }

  QImage result(over_black.size(), QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < result.height(); ++y) {
    const auto* black =
        static_cast<const QRgb*>(static_cast<const void*>(over_black.constScanLine(y)));
    const auto* white =
        static_cast<const QRgb*>(static_cast<const void*>(over_white.constScanLine(y)));
    auto* out = static_cast<QRgb*>(static_cast<void*>(result.scanLine(y)));
    for (int x = 0; x < result.width(); ++x) {
      const int spread = std::max({qRed(white[x]) - qRed(black[x]),
                                   qGreen(white[x]) - qGreen(black[x]),
                                   qBlue(white[x]) - qBlue(black[x])});
      const int alpha = std::clamp(255 - spread, 0, 255);
      out[x] = qRgba(std::min(qRed(black[x]), alpha),
                     std::min(qGreen(black[x]), alpha),
                     std::min(qBlue(black[x]), alpha),
                     alpha);
    }
  }
  return result;
}

} // namespace

RpgHudProjectorBridge::RpgHudProjectorBridge(QObject* parent)
    : QObject(parent) {
}

void RpgHudProjectorBridge::set_projection(Projection projection) {
  m_projection = std::move(projection);
}

auto RpgHudProjectorBridge::project_world(double x,
                                          double y,
                                          double z) const -> QVariantMap {
  QPointF screen;
  const bool valid = m_projection && m_projection(QVector3D(static_cast<float>(x),
                                                            static_cast<float>(y),
                                                            static_cast<float>(z)),
                                                  screen);
  return {{QStringLiteral("valid"), valid},
          {QStringLiteral("x"), screen.x()},
          {QStringLiteral("y"), screen.y()}};
}

RpgHudFeedbackSource::RpgHudFeedbackSource(QObject* parent)
    : QObject(parent) {
}

void RpgHudFeedbackSource::push(const App::Core::WorldFeedbackTick& tick) {
  m_store.push(tick);
}

void RpgHudFeedbackSource::update(float dt) {
  m_store.update(dt);
}

auto RpgHudFeedbackSource::pop_feedback_ticks() -> QVariantList {
  return App::Core::WorldFeedbackStore::to_variant(m_store.pop_ready());
}

struct RpgHud::Impl {
  QQmlEngine engine;
  std::unique_ptr<QQuickWindow> window;
  std::unique_ptr<QQuickItem> host;
  RpgHudProjectorBridge bridge;
  RpgHudFeedbackSource feedback;
  CaptureAnimationDriver animation;
  QString error;
  bool ready = false;
  bool settled = false;
};

void RpgHud::select_software_scene_graph() {
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
}

RpgHud::RpgHud()
    : m_impl(std::make_unique<Impl>()) {
  init_rpg_hud_resources();
  auto& impl = *m_impl;
  impl.animation.install();
  impl.engine.addImportPath(QStringLiteral("qrc:/"));

  impl.window = std::make_unique<QQuickWindow>();

  QQmlComponent component(&impl.engine);
  component.setData(k_host_source, QUrl(QString::fromLatin1(k_host_url)));
  QObject* created = component.create();
  auto* item = qobject_cast<QQuickItem*>(created);
  if (item == nullptr) {
    impl.error = component.errorString().trimmed();
    if (impl.error.isEmpty()) {
      impl.error = QStringLiteral("the RPG HUD host did not create a Quick item");
    }
    delete created;
    return;
  }
  impl.host.reset(item);
  impl.host->setProperty("camera", QVariant::fromValue<QObject*>(&impl.bridge));
  impl.host->setProperty("feedback", QVariant::fromValue<QObject*>(&impl.feedback));
  impl.host->setParentItem(impl.window->contentItem());

  impl.ready = true;
}

RpgHud::~RpgHud() {
  if (m_impl != nullptr) {
    m_impl->host.reset();
    m_impl->window.reset();
    m_impl->animation.uninstall();
  }
}

auto RpgHud::ready() const -> bool {
  return m_impl->ready;
}

auto RpgHud::error() const -> const QString& {
  return m_impl->error;
}

void RpgHud::set_projection(RpgHudProjectorBridge::Projection projection) {
  m_impl->bridge.set_projection(std::move(projection));
}

void RpgHud::push_feedback(const App::Core::WorldFeedbackTick& tick) {
  m_impl->feedback.push(tick);
}

void RpgHud::paint(QImage& frame,
                   const QVariantMap& status,
                   double capture_seconds,
                   float dt) {
  auto& impl = *m_impl;
  if (!impl.ready || frame.isNull() || status.isEmpty()) {
    return;
  }
  impl.feedback.update(dt);

  const QSize size = frame.size();
  if (impl.window->size() != size) {
    impl.window->setGeometry(0, 0, size.width(), size.height());
    impl.window->contentItem()->setSize(size);
    impl.host->setSize(size);
  }

  impl.host->setProperty("status", status);
  QMetaObject::invokeMethod(impl.host.get(), "advance_projection");

  constexpr qint64 k_settle_milliseconds = 3000;
  constexpr qint64 k_settle_step_milliseconds = 50;
  if (!impl.settled) {
    for (qint64 ms = 0; ms <= k_settle_milliseconds; ms += k_settle_step_milliseconds) {
      impl.animation.advance_to(ms);
      (void)impl.window->grabWindow();
    }
    impl.settled = true;
  }
  impl.animation.advance_to(k_settle_milliseconds +
                            std::llround(capture_seconds * 1000.0));

  const QImage hud = grab_with_alpha(*impl.window);
  if (hud.isNull()) {
    return;
  }
  QPainter painter(&frame);
  painter.drawImage(QRect(QPoint(0, 0), size), hud);
}

} // namespace Arena::Promo
