#pragma once

#include <QImage>

#include "arena_casting.h"

namespace Arena::Promo {

void paint_casting_overlay(QImage& frame, const ArenaCastingSnapshot& snapshot);

[[nodiscard]] auto casting_overlay_height(int frame_height) -> int;

} // namespace Arena::Promo
