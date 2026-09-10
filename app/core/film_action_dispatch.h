#pragma once

class GameEngine;
class QQuickWindow;

namespace App::Core {

struct BenchmarkAction;

void apply_benchmark_action(GameEngine* engine,
                            QQuickWindow* window,
                            const BenchmarkAction& action);

} // namespace App::Core
