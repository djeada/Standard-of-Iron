#pragma once

#include <QString>

#include <algorithm>

namespace App::Core {

[[nodiscard]] inline auto format_loading_overlay_line(int frame_index,
                                                      int frame_total,
                                                      qint64 now_ms,
                                                      qint64 previous_ms) -> QString {
  const qint64 since_previous_ms = std::max<qint64>(0, now_ms - previous_ms);
  return QStringLiteral("SOI_LOADING_OVERLAY: frame %1 of %2 presented at %3ms "
                        "(+%4ms since the previous one)")
      .arg(frame_index)
      .arg(frame_total)
      .arg(now_ms)
      .arg(since_previous_ms);
}

} // namespace App::Core
