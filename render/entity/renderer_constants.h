#pragma once

#include <QVector3D>
#include <cstddef>

namespace Render::GL {

static constexpr std::size_t MAX_EXTRAS_CACHE_SIZE = 10000;

static constexpr float MOUNTED_KNIGHT_ATTACK_CYCLE_TIME = 0.70F;
static constexpr float MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME =
    1.0F / MOUNTED_KNIGHT_ATTACK_CYCLE_TIME;

static constexpr float SPEARMAN_ATTACK_CYCLE_TIME = 0.80F;
static constexpr float SPEARMAN_INV_ATTACK_CYCLE_TIME =
    1.0F / SPEARMAN_ATTACK_CYCLE_TIME;

static constexpr float KNIGHT_ATTACK_CYCLE_TIME = 0.60F;
static constexpr float KNIGHT_INV_ATTACK_CYCLE_TIME =
    1.0F / KNIGHT_ATTACK_CYCLE_TIME;

static constexpr float ARCHER_ATTACK_CYCLE_TIME = 1.20F;
static constexpr float ARCHER_INV_ATTACK_CYCLE_TIME =
    1.0F / ARCHER_ATTACK_CYCLE_TIME;

static const QVector3D STEEL_TINT(0.95F, 0.96F, 1.0F);
static const QVector3D IRON_TINT(0.88F, 0.90F, 0.92F);

static const QVector3D BRASS_TINT(1.3F, 1.1F, 0.7F);

static const QVector3D CHAINMAIL_TINT(0.85F, 0.88F, 0.92F);

static const QVector3D DARK_METAL(0.15F, 0.15F, 0.15F);
static const QVector3D VISOR_COLOR(0.1F, 0.1F, 0.1F);

} 
