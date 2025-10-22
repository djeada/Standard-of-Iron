#pragma once

#include <QVector3D>
#include <cstddef>

namespace Render::GL {

static constexpr std::size_t MAX_EXTRAS_CACHE_SIZE = 10000;

static constexpr float MOUNTED_KNIGHT_ATTACK_CYCLE_TIME = 0.70f;
static constexpr float MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME =
    1.0f / MOUNTED_KNIGHT_ATTACK_CYCLE_TIME;

static constexpr float SPEARMAN_ATTACK_CYCLE_TIME = 0.80f;
static constexpr float SPEARMAN_INV_ATTACK_CYCLE_TIME =
    1.0f / SPEARMAN_ATTACK_CYCLE_TIME;

static constexpr float KNIGHT_ATTACK_CYCLE_TIME = 0.60f;
static constexpr float KNIGHT_INV_ATTACK_CYCLE_TIME =
    1.0f / KNIGHT_ATTACK_CYCLE_TIME;

static constexpr float ARCHER_ATTACK_CYCLE_TIME = 1.20f;
static constexpr float ARCHER_INV_ATTACK_CYCLE_TIME =
    1.0f / ARCHER_ATTACK_CYCLE_TIME;

static const QVector3D STEEL_TINT(0.95f, 0.96f, 1.0f);
static const QVector3D IRON_TINT(0.88f, 0.90f, 0.92f);

static const QVector3D BRASS_TINT(1.3f, 1.1f, 0.7f);

static const QVector3D CHAINMAIL_TINT(0.85f, 0.88f, 0.92f);

static const QVector3D DARK_METAL(0.15f, 0.15f, 0.15f);
static const QVector3D VISOR_COLOR(0.1f, 0.1f, 0.1f);

} // namespace Render::GL
