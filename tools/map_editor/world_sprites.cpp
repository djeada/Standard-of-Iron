#include "world_sprites.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QRectF>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace MapEditor {

namespace {

constexpr float k_shadow_x = 0.34F;
constexpr float k_shadow_y = 0.44F;
const QColor k_shadow(6, 8, 10, 92);
const QColor k_roof_lit(196, 108, 72);
const QColor k_roof_shade(146, 72, 50);
const QColor k_roof_ridge(96, 46, 34);
const QColor k_plaster(226, 212, 184);
const QColor k_stone(176, 168, 150);
const QColor k_stone_lit(212, 204, 186);
const QColor k_stone_dark(118, 110, 98);
const QColor k_timber(112, 82, 54);
const QColor k_timber_lit(156, 120, 82);
const QColor k_canvas_lit(236, 222, 190);
const QColor k_canvas_shade(190, 172, 138);

auto unit_random(unsigned seed, unsigned salt) -> float {
  unsigned h = seed * 374761393U + salt * 668265263U + 2246822519U;
  h = (h ^ (h >> 13U)) * 1274126177U;
  h ^= h >> 16U;
  return static_cast<float>(h & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

struct Local {
  QPainter& painter;
  float rotation_deg;

  [[nodiscard]] auto shadow_offset(float height) const -> QPointF {
    const float radians = -rotation_deg * std::numbers::pi_v<float> / 180.0F;
    const float sx = k_shadow_x * height;
    const float sy = k_shadow_y * height;
    return {sx * std::cos(radians) - sy * std::sin(radians),
            sx * std::sin(radians) + sy * std::cos(radians)};
  }

  [[nodiscard]] auto lit_from_top() const -> bool {
    const float radians = rotation_deg * std::numbers::pi_v<float> / 180.0F;
    return -std::sin(radians) * -0.55F + std::cos(radians) * 0.6F > 0.0F;
  }

  [[nodiscard]] auto lit_from_left() const -> bool {
    const float radians = rotation_deg * std::numbers::pi_v<float> / 180.0F;
    return std::cos(radians) * 0.55F + std::sin(radians) * 0.6F > 0.0F;
  }
};

auto hairline(const QColor& color, float width) -> QPen {
  return {color, width, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin};
}

void owner_plate(QPainter& painter, const QRectF& footprint, const QColor& owner) {
  if (!owner.isValid()) {
    return;
  }
  QColor fill = owner;
  fill.setAlpha(40);
  QColor rim = owner;
  rim.setAlpha(170);
  const double grow = 0.45;
  painter.setBrush(fill);
  painter.setPen(hairline(rim, 0.12F));
  painter.drawRoundedRect(footprint.adjusted(-grow, -grow, grow, grow), 0.5, 0.5);
}

void pennant(QPainter& painter, const QPointF& foot, const QColor& owner, float size) {
  if (!owner.isValid()) {
    return;
  }
  painter.setPen(hairline(QColor(40, 30, 22), size * 0.1F));
  painter.drawLine(foot, foot + QPointF(0.0, -size));
  QPolygonF flag;
  flag << (foot + QPointF(0.0, -size)) << (foot + QPointF(size * 0.75, -size * 0.78))
       << (foot + QPointF(0.0, -size * 0.52));
  painter.setPen(hairline(owner.darker(170), size * 0.06F));
  painter.setBrush(owner);
  painter.drawPolygon(flag);
}

void gable_roof(const Local& local,
                const QRectF& rect,
                bool ridge_along_x,
                const QColor& lit,
                const QColor& shade,
                const QColor& ridge) {
  QPainter& painter = local.painter;
  painter.setPen(Qt::NoPen);
  if (ridge_along_x) {
    const QRectF top(rect.left(), rect.top(), rect.width(), rect.height() * 0.5);
    const QRectF bottom(
        rect.left(), rect.center().y(), rect.width(), rect.height() * 0.5);
    const bool top_lit = local.lit_from_top();
    painter.setBrush(top_lit ? lit : shade);
    painter.drawRect(top);
    painter.setBrush(top_lit ? shade : lit);
    painter.drawRect(bottom);
    painter.setPen(
        hairline(QColor(ridge.red(), ridge.green(), ridge.blue(), 70), 0.05F));
    for (double x = rect.left() + 0.45; x < rect.right() - 0.1; x += 0.45) {
      painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    painter.setPen(hairline(ridge, 0.14F));
    painter.drawLine(QPointF(rect.left(), rect.center().y()),
                     QPointF(rect.right(), rect.center().y()));
  } else {
    const QRectF left(rect.left(), rect.top(), rect.width() * 0.5, rect.height());
    const QRectF right(
        rect.center().x(), rect.top(), rect.width() * 0.5, rect.height());
    const bool left_lit = local.lit_from_left();
    painter.setBrush(left_lit ? lit : shade);
    painter.drawRect(left);
    painter.setBrush(left_lit ? shade : lit);
    painter.drawRect(right);
    painter.setPen(
        hairline(QColor(ridge.red(), ridge.green(), ridge.blue(), 70), 0.05F));
    for (double y = rect.top() + 0.45; y < rect.bottom() - 0.1; y += 0.45) {
      painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
    painter.setPen(hairline(ridge, 0.14F));
    painter.drawLine(QPointF(rect.center().x(), rect.top()),
                     QPointF(rect.center().x(), rect.bottom()));
  }
  painter.setPen(hairline(QColor(60, 34, 26, 200), 0.08F));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect);
}

void cast_rect(const Local& local, const QRectF& rect, float height) {
  local.painter.setPen(Qt::NoPen);
  local.painter.setBrush(k_shadow);
  local.painter.drawRect(rect.translated(local.shadow_offset(height)));
}

void house(const Local& local, const QRectF& rect, bool ridge_along_x) {
  cast_rect(local, rect, 1.3F);
  local.painter.setPen(Qt::NoPen);
  local.painter.setBrush(k_plaster);
  local.painter.drawRect(rect);
  gable_roof(local,
             rect.adjusted(0.12, 0.12, -0.12, -0.12),
             ridge_along_x,
             k_roof_lit,
             k_roof_shade,
             k_roof_ridge);
}

void tower_top(const Local& local, const QRectF& rect, float height) {
  QPainter& painter = local.painter;
  cast_rect(local, rect, height);
  painter.setPen(hairline(k_stone_dark, 0.1F));
  painter.setBrush(k_stone);
  painter.drawRect(rect);
  painter.setPen(Qt::NoPen);
  const double tooth = std::min(rect.width(), rect.height()) / 5.0;
  for (int step = 0; step < 5; step += 2) {
    painter.setBrush(k_stone_lit);
    painter.drawRect(
        QRectF(rect.left() + step * tooth, rect.top(), tooth, tooth * 0.7));
    painter.drawRect(QRectF(
        rect.left() + step * tooth, rect.bottom() - tooth * 0.7, tooth, tooth * 0.7));
    painter.drawRect(
        QRectF(rect.left(), rect.top() + step * tooth, tooth * 0.7, tooth));
    painter.drawRect(QRectF(
        rect.right() - tooth * 0.7, rect.top() + step * tooth, tooth * 0.7, tooth));
  }
  painter.setBrush(QColor(98, 90, 80));
  painter.drawRect(rect.adjusted(tooth, tooth, -tooth, -tooth));
}

void paint_barracks(const Local& local, const QColor& owner) {
  QPainter& painter = local.painter;
  const QRectF yard(-4.4, -4.4, 8.8, 8.8);
  owner_plate(painter, yard, owner);
  cast_rect(local, yard, 1.6F);
  painter.setPen(hairline(k_stone_dark, 0.12F));
  painter.setBrush(k_stone);
  painter.drawRect(yard);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(164, 142, 104));
  painter.drawRect(yard.adjusted(0.8, 0.8, -0.8, -0.8));
  painter.setBrush(QColor(146, 124, 90, 150));
  painter.drawRect(QRectF(-0.9, 0.2, 1.8, 3.4));
  gable_roof(local,
             QRectF(-3.5, -3.6, 7.0, 2.3),
             true,
             k_roof_lit,
             k_roof_shade,
             k_roof_ridge);
  gable_roof(local,
             QRectF(-3.6, -1.0, 2.1, 4.4),
             false,
             k_roof_lit,
             k_roof_shade,
             k_roof_ridge);
  gable_roof(local,
             QRectF(1.5, -1.0, 2.1, 4.4),
             false,
             k_roof_lit,
             k_roof_shade,
             k_roof_ridge);
  for (const QPointF& corner : {QPointF(-4.4, -4.4),
                                QPointF(2.6, -4.4),
                                QPointF(-4.4, 2.6),
                                QPointF(2.6, 2.6)}) {
    tower_top(local, QRectF(corner.x(), corner.y(), 1.8, 1.8), 2.4F);
  }
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(60, 44, 32));
  painter.drawRect(QRectF(-0.8, 3.7, 1.6, 0.7));
  pennant(painter, QPointF(0.0, 1.2), owner, 2.2F);
}

void paint_home(const Local& local, const QColor& owner, unsigned seed) {
  const QRectF plot(-2.2, -2.2, 4.4, 4.4);
  owner_plate(local.painter, plot, owner);
  const bool along_x = unit_random(seed, 1U) > 0.5F;
  house(local,
        along_x ? QRectF(-2.0, -1.45, 4.0, 2.9) : QRectF(-1.45, -2.0, 2.9, 4.0),
        along_x);
  local.painter.setPen(Qt::NoPen);
  local.painter.setBrush(QColor(70, 60, 54));
  local.painter.drawRect(along_x ? QRectF(1.0, -0.95, 0.42, 0.42)
                                 : QRectF(-0.95, 1.0, 0.42, 0.42));
}

void paint_tower(const Local& local, const QColor& owner) {
  QPainter& painter = local.painter;
  const QRectF body(-1.6, -1.6, 3.2, 3.2);
  owner_plate(painter, body, owner);
  tower_top(local, body, 3.4F);
  QRadialGradient cone(QPointF(-0.3, -0.35), 1.3);
  cone.setColorAt(0.0, k_roof_lit.lighter(118));
  cone.setColorAt(1.0, k_roof_shade);
  painter.setPen(hairline(k_roof_ridge, 0.08F));
  painter.setBrush(cone);
  painter.drawEllipse(QPointF(0.0, 0.0), 0.95, 0.95);
  pennant(painter, QPointF(0.0, 0.0), owner, 1.5F);
}

void paint_marketplace(const Local& local, const QColor& owner, unsigned seed) {
  QPainter& painter = local.painter;
  const QRectF square(-3.0, -3.0, 6.0, 6.0);
  owner_plate(painter, square, owner);
  painter.setPen(hairline(QColor(140, 126, 100), 0.08F));
  painter.setBrush(QColor(198, 184, 156));
  painter.drawRect(square);
  painter.setPen(hairline(QColor(170, 156, 128, 150), 0.04F));
  for (double step = -2.4; step < 3.0; step += 0.6) {
    painter.drawLine(QPointF(step, -3.0), QPointF(step, 3.0));
    painter.drawLine(QPointF(-3.0, step), QPointF(3.0, step));
  }
  const std::array<QColor, 4> cloths = {QColor(188, 70, 58),
                                        QColor(214, 170, 74),
                                        QColor(80, 122, 150),
                                        QColor(120, 150, 92)};
  int stall = 0;
  for (const QPointF& at : {QPointF(-2.5, -2.5),
                            QPointF(0.6, -2.5),
                            QPointF(-2.5, 0.6),
                            QPointF(0.6, 0.6)}) {
    const QRectF awning(at.x(), at.y(), 1.9, 1.9);
    cast_rect(local, awning, 0.8F);
    const QColor cloth = cloths[(static_cast<unsigned>(stall) +
                                 static_cast<unsigned>(unit_random(seed, 3U) * 4.0F)) %
                                4U];
    painter.setPen(Qt::NoPen);
    for (int stripe = 0; stripe < 5; ++stripe) {
      painter.setBrush(stripe % 2 == 0 ? cloth : QColor(238, 228, 204));
      painter.drawRect(QRectF(at.x() + stripe * 0.38, at.y(), 0.38, 1.9));
    }
    painter.setPen(hairline(QColor(70, 50, 36, 190), 0.06F));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(awning);
    ++stall;
  }
  painter.setPen(hairline(k_stone_dark, 0.08F));
  painter.setBrush(k_stone_lit);
  painter.drawEllipse(QPointF(0.0, 0.0), 0.55, 0.55);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(52, 96, 128));
  painter.drawEllipse(QPointF(0.0, 0.0), 0.32, 0.32);
}

void paint_temple(const Local& local, const QColor& owner) {
  QPainter& painter = local.painter;
  const QRectF base(-6.2, -4.6, 12.4, 9.2);
  owner_plate(painter, base, owner);
  cast_rect(local, base, 1.0F);
  painter.setPen(hairline(QColor(150, 144, 130), 0.1F));
  painter.setBrush(QColor(224, 218, 204));
  painter.drawRect(base);
  painter.setBrush(QColor(206, 200, 186));
  painter.drawRect(base.adjusted(0.6, 0.6, -0.6, -0.6));
  const QRectF cella(-4.6, -3.0, 8.2, 6.0);
  cast_rect(local, cella, 2.6F);
  painter.setPen(hairline(QColor(120, 114, 102), 0.06F));
  painter.setBrush(QColor(244, 240, 230));
  for (double x = -5.1; x <= 4.3; x += 1.16) {
    painter.drawEllipse(QPointF(x, -3.6), 0.3, 0.3);
    painter.drawEllipse(QPointF(x, 3.6), 0.3, 0.3);
  }
  for (double y = -2.5; y <= 2.6; y += 1.25) {
    painter.drawEllipse(QPointF(-5.2, y), 0.3, 0.3);
    painter.drawEllipse(QPointF(4.2, y), 0.3, 0.3);
  }
  gable_roof(local, cella, true, k_roof_lit, k_roof_shade, k_roof_ridge);
  painter.setPen(hairline(k_stone_dark, 0.08F));
  painter.setBrush(k_stone_lit);
  painter.drawRect(QRectF(4.7, -0.5, 1.0, 1.0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(255, 176, 70));
  painter.drawEllipse(QPointF(5.2, 0.0), 0.26, 0.26);
}

void paint_farm(const Local& local, const QColor& owner, unsigned seed) {
  QPainter& painter = local.painter;
  const QRectF field(-6.8, -6.8, 13.6, 13.6);
  owner_plate(painter, field, owner);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(122, 104, 66));
  painter.drawRect(field);
  const std::array<QColor, 3> crops = {
      QColor(150, 160, 78), QColor(196, 176, 92), QColor(112, 146, 76)};
  const bool rows_along_x = unit_random(seed, 4U) > 0.5F;
  int row = 0;
  for (double at = -6.4; at < 6.4; at += 0.8) {
    painter.setBrush(crops[static_cast<unsigned>(
        (row / 5 + static_cast<int>(unit_random(seed, 5U) * 3.0F)) % 3)]);
    painter.drawRect(rows_along_x ? QRectF(-6.4, at, 12.8, 0.46)
                                  : QRectF(at, -6.4, 0.46, 12.8));
    ++row;
  }
  painter.setPen(hairline(QColor(88, 66, 44), 0.14F));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(field);
  house(local, QRectF(-6.2, -6.2, 3.0, 2.4), true);
  for (const QPointF& stack : {QPointF(-2.4, -5.2), QPointF(-1.2, -5.5)}) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(k_shadow);
    painter.drawEllipse(stack + local.shadow_offset(0.8F), 0.55, 0.5);
    painter.setBrush(QColor(206, 180, 96));
    painter.drawEllipse(stack, 0.55, 0.55);
    painter.setBrush(QColor(232, 210, 130));
    painter.drawEllipse(stack - QPointF(0.12, 0.14), 0.28, 0.28);
  }
}

void paint_village(const Local& local, const QColor& owner) {
  owner_plate(local.painter, QRectF(-4.0, -4.0, 8.0, 8.0), owner);
  house(local, QRectF(-3.6, -3.4, 3.6, 2.6), true);
  house(local, QRectF(0.6, -2.6, 2.6, 3.8), false);
  house(local, QRectF(-2.8, 0.6, 3.2, 2.6), true);
}

void paint_wall_block(const Local& local, const QColor& owner) {
  QPainter& painter = local.painter;
  const QRectF block(-1.0, -1.0, 2.0, 2.0);
  owner_plate(painter, block, owner);
  cast_rect(local, block, 1.6F);
  painter.setPen(hairline(QColor(64, 46, 30), 0.08F));
  painter.setBrush(k_timber);
  painter.drawRect(block);
  painter.setPen(Qt::NoPen);
  painter.setBrush(k_timber_lit);
  for (double x = -0.85; x < 0.9; x += 0.5) {
    painter.drawEllipse(QPointF(x + 0.1, -0.5), 0.2, 0.2);
    painter.drawEllipse(QPointF(x + 0.1, 0.5), 0.2, 0.2);
  }
}

void paint_tent(const Local& local) {
  QPainter& painter = local.painter;
  const QRectF cloth(-1.4, -1.05, 2.8, 2.1);
  cast_rect(local, cloth, 0.9F);
  painter.setPen(hairline(QColor(96, 84, 66, 150), 0.04F));
  for (const QPointF& peg : {QPointF(-1.8, -1.4),
                             QPointF(1.8, -1.4),
                             QPointF(-1.8, 1.4),
                             QPointF(1.8, 1.4)}) {
    painter.drawLine(QPointF(peg.x() * 0.75, peg.y() * 0.72), peg);
  }
  gable_roof(local, cloth, true, k_canvas_lit, k_canvas_shade, QColor(120, 104, 80));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(58, 46, 38, 200));
  QPolygonF door;
  door << QPointF(1.4, -0.42) << QPointF(1.4, 0.42) << QPointF(0.86, 0.0);
  painter.drawPolygon(door);
}

void paint_cart(const Local& local) {
  QPainter& painter = local.painter;
  const QRectF bed(-1.1, -0.6, 2.2, 1.2);
  cast_rect(local, bed, 0.6F);
  painter.setPen(hairline(QColor(46, 34, 26), 0.2F));
  for (const double x : {-0.7, 0.7}) {
    painter.drawLine(QPointF(x - 0.28, -0.78), QPointF(x + 0.28, -0.78));
    painter.drawLine(QPointF(x - 0.28, 0.78), QPointF(x + 0.28, 0.78));
  }
  painter.setPen(hairline(QColor(70, 50, 34), 0.06F));
  painter.setBrush(k_timber_lit);
  painter.drawRect(bed);
  painter.setPen(hairline(QColor(96, 70, 46, 160), 0.04F));
  for (double x = -0.8; x < 1.0; x += 0.36) {
    painter.drawLine(QPointF(x, -0.6), QPointF(x, 0.6));
  }
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(208, 194, 158));
  painter.drawRoundedRect(QRectF(-0.8, -0.4, 1.2, 0.8), 0.18, 0.18);
  painter.setPen(hairline(k_timber, 0.1F));
  painter.drawLine(QPointF(1.1, 0.0), QPointF(2.0, 0.0));
}

void paint_weapon_rack(const Local& local) {
  QPainter& painter = local.painter;
  painter.setPen(hairline(k_shadow, 0.16F));
  painter.drawLine(QPointF(-0.9, 0.0) + local.shadow_offset(0.8F),
                   QPointF(0.9, 0.0) + local.shadow_offset(0.8F));
  painter.setPen(hairline(k_timber, 0.16F));
  painter.drawLine(QPointF(-0.9, 0.0), QPointF(0.9, 0.0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(k_timber_lit);
  painter.drawEllipse(QPointF(-0.9, 0.0), 0.14, 0.14);
  painter.drawEllipse(QPointF(0.9, 0.0), 0.14, 0.14);
  for (double x = -0.6; x <= 0.61; x += 0.4) {
    painter.setPen(hairline(QColor(196, 190, 176), 0.06F));
    painter.drawLine(QPointF(x - 0.12, 0.55), QPointF(x + 0.12, -0.55));
    painter.setPen(hairline(QColor(82, 86, 92), 0.1F));
    painter.drawLine(QPointF(x + 0.08, -0.38), QPointF(x + 0.12, -0.55));
  }
}

void stone_run(const Local& local, const QRectF& rect) {
  QPainter& painter = local.painter;
  cast_rect(local, rect, 0.9F);
  painter.setPen(hairline(QColor(94, 90, 84), 0.05F));
  painter.setBrush(QColor(156, 152, 142));
  painter.drawRect(rect);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(196, 192, 180));
  painter.drawRect(
      rect.adjusted(0.06, 0.06, -rect.width() * 0.35, -rect.height() * 0.4));
}

void paint_ruins(const Local& local, unsigned seed) {
  QPainter& painter = local.painter;
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(96, 88, 76, 110));
  painter.drawRect(QRectF(-2.5, -1.8, 5.0, 3.6));
  stone_run(local, QRectF(-2.7, -2.0, 3.3, 0.5));
  stone_run(local, QRectF(-2.7, -2.0, 0.5, 2.6));
  stone_run(local, QRectF(1.5, -2.0, 1.2, 0.5));
  stone_run(local, QRectF(2.2, -0.4, 0.5, 2.4));
  stone_run(local, QRectF(-0.6, 1.5, 2.2, 0.5));
  for (unsigned index = 0; index < 9U; ++index) {
    const QPointF at((unit_random(seed, 10U + index) - 0.5F) * 4.6F,
                     (unit_random(seed, 30U + index) - 0.5F) * 3.2F);
    const float r = 0.12F + 0.14F * unit_random(seed, 50U + index);
    painter.setBrush(index % 2U == 0U ? QColor(132, 128, 120) : QColor(176, 172, 162));
    painter.drawEllipse(at, r, r);
  }
}

void paint_abandoned_home(const Local& local, unsigned seed) {
  QPainter& painter = local.painter;
  const QRectF rect(-2.1, -1.7, 4.2, 3.4);
  cast_rect(local, rect, 1.1F);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(166, 156, 138));
  painter.drawRect(rect);
  gable_roof(local,
             rect.adjusted(0.14, 0.14, -0.14, -0.14),
             true,
             QColor(140, 112, 94),
             QColor(104, 84, 74),
             QColor(66, 52, 46));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(34, 28, 26));
  QPolygonF hole;
  const float shift = unit_random(seed, 2U) * 1.4F - 0.7F;
  hole << QPointF(shift - 0.7, -0.5) << QPointF(shift + 0.1, -0.9)
       << QPointF(shift + 0.9, -0.3) << QPointF(shift + 0.5, 0.5)
       << QPointF(shift - 0.4, 0.3);
  painter.drawPolygon(hole);
  painter.setBrush(QColor(96, 104, 70, 150));
  painter.drawEllipse(QPointF(-1.5, 1.2), 0.5, 0.36);
}

void paint_statue(const Local& local) {
  QPainter& painter = local.painter;
  const QRectF plinth(-0.5, -0.5, 1.0, 1.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(k_shadow);
  QPolygonF cast;
  const QPointF tip = local.shadow_offset(4.2F);
  cast << QPointF(-0.3, 0.2) << QPointF(0.3, -0.2) << (tip + QPointF(0.2, -0.1))
       << (tip + QPointF(-0.2, 0.2));
  painter.drawPolygon(cast);
  cast_rect(local, plinth, 0.5F);
  painter.setPen(hairline(k_stone_dark, 0.06F));
  painter.setBrush(k_stone_lit);
  painter.drawRect(plinth);
  painter.setBrush(QColor(158, 154, 146));
  painter.drawEllipse(QPointF(0.0, 0.0), 0.3, 0.3);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(222, 218, 208));
  painter.drawEllipse(QPointF(-0.08, -0.1), 0.14, 0.14);
}

void paint_firecamp(
    const Local& local, float glow_radius, unsigned seed, double seconds, float night) {
  QPainter& painter = local.painter;
  const double flicker = 0.86 + 0.14 * std::sin(seconds * 9.0 + seed % 17U) *
                                    std::sin(seconds * 5.3 + seed % 7U);
  const double reach = std::clamp(static_cast<double>(glow_radius), 2.0, 7.0) *
                       flicker * (1.0 + 0.7 * static_cast<double>(night));
  QRadialGradient pool(QPointF(0.0, 0.0), reach);
  pool.setColorAt(0.0, QColor(255, 190, 96, night > 0.5F ? 150 : 84));
  pool.setColorAt(0.45, QColor(255, 150, 60, night > 0.5F ? 70 : 34));
  pool.setColorAt(1.0, QColor(255, 130, 40, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(pool);
  painter.drawEllipse(QPointF(0.0, 0.0), reach, reach);
  painter.setBrush(QColor(34, 26, 22, 170));
  painter.drawEllipse(QPointF(0.0, 0.0), 0.86, 0.86);
  for (int stone = 0; stone < 8; ++stone) {
    const double angle = stone * 0.7854 + (seed % 5U) * 0.2;
    painter.setBrush(stone % 2 == 0 ? QColor(128, 124, 118) : QColor(164, 160, 152));
    painter.drawEllipse(QPointF(std::cos(angle), std::sin(angle)) * 0.68, 0.17, 0.17);
  }
  QRadialGradient flame(QPointF(0.0, 0.0), 0.5 * flicker);
  flame.setColorAt(0.0, QColor(255, 248, 196));
  flame.setColorAt(0.5, QColor(255, 176, 56));
  flame.setColorAt(1.0, QColor(214, 72, 26, 0));
  painter.setBrush(flame);
  painter.drawEllipse(QPointF(0.0, 0.0), 0.5 * flicker, 0.5 * flicker);
}

void paint_shrine(const Local& local, unsigned seed, double seconds) {
  QPainter& painter = local.painter;
  const double pulse = 0.8 + 0.2 * std::sin(seconds * 1.7 + seed % 11U);
  QRadialGradient aura(QPointF(0.0, 0.0), 3.4 * pulse);
  aura.setColorAt(0.0, QColor(196, 150, 255, 90));
  aura.setColorAt(1.0, QColor(150, 96, 230, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(aura);
  painter.drawEllipse(QPointF(0.0, 0.0), 3.4 * pulse, 3.4 * pulse);
  painter.setBrush(k_shadow);
  painter.drawEllipse(local.shadow_offset(0.5F), 1.5, 1.4);
  painter.setPen(hairline(QColor(74, 70, 86), 0.08F));
  painter.setBrush(QColor(128, 124, 140));
  painter.drawEllipse(QPointF(0.0, 0.0), 1.4, 1.4);
  painter.setBrush(QColor(158, 154, 172));
  painter.drawEllipse(QPointF(0.0, 0.0), 0.95, 0.95);
  for (int stone = 0; stone < 4; ++stone) {
    const double angle = stone * 1.5708 + 0.7854;
    const QPointF at = QPointF(std::cos(angle), std::sin(angle)) * 1.15;
    painter.setPen(Qt::NoPen);
    painter.setBrush(k_shadow);
    painter.drawEllipse(at + local.shadow_offset(1.4F), 0.24, 0.2);
    painter.setBrush(QColor(196, 192, 208));
    painter.drawEllipse(at, 0.22, 0.22);
  }
  QRadialGradient core(QPointF(0.0, 0.0), 0.6);
  core.setColorAt(0.0, QColor(248, 232, 255));
  core.setColorAt(0.6, QColor(186, 130, 255));
  core.setColorAt(1.0, QColor(120, 70, 210, 0));
  painter.setBrush(core);
  painter.drawEllipse(QPointF(0.0, 0.0), 0.6, 0.6);
}

void paint_gold_vein(const Local& local, unsigned seed, double seconds) {
  QPainter& painter = local.painter;
  const double pulse = 0.82 + 0.18 * std::sin(seconds * 1.3 + seed % 13U);
  QRadialGradient aura(QPointF(0.0, 0.0), 2.6 * pulse);
  aura.setColorAt(0.0, QColor(170, 110, 230, 70));
  aura.setColorAt(1.0, QColor(120, 70, 200, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(aura);
  painter.drawEllipse(QPointF(0.0, 0.0), 2.6 * pulse, 2.6 * pulse);
  QPolygonF rock;
  for (int point = 0; point < 8; ++point) {
    const double angle = point * 0.7854;
    const double reach =
        1.0 + 0.3 * unit_random(seed, 70U + static_cast<unsigned>(point));
    rock << QPointF(std::cos(angle) * reach, std::sin(angle) * reach);
  }
  painter.setBrush(k_shadow);
  painter.drawPolygon(rock.translated(local.shadow_offset(0.9F)));
  painter.setBrush(QColor(62, 54, 60));
  painter.drawPolygon(rock);
  painter.setBrush(QColor(92, 82, 88));
  painter.drawEllipse(QPointF(-0.2, -0.24), 0.62, 0.56);
  for (unsigned fleck = 0; fleck < 7U; ++fleck) {
    const QPointF at((unit_random(seed, 80U + fleck) - 0.5F) * 1.5F,
                     (unit_random(seed, 90U + fleck) - 0.5F) * 1.5F);
    painter.setBrush(fleck % 2U == 0U ? QColor(244, 204, 84) : QColor(206, 158, 52));
    painter.drawEllipse(at, 0.13, 0.1);
  }
}

} // namespace

auto has_structure_sprite(const QString& type) -> bool {
  return type == QLatin1String("barracks") || type == QLatin1String("home") ||
         type == QLatin1String("defense_tower") ||
         type == QLatin1String("marketplace") || type == QLatin1String("temple") ||
         type == QLatin1String("farm") || type == QLatin1String("village") ||
         type == QLatin1String("wall_segment");
}

auto structure_sprite_radius_cells(const QString& type) -> float {
  if (type == QLatin1String("barracks")) {
    return 5.2F;
  }
  if (type == QLatin1String("temple")) {
    return 6.6F;
  }
  if (type == QLatin1String("farm")) {
    return 7.4F;
  }
  if (type == QLatin1String("marketplace")) {
    return 3.6F;
  }
  if (type == QLatin1String("village")) {
    return 4.6F;
  }
  if (type == QLatin1String("defense_tower")) {
    return 2.2F;
  }
  if (type == QLatin1String("wall_segment")) {
    return 1.5F;
  }
  return 2.8F;
}

void paint_structure_sprite(QPainter& painter,
                            const QString& type,
                            const QPointF& centre,
                            float cell_px,
                            float rotation_deg,
                            const QColor& owner,
                            unsigned seed) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(centre);
  painter.rotate(static_cast<double>(rotation_deg));
  painter.scale(static_cast<double>(cell_px), static_cast<double>(cell_px));
  const Local local{painter, rotation_deg};
  if (type == QLatin1String("barracks")) {
    paint_barracks(local, owner);
  } else if (type == QLatin1String("home")) {
    paint_home(local, owner, seed);
  } else if (type == QLatin1String("defense_tower")) {
    paint_tower(local, owner);
  } else if (type == QLatin1String("marketplace")) {
    paint_marketplace(local, owner, seed);
  } else if (type == QLatin1String("temple")) {
    paint_temple(local, owner);
  } else if (type == QLatin1String("farm")) {
    paint_farm(local, owner, seed);
  } else if (type == QLatin1String("village")) {
    paint_village(local, owner);
  } else if (type == QLatin1String("wall_segment")) {
    paint_wall_block(local, owner);
  }
  painter.restore();
}

auto has_prop_sprite(const QString& type) -> bool {
  return type == QLatin1String("tent") || type == QLatin1String("supply_cart") ||
         type == QLatin1String("weapon_rack") || type == QLatin1String("ruins") ||
         type == QLatin1String("abandoned_home") || type == QLatin1String("statue") ||
         type == QLatin1String("firecamp") || type == QLatin1String("magic_shrine") ||
         type == QLatin1String("cursed_gold_vein");
}

void paint_prop_sprite(QPainter& painter,
                       const QString& type,
                       const QPointF& centre,
                       float cell_px,
                       float scale,
                       float rotation_deg,
                       float glow_radius_cells,
                       unsigned seed,
                       double seconds,
                       float night) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(centre);
  painter.rotate(static_cast<double>(rotation_deg));
  const double unit =
      static_cast<double>(cell_px) * std::clamp(static_cast<double>(scale), 0.5, 2.5);
  painter.scale(unit, unit);
  const Local local{painter, rotation_deg};
  if (type == QLatin1String("tent")) {
    paint_tent(local);
  } else if (type == QLatin1String("supply_cart")) {
    paint_cart(local);
  } else if (type == QLatin1String("weapon_rack")) {
    paint_weapon_rack(local);
  } else if (type == QLatin1String("ruins")) {
    paint_ruins(local, seed);
  } else if (type == QLatin1String("abandoned_home")) {
    paint_abandoned_home(local, seed);
  } else if (type == QLatin1String("statue")) {
    paint_statue(local);
  } else if (type == QLatin1String("firecamp")) {
    paint_firecamp(
        local, glow_radius_cells / std::max(scale, 0.5F), seed, seconds, night);
  } else if (type == QLatin1String("magic_shrine")) {
    paint_shrine(local, seed, seconds);
  } else if (type == QLatin1String("cursed_gold_vein")) {
    paint_gold_vein(local, seed, seconds);
  }
  painter.restore();
}

void paint_palisade(QPainter& painter,
                    const QPolygonF& path,
                    float cell_px,
                    const QColor& owner) {
  if (path.size() < 2) {
    return;
  }
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  const double thick = std::max(static_cast<double>(cell_px) * 0.95, 2.4);
  const QPointF cast(thick * 0.42, thick * 0.54);
  painter.setPen(QPen(k_shadow, thick, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  painter.drawPolyline(path.translated(cast));
  if (owner.isValid()) {
    QColor band = owner;
    band.setAlpha(120);
    painter.setPen(QPen(band, thick + 3.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.drawPolyline(path);
  }
  painter.setPen(
      QPen(QColor(74, 54, 36), thick, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  painter.drawPolyline(path);
  painter.setPen(
      QPen(k_timber_lit, thick * 0.5, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  painter.drawPolyline(path.translated(-thick * 0.1, -thick * 0.12));
  if (cell_px >= 3.0F) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(196, 160, 112));
    const double spacing = std::max(static_cast<double>(cell_px) * 1.1, 4.0);
    for (int index = 0; index + 1 < path.size(); ++index) {
      const QPointF span = path[index + 1] - path[index];
      const double length = std::hypot(span.x(), span.y());
      const int posts = std::max(1, static_cast<int>(length / spacing));
      for (int post = 0; post <= posts; ++post) {
        const QPointF at = path[index] + span * (static_cast<double>(post) / posts);
        painter.drawEllipse(
            at - QPointF(thick * 0.1, thick * 0.12), thick * 0.2, thick * 0.2);
      }
    }
  }
  painter.restore();
}

void paint_gatehouse(QPainter& painter,
                     const QPointF& centre,
                     float cell_px,
                     float rotation_deg,
                     float span_cells,
                     float depth_cells,
                     const QColor& owner) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(centre);
  painter.rotate(static_cast<double>(rotation_deg));
  painter.scale(static_cast<double>(cell_px), static_cast<double>(cell_px));
  const Local local{painter, rotation_deg};
  const double half_span = std::max(static_cast<double>(span_cells) * 0.5, 1.5);
  const double half_depth = std::max(static_cast<double>(depth_cells) * 0.5, 0.9);
  const double pier = half_span * 0.34;
  owner_plate(painter,
              QRectF(-half_span, -half_depth, half_span * 2.0, half_depth * 2.0),
              owner);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(92, 66, 42));
  painter.drawRect(QRectF(-half_span + pier,
                          -half_depth * 0.35,
                          (half_span - pier) * 2.0,
                          half_depth * 0.7));
  painter.setPen(hairline(QColor(52, 36, 24), 0.06F));
  painter.drawLine(QPointF(0.0, -half_depth * 0.35), QPointF(0.0, half_depth * 0.35));
  tower_top(local, QRectF(-half_span, -half_depth, pier, half_depth * 2.0), 2.6F);
  tower_top(local, QRectF(half_span - pier, -half_depth, pier, half_depth * 2.0), 2.6F);
  painter.restore();
}

} // namespace MapEditor
