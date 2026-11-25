#pragma once

#include <QVector3D>

#include <array>

#include "render/entity/building_archetype_desc.h"

namespace Render::GL {

struct MarketPaving {
  float min_x{-1.0F};
  float max_x{1.0F};
  float min_z{-1.0F};
  float max_z{1.0F};
  float cell{0.30F};
  float gap{0.010F};
  float base_y{0.0F};
  float min_rise{0.008F};
  float max_rise{0.012F};
  std::array<QVector3D, 3> tones{};
  int seed{0};
};

struct MarketGoodsPalette {
  QVector3D clay;
  QVector3D clay_dark;
  QVector3D wicker;
  QVector3D wicker_dark;
  QVector3D burlap;
  QVector3D timber;
  QVector3D timber_dark;
  QVector3D metal;
  QVector3D rope;
};

struct MarketStall {
  float cx{0.0F};
  float cz{0.0F};
  float side{1.0F};
  float ground_y{0.0F};
  float counter_top{0.35F};
  float back_post_top{0.87F};
  float front_post_top{0.75F};
  QVector3D board;
  QVector3D apron;
  QVector3D apron_panel;
};

void add_market_paving(BuildingArchetypeDesc& desc, const MarketPaving& paving);

void add_market_amphora(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        const QVector3D& clay,
                        const QVector3D& band,
                        float scale = 1.0F,
                        bool slender = false);

void add_market_basket(BuildingArchetypeDesc& desc,
                       const QVector3D& base,
                       float radius,
                       const MarketGoodsPalette& palette,
                       const QVector3D& goods_a,
                       const QVector3D& goods_b);

void add_market_sack(BuildingArchetypeDesc& desc,
                     const QVector3D& base,
                     float radius,
                     float height,
                     const MarketGoodsPalette& palette,
                     const QVector3D& content,
                     bool open);

void add_market_crate(BuildingArchetypeDesc& desc,
                      const QVector3D& center,
                      const QVector3D& half,
                      const MarketGoodsPalette& palette,
                      bool lidded);

void add_market_fruit(BuildingArchetypeDesc& desc,
                      const QVector3D& centre,
                      float radius,
                      const QVector3D& colour);

void add_market_scale(BuildingArchetypeDesc& desc,
                      const QVector3D& base,
                      const MarketGoodsPalette& palette);

void add_market_stall(BuildingArchetypeDesc& desc,
                      const MarketStall& stall,
                      const MarketGoodsPalette& palette);

} // namespace Render::GL
