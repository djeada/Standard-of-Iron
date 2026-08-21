#ifndef ARENA_TYPOGRAPHY_H
#define ARENA_TYPOGRAPHY_H

#include <QFont>

namespace Arena::Typography {

// The type the promo camera sees. Every string burned into a captured frame
// goes through one of these, so a reel rendered on a build box matches one
// rendered on a laptop -- previously each call site took `painter.font()`,
// which is whatever the host's default happened to be.
//
// Only two presets, because the arena only burns in two kinds of string.
// Headlines and subtitles are not here: those are the act cards, and they are
// composited afterwards by scripts/promo-edit.py, which owns its own sizing
// and resolves the same bundled face by path. Adding unused presets here would
// invite a second, silently disagreeing type scale for the same reels.
//
// Deliberately not used for the arena's own debug overlays and inspector HUD:
// those are tooling, they want a plain readable sans, and putting a display
// face on a nine-line combat dump only makes it harder to read.
//
// `ui` is the viewport's scale factor, the same one the promo runner already
// multiplies its pixel sizes by.

// Counters and tallies burned into the frame: takedowns, unit counts, timers.
auto number(double ui) -> QFont;

// Corner annotations and state stamps: "ARROW READY", "NOCKING", "STAMINA".
auto small_label(double ui) -> QFont;

} // namespace Arena::Typography

#endif
