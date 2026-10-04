#pragma once

class GameEngine;
class QGuiApplication;
class QObject;
class QQuickWindow;

namespace App::Bootstrap {

auto start_renderer_self_test(QGuiApplication& app,
                              QObject* root_obj,
                              QQuickWindow* window) -> int;

void start_release_self_test(QGuiApplication& app,
                             QQuickWindow* window,
                             GameEngine* game_engine);

} // namespace App::Bootstrap
