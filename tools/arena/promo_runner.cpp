#include "promo_runner.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "arena_audio_recorder.h"
#include "arena_scenario.h"
#include "arena_typography.h"
#include "arena_viewport.h"
#include "game/session/session_context.h"
#include "game/visuals/team_colors.h"
#include "render/humanoid/runtime/runtime_stats.h"
#include "video_encoder.h"

namespace Arena::Promo {
namespace {

constexpr float k_scenario_tail_seconds = 2.0F;
constexpr int k_pass_watchdog_ms = 1'800'000;
constexpr float k_pass_watchdog_scale = 10.0F;
constexpr float k_fast_forward_margin_seconds = 3.0F;
constexpr int k_pass_warmup_frames = 3;

constexpr double k_card_dissolve_seconds = 0.55;
constexpr int k_first_frame_min_peak = 8;
constexpr int k_max_black_frames_skipped = 45;

void paint_meter(QPainter& painter,
                 const QRectF& rect,
                 float ratio,
                 const QColor& fill,
                 const QString& label) {
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(6, 6, 8, 190));
  painter.drawRect(rect);
  QRectF filled = rect.adjusted(2.0, 2.0, -2.0, -2.0);
  filled.setWidth(filled.width() * std::clamp(ratio, 0.0F, 1.0F));
  painter.setBrush(fill);
  painter.drawRect(filled);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor(0, 0, 0, 170), 1.0));
  painter.drawRect(rect);
  if (!label.isEmpty()) {
    painter.setPen(QColor(238, 232, 218, 225));
    painter.drawText(
        rect.adjusted(8.0, 0.0, -8.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, label);
  }
}

void paint_rpg_bow_hud(QImage& frame, const ArenaViewport::RpgBowHudState& state) {
  if (frame.isNull() || !state.valid) {
    return;
  }

  const double width = frame.width();
  const double height = frame.height();
  const double ui = std::clamp(height / 1080.0, 0.6, 2.4);

  QPainter painter(&frame);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  painter.setFont(Arena::Typography::small_label(ui));

  const QPointF centre(width * 0.5, height * 0.5);

  if (state.bow_stance) {

    const double half_fov =
        std::clamp(static_cast<double>(state.fov_degrees), 20.0, 110.0) *
        std::numbers::pi / 360.0;
    const double focal = (height * 0.5) / std::tan(half_fov);
    const double spread = std::min(
        height * 0.14,
        focal * std::tan(std::min(14.0, static_cast<double>(state.spread_degrees)) *
                         std::numbers::pi / 180.0));

    QColor reticle(243, 239, 230, 245);
    if (state.strained) {
      reticle = QColor(255, 122, 90, 245);
    } else if (state.full_draw) {
      reticle = QColor(255, 224, 122, 250);
    } else if (state.target_in_reticle) {
      reticle = QColor(120, 236, 255, 245);
    }

    const double gap = (7.0 * ui) + spread;
    const double tick = 26.0 * ui;
    const double thickness = std::max(2.0, 3.0 * ui);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(
        QPen(QColor(0, 0, 0, 130), thickness + (2.0 * ui), Qt::SolidLine, Qt::FlatCap));
    for (int axis = 0; axis < 4; ++axis) {
      const double dx = (axis == 2) ? -1.0 : ((axis == 3) ? 1.0 : 0.0);
      const double dy = (axis == 0) ? -1.0 : ((axis == 1) ? 1.0 : 0.0);
      painter.drawLine(
          QPointF(centre.x() + (dx * gap), centre.y() + (dy * gap)),
          QPointF(centre.x() + (dx * (gap + tick)), centre.y() + (dy * (gap + tick))));
    }
    painter.setPen(QPen(reticle, thickness, Qt::SolidLine, Qt::FlatCap));
    for (int axis = 0; axis < 4; ++axis) {
      const double dx = (axis == 2) ? -1.0 : ((axis == 3) ? 1.0 : 0.0);
      const double dy = (axis == 0) ? -1.0 : ((axis == 1) ? 1.0 : 0.0);
      painter.drawLine(
          QPointF(centre.x() + (dx * gap), centre.y() + (dy * gap)),
          QPointF(centre.x() + (dx * (gap + tick)), centre.y() + (dy * (gap + tick))));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(reticle);
    const double dot = std::max(2.0, 3.2 * ui);
    painter.drawEllipse(centre, dot, dot);

    if (state.drawing) {

      const double radius = 46.0 * ui;
      const QRectF ring(
          centre.x() - radius, centre.y() - radius, radius * 2.0, radius * 2.0);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(QColor(0, 0, 0, 120), std::max(3.0, 4.0 * ui)));
      painter.drawArc(ring, 0, 360 * 16);
      painter.setPen(
          QPen(reticle, std::max(3.0, 4.0 * ui), Qt::SolidLine, Qt::RoundCap));
      painter.drawArc(
          ring,
          90 * 16,
          -static_cast<int>(std::round(state.draw_progress * 360.0 * 16.0)));
    }

    if (state.hit_confirm > 0.0F) {
      const double radius = (16.0 + (26.0 * (1.0 - state.hit_confirm))) * ui;
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(
          QColor(238, 74, 52, static_cast<int>(std::round(220.0 * state.hit_confirm))),
          std::max(2.0, 3.0 * ui)));
      painter.drawEllipse(centre, radius, radius);
    }
  }

  const double bar_width = 300.0 * ui;
  const double bar_height = 18.0 * ui;
  const double margin = 42.0 * ui;
  double bar_y = height - margin - (bar_height * 2.0) - (7.0 * ui);
  paint_meter(painter,
              QRectF(margin, bar_y, bar_width, bar_height),
              state.health_ratio,
              QColor(178, 46, 40, 235),
              QStringLiteral("HP"));
  bar_y += bar_height + (7.0 * ui);
  paint_meter(painter,
              QRectF(margin, bar_y, bar_width, bar_height),
              state.stamina_ratio,
              QColor(74, 138, 82, 235),
              QStringLiteral("STAMINA"));

  if (state.bow_stance) {
    const double nock_width = 190.0 * ui;
    const double nock_height = 12.0 * ui;
    const QRectF nock(width - margin - nock_width,
                      height - margin - nock_height,
                      nock_width,
                      nock_height);
    const bool ready = state.recovery_ratio <= 0.001F;
    paint_meter(painter,
                nock,
                ready ? 1.0F : (1.0F - state.recovery_ratio),
                ready ? QColor(214, 186, 116, 235) : QColor(122, 92, 58, 220),
                QString());
    painter.setPen(ready ? QColor(240, 224, 178, 235) : QColor(168, 150, 122, 215));
    painter.drawText(
        QRectF(nock.left(), nock.top() - (24.0 * ui), nock.width(), 20.0 * ui),
        Qt::AlignRight | Qt::AlignVCenter,
        ready ? QStringLiteral("ARROW READY") : QStringLiteral("NOCKING"));
  }

  painter.setFont(Arena::Typography::number(ui));
  painter.setPen(QColor(0, 0, 0, 170));
  const QString takedowns =
      QStringLiteral("TAKEDOWNS  %1").arg(state.takedowns, 2, 10, QLatin1Char('0'));
  const QRectF counter(width - margin - (340.0 * ui), margin, 340.0 * ui, 30.0 * ui);
  painter.drawText(counter.adjusted(2.0, 2.0, 2.0, 2.0),
                   Qt::AlignRight | Qt::AlignVCenter,
                   takedowns);
  painter.setPen(QColor(238, 226, 196, 240));
  painter.drawText(counter, Qt::AlignRight | Qt::AlignVCenter, takedowns);
}

auto format_clock(float seconds) -> QString {
  const int total = std::max(0, static_cast<int>(std::lround(seconds)));
  return QStringLiteral("%1:%2")
      .arg(total / 60)
      .arg(total % 60, 2, 10, QLatin1Char('0'));
}

auto side_display_name(QString label) -> QString {
  return label.replace(QLatin1Char('_'), QLatin1Char(' ')).toUpper();
}

auto census_display(const QString& census) -> QString {
  QStringList parts;
  for (const QString& entry : census.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const qsizetype split = entry.lastIndexOf(QLatin1Char('x'));
    if (split <= 0) {
      parts.push_back(entry.toUpper());
      continue;
    }
    parts.push_back(QStringLiteral("%1 × %2").arg(
        entry.mid(split + 1),
        entry.left(split).replace(QLatin1Char('_'), QLatin1Char(' ')).toUpper()));
  }
  return parts.join(QStringLiteral("   "));
}

auto card_font(double ui, double pixels) -> QFont {
  QFont font = Arena::Typography::small_label(1.0);
  font.setPixelSize(static_cast<int>(std::lround(pixels * ui)));
  return font;
}

auto paint_matchup_card(const Spec& spec,
                        const Arena::ArenaScenarioReport* report) -> QImage {
  QImage card(spec.width, spec.height, QImage::Format_RGB32);
  card.fill(QColor(11, 10, 8));

  const double width = card.width();
  const double height = card.height();
  const double ui = std::clamp(width / 1080.0, 0.4, 2.4);

  const QColor parchment(238, 232, 218);
  const QColor faded(168, 158, 138);
  const QColor gold(214, 186, 116);
  const QColor blood(206, 74, 56);

  QPainter painter(&card);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  constexpr int k_text_flags = Qt::AlignHCenter | Qt::AlignVCenter;
  const auto text =
      [&](double y, double pixels, const QColor& color, const QString& value) {
        const QRectF box(width * 0.06, y, width * 0.88, pixels * ui * 1.6);
        QFont font = card_font(ui, pixels);
        for (int attempt = 0; attempt < 8; ++attempt) {
          painter.setFont(font);
          const double drawn = painter.boundingRect(box, k_text_flags, value).width();
          if (drawn <= box.width() || drawn <= 0.0) {
            break;
          }
          const int shrunk = std::max(
              8,
              static_cast<int>(std::floor(font.pixelSize() * (box.width() / drawn))));
          if (shrunk >= font.pixelSize()) {
            break;
          }
          font.setPixelSize(shrunk);
        }
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText(box, k_text_flags, value);
        return y + (pixels * ui * 1.6);
      };

  const bool tracked_sides =
      report != nullptr && report->battle.tracked && report->battle.sides.size() >= 2U;

  constexpr double k_heading_block = 41.6 + 10.0 + 89.6 + 20.0;
  constexpr double k_verdict_block = 99.2 + 6.0 + 38.4 + 46.0;
  constexpr double k_side_block = 54.4 + 6.0 + 44.8 + 2.0 + 30.4 + 10.0 + 18.0 + 52.0;
  const double content =
      (k_heading_block + k_verdict_block + (tracked_sides ? 2.0 * k_side_block : 0.0)) *
      ui;

  double y = std::max(height * 0.10, (height - content) * 0.5);
  y = text(y, 26.0, faded, spec.title);
  y += 10.0 * ui;
  y = text(y, 56.0, parchment, QStringLiteral("BATTLE REPORT"));
  y += 20.0 * ui;

  const bool tracked = tracked_sides;

  QString verdict = QStringLiteral("NO CONTEST");
  QColor verdict_color = faded;
  QString clock_line;
  if (tracked) {
    const auto& battle = report->battle;
    if (battle.decided && !battle.victor_label.isEmpty()) {
      verdict = QStringLiteral("%1 WIN").arg(side_display_name(battle.victor_label));
      verdict_color = gold;
      clock_line =
          QStringLiteral("DECIDED AT %1").arg(format_clock(battle.decided_at_seconds));
    } else if (battle.decided) {
      verdict = QStringLiteral("BOTH SIDES FELL");
      verdict_color = blood;
      clock_line =
          QStringLiteral("DECIDED AT %1").arg(format_clock(battle.decided_at_seconds));
    } else {
      const auto& first = battle.sides[0];
      const auto& second = battle.sides[1];
      const auto& ahead = first.living_units >= second.living_units ? first : second;
      verdict = first.living_units == second.living_units
                    ? QStringLiteral("STILL LEVEL")
                    : QStringLiteral("%1 AHEAD").arg(side_display_name(ahead.label));
      clock_line = QStringLiteral("NO DECISION AFTER %1")
                       .arg(format_clock(report->elapsed_seconds));
    }
  }
  y = text(y, 62.0, verdict_color, verdict);
  if (!clock_line.isEmpty()) {
    y += 6.0 * ui;
    y = text(y, 24.0, faded, clock_line);
  }
  y += 46.0 * ui;

  if (!tracked) {
    return card;
  }

  for (std::size_t index = 0;
       index < std::min<std::size_t>(2U, report->battle.sides.size());
       ++index) {
    const auto& side = report->battle.sides[index];
    const int started = std::max(side.peak_units, side.living_units);
    const QVector3D team = Game::Visuals::team_colorForOwner(side.owner_id);
    const QColor team_color(static_cast<int>(std::lround(team.x() * 255.0F)),
                            static_cast<int>(std::lround(team.y() * 255.0F)),
                            static_cast<int>(std::lround(team.z() * 255.0F)));

    const int men_started = std::max(side.peak_soldiers, side.living_soldiers);
    const bool have_men = men_started > 0;

    y = text(y, 34.0, parchment, side_display_name(side.label));
    y += 6.0 * ui;
    y = text(y,
             28.0,
             side.living_units > 0 ? gold : blood,
             side.living_units <= 0 ? QStringLiteral("WIPED OUT")
             : have_men             ? QStringLiteral("%1 OF %2 MEN LEFT STANDING")
                              .arg(side.living_soldiers)
                              .arg(men_started)
                        : QStringLiteral("%1 OF %2 LEFT STANDING")
                              .arg(side.living_units)
                              .arg(started));
    if (have_men) {
      y += 2.0 * ui;
      y = text(y,
               19.0,
               faded,
               QStringLiteral("%1 OF %2 UNITS").arg(side.living_units).arg(started));
    }

    const double bar_height = 18.0 * ui;
    const QRectF track(width * 0.14, y + (10.0 * ui), width * 0.72, bar_height);
    const double share = have_men ? static_cast<double>(side.living_soldiers) /
                                        static_cast<double>(men_started)
                         : started > 0 ? static_cast<double>(side.living_units) /
                                             static_cast<double>(started)
                                       : 0.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(46, 42, 36));
    painter.drawRect(track);
    painter.setBrush(team_color);
    painter.drawRect(
        QRectF(track.left(), track.top(), track.width() * share, track.height()));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 150), std::max(1.0, 2.0 * ui)));
    painter.drawRect(track);

    y = track.bottom() + (52.0 * ui);
  }

  return card;
}

auto paint_report_card(const Spec& spec,
                       const Arena::ArenaScenarioReport* report) -> QImage {
  QImage card(spec.width, spec.height, QImage::Format_RGB32);
  card.fill(QColor(11, 10, 8));

  const double width = card.width();
  const double height = card.height();
  const double ui = std::clamp(height / 1080.0, 0.4, 2.4);

  const QColor parchment(238, 232, 218);
  const QColor faded(168, 158, 138);
  const QColor gold(214, 186, 116);
  const QColor blood(206, 74, 56);
  const QColor laurel(122, 178, 112);

  QPainter painter(&card);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  const auto line = [&](double y, const QColor& color) {
    painter.setPen(QPen(color, std::max(1.0, 2.0 * ui)));
    painter.drawLine(QPointF(width * 0.14, y), QPointF(width * 0.86, y));
  };
  const auto text = [&](double y,
                        double pixels,
                        const QColor& color,
                        const QString& value,
                        double left = 0.0,
                        double right = 1.0) {
    painter.setFont(card_font(ui, pixels));
    painter.setPen(color);
    painter.drawText(QRectF(width * left, y, width * (right - left), pixels * ui * 1.6),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     value);
    return y + (pixels * ui * 1.6);
  };

  double y = height * 0.16;
  y = text(y, 20.0, faded, spec.title);
  y += 6.0 * ui;
  y = text(y, 46.0, parchment, QStringLiteral("BATTLE REPORT"));
  y += 14.0 * ui;
  line(y, QColor(gold.red(), gold.green(), gold.blue(), 140));
  y += 26.0 * ui;

  const bool tracked =
      report != nullptr && report->battle.tracked && !report->battle.sides.empty();

  QString verdict = QStringLiteral("SCENARIO COMPLETE");
  QString clock_line;
  QColor verdict_color = parchment;
  if (tracked) {
    const auto& battle = report->battle;
    if (battle.decided && !battle.victor_label.isEmpty()) {
      verdict = QStringLiteral("%1 WINS").arg(side_display_name(battle.victor_label));
      verdict_color = gold;
      clock_line =
          QStringLiteral("DECIDED AT %1").arg(format_clock(battle.decided_at_seconds));
    } else if (battle.decided) {
      verdict = QStringLiteral("MUTUAL DESTRUCTION");
      verdict_color = blood;
      clock_line =
          QStringLiteral("DECIDED AT %1").arg(format_clock(battle.decided_at_seconds));
    } else {
      verdict = QStringLiteral("STALEMATE");
      clock_line = QStringLiteral("NO DECISION AFTER %1")
                       .arg(format_clock(report->elapsed_seconds));
    }
  }
  y = text(y, 52.0, verdict_color, verdict);
  if (!clock_line.isEmpty()) {
    y += 4.0 * ui;
    y = text(y, 20.0, faded, clock_line);
  }
  y += 24.0 * ui;

  if (!tracked) {
    return card;
  }

  const std::size_t columns = std::min<std::size_t>(report->battle.sides.size(), 2U);
  const double column_top = y;
  double column_bottom = y;
  for (std::size_t index = 0; index < columns; ++index) {
    const auto& side = report->battle.sides[index];
    const double left = index == 0 ? 0.10 : 0.52;
    const double right = index == 0 ? 0.48 : 0.90;

    double row = column_top;
    row = text(row, 30.0, parchment, side_display_name(side.label), left, right);
    row += 4.0 * ui;
    const QString doctrine =
        QStringLiteral("%1 · %2").arg(side.strategy.toUpper(), side.posture.toUpper());
    row = text(row, 17.0, gold, doctrine.trimmed(), left, right);
    row += 14.0 * ui;
    row = text(row,
               19.0,
               parchment,
               QStringLiteral("UNITS RAISED %1  ·  PEAK ARMY %2")
                   .arg(side.units_produced)
                   .arg(side.peak_units),
               left,
               right);
    row = text(row,
               19.0,
               parchment,
               QStringLiteral("BUILDINGS RAISED %1").arg(side.buildings_constructed),
               left,
               right);
    if (!side.building_census.isEmpty()) {
      painter.setFont(card_font(ui, 14.0));
      painter.setPen(faded);
      const QRectF census_rect(
          width * left, row, width * (right - left), 14.0 * ui * 4.0);
      const int census_flags = Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap;
      const QString census = census_display(side.building_census);
      painter.drawText(census_rect, census_flags, census);
      row +=
          painter.boundingRect(census_rect, census_flags, census).height() + (6.0 * ui);
    }
    row += 10.0 * ui;
    row = text(row,
               19.0,
               parchment,
               QStringLiteral("STANDING %1 UNITS  ·  %2 BUILDINGS")
                   .arg(side.living_units)
                   .arg(side.living_buildings),
               left,
               right);
    row = text(row,
               19.0,
               parchment,
               QStringLiteral("TIME ON THE ATTACK %1")
                   .arg(format_clock(side.seconds_attacking)),
               left,
               right);
    row += 12.0 * ui;
    if (side.eliminated_at >= 0.0F) {
      row = text(row,
                 21.0,
                 blood,
                 QStringLiteral("FELL AT %1").arg(format_clock(side.eliminated_at)),
                 left,
                 right);
    } else {
      row = text(row, 21.0, laurel, QStringLiteral("STANDS AT THE END"), left, right);
    }
    column_bottom = std::max(column_bottom, row);
  }

  painter.setPen(QPen(QColor(faded.red(), faded.green(), faded.blue(), 90),
                      std::max(1.0, 2.0 * ui)));
  painter.drawLine(QPointF(width * 0.5, column_top + (6.0 * ui)),
                   QPointF(width * 0.5, column_bottom));

  line(column_bottom + (26.0 * ui), QColor(gold.red(), gold.green(), gold.blue(), 140));
  return card;
}

auto brightest_sample(const QImage& frame) -> int {
  if (frame.isNull()) {
    return 0;
  }
  const QImage probe =
      frame.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::FastTransformation)
          .convertToFormat(QImage::Format_Grayscale8);
  int peak = 0;
  for (int y = 0; y < probe.height(); ++y) {
    const uchar* line = probe.constScanLine(y);
    for (int x = 0; x < probe.width(); ++x) {
      peak = std::max(peak, int(line[x]));
    }
  }
  return peak;
}

struct ShotResult {
  QString name;
  QString scenario;
  QString clip_path;
  QString poster_path;
  int frames{0};
  float scene_duration{0.0F};
  float clip_duration{0.0F};
};

class Recorder {
public:
  Recorder(ArenaViewport& viewport, const Spec& spec, RunOptions options)
      : m_viewport(viewport)
      , m_spec(spec)
      , m_options(std::move(options))
      , m_passes(plan_passes(spec))
      , m_results(spec.shots.size()) {}

  auto start(QString* error) -> bool {
    if (!VideoEncoder::ffmpeg_available()) {
      if (error != nullptr) {
        *error = QStringLiteral("ffmpeg is required for --promo-spec but was not "
                                "found on PATH");
      }
      return false;
    }
    if (!QDir().mkpath(m_options.output_directory)) {
      if (error != nullptr) {
        *error = QStringLiteral("could not create promo output directory %1")
                     .arg(m_options.output_directory);
      }
      return false;
    }

    m_viewport.set_promo_mode(true);
    m_viewport.set_capture_resolution(m_spec.width * m_spec.supersample,
                                      m_spec.height * m_spec.supersample);
    m_viewport.set_capture_sink([this](const QImage& frame) { on_frame(frame); });
    m_viewport.set_frame_hook([this](float scenario_time) { on_tick(scenario_time); });
    QTimer::singleShot(250, [this]() { begin_next_pass(); });
    return true;
  }

private:
  [[nodiscard]] auto current_shot() const -> const Shot& {
    return m_spec.shots[m_passes[m_pass_index].shots[m_slot_index]];
  }

  [[nodiscard]] auto current_shot_index() const -> std::size_t {
    return m_passes[m_pass_index].shots[m_slot_index];
  }

  void begin_next_pass() {
    if (m_pass_index >= m_passes.size()) {
      finish_run();
      return;
    }
    const CapturePass& pass = m_passes[m_pass_index];
    if (Arena::Scenarios::find_definition(pass.scenario) == nullptr) {
      qCritical().noquote() << QStringLiteral("Promo pass names unknown scenario '%1'")
                                   .arg(pass.scenario);
      m_failed = true;
      ++m_pass_index;
      QTimer::singleShot(0, [this]() { begin_next_pass(); });
      return;
    }

    float last_end = 0.0F;
    for (std::size_t index : pass.shots) {
      const Shot& shot = m_spec.shots[index];
      last_end = std::max(last_end, shot.start_seconds + shot.duration_seconds);
    }

    m_slot_index = 0;
    m_pass_active = true;
    m_pass_frames = 0;
    m_viewport.set_terrain_seed(pass.seed);
    m_viewport.set_batch_fixed_step(idle_step());
    m_viewport.set_batch_render_suppressed(false);
    m_viewport.set_scenario_duration_override(last_end + k_scenario_tail_seconds);
    m_viewport.set_capture_active(false);
    m_viewport.clear_cinematic_view();
    m_viewport.load_scenario(pass.scenario);
    if (m_spec.audio) {
      m_audio = std::make_unique<AudioRecorder>();
      if (!m_audio->start(m_viewport.world(), m_viewport.session().nations())) {
        qWarning().noquote() << QStringLiteral(
                                    "Promo pass over '%1' runs without audio")
                                    .arg(pass.scenario);
        m_audio.reset();
      } else {
        m_audio->play_music_bed(m_spec.music_track, m_spec.music_volume);
      }
    }

    qInfo().noquote() << QStringLiteral("Promo pass %1/%2: %3, %4 shot(s) across "
                                        "%5 s of scenario")
                             .arg(m_pass_index + 1U)
                             .arg(m_passes.size())
                             .arg(pass.scenario)
                             .arg(pass.shots.size())
                             .arg(QString::number(last_end, 'f', 1));

    const std::size_t guarded_pass = m_pass_index;
    const int watchdog_ms =
        std::max(k_pass_watchdog_ms,
                 static_cast<int>(last_end * 1000.0F * k_pass_watchdog_scale));
    QTimer::singleShot(watchdog_ms, [this, guarded_pass]() {
      if (m_pass_active && m_pass_index == guarded_pass) {
        qCritical().noquote() << QStringLiteral("Promo pass over scenario '%1' "
                                                "exceeded its watchdog")
                                     .arg(m_passes[guarded_pass].scenario);
        m_failed = true;
        end_shot();
        end_pass();
      }
    });
    begin_shot();
  }

  [[nodiscard]] auto idle_step() const -> float {
    return 1.0F / static_cast<float>(m_spec.fps);
  }

  void begin_shot() {
    if (!m_pass_active) {
      return;
    }
    if (m_slot_index >= m_passes[m_pass_index].shots.size()) {
      end_pass();
      return;
    }
    const Shot& shot = current_shot();
    m_clip_path =
        QDir(m_options.output_directory)
            .filePath(QStringLiteral("%1_%2.mp4")
                          .arg(current_shot_index() + 1U, 2, 10, QLatin1Char('0'))
                          .arg(shot.name));
    QString encoder_error;
    m_encoder = std::make_unique<VideoEncoder>();
    if (!m_encoder->open(
            m_clip_path, m_spec.width, m_spec.height, m_spec.fps, &encoder_error)) {
      qCritical().noquote()
          << QStringLiteral("Promo shot '%1': %2").arg(shot.name, encoder_error);
      m_failed = true;
      m_encoder.reset();
      ++m_slot_index;
      begin_shot();
      return;
    }

    m_step_seconds = 1.0F / (static_cast<float>(m_spec.fps) * shot.slow_motion);
    m_target_frames = std::max(
        1, static_cast<int>(std::lround(shot.duration_seconds / m_step_seconds)));
    m_frames_written = 0;
    m_black_frames_skipped = 0;
    m_focus_valid = false;
    m_logged_framing = false;
    m_shot_active = true;
    m_shot_armed = false;
    m_card_active = false;
    m_card_frames_written = 0;
    m_card_frames_target = 0;
    m_card_image.reset();
    m_last_frame.reset();
    m_viewport.set_batch_fixed_step(idle_step());
    m_viewport.set_flame_card(shot.flame_card, shot.flame_speed, shot.flame_intensity);
    m_viewport.set_capture_gameplay_ui(shot.gameplay_ui && !shot.flame_card,
                                       shot.gameplay_ui_all_owners);

    qInfo().noquote() << QStringLiteral("  shot %1: %2 (%3 frames at %4x%5, from "
                                        "%6 s)")
                             .arg(current_shot_index() + 1U)
                             .arg(shot.name)
                             .arg(m_target_frames)
                             .arg(m_spec.width)
                             .arg(m_spec.height)
                             .arg(QString::number(shot.start_seconds, 'f', 1));
  }

  void on_tick(float scenario_time) {
    ++m_pass_frames;
    static const bool log_progress =
        !qEnvironmentVariableIsEmpty("SOI_PROMO_LOG_SIDES");
    if (log_progress) {
      const int bucket = static_cast<int>(scenario_time / 15.0F);
      if (bucket != m_logged_bucket) {
        m_logged_bucket = bucket;
        const auto describe = [](const std::optional<QVector3D>& point) {
          return point.has_value() ? QStringLiteral("(%1, %2)")
                                         .arg(QString::number(point->x(), 'f', 1),
                                              QString::number(point->z(), 'f', 1))
                                   : QStringLiteral("none");
        };
        qInfo().noquote()
            << QStringLiteral("  sides at %1 s: %2 battle %3 army2 %4 army3 %5")
                   .arg(QString::number(scenario_time, 'f', 1))
                   .arg(m_viewport.ai_activity_summary())
                   .arg(describe(m_viewport.scenario_battle_center(0, 30.0F)))
                   .arg(describe(m_viewport.scenario_army_center(2, 24.0F)))
                   .arg(describe(m_viewport.scenario_army_center(3, 24.0F)));
      }
    }
    if (!m_shot_active) {
      return;
    }
    if (m_card_active) {
      tick_report_card();
      return;
    }
    const Shot& shot = current_shot();
    const float shot_time = std::max(0.0F, scenario_time - shot.start_seconds);

    QVector3D target;
    if (shot.gameplay_camera || shot.flame_card) {

      m_viewport.clear_cinematic_view();
    } else {
      const QVector3D focus = resolve_focus(shot);
      Pose pose = evaluate(shot.keys, shot_time);
      target = focus + shot.focus.offset + QVector3D(0.0F, pose.height, 0.0F);
      if (shot.shake > 0.0F) {
        target += shake_offset(m_frames_written, shot.shake);
      }
      m_viewport.set_cinematic_view(
          target, pose.distance, pose.pitch, pose.yaw, pose.fov, pose.roll);
    }

    const bool in_window =
        scenario_time >= shot.start_seconds && m_pass_frames > k_pass_warmup_frames;
    const bool far_from_window =
        !in_window && m_pass_frames > k_pass_warmup_frames &&
        shot.start_seconds - scenario_time > k_fast_forward_margin_seconds;
    static const bool fast_forward_enabled =
        qEnvironmentVariableIsEmpty("SOI_PROMO_NO_FASTFORWARD");
    m_viewport.set_batch_render_suppressed(fast_forward_enabled && far_from_window);
    if (in_window && !m_shot_armed) {
      m_shot_armed = true;
      m_viewport.set_batch_fixed_step(m_step_seconds);
      m_viewport.set_capture_active(false);
      if (m_audio != nullptr) {
        m_audio->advance(idle_step(), false);
        m_audio->begin_clip();
      }
      return;
    }
    const bool recording = in_window && m_frames_written < m_target_frames;
    m_viewport.set_capture_active(recording);
    if (m_audio != nullptr) {
      m_audio->advance(m_shot_armed ? m_step_seconds : idle_step(), recording);
    }

    if (recording && !m_logged_framing) {
      m_logged_framing = true;
      qInfo().noquote() << QStringLiteral("  at %1 s: %2")
                               .arg(QString::number(scenario_time, 'f', 1))
                               .arg(m_viewport.ai_activity_summary());
      const auto& stats = Render::GL::get_humanoid_render_stats();
      qInfo().noquote() << QStringLiteral(
                               "  focus (%1, %2, %3) %4; soldiers %5/%6 drawn, culled "
                               "frustum %7 fog %8 lod %9")
                               .arg(QString::number(target.x(), 'f', 1),
                                    QString::number(target.y(), 'f', 1),
                                    QString::number(target.z(), 'f', 1),
                                    m_focus_valid ? QStringLiteral("tracked")
                                                  : QStringLiteral("UNRESOLVED"))
                               .arg(stats.soldiers_rendered)
                               .arg(stats.soldiers_total)
                               .arg(stats.soldiers_skipped_frustum)
                               .arg(stats.soldiers_skipped_fog)
                               .arg(stats.soldiers_skipped_lod);
    }

    if (!recording && m_frames_written >= m_target_frames) {
      if (begin_report_card()) {
        return;
      }
      end_shot();
      advance_within_pass();
      return;
    }

    if (m_viewport.active_scenario_finished() && m_frames_written < m_target_frames) {
      qWarning().noquote() << QStringLiteral(
                                  "Promo shot '%1' ended early with %2 of %3 frames")
                                  .arg(shot.name)
                                  .arg(m_frames_written)
                                  .arg(m_target_frames);
      if (begin_report_card()) {
        return;
      }
      end_shot();
      end_pass();
    }
  }

  [[nodiscard]] auto begin_report_card() -> bool {
    if (m_card_active || m_encoder == nullptr || m_frames_written == 0 ||
        current_shot().report_card_seconds <= 0.0F) {
      return false;
    }
    m_card_active = true;
    m_card_frames_written = 0;
    m_card_dissolve_from = m_last_frame;
    m_card_frames_target =
        std::max(1,
                 static_cast<int>(std::lround(current_shot().report_card_seconds *
                                              static_cast<float>(m_spec.fps))));
    m_viewport.set_capture_active(false);
    m_viewport.set_batch_render_suppressed(true);
    qInfo().noquote() << QStringLiteral("  closing on the battle report (%1 s)")
                             .arg(QString::number(
                                 current_shot().report_card_seconds, 'f', 1));
    return true;
  }

  void tick_report_card() {
    m_viewport.set_capture_active(false);
    if (!m_viewport.active_scenario_finished()) {

      return;
    }
    if (!m_card_image.has_value()) {
      m_card_image =
          m_spec.report_card_style == ReportCardStyle::Matchup
              ? paint_matchup_card(m_spec, m_viewport.active_scenario_report())
              : paint_report_card(m_spec, m_viewport.active_scenario_report());
      m_last_frame = m_card_image;

      if (m_audio != nullptr) {
        const auto* report = m_viewport.active_scenario_report();
        const bool decided = report != nullptr && report->battle.tracked &&
                             report->battle.decided &&
                             !report->battle.victor_label.isEmpty();
        const QString& sound =
            decided ? m_spec.report_sound_decided : m_spec.report_sound_undecided;
        m_audio->play_one_shot(sound, m_spec.report_sound_volume);
      }
    }

    const QImage& card = *m_card_image;
    QImage composed = card;
    const int dissolve_frames =
        std::max(1,
                 static_cast<int>(std::lround(static_cast<double>(m_spec.fps) *
                                              k_card_dissolve_seconds)));
    if (m_card_dissolve_from.has_value() && m_card_frames_written < dissolve_frames &&
        m_card_dissolve_from->size() == card.size()) {
      const double linear = static_cast<double>(m_card_frames_written + 1) /
                            static_cast<double>(dissolve_frames);
      const double eased = linear * linear * (3.0 - 2.0 * linear);
      composed = *m_card_dissolve_from;
      if (composed.format() != QImage::Format_RGB32 &&
          composed.format() != QImage::Format_ARGB32) {
        composed = composed.convertToFormat(QImage::Format_RGB32);
      }
      QPainter painter(&composed);
      painter.setOpacity(eased);
      painter.drawImage(0, 0, card);
    }

    QString error;
    if (!m_encoder->write_frame(composed, &error)) {
      qCritical().noquote() << QStringLiteral("Promo encode failed: %1").arg(error);
      m_failed = true;
      m_card_active = false;
      end_shot();
      end_pass();
      return;
    }
    ++m_frames_written;
    ++m_card_frames_written;
    if (m_audio != nullptr) {
      m_audio->advance(idle_step(), true);
    }
    if (m_card_frames_written >= m_card_frames_target) {
      m_card_active = false;
      end_shot();
      end_pass();
    }
  }

  void on_frame(const QImage& frame) {
    if (!m_shot_active || m_encoder == nullptr) {
      return;
    }
    QImage output = frame;
    if (m_spec.supersample > 1) {
      output = frame.scaled(
          m_spec.width, m_spec.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (current_shot().rpg_hud) {
      if (output.format() != QImage::Format_ARGB32 &&
          output.format() != QImage::Format_RGB32) {
        output = output.convertToFormat(QImage::Format_ARGB32);
      }
      paint_rpg_bow_hud(output, m_viewport.rpg_bow_hud_state());
    }

    if (m_frames_written == 0) {
      const int peak = brightest_sample(output);
      if (peak < k_first_frame_min_peak) {
        if (m_black_frames_skipped < k_max_black_frames_skipped) {
          ++m_black_frames_skipped;
          return;
        }
        qWarning().noquote() << QStringLiteral(
                                    "Promo shot '%1' is still black after %2 frames; "
                                    "recording it anyway")
                                    .arg(current_shot().name)
                                    .arg(m_black_frames_skipped);
      } else if (m_black_frames_skipped > 0) {
        qInfo().noquote() << QStringLiteral("  skipped %1 black lead-in frame(s)")
                                 .arg(m_black_frames_skipped);
      }
    }

    if (current_shot().gameplay_ui && !current_shot().flame_card) {
      if (output.format() != QImage::Format_ARGB32 &&
          output.format() != QImage::Format_RGB32) {
        output = output.convertToFormat(QImage::Format_ARGB32);
      }
      m_viewport.paint_capture_gameplay_ui(output);
    }

    QString error;
    if (!m_encoder->write_frame(output, &error)) {
      qCritical().noquote() << QStringLiteral("Promo encode failed: %1").arg(error);
      m_failed = true;
      end_shot();
      end_pass();
      return;
    }
    ++m_frames_written;
    m_last_frame = output;
  }

  auto resolve_focus(const Shot& shot) -> QVector3D {
    std::optional<QVector3D> raw;
    switch (shot.focus.mode) {
    case FocusMode::Point:
      raw = shot.focus.point;
      break;
    case FocusMode::Group:
      raw = m_viewport.scenario_group_center(shot.focus.group);
      break;
    case FocusMode::GroupPair: {
      const auto first = m_viewport.scenario_group_center(shot.focus.group);
      const auto second = m_viewport.scenario_group_center(shot.focus.second_group);
      if (first.has_value() && second.has_value()) {
        raw = (*first + *second) * 0.5F;
      } else if (first.has_value()) {
        raw = first;
      } else {
        raw = second;
      }
      break;
    }
    case FocusMode::AllUnits:
      raw = m_viewport.scenario_center_of_mass();
      break;
    case FocusMode::Battle:
      raw = m_viewport.scenario_battle_center(shot.focus.owner,
                                              shot.focus.engagement_radius);
      if (!raw.has_value() && !m_focus_valid) {
        raw = shot.focus.point;
      }
      break;
    case FocusMode::Army:
      raw = m_viewport.scenario_army_center(shot.focus.owner, shot.focus.home_radius);
      if (!raw.has_value() && !m_focus_valid) {
        raw = shot.focus.point;
      }
      break;
    }

    if (!raw.has_value()) {
      return m_focus_valid ? m_smoothed_focus : QVector3D{};
    }
    if (!m_focus_valid || shot.focus.smoothing <= 0.0F) {
      m_smoothed_focus = *raw;
      m_focus_valid = true;
      return m_smoothed_focus;
    }
    const float blend =
        1.0F - std::exp(-m_step_seconds / std::max(0.01F, shot.focus.smoothing));
    m_smoothed_focus += (*raw - m_smoothed_focus) * blend;
    return m_smoothed_focus;
  }

  void end_shot() {
    if (!m_shot_active) {
      return;
    }
    m_shot_active = false;
    m_viewport.set_capture_active(false);
    m_viewport.set_batch_fixed_step(idle_step());
    m_viewport.set_flame_card(false);

    const std::size_t shot_index = current_shot_index();
    const Shot& shot = m_spec.shots[shot_index];
    QString error;
    if (m_encoder != nullptr && !m_encoder->close(&error)) {
      qCritical().noquote()
          << QStringLiteral("Promo shot '%1': %2").arg(shot.name, error);
      m_failed = true;
    }
    m_encoder.reset();

    if (m_audio != nullptr && m_frames_written > 0) {
      const QString wav_path = m_clip_path + QStringLiteral(".wav");
      QString audio_error;
      if (!m_audio->write_clip(wav_path)) {
        qWarning().noquote() << QStringLiteral(
                                    "Promo shot '%1': audio track not written")
                                    .arg(shot.name);
      } else if (!AudioRecorder::mux(m_clip_path, wav_path, &audio_error)) {
        qWarning().noquote()
            << QStringLiteral("Promo shot '%1': %2").arg(shot.name, audio_error);
      } else {
        qInfo().noquote() << QStringLiteral("  muxed %1 s of game audio into %2")
                                 .arg(QString::number(m_audio->clip_seconds(), 'f', 2))
                                 .arg(QFileInfo(m_clip_path).fileName());
      }
      m_audio->begin_clip();
    }

    ShotResult result;
    result.name = shot.name;
    result.scenario = shot.scenario;
    result.clip_path = m_clip_path;
    result.frames = m_frames_written;
    result.scene_duration = static_cast<float>(m_frames_written) * m_step_seconds;
    result.clip_duration =
        static_cast<float>(m_frames_written) / static_cast<float>(m_spec.fps);
    if (m_options.write_posters && m_last_frame.has_value()) {
      result.poster_path =
          QDir(m_options.output_directory)
              .filePath(QStringLiteral("%1_%2.png")
                            .arg(shot_index + 1U, 2, 10, QLatin1Char('0'))
                            .arg(shot.name));
      m_last_frame->save(result.poster_path);
    }
    m_results[shot_index] = result;

    qInfo().noquote() << QStringLiteral("  wrote %1 (%2 frames, %3 s)")
                             .arg(QFileInfo(m_clip_path).fileName())
                             .arg(m_frames_written)
                             .arg(QString::number(result.clip_duration, 'f', 2));
  }

  void advance_within_pass() {
    ++m_slot_index;
    begin_shot();
  }

  void end_pass() {
    if (!m_pass_active) {
      return;
    }
    m_pass_active = false;
    m_audio.reset();
    ++m_pass_index;
    QTimer::singleShot(0, [this]() { begin_next_pass(); });
  }

  void finish_run() {
    m_viewport.set_capture_sink(nullptr);
    m_viewport.set_frame_hook(nullptr);
    m_viewport.set_batch_render_suppressed(false);
    write_manifest();
    qInfo().noquote() << QStringLiteral("Promo capture complete: %1 shot(s) in %2")
                             .arg(recorded_count())
                             .arg(QDir(m_options.output_directory).absolutePath());
    QApplication::exit(m_failed ? 1 : 0);
  }

  [[nodiscard]] auto recorded_count() const -> std::size_t {
    return static_cast<std::size_t>(
        std::count_if(m_results.begin(), m_results.end(), [](const ShotResult& result) {
          return result.frames > 0;
        }));
  }

  void write_manifest() {
    QJsonArray shots;
    for (const ShotResult& result : m_results) {
      if (result.frames <= 0) {
        continue;
      }
      shots.append(QJsonObject{
          {QStringLiteral("name"), result.name},
          {QStringLiteral("scenario"), result.scenario},
          {QStringLiteral("clip"), QFileInfo(result.clip_path).fileName()},
          {QStringLiteral("poster"),
           result.poster_path.isEmpty() ? QString{}
                                        : QFileInfo(result.poster_path).fileName()},
          {QStringLiteral("frames"), result.frames},
          {QStringLiteral("scene_seconds"), result.scene_duration},
          {QStringLiteral("clip_seconds"), result.clip_duration}});
    }
    const QJsonObject manifest{{QStringLiteral("id"), m_spec.id},
                               {QStringLiteral("title"), m_spec.title},
                               {QStringLiteral("width"), m_spec.width},
                               {QStringLiteral("height"), m_spec.height},
                               {QStringLiteral("fps"), m_spec.fps},
                               {QStringLiteral("shots"), shots}};
    QFile file(QDir(m_options.output_directory).filePath(QStringLiteral("shots.json")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }
  }

  ArenaViewport& m_viewport;
  const Spec& m_spec;
  RunOptions m_options;
  std::vector<CapturePass> m_passes;
  std::unique_ptr<VideoEncoder> m_encoder;
  std::vector<ShotResult> m_results;
  std::optional<QImage> m_last_frame;
  std::optional<QImage> m_card_image;
  std::optional<QImage> m_card_dissolve_from;
  int m_card_frames_written{0};
  int m_card_frames_target{0};
  bool m_card_active{false};
  QString m_clip_path;
  QVector3D m_smoothed_focus;
  std::size_t m_pass_index{0};
  std::size_t m_slot_index{0};
  int m_target_frames{0};
  int m_frames_written{0};
  int m_pass_frames{0};
  int m_black_frames_skipped{0};
  float m_step_seconds{1.0F / 60.0F};
  bool m_pass_active{false};
  bool m_shot_active{false};
  bool m_shot_armed{false};
  bool m_focus_valid{false};
  bool m_logged_framing{false};
  std::unique_ptr<AudioRecorder> m_audio;
  int m_logged_bucket{-1};
  bool m_failed{false};
};

} // namespace

auto run(ArenaViewport& viewport,
         const Spec& spec,
         const RunOptions& options,
         QString* error) -> int {
  Recorder recorder(viewport, spec, options);
  if (!recorder.start(error)) {
    return 2;
  }
  return QApplication::exec();
}

} // namespace Arena::Promo
