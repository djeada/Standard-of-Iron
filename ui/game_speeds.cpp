#include "game_speeds.h"

#include "app/core/game_speed.h"

namespace {

namespace Speed = App::Core::GameSpeed;

}

GameSpeeds* GameSpeeds::instance() {
  static GameSpeeds speeds;
  return &speeds;
}

GameSpeeds* GameSpeeds::create(QQmlEngine* engine, QJSEngine* script_engine) {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)
  auto* speeds = instance();
  QQmlEngine::setObjectOwnership(speeds, QQmlEngine::CppOwnership);
  return speeds;
}

QVariantList GameSpeeds::options() {
  QVariantList list;
  list.reserve(static_cast<int>(Speed::k_options.size()));
  for (const float option : Speed::k_options) {
    list.append(static_cast<qreal>(option));
  }
  return list;
}

qreal GameSpeeds::minimum() {
  return static_cast<qreal>(Speed::k_min);
}

qreal GameSpeeds::maximum() {
  return static_cast<qreal>(Speed::k_max);
}

qreal GameSpeeds::sanitized(qreal speed) {
  return static_cast<qreal>(Speed::sanitize(static_cast<float>(speed)));
}

qreal GameSpeeds::stepped(qreal speed, int direction) {
  return static_cast<qreal>(Speed::stepped(static_cast<float>(speed), direction));
}

int GameSpeeds::index_of(qreal speed) {
  return static_cast<int>(
      Speed::nearest_index(Speed::sanitize(static_cast<float>(speed))));
}

QString GameSpeeds::label(qreal speed) {
  const qreal rounded = qRound(speed * 10.0) / 10.0;
  const auto whole = static_cast<int>(qRound(rounded));
  const QString number = qFuzzyCompare(rounded, static_cast<qreal>(whole))
                             ? QString::number(whole)
                             : QString::number(rounded, 'f', 1);
  return number + QStringLiteral("×");
}

QStringList GameSpeeds::labels() {
  QStringList list;
  list.reserve(static_cast<int>(Speed::k_options.size()));
  for (const float option : Speed::k_options) {
    list.append(label(static_cast<qreal>(option)));
  }
  return list;
}
