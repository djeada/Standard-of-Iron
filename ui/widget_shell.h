#ifndef SOI_UI_WIDGET_SHELL_H
#define SOI_UI_WIDGET_SHELL_H

class QApplication;
class QMainWindow;
class QString;
class QWidget;

namespace UiShell {

void apply(QApplication& app);

void prepare_tool_window(QMainWindow& window);

void capture_and_exit(QWidget& window, const QString& path, int delay_ms);

} // namespace UiShell

#endif
