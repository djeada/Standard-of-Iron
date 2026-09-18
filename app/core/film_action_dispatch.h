#pragma once

#include <QPointF>

#include <optional>

class GameEngine;
class QQuickWindow;

namespace App::Core {

struct BenchmarkAction;

void apply_benchmark_action(GameEngine* engine,
                            QQuickWindow* window,
                            const BenchmarkAction& action);

[[nodiscard]] auto
resolve_action_pointer(GameEngine* engine,
                       QQuickWindow* window,
                       const BenchmarkAction& action) -> std::optional<QPointF>;

[[nodiscard]] auto
resolve_drag_origin(GameEngine* engine,
                    QQuickWindow* window,
                    const BenchmarkAction& action) -> std::optional<QPointF>;

[[nodiscard]] auto action_is_click(const BenchmarkAction& action) -> bool;

} // namespace App::Core
