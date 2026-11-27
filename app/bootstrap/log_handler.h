#pragma once

#include <QString>
#include <QStringList>

namespace App::Bootstrap {

void install_log_message_handler();

// The tail of what the game has logged in this process, oldest first, already
// formatted the way the console shows it. A support bundle is built from this
// instead of a log file because a normal run writes none: stderr goes to the
// terminal and nowhere else.
[[nodiscard]] auto recent_log_lines(int max_lines) -> QStringList;

// Only for tests: a case that asserts on the tail must not inherit whatever the
// suite logged before it ran.
void clear_recent_log_lines();

} // namespace App::Bootstrap
