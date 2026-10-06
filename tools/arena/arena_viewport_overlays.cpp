#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "app/commander/commander_control_controller.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/component_commander.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "render/geom/arrow.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_profile.h"
#include "scene/camera.h"

using namespace arena_viewport_internal;

namespace {

auto sample_grid(const Game::Map::TerrainField& field, int x, int z) -> float {
  if (field.heights.empty() || field.width <= 0 || field.height <= 0) {
    return 0.0F;
  }
  x = std::clamp(x, 0, field.width - 1);
  z = std::clamp(z, 0, field.height - 1);
  return field.heights[static_cast<size_t>(z * field.width + x)];
}

auto terrain_world_position(const Game::Map::TerrainField& field,
                            int x,
                            int z) -> QVector3D {
  float const half_width = static_cast<float>(field.width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(field.height) * 0.5F - 0.5F;
  float const world_x = (static_cast<float>(x) - half_width) * field.tile_size;
  float const world_z = (static_cast<float>(z) - half_height) * field.tile_size;
  float const world_y = sample_grid(field, x, z);
  return {world_x, world_y, world_z};
}

} // namespace

void ArenaViewport::draw_debug_overlay(QPainter& painter) {
  if (!m_clean_capture && (m_scenario_runner == nullptr ||
                           !m_scenario_runner->definition().suppress_spawn_anchor)) {
    draw_spawn_anchor_marker(painter);
  }
  draw_selection_marquee(painter);
  if (m_normals_overlay_enabled) {
    draw_terrain_normals(painter);
  }
  if (m_pose_overlay_enabled) {
    draw_pose_overlay(painter);
  }
  if (m_combat_debug_overlay_enabled) {
    draw_combat_animation_overlay(painter);
  }
  if (!m_clean_capture) {
    draw_floating_numbers(painter);
  }
}

void ArenaViewport::draw_floating_numbers(QPainter& painter) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }
  m_feedback.draw(painter, [this](float x, float y, float z, QPointF& out) {
    return m_camera->world_to_screen(QVector3D(x, y, z), width(), height(), out);
  });
}

void ArenaViewport::draw_spawn_anchor_marker(QPainter& painter) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  QPointF screen_anchor;
  if (!Game::Systems::PickingService::world_to_screen(
          *m_camera, width(), height(), resolve_spawn_anchor_world(), screen_anchor)) {
    return;
  }

  QColor const marker_color = m_spawn_anchor_world_valid ? QColor(255, 210, 96, 235)
                                                         : QColor(190, 190, 190, 200);
  QString const label = m_spawn_anchor_world_valid ? QStringLiteral("Spawn")
                                                   : QStringLiteral("Spawn (default)");

  painter.save();
  painter.setPen(QPen(marker_color, 2.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(screen_anchor, 10.0, 10.0);
  painter.drawLine(screen_anchor + QPointF(-15.0, 0.0),
                   screen_anchor + QPointF(-4.0, 0.0));
  painter.drawLine(screen_anchor + QPointF(4.0, 0.0),
                   screen_anchor + QPointF(15.0, 0.0));
  painter.drawLine(screen_anchor + QPointF(0.0, -15.0),
                   screen_anchor + QPointF(0.0, -4.0));
  painter.drawLine(screen_anchor + QPointF(0.0, 4.0),
                   screen_anchor + QPointF(0.0, 15.0));
  painter.setBrush(marker_color);
  painter.drawEllipse(screen_anchor, 3.0, 3.0);

  QRectF const label_box(
      screen_anchor.x() + 14.0, screen_anchor.y() - 25.0, 110.0, 20.0);
  painter.fillRect(label_box, QColor(0, 0, 0, 140));
  painter.setPen(QColor(245, 245, 245, 230));
  painter.drawText(
      label_box.adjusted(6.0, 0.0, -6.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, label);
  painter.restore();
}

void ArenaViewport::draw_selection_marquee(QPainter& painter) {
  if (!m_selection_drag_active) {
    return;
  }

  QRect const rect = QRect(m_selection_anchor, m_selection_current).normalized();
  if (rect.width() < k_selection_drag_threshold &&
      rect.height() < k_selection_drag_threshold) {
    return;
  }

  painter.save();
  painter.setPen(QPen(QColor(86, 170, 255, 220), 1.5F, Qt::DashLine));
  painter.fillRect(rect, QColor(86, 170, 255, 45));
  painter.drawRect(rect);
  painter.restore();
}

void ArenaViewport::draw_terrain_normals(QPainter& painter) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }
  const auto& field = m_session.terrain().terrain_field();
  if (field.empty()) {
    return;
  }

  painter.setPen(QPen(QColor(80, 230, 180, 180), 1.0));

  int const step = std::max(4, std::min(field.width, field.height) / 18);
  for (int z = 0; z < field.height; z += step) {
    for (int x = 0; x < field.width; x += step) {
      float const h_l = sample_grid(field, x - 1, z);
      float const h_r = sample_grid(field, x + 1, z);
      float const h_d = sample_grid(field, x, z - 1);
      float const h_u = sample_grid(field, x, z + 1);

      QVector3D normal(-(h_r - h_l) / (2.0F * field.tile_size),
                       1.0F,
                       -(h_u - h_d) / (2.0F * field.tile_size));
      if (normal.lengthSquared() <= std::numeric_limits<float>::epsilon()) {
        continue;
      }
      normal.normalize();

      QVector3D const start = terrain_world_position(field, x, z);
      QVector3D const end = start + normal * 1.5F;

      QPointF screen_start;
      QPointF screen_end;
      if (!m_camera->world_to_screen(start, width(), height(), screen_start) ||
          !m_camera->world_to_screen(end, width(), height(), screen_end)) {
        continue;
      }
      painter.drawLine(screen_start, screen_end);
    }
  }
}

void ArenaViewport::draw_pose_overlay(QPainter& painter) {
  if (m_camera == nullptr || m_world == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    return;
  }

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }

    QVector3D const origin(
        transform->position.x, transform->position.y + 1.0F, transform->position.z);
    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const axis_length = std::max(0.8F, transform->scale.y * 1.5F);
    QVector3D const forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    QVector3D const right(std::cos(yaw_rad), 0.0F, -std::sin(yaw_rad));
    QVector3D const up(0.0F, 1.0F, 0.0F);

    struct AxisLine {
      QVector3D end;
      QColor color;
    };

    std::array<AxisLine, 3> const axes{
        AxisLine{origin + right * axis_length, QColor(255, 120, 120, 220)},
        AxisLine{origin + up * axis_length, QColor(120, 220, 255, 220)},
        AxisLine{origin + forward * axis_length, QColor(160, 255, 120, 220)}};

    QPointF screen_origin;
    if (!m_camera->world_to_screen(origin, width(), height(), screen_origin)) {
      continue;
    }

    for (const auto& axis : axes) {
      QPointF screen_end;
      if (!m_camera->world_to_screen(axis.end, width(), height(), screen_end)) {
        continue;
      }
      painter.setPen(QPen(axis.color, 2.0));
      painter.drawLine(screen_origin, screen_end);
    }
  }
}

void ArenaViewport::draw_combat_animation_overlay(QPainter& painter) {
  if (m_world == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    return;
  }

  auto const* debug_unit =
      Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(ids.front());
  if (debug_unit == nullptr) {
    return;
  }

  QFont font = painter.font();
  font.setPixelSize(12);
  painter.setFont(font);
  QFontMetrics const fm(font);
  int const line_h = fm.height() + 2;
  int const pad = 6;

  QStringList lines;
  auto const& unit = debug_unit->unit;
  lines << QStringLiteral("Combat Debug")
        << QStringLiteral("phase=%1 %2  attack=%3  var=%4  locomotion=%5")
               .arg(QString::fromLatin1(
                   Render::Profiling::combat_phase_name(unit.combat_phase)))
               .arg(unit.combat_phase_progress, 0, 'f', 2)
               .arg(unit.is_attacking ? QStringLiteral("on") : QStringLiteral("off"))
               .arg(unit.attack_variant)
               .arg(QString::fromLatin1(
                   Render::Profiling::locomotion_state_name(unit.locomotion_state)))
        << QStringLiteral("attackPhase=%1  meleeLock=%2  target=%3")
               .arg(unit.attack_phase, 0, 'f', 2)
               .arg(unit.is_in_melee_lock ? QStringLiteral("yes")
                                          : QStringLiteral("no"))
               .arg(unit.attack_target_id)
        << QStringLiteral("sources combat=%1 meleeLock=%2 elephant=%3")
               .arg(unit.attack_from_combat_state ? QStringLiteral("1")
                                                  : QStringLiteral("0"))
               .arg(unit.attack_from_melee_lock ? QStringLiteral("1")
                                                : QStringLiteral("0"))
               .arg(unit.elephant_attack_override ? QStringLiteral("1")
                                                  : QStringLiteral("0"));

  if (m_attack_scrub_enabled && ids.front() == m_attack_scrub_entity_id) {
    lines << QStringLiteral("scrub=%1").arg(m_attack_scrub_phase, 0, 'f', 2);
  }

  int soldier_lines = 0;
  for (const auto& soldier : debug_unit->soldiers) {
    if (soldier_lines >= 8) {
      lines << QStringLiteral("... %1 more soldiers")
                   .arg(static_cast<int>(debug_unit->soldiers.size()) - soldier_lines);
      break;
    }

    QStringList flags;
    if (soldier.transient_recovery_override) {
      flags << QStringLiteral("recovery");
    }
    if (soldier.visual_state_changed) {
      flags << QStringLiteral("state");
    }
    if (soldier.attack_phase_reset) {
      flags << QStringLiteral("phase-reset");
    }
    if (soldier.variant_changed) {
      flags << QStringLiteral("variant");
    }
    if (soldier.lod_changed) {
      flags << QStringLiteral("lod");
    }
    if (soldier.movement_state_changed) {
      flags << QStringLiteral("move");
    }
    if (soldier.churn_flagged) {
      flags << QStringLiteral("churn");
    }
    lines << QStringLiteral("#%1 %2 %3 ap=%4 state=%5 lod=%6 cull=%7 tps=%8 %9")
                 .arg(soldier.soldier_index)
                 .arg(QString::fromLatin1(Render::Profiling::soldier_visual_state_name(
                     soldier.visual_state)))
                 .arg(QString::fromLatin1(
                     Render::Profiling::combat_phase_name(soldier.combat_phase)))
                 .arg(soldier.attack_phase, 0, 'f', 2)
                 .arg(QString::fromLatin1(
                     Render::Profiling::animation_state_name(soldier.animation_state)))
                 .arg(soldier.lod)
                 .arg(QString::fromLatin1(
                     Render::Profiling::soldier_cull_reason_name(soldier.cull_reason)))
                 .arg(soldier.transitions_last_second)
                 .arg(flags.join(QLatin1Char(',')));
    ++soldier_lines;
  }

  int max_w = 0;
  for (const auto& line : lines) {
    max_w = std::max(max_w, fm.horizontalAdvance(line));
  }
  QRect const box(width() - max_w - pad * 3,
                  pad,
                  max_w + pad * 2,
                  static_cast<int>(lines.size()) * line_h + pad * 2);
  painter.fillRect(box, QColor(0, 0, 0, 160));
  painter.setPen(QColor(235, 235, 235, 230));
  for (int i = 0; i < lines.size(); ++i) {
    painter.drawText(
        box.left() + pad, box.top() + pad + (i + 1) * line_h - 2, lines[i]);
  }
}

void ArenaViewport::draw_stats_overlay(QPainter& painter) {
  if (width() <= 0 || height() <= 0) {
    return;
  }

  int player_count = 0;
  int enemy_count = 0;
  if (m_world != nullptr) {
    for (auto* entity : m_world->collect_entities_with<Engine::Core::UnitComponent>()) {
      auto* uc = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
      if (uc == nullptr || uc->health <= 0) {
        continue;
      }
      if (uc->owner_id == k_local_owner_id) {
        ++player_count;
      } else {
        ++enemy_count;
      }
    }
  }

  QFont font = painter.font();
  font.setPixelSize(13);
  font.setBold(true);
  painter.setFont(font);

  QFontMetrics const fm(font);
  int const line_h = fm.height() + 2;
  int const pad = 6;

  QStringList lines;
  auto const& profile = Render::Profiling::global_profile();
  lines << QStringLiteral("FPS: %1").arg(static_cast<int>(m_fps + 0.5F));
  lines << QStringLiteral("Time: %1").arg(lighting_summary());
  lines << QStringLiteral("Player: %1").arg(player_count);
  lines << QStringLiteral("Enemy:  %1").arg(enemy_count);
  lines << QStringLiteral("Avg/P95: %1 / %2 ms")
               .arg(profile.average_frame_ms, 0, 'f', 2)
               .arg(profile.p95_frame_ms, 0, 'f', 2);
  lines << QStringLiteral("Visible Soldiers: %1").arg(profile.visible_soldiers);
  lines << QStringLiteral("Cache Misses: %1").arg(profile.render_asset_cache_misses);
  lines << QStringLiteral("Anim Prep: %1 ms")
               .arg(static_cast<double>(profile.humanoid_preparation_us) / 1000.0,
                    0,
                    'f',
                    2);
  if (m_combat_debug_overlay_enabled) {
    lines << QStringLiteral("Churn Flags: %1")
                 .arg(static_cast<int>(
                     Render::Profiling::CombatAnimationDiagnostics::instance()
                         .flagged_unit_count()));
  }
  if (m_paused) {
    lines << QStringLiteral("PAUSED");
  }

  int const box_w = [&]() {
    int max_w = 0;
    for (const auto& line : lines) {
      max_w = std::max(max_w, fm.horizontalAdvance(line));
    }
    return max_w + pad * 2;
  }();
  int const box_h = lines.size() * line_h + pad * 2;

  QRect const box(pad, pad, box_w, box_h);
  painter.fillRect(box, QColor(0, 0, 0, 140));

  painter.setPen(QColor(220, 220, 220, 220));
  for (int i = 0; i < lines.size(); ++i) {
    painter.drawText(pad * 2, pad + (i + 1) * line_h - 2, lines[i]);
  }
}

void ArenaViewport::draw_rpg_hud(QPainter& painter) {
  if (!m_rpg_interactive || m_world == nullptr || m_rpg_commander_id == 0 ||
      width() <= 0 || height() <= 0) {
    return;
  }
  auto* commander = m_world->get_entity(m_rpg_commander_id);
  if (commander == nullptr) {
    return;
  }

  auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
  auto const* stamina = commander->get_component<Engine::Core::StaminaComponent>();
  auto const* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  auto const* motion =
      commander->get_component<Engine::Core::MotionPresentationComponent>();
  auto const* combat = commander->get_component<Engine::Core::CombatStateComponent>();

  int const bar_w = 240;
  int const bar_h = 14;
  int const pad = 10;
  int const left = pad;
  int y = height() - pad - (bar_h * 2) - 6 - 18;

  auto draw_bar = [&](float ratio, const QColor& fill, const QString& label) {
    QRect const outer(left, y, bar_w, bar_h);
    painter.fillRect(outer, QColor(0, 0, 0, 170));
    QRect inner = outer.adjusted(1, 1, -1, -1);
    inner.setWidth(static_cast<int>(static_cast<float>(inner.width()) *
                                    std::clamp(ratio, 0.0F, 1.0F)));
    painter.fillRect(inner, fill);
    painter.setPen(QColor(240, 240, 240, 235));
    painter.drawText(
        outer.adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    y += bar_h + 6;
  };

  QFont hud_font = painter.font();
  hud_font.setPixelSize(11);
  hud_font.setBold(true);
  painter.setFont(hud_font);

  auto const* commander_unit = commander->get_component<Engine::Core::UnitComponent>();
  if (commander_unit != nullptr && commander_unit->max_health > 0) {
    float const ratio = static_cast<float>(commander_unit->health) /
                        static_cast<float>(commander_unit->max_health);
    draw_bar(ratio,
             QColor(196, 62, 54, 225),
             QStringLiteral("HP  %1 / %2")
                 .arg(commander_unit->health)
                 .arg(commander_unit->max_health));
  }
  if (stamina != nullptr && stamina->max_stamina > 0.0F) {
    draw_bar(stamina->get_stamina_ratio(),
             stamina->is_running ? QColor(214, 168, 54, 225) : QColor(86, 150, 92, 225),
             QStringLiteral("STA %1%").arg(
                 static_cast<int>(stamina->get_stamina_ratio() * 100.0F)));
  }

  QStringList state_parts;
  if (motion != nullptr) {
    switch (motion->state) {
    case Engine::Core::MotionPresentationState::Run:
      state_parts << QStringLiteral("RUN");
      break;
    case Engine::Core::MotionPresentationState::Walk:
      state_parts << QStringLiteral("WALK");
      break;
    case Engine::Core::MotionPresentationState::Turning:
      state_parts << QStringLiteral("TURN");
      break;
    case Engine::Core::MotionPresentationState::Yielding:
      state_parts << QStringLiteral("YIELD");
      break;
    case Engine::Core::MotionPresentationState::Recovering:
      state_parts << QStringLiteral("RECOVER");
      break;
    case Engine::Core::MotionPresentationState::ForcedDisplacement:
      state_parts << QStringLiteral("FORCED");
      break;
    default:
      state_parts << QStringLiteral("IDLE");
      break;
    }
  }
  if (m_rpg_commander_controller != nullptr &&
      m_rpg_commander_controller->is_dodge_rolling()) {
    state_parts << QStringLiteral("DODGE");
  }
  if (guard != nullptr && guard->active) {
    state_parts << QStringLiteral("GUARD");
  }
  if (guard != nullptr && guard->guard_break_remaining > 0.0F) {
    state_parts << QStringLiteral("GUARD-BREAK");
  }
  if (rpg != nullptr && rpg->dodge_grace_remaining > 0.0F) {
    state_parts << QStringLiteral("ROLLING");
  }
  if (combat != nullptr &&
      combat->animation_state != Engine::Core::CombatAnimationState::Idle) {
    state_parts << QStringLiteral("ATTACK");
  }
  if (m_rpg_commander_controller != nullptr &&
      m_rpg_commander_controller->locked_target_id() != 0) {
    state_parts << QStringLiteral("LOCKED");
  }

  painter.setPen(QColor(240, 240, 240, 235));
  painter.drawText(left + 2, y + 12, state_parts.join(QStringLiteral("  ")));

  QPoint const center(width() / 2, height() / 2);
  painter.setPen(QColor(255, 255, 255, 150));
  painter.drawLine(center.x() - 7, center.y(), center.x() - 2, center.y());
  painter.drawLine(center.x() + 2, center.y(), center.x() + 7, center.y());
  painter.drawLine(center.x(), center.y() - 7, center.x(), center.y() - 2);
  painter.drawLine(center.x(), center.y() + 2, center.x(), center.y() + 7);
}

void ArenaViewport::draw_controls_overlay(QPainter& painter) {
  if (width() <= 0 || height() <= 0) {
    return;
  }

  QFont title_font = painter.font();
  title_font.setPixelSize(13);
  title_font.setBold(true);
  QFont body_font = title_font;
  body_font.setBold(false);

  if (m_rpg_interactive) {
    QString const rpg_title = QStringLiteral("RPG Commander Controls");
    QStringList const rpg_lines{
        QStringLiteral("W A S D: move (relative to view)"),
        QStringLiteral("Mouse: look"),
        QStringLiteral("Shift: run"),
        QStringLiteral("LMB: attack (hold to chain)"),
        QStringLiteral("MMB: heavy attack"),
        QStringLiteral("RMB: guard"),
        QStringLiteral("Space: dodge roll"),
        QStringLiteral("Ctrl: jump"),
        QStringLiteral("R: cycle lock-on"),
        QStringLiteral("C: close camera"),
        QStringLiteral("F: weapon special (guard + F: shield bash)"),
        QStringLiteral("X: switch weapon stance"),
        QStringLiteral("V: vanguard rush"),
        QStringLiteral("G: second wind"),
        QStringLiteral("Esc / Tab: leave RPG control"),
        QStringLiteral("F1 or ?: toggle this help"),
    };

    QFontMetrics const rpg_title_metrics(title_font);
    QFontMetrics const rpg_body_metrics(body_font);
    int const rpg_pad = 8;
    int const rpg_title_gap = 4;
    int const rpg_line_h = rpg_body_metrics.height() + 2;

    int rpg_box_w = rpg_title_metrics.horizontalAdvance(rpg_title);
    for (const auto& line : rpg_lines) {
      rpg_box_w = std::max(rpg_box_w, rpg_body_metrics.horizontalAdvance(line));
    }
    rpg_box_w += rpg_pad * 2;

    int const rpg_box_h = rpg_pad * 2 + rpg_title_metrics.height() + rpg_title_gap +
                          static_cast<int>(rpg_lines.size()) * rpg_line_h;
    QRect const rpg_box(width() - rpg_box_w - rpg_pad, rpg_pad, rpg_box_w, rpg_box_h);
    painter.fillRect(rpg_box, QColor(0, 0, 0, 150));
    painter.setPen(QColor(235, 235, 235, 230));

    int rpg_y = rpg_box.top() + rpg_pad + rpg_title_metrics.ascent();
    painter.setFont(title_font);
    painter.drawText(rpg_box.left() + rpg_pad, rpg_y, rpg_title);

    painter.setFont(body_font);
    rpg_y += rpg_title_gap + (rpg_title_metrics.descent() + rpg_line_h);
    for (const auto& line : rpg_lines) {
      painter.drawText(rpg_box.left() + rpg_pad, rpg_y, line);
      rpg_y += rpg_line_h;
    }
    return;
  }

  QString const title = QStringLiteral("Arena Controls");
  QStringList const lines{
      QStringLiteral("Tab: take direct RPG control of a commander"),
      QStringLiteral("LMB click: select + set spawn anchor"),
      QStringLiteral("LMB drag: box select"),
      QStringLiteral("Shift+LMB: add to selection"),
      QStringLiteral("RMB: move / attack like the main game"),
      QStringLiteral("Wheel: fast zoom (Shift: very fast)"),
      QStringLiteral("Middle drag: orbit"),
      QStringLiteral("Shift+Middle / Alt+RMB drag: fast pan"),
      QStringLiteral("Arrow keys: pan camera"),
      QStringLiteral("Shift+Arrows: faster pan"),
      QStringLiteral("Q / E: yaw"),
      QStringLiteral("R / F: tilt overhead / towards the horizon"),
      QStringLiteral("Home: show whole terrain"),
      QStringLiteral("X: select local army"),
      QStringLiteral("Space: pause"),
      QStringLiteral("F1 or ?: toggle this help"),
  };

  QFontMetrics const title_metrics(title_font);
  QFontMetrics const body_metrics(body_font);
  int const pad = 8;
  int const title_gap = 4;
  int const line_h = body_metrics.height() + 2;

  int box_w = title_metrics.horizontalAdvance(title);
  for (const auto& line : lines) {
    box_w = std::max(box_w, body_metrics.horizontalAdvance(line));
  }
  box_w += pad * 2;

  int const box_h = pad * 2 + title_metrics.height() + title_gap +
                    static_cast<int>(lines.size()) * line_h;
  QRect const box(width() - box_w - pad, pad, box_w, box_h);
  painter.fillRect(box, QColor(0, 0, 0, 150));
  painter.setPen(QColor(235, 235, 235, 230));

  int y = box.top() + pad + title_metrics.ascent();
  painter.setFont(title_font);
  painter.drawText(box.left() + pad, y, title);

  painter.setFont(body_font);
  y += title_gap + (title_metrics.descent() + line_h);
  for (const auto& line : lines) {
    painter.drawText(box.left() + pad, y, line);
    y += line_h;
  }
}
