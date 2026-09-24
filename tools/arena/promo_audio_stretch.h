#pragma once

#include <QString>
#include <QStringList>

namespace Arena::Promo {

[[nodiscard]] inline auto audio_stretch_filter(float audio_seconds,
                                               float video_seconds) -> QString {
  if (audio_seconds <= 0.0F || video_seconds <= 0.0F ||
      audio_seconds >= video_seconds * 0.99F) {
    return {};
  }

  float tempo = audio_seconds / video_seconds;
  QStringList stages;
  while (tempo < 0.5F) {
    stages << QStringLiteral("atempo=0.5");
    tempo *= 2.0F;
  }
  stages << QStringLiteral("atempo=%1").arg(QString::number(tempo, 'f', 5));
  return stages.join(QLatin1Char(','));
}

} // namespace Arena::Promo
