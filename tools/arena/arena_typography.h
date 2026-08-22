#ifndef ARENA_TYPOGRAPHY_H
#define ARENA_TYPOGRAPHY_H

#include <QFont>

namespace Arena::Typography {

auto number(double ui) -> QFont;

auto small_label(double ui) -> QFont;

} // namespace Arena::Typography

#endif
