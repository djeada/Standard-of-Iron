#include "promo_casting_overlay.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>

#include <algorithm>
#include <cmath>

#include "arena_typography.h"

namespace Arena::Promo {
namespace {

constexpr double k_strip_height_fraction = 0.082;

auto to_color(const QVector3D& rgb, int alpha = 255) -> QColor {
  const auto channel = [](float value) {
    return std::clamp(static_cast<int>(std::lround(value * 255.0F)), 0, 255);
  };
  return QColor(channel(rgb.x()), channel(rgb.y()), channel(rgb.z()), alpha);
}

auto display_name(QString label) -> QString {
  return label.replace(QLatin1Char('_'), QLatin1Char(' ')).toUpper();
}

auto clock_text(float seconds) -> QString {
  const int total = std::max(0, static_cast<int>(std::lround(seconds)));
  return QStringLiteral("%1:%2")
      .arg(total / 60)
      .arg(total % 60, 2, 10, QLatin1Char('0'));
}

auto strip_font(double ui, double pixels, bool bold = false) -> QFont {
  QFont font = Arena::Typography::small_label(1.0);
  font.setPixelSize(std::max(6, static_cast<int>(std::lround(pixels * ui))));
  font.setBold(bold);
  return font;
}

auto state_word(const ArenaBattleSideResult& census) -> QString {
  if (census.eliminated_at >= 0.0F) {
    return QStringLiteral("DESTROYED");
  }
  if (census.wave_committed && census.wave_size > 0) {
    return QStringLiteral("WAVE OF %1 MARCHING").arg(census.wave_size);
  }
  return census.ai_state.toUpper();
}

} // namespace

auto casting_overlay_height(int frame_height) -> int {
  return std::max(
      24, static_cast<int>(std::lround(frame_height * k_strip_height_fraction)));
}

void paint_casting_overlay(QImage& frame, const ArenaCastingSnapshot& snapshot) {
  if (frame.isNull() || !snapshot.valid || snapshot.sides.size() < 2U) {
    return;
  }
  if (frame.format() != QImage::Format_ARGB32 &&
      frame.format() != QImage::Format_RGB32) {
    frame = frame.convertToFormat(QImage::Format_ARGB32);
  }

  const double width = frame.width();
  const double height = frame.height();
  const double ui = std::clamp(height / 1080.0, 0.4, 2.4);
  const double strip = casting_overlay_height(frame.height());

  const QColor ink(238, 232, 218);
  const QColor faded(190, 182, 164);
  const QColor gold(214, 186, 116);

  QPainter painter(&frame);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  QLinearGradient shade(0.0, 0.0, 0.0, strip * 1.35);
  shade.setColorAt(0.0, QColor(10, 9, 8, 228));
  shade.setColorAt(0.78, QColor(10, 9, 8, 210));
  shade.setColorAt(1.0, QColor(10, 9, 8, 0));
  painter.fillRect(QRectF(0.0, 0.0, width, strip * 1.35), shade);

  const auto& left = snapshot.sides[0];
  const auto& right = snapshot.sides[1];
  const QColor left_color = to_color(left.color);
  const QColor right_color = to_color(right.color);

  const double bar_height = std::max(2.0, 5.0 * ui);
  painter.fillRect(QRectF(0.0, 0.0, width * 0.5, bar_height), left_color);
  painter.fillRect(QRectF(width * 0.5, 0.0, width * 0.5, bar_height), right_color);

  const double margin = 22.0 * ui;
  const double name_pixels = 26.0;
  const double figure_pixels = 16.0;
  const double name_y = bar_height + 6.0 * ui;
  const double figures_y = name_y + name_pixels * ui * 1.15;
  const double centre_width = std::min(width * 0.26, 340.0 * ui);
  const double side_width = (width - centre_width) * 0.5 - margin;

  const auto paint_side =
      [&](const ArenaCastingSide& side, const QColor& color, bool on_left) {
        const double x0 = on_left ? margin : width - margin - side_width;
        const Qt::Alignment align = on_left ? Qt::AlignLeft : Qt::AlignRight;
        const QRectF name_rect(x0, name_y, side_width, name_pixels * ui * 1.2);
        painter.setFont(strip_font(ui, name_pixels, true));
        painter.setPen(color);
        painter.drawText(
            name_rect, align | Qt::AlignVCenter, display_name(side.census.label));

        const QString figures =
            QStringLiteral("ARMY %1 · %2 MEN    TOWN %3    GOLD %4  FOOD %5  WOOD %6")
                .arg(side.census.living_units)
                .arg(side.census.living_soldiers)
                .arg(side.census.living_buildings)
                .arg(side.gold)
                .arg(side.food)
                .arg(side.wood);
        const QRectF figures_rect(x0, figures_y, side_width, figure_pixels * ui * 1.3);
        painter.setFont(strip_font(ui, figure_pixels));
        painter.setPen(ink);
        painter.drawText(figures_rect, align | Qt::AlignVCenter, figures);

        const QString doctrine = QStringLiteral("%1 · %2   %3")
                                     .arg(side.census.strategy.toUpper(),
                                          side.census.posture.toUpper(),
                                          state_word(side.census));
        const QRectF doctrine_rect(x0,
                                   figures_y + figure_pixels * ui * 1.25,
                                   side_width,
                                   figure_pixels * ui * 1.2);
        painter.setFont(strip_font(ui, figure_pixels * 0.82));
        painter.setPen(faded);
        painter.drawText(doctrine_rect, align | Qt::AlignVCenter, doctrine);
      };
  paint_side(left, left_color, true);
  paint_side(right, right_color, false);

  const QRectF clock_rect((width - centre_width) * 0.5,
                          name_y - 2.0 * ui,
                          centre_width,
                          name_pixels * ui * 1.3);
  painter.setFont(strip_font(ui, name_pixels * 1.15, true));
  painter.setPen(snapshot.decided ? gold : ink);
  painter.drawText(clock_rect, Qt::AlignCenter, clock_text(snapshot.elapsed_seconds));

  const double bar_width = centre_width * 0.86;
  const double bar_x = (width - bar_width) * 0.5;
  const double bar_y = figures_y + 4.0 * ui;
  const double bar_h = std::max(3.0, 8.0 * ui);
  const int left_men = std::max(0, left.census.living_soldiers);
  const int right_men = std::max(0, right.census.living_soldiers);
  const int total_men = left_men + right_men;
  const double left_share =
      total_men > 0 ? static_cast<double>(left_men) / static_cast<double>(total_men)
                    : 0.5;
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(40, 38, 34, 220));
  painter.drawRoundedRect(
      QRectF(bar_x, bar_y, bar_width, bar_h), bar_h * 0.5, bar_h * 0.5);
  QPainterPath clip;
  clip.addRoundedRect(QRectF(bar_x, bar_y, bar_width, bar_h), bar_h * 0.5, bar_h * 0.5);
  painter.save();
  painter.setClipPath(clip);
  painter.fillRect(QRectF(bar_x, bar_y, bar_width * left_share, bar_h), left_color);
  painter.fillRect(
      QRectF(
          bar_x + bar_width * left_share, bar_y, bar_width * (1.0 - left_share), bar_h),
      right_color);
  painter.restore();

  painter.setFont(strip_font(ui, figure_pixels * 0.8));
  painter.setPen(faded);
  painter.drawText(
      QRectF(bar_x, bar_y + bar_h + 2.0 * ui, bar_width, figure_pixels * ui * 1.2),
      Qt::AlignCenter,
      QStringLiteral("%1 · SOLDIERS IN THE FIELD · %2").arg(left_men).arg(right_men));
}

} // namespace Arena::Promo
