#include "campaign_film_overlay.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace CampaignFilm {

namespace {

const QColor k_ink(30, 23, 18);
const QColor k_parchment(244, 234, 212);
const QColor k_gold(219, 178, 92);
const QColor k_sea_ink(44, 74, 98);

auto with_alpha(QColor color, float alpha) -> QColor {
  color.setAlphaF(std::clamp(static_cast<float>(color.alphaF()) * alpha, 0.0F, 1.0F));
  return color;
}

auto make_font(const QString& family,
               float pixel_size,
               float spacing_percent) -> QFont {
  QFont font(family);
  font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixel_size))));
  font.setLetterSpacing(QFont::PercentageSpacing, 100.0 + spacing_percent);
  font.setHintingPreference(QFont::PreferNoHinting);
  font.setStyleStrategy(QFont::PreferAntialias);
  return font;
}

struct TextStyle {
  QFont font;
  QColor fill;
  QColor halo;
  float halo_width = 0.0F;
};

auto text_path(const QString& text, const QFont& font) -> QPainterPath {
  QPainterPath path;
  path.addText(QPointF(0.0, 0.0), font, text);
  return path;
}

void draw_text(QPainter& painter,
               const QString& text,
               const TextStyle& style,
               const QPointF& anchor,
               float align_x,
               float alpha) {
  if (text.isEmpty() || alpha <= 0.0F) {
    return;
  }
  QPainterPath path = text_path(text, style.font);
  const QRectF bounds = path.boundingRect();
  const QFontMetricsF metrics(style.font);
  const QPointF origin(anchor.x() - bounds.width() * align_x - bounds.left(),
                       anchor.y() + metrics.capHeight() * 0.5);
  path.translate(origin);
  if (style.halo_width > 0.0F && style.halo.alpha() > 0) {
    QPen pen(with_alpha(style.halo, alpha), style.halo_width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.strokePath(path, pen);
  }
  painter.fillPath(path, with_alpha(style.fill, alpha));
}

auto text_width(const QString& text, const QFont& font) -> float {
  return static_cast<float>(text_path(text, font).boundingRect().width());
}

void draw_marker_glyph(QPainter& painter,
                       const QPointF& center,
                       const QString& kind,
                       float radius,
                       float alpha,
                       float scale) {
  painter.setPen(Qt::NoPen);
  painter.setBrush(with_alpha(QColor(0, 0, 0, 90), alpha));
  painter.drawEllipse(
      center + QPointF(0.8 * scale, 1.2 * scale), radius * 1.08, radius * 1.08);
  if (kind == QStringLiteral("battle")) {
    painter.setBrush(with_alpha(k_parchment, alpha));
    QPen ring(with_alpha(k_ink, alpha), 1.5F * scale);
    painter.setPen(ring);
    painter.drawEllipse(center, radius, radius);
    QPen blade(with_alpha(QColor(150, 32, 24), alpha), 1.9F * scale);
    blade.setCapStyle(Qt::RoundCap);
    painter.setPen(blade);
    const float arm = radius * 0.52F;
    painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
    painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));
    return;
  }
  painter.setBrush(with_alpha(k_ink, alpha));
  painter.setPen(QPen(with_alpha(k_parchment, alpha), 1.4F * scale));
  painter.drawEllipse(center, radius * 0.5, radius * 0.5);
}

} // namespace

auto format_army_line(const QMap<QString, double>& values) -> QString {
  const QLocale locale(QLocale::English);
  QStringList parts;
  for (auto it = values.begin(); it != values.end(); ++it) {
    parts.push_back(QStringLiteral("%1 %2").arg(
        locale.toString(static_cast<qlonglong>(std::llround(it.value()))), it.key()));
  }
  return parts.join(QStringLiteral(", "));
}

void paint_overlay(QImage& image,
                   const Spec& spec,
                   const FrameEval& eval,
                   const Projector& project,
                   const OverlayFonts& fonts,
                   const OverlayOptions& options) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  const float s = options.pixel_scale;
  const float width = static_cast<float>(image.width());
  const float height = static_cast<float>(image.height());

  if (options.draw_text) {
    for (const auto& value : eval.labels) {
      if (value.alpha <= 0.0F) {
        continue;
      }
      const auto& label = spec.labels[value.index];
      const auto where = project(value.uv);
      if (!where) {
        continue;
      }
      TextStyle style;
      QString text = label.text;
      if (label.style == QStringLiteral("region")) {
        style.font = make_font(
            fonts.display, (label.size_px > 0 ? label.size_px : 30.0F) * s, 32.0F);
        style.fill = with_alpha(k_ink, 0.62F);
        style.halo = with_alpha(k_parchment, 0.35F);
        style.halo_width = 3.0F * s;
        text = text.toUpper();
      } else if (label.style == QStringLiteral("sea")) {
        style.font = make_font(
            fonts.text, (label.size_px > 0 ? label.size_px : 24.0F) * s, 38.0F);
        style.fill = with_alpha(k_sea_ink, 0.80F);
      } else {
        style.font = make_font(
            fonts.display, (label.size_px > 0 ? label.size_px : 18.0F) * s, 12.0F);
        style.fill = k_ink;
        style.halo = with_alpha(k_parchment, 0.85F);
        style.halo_width = 4.0F * s;
        text = text.toUpper();
      }
      const QPointF anchor =
          *where + QPointF(label.offset_px.x() * s, label.offset_px.y() * s);
      draw_text(painter, text, style, anchor, 0.5F, value.alpha);
    }
  }

  for (const auto& value : eval.markers) {
    if (value.alpha <= 0.0F) {
      continue;
    }
    const auto& marker = spec.markers[value.index];
    const auto where = project(marker.uv);
    if (!where) {
      continue;
    }
    const float radius =
        (marker.kind == QStringLiteral("battle") ? 8.5F : 6.0F) * s * value.pop;
    draw_marker_glyph(painter, *where, marker.kind, radius, value.alpha, s);
    if (!options.draw_text) {
      continue;
    }
    TextStyle name_style;
    name_style.font = make_font(fonts.display, 19.0F * s, 12.0F);
    name_style.fill = k_ink;
    name_style.halo = with_alpha(k_parchment, 0.88F);
    name_style.halo_width = 4.5F * s;
    TextStyle date_style;
    date_style.font = make_font(fonts.text, 15.0F * s, 4.0F);
    date_style.fill = with_alpha(k_ink, 0.85F);
    date_style.halo = with_alpha(k_parchment, 0.85F);
    date_style.halo_width = 3.5F * s;

    const QString name = marker.show_name ? marker.name.toUpper() : QString();
    const QString date = marker.show_date ? marker.date : QString();
    const float gap = radius + 8.0F * s;
    const float name_w = text_width(name, name_style.font);
    const float date_w = text_width(date, date_style.font);
    const float block_w = std::max(name_w, date_w);
    const bool left = where->x() + gap + block_w > width - 24.0F * s;
    const float align = left ? 1.0F : 0.0F;
    const float x = static_cast<float>(where->x()) + (left ? -gap : gap);
    const bool two_lines = !name.isEmpty() && !date.isEmpty();
    const float name_y = static_cast<float>(where->y()) - (two_lines ? 9.0F * s : 0.0F);
    const float date_y =
        static_cast<float>(where->y()) + (two_lines ? 11.0F * s : 0.0F);
    draw_text(painter, name, name_style, QPointF(x, name_y), align, value.text_alpha);
    draw_text(painter, date, date_style, QPointF(x, date_y), align, value.text_alpha);
  }

  if (options.draw_text) {
    for (const auto& value : eval.stamps) {
      if (value.alpha <= 0.0F) {
        continue;
      }
      const auto& stamp = spec.stamps[value.index];
      const float left = width * 0.06F;
      const float base = height * 0.88F;
      TextStyle shadow;
      shadow.font = make_font(fonts.display, 36.0F * s, 14.0F);
      shadow.fill = QColor(0, 0, 0, 120);
      TextStyle title = shadow;
      title.fill = k_parchment;
      draw_text(painter,
                stamp.title.toUpper(),
                shadow,
                QPointF(left + 1.5F * s, base - 34.0F * s + 2.0F * s),
                0.0F,
                value.alpha);
      draw_text(painter,
                stamp.title.toUpper(),
                title,
                QPointF(left, base - 34.0F * s),
                0.0F,
                value.alpha);
      const float rule_w =
          std::max(60.0F * s, text_width(stamp.title.toUpper(), title.font) * 0.45F);
      painter.setPen(Qt::NoPen);
      painter.setBrush(with_alpha(k_gold, value.alpha));
      painter.drawRect(QRectF(left, base - 12.0F * s, rule_w * value.alpha, 2.0F * s));
      TextStyle sub_shadow;
      sub_shadow.font = make_font(fonts.text, 24.0F * s, 6.0F);
      sub_shadow.fill = QColor(0, 0, 0, 120);
      TextStyle sub = sub_shadow;
      sub.fill = with_alpha(k_parchment, 0.92F);
      draw_text(painter,
                stamp.subtitle,
                sub_shadow,
                QPointF(left + 1.5F * s, base + 14.0F * s + 2.0F * s),
                0.0F,
                value.alpha);
      draw_text(painter,
                stamp.subtitle,
                sub,
                QPointF(left, base + 14.0F * s),
                0.0F,
                value.alpha);
    }
  }

  if (options.draw_armies) {
    for (std::size_t i = 0; i < eval.armies.size(); ++i) {
      const auto& value = eval.armies[i];
      if (value.alpha <= 0.0F) {
        continue;
      }
      const auto where = project(value.uv);
      if (!where) {
        continue;
      }
      TextStyle style;
      style.font = make_font(fonts.text, 16.0F * s, 2.0F);
      style.fill = QColor(255, 80, 200);
      style.halo = QColor(0, 0, 0, 200);
      style.halo_width = 3.0F * s;
      const QString line = QStringLiteral("[%1] %2").arg(
          spec.armies[i].label, format_army_line(value.values));
      draw_text(
          painter, line, style, *where + QPointF(0.0, 34.0 * s), 0.5F, value.alpha);
    }
  }
}

} // namespace CampaignFilm
