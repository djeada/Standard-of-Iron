#pragma once

#include <QSize>
#include <QString>

class QQuickWindow;

namespace App::Bootstrap {

void capture_screenshot_and_exit(QQuickWindow* window,
                                 const QString& path,
                                 const QString& view,
                                 int delay_ms,
                                 QSize size);

} // namespace App::Bootstrap
