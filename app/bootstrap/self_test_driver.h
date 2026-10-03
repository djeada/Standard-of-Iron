#pragma once

class GameEngine;
class QGuiApplication;
class QObject;
class QQuickWindow;

namespace App::Bootstrap {

// --renderer-self-test: show the gameplay view and exit once a frame has been
// presented. Returns -1 when the test is armed, otherwise the exit code.
auto start_renderer_self_test(QGuiApplication& app,
                              QObject* root_obj,
                              QQuickWindow* window) -> int;

// --release-self-test: wait for the packaged mission to load, then exit once
// three gameplay frames have been presented after it.
void start_release_self_test(QGuiApplication& app,
                             QQuickWindow* window,
                             GameEngine* game_engine);

} // namespace App::Bootstrap
