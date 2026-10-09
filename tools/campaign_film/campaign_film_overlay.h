#pragma once

#include <QImage>
#include <QPointF>
#include <QString>
#include <QVector2D>

#include <functional>
#include <optional>

#include "campaign_film_spec.h"

namespace CampaignFilm {

using Projector = std::function<std::optional<QPointF>(const QVector2D& uv)>;

struct OverlayFonts {
  QString display;
  QString text;
};

struct OverlayOptions {
  float pixel_scale = 1.0F;
  bool draw_text = true;
  bool draw_armies = false;
};

void paint_overlay(QImage& image,
                   const Spec& spec,
                   const FrameEval& eval,
                   const Projector& project,
                   const OverlayFonts& fonts,
                   const OverlayOptions& options);

[[nodiscard]] auto format_army_line(const QMap<QString, double>& values) -> QString;

} // namespace CampaignFilm
