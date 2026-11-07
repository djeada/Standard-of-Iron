#include "command_palette.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace MapEditor {

auto palette_score(const QString& query, const QString& text) -> int {
  const QString needle = query.trimmed().toLower();
  if (needle.isEmpty()) {
    return 1;
  }
  const QString hay = text.toLower();
  if (hay.startsWith(needle)) {
    return 1000;
  }
  const qsizetype word = hay.indexOf(QLatin1Char(' ') + needle);
  if (word >= 0) {
    return 800 - static_cast<int>(word);
  }
  const qsizetype inside = hay.indexOf(needle);
  if (inside >= 0) {
    return 600 - static_cast<int>(inside);
  }
  const QStringList words = needle.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (words.size() > 1) {
    const bool all =
        std::all_of(words.begin(), words.end(), [&hay](const QString& word) {
          return hay.contains(word);
        });
    return all ? 500 : 0;
  }
  int position = 0;
  int gaps = 0;
  for (const QChar c : needle) {
    if (c.isSpace()) {
      continue;
    }
    const qsizetype found = hay.indexOf(c, position);
    if (found < 0) {
      return 0;
    }
    gaps += static_cast<int>(found - position);
    position = static_cast<int>(found) + 1;
  }
  return std::max(1, 300 - gaps);
}

CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint) {
  setObjectName(QStringLiteral("command_palette"));
  setStyleSheet(QStringLiteral(
      "#command_palette{background:#231B14;border:1px solid #A7814A;border-radius:8px;}"
      "QLineEdit{font-size:13pt;padding:8px 10px;border:none;border-bottom:1px solid "
      "#7A5C36;"
      "background:#1D1610;color:#F4E7C8;}"
      "QListWidget{border:none;background:#231B14;font-size:10.5pt;}"
      "QListWidget::item{padding:6px 10px;}"
      "QListWidget::item:selected{background:#6B4D2A;color:#F4E7C8;}"
      "QLabel{color:#8D7146;padding:4px 10px;}"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);
  m_input = new QLineEdit(this);
  m_input->setPlaceholderText(
      QStringLiteral("Jump to a site, mission or map, or run a command…"));
  m_input->installEventFilter(this);
  layout->addWidget(m_input);
  m_list = new QListWidget(this);
  m_list->setUniformItemSizes(true);
  layout->addWidget(m_list, 1);
  m_hint =
      new QLabel(QStringLiteral("↑↓ to choose · Enter to go · Esc to close"), this);
  layout->addWidget(m_hint);
  connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::refilter);
  connect(m_list, &QListWidget::itemActivated, this, [this]() { activate_current(); });
  connect(m_list, &QListWidget::itemClicked, this, [this]() { activate_current(); });
  resize(640, 440);
}

void CommandPalette::set_entries(QVector<PaletteEntry> entries) {
  m_entries = std::move(entries);
  refilter();
}

void CommandPalette::open_with(const QString& text) {
  if (parentWidget() != nullptr) {
    const QRect area = parentWidget()->window()->geometry();
    move(area.center().x() - (width() / 2), area.top() + 90);
  }
  m_input->setText(text);
  refilter();
  show();
  raise();
  activateWindow();
  m_input->setFocus();
  if (text.isEmpty()) {
    m_input->selectAll();
  } else {
    m_input->end(false);
  }
}

void CommandPalette::refilter() {
  const QString query = m_input->text();
  QVector<std::pair<int, int>> scored;
  for (int i = 0; i < m_entries.size(); ++i) {
    const auto& entry = m_entries[i];
    const int score = std::max(
        palette_score(query, entry.title),
        palette_score(query, entry.title + QLatin1Char(' ') + entry.detail) * 9 / 10);
    if (score > 0) {
      scored.append({score, i});
    }
  }
  std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });
  constexpr int k_substring_score = 300;
  if (!scored.isEmpty() && scored.first().first > k_substring_score) {
    scored.erase(std::remove_if(scored.begin(),
                                scored.end(),
                                [](const auto& entry) {
                                  return entry.first <= k_substring_score;
                                }),
                 scored.end());
  }
  m_list->clear();
  m_visible.clear();
  for (const auto& [score, index] : scored.mid(0, 200)) {
    const auto& entry = m_entries[index];
    auto* item = new QListWidgetItem(
        QStringLiteral("%1    %2")
            .arg(entry.title,
                 entry.detail.isEmpty() ? QString()
                                        : QStringLiteral("— %1").arg(entry.detail)),
        m_list);
    item->setToolTip(entry.category);
    m_visible.append(index);
  }
  m_list->setCurrentRow(m_list->count() > 0 ? 0 : -1);
  m_hint->setText(QStringLiteral("%1 result(s) · ↑↓ choose · Enter go · Esc close")
                      .arg(m_list->count()));
}

void CommandPalette::activate_current() {
  const int row = m_list->currentRow();
  if (row < 0 || row >= m_visible.size()) {
    return;
  }
  const auto run = m_entries[m_visible[row]].run;
  hide();
  if (run) {
    run();
  }
}

auto CommandPalette::eventFilter(QObject* watched, QEvent* event) -> bool {
  if (watched == m_input && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
    case Qt::Key_Down:
      m_list->setCurrentRow(std::min(m_list->currentRow() + 1, m_list->count() - 1));
      return true;
    case Qt::Key_Up:
      m_list->setCurrentRow(std::max(m_list->currentRow() - 1, 0));
      return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      activate_current();
      return true;
    case Qt::Key_Escape:
      hide();
      return true;
    default:
      break;
    }
  }
  return QDialog::eventFilter(watched, event);
}

} // namespace MapEditor
