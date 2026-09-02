#include "arena_feedback.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPen>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <cmath>

#include "app/world/world_feedback_events.h"
#include "game/core/world.h"

namespace {

using App::Core::FeedbackKind;
using App::Core::WorldFeedbackTick;

constexpr float k_tick_life_seconds = 0.85F;
constexpr float k_killing_blow_life_seconds = 1.05F;

constexpr float k_fade_starts_at = 0.55F;

auto accent_for(const WorldFeedbackTick& tick) -> QColor {
  switch (tick.kind) {
  case FeedbackKind::Resource:
    return tick.amount < 0 ? QColor(QStringLiteral("#e4a05d"))
                           : QColor(QStringLiteral("#8fe0a8"));
  case FeedbackKind::Reserve:
    return QColor(QStringLiteral("#9db8ff"));
  case FeedbackKind::Heal:
    return QColor(QStringLiteral("#8fe0a8"));
  case FeedbackKind::Damage:
  case FeedbackKind::Count:
    break;
  }
  if (tick.killing_blow) {
    return tick.incoming ? QColor(QStringLiteral("#ff8a6a"))
                         : QColor(QStringLiteral("#ffe08a"));
  }
  return tick.incoming ? QColor(QStringLiteral("#ff6b5a"))
                       : QColor(QStringLiteral("#ffd36b"));
}

auto resource_glyph(int resource) -> QString {
  switch (resource) {
  case 0:
    return QStringLiteral("●");
  case 1:
    return QStringLiteral("❀");
  case 2:
    return QStringLiteral("║");
  case 3:
    return QStringLiteral("▰");
  case 4:
    return QStringLiteral("◆");
  default:
    break;
  }
  return {};
}

auto signed_text(int amount) -> QString {
  return (amount > 0 ? QStringLiteral("+") : QStringLiteral("")) +
         QString::number(amount);
}

auto body_for(const WorldFeedbackTick& tick) -> QString {
  if (tick.kind == FeedbackKind::Resource || tick.kind == FeedbackKind::Reserve) {
    return signed_text(tick.amount);
  }
  const int magnitude = std::abs(tick.amount);
  if (tick.kind == FeedbackKind::Heal) {
    return QStringLiteral("+") + QString::number(magnitude);
  }
  return (tick.incoming ? QStringLiteral("-") : QStringLiteral("")) +
         QString::number(magnitude);
}

auto label_for(const WorldFeedbackTick& tick) -> QString {
  QString text;
  if (tick.killing_blow) {
    text += QStringLiteral("☠ ");
  }
  if (tick.resource >= 0) {
    text += resource_glyph(tick.resource);
  }
  text += body_for(tick);
  if (tick.kind == FeedbackKind::Reserve) {
    text += QStringLiteral(" POP");
  }
  if (tick.paired_resource >= 0) {
    text += QStringLiteral(" → ") + resource_glyph(tick.paired_resource) +
            signed_text(tick.paired_amount);
  }
  if (tick.hits > 1 && tick.kind == FeedbackKind::Damage) {
    text += QStringLiteral(" ×") + QString::number(tick.hits);
  }
  return text;
}

auto out_cubic(float t) -> float {
  const float inverted = 1.0F - t;
  return 1.0F - (inverted * inverted * inverted);
}

} // namespace

ArenaFeedback::ArenaFeedback() {
  m_hit_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& event) {
            if (m_world == nullptr) {
              return;
            }
            auto tick = App::Core::combat_feedback_tick(
                *m_world, event, App::Core::FeedbackStyle::Tick);
            if (!tick.has_value()) {
              return;
            }

            tick->incoming = true;
            m_store.push(*tick);
          });

  m_world_feedback_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::WorldFeedbackEvent>(
          [this](const Engine::Core::WorldFeedbackEvent& event) {
            if (m_world == nullptr) {
              return;
            }
            if (auto tick = App::Core::world_feedback_tick(*m_world, event);
                tick.has_value()) {
              m_store.push(*tick);
            }
          });
}

void ArenaFeedback::collect_ready() {
  for (auto& tick : m_store.pop_ready()) {
    const float life =
        tick.killing_blow ? k_killing_blow_life_seconds : k_tick_life_seconds;
    m_floaters.push_back(Floater{.tick = tick, .age = 0.0F, .life = life});
  }

  constexpr std::size_t k_max_floaters = 48;
  if (m_floaters.size() > k_max_floaters) {
    m_floaters.erase(m_floaters.begin(),
                     m_floaters.begin() + static_cast<std::ptrdiff_t>(
                                              m_floaters.size() - k_max_floaters));
  }
}

void ArenaFeedback::advance(float dt) {
  if (dt <= 0.0F) {
    return;
  }
  m_store.update(dt);
  collect_ready();

  for (auto& floater : m_floaters) {
    floater.age += dt;
  }
  std::erase_if(m_floaters,
                [](const Floater& floater) { return floater.age >= floater.life; });
}

void ArenaFeedback::clear() {
  m_store.clear();
  m_floaters.clear();
}

void ArenaFeedback::draw(QPainter& painter,
                         const Projector& project,
                         float ui_scale) const {
  if (m_floaters.empty() || !project) {
    return;
  }

  const qreal ui = std::clamp(static_cast<qreal>(ui_scale), 0.5, 4.0);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& floater : m_floaters) {
    QPointF anchor;
    if (!project(floater.tick.x, floater.tick.y, floater.tick.z, anchor)) {
      continue;
    }

    const float progress =
        out_cubic(std::clamp(floater.age / floater.life, 0.0F, 1.0F));
    const float weight = std::clamp(floater.tick.severity * 1.4F, 0.0F, 1.0F);
    const float rise =
        34.0F + (weight * 22.0F) + (floater.tick.killing_blow ? 12.0F : 0.0F);
    const float drift = static_cast<float>(floater.tick.lane) * 9.0F;
    const float opacity =
        progress < k_fade_starts_at
            ? 1.0F
            : std::max(0.0F, 1.0F - ((progress - k_fade_starts_at) / 0.45F));

    QFont font = painter.font();
    font.setPixelSize(std::max(
        8,
        static_cast<int>(std::lround(
            (14.0 + (floater.tick.killing_blow ? 4.0 : 0.0) + (weight * 3.0)) * ui))));
    font.setBold(true);
    painter.setFont(font);

    const QString text = label_for(floater.tick);
    const QFontMetrics metrics(font);
    const QRectF text_bounds = metrics.boundingRect(text);
    const qreal pill_width = text_bounds.width() + (14.0 * ui);
    const qreal pill_height = text_bounds.height() + (6.0 * ui);

    const QPointF centre(anchor.x() + (static_cast<qreal>(drift * progress) * ui),
                         anchor.y() - (18.0 * ui) -
                             (static_cast<qreal>(rise * progress) * ui));
    const QRectF pill(centre.x() - (pill_width * 0.5),
                      centre.y() - (pill_height * 0.5),
                      pill_width,
                      pill_height);

    const QColor accent = accent_for(floater.tick);
    painter.setOpacity(static_cast<qreal>(opacity));

    QColor fill =
        floater.tick.killing_blow ? QColor(0x20, 0x0A, 0x04) : QColor(0x10, 0x0C, 0x08);
    fill.setAlpha(floater.tick.killing_blow ? 224 : 200);
    painter.setBrush(fill);
    painter.setPen(QPen(accent, (floater.tick.killing_blow ? 2.0 : 1.0) * ui));
    painter.drawRoundedRect(pill, pill_height * 0.5, pill_height * 0.5);

    const QColor body =
        (floater.tick.incoming && floater.tick.kind == FeedbackKind::Damage)
            ? QColor(QStringLiteral("#ffe6df"))
            : QColor(QStringLiteral("#fff6d6"));
    painter.setPen(accent);
    painter.drawText(pill.translated(0.0, 1.0 * ui), Qt::AlignCenter, text);
    painter.setPen(body);
    painter.drawText(pill, Qt::AlignCenter, text);
  }

  painter.setOpacity(1.0);
  painter.restore();
}
