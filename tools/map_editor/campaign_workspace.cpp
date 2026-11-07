#include "campaign_workspace.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <tuple>
#include <utility>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_encounter.h"
#include "game/strategic/strategic_events.h"
#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_json.h"
#include "game/strategic/strategic_supply.h"
#include "world_canvas.h"
#include "world_document.h"

namespace MapEditor {

namespace {

namespace S = Game::Strategic;

constexpr int k_id_role = Qt::UserRole + 1;

const char* const k_document_css =
    "body{color:#F4E7C8;} a{color:#E8C98B;text-decoration:none;}"
    "h2{margin:0 0 2px 0;font-size:15pt;font-weight:600;}"
    "h4{margin:10px 0 3px 0;color:#C7A66D;font-size:9pt;text-transform:uppercase;}"
    ".dim{color:#9C8158;} .bad{color:#ff8a7a;} .warn{color:#f5c060;} "
    ".good{color:#8fdc9f;}"
    "td{padding:1px 8px 1px 0;}";

auto title_case(QString text) -> QString {
  text.replace(QLatin1Char('_'), QLatin1Char(' '));
  if (!text.isEmpty()) {
    text[0] = text[0].toUpper();
  }
  return text;
}

auto meter(int value) -> QString {
  QString out;
  for (int i = 0; i < S::k_resource_max; ++i) {
    out += i < value ? QStringLiteral("●") : QStringLiteral("○");
  }
  return out;
}

auto number(int value) -> QString {
  return QLocale(QLocale::English).toString(value);
}

auto plural(int count, const QString& one, const QString& many) -> QString {
  return QStringLiteral("%1 %2").arg(count).arg(count == 1 ? one : many);
}

auto make_browser(QWidget* parent) -> QTextBrowser* {
  auto* browser = new QTextBrowser(parent);
  browser->setOpenLinks(false);
  browser->document()->setDefaultStyleSheet(QString::fromLatin1(k_document_css));
  browser->setFrameShape(QFrame::NoFrame);
  return browser;
}

auto tool_button(const QString& text,
                 const QString& tip,
                 QWidget* parent) -> QToolButton* {
  auto* button = new QToolButton(parent);
  button->setText(text);
  button->setToolTip(tip);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return button;
}

auto no_insert_combo(QWidget* parent) -> QComboBox* {
  auto* box = new QComboBox(parent);
  box->setEditable(true);
  box->setInsertPolicy(QComboBox::NoInsert);
  box->completer()->setCompletionMode(QCompleter::PopupCompletion);
  box->completer()->setFilterMode(Qt::MatchContains);
  box->completer()->setCaseSensitivity(Qt::CaseInsensitive);
  return box;
}

} // namespace

class ChronologyStrip : public QWidget {
public:
  explicit ChronologyStrip(WorldDocument* document, QWidget* parent = nullptr)
      : QWidget(parent)
      , m_document(document) {
    setMinimumHeight(58);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
  }

protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(28, 21, 15));
    const auto& events = m_document->definition().events;
    m_hits.clear();
    if (events.isEmpty()) {
      return;
    }
    const double left = 26.0;
    const double right = width() - 16.0;
    const double axis = height() - 22.0;
    const int slot_count = static_cast<int>(events.size()) + 1;
    const auto x_of = [&](int slot) {
      return left + ((right - left) * slot / (slot_count - 1.0));
    };
    painter.setPen(QPen(QColor(140, 106, 62), 2.0));
    painter.drawLine(QPointF(left, axis), QPointF(right, axis));
    const int current = m_document->timeline_index();
    painter.setPen(QPen(QColor(212, 161, 90), 3.0));
    painter.drawLine(QPointF(left, axis), QPointF(x_of(current), axis));
    QFont font = painter.font();
    font.setPointSizeF(7.5);
    painter.setFont(font);
    int last_year = 0;
    double last_label_x = -1e9;
    for (int slot = 0; slot < slot_count; ++slot) {
      const double x = x_of(slot);
      const auto* event = slot == 0 ? nullptr : &events[slot - 1];
      const int year = event != nullptr ? event->year : events.first().year;
      if (slot == 0 || year != last_year) {
        painter.setPen(QPen(QColor(140, 106, 62), 1.0));
        painter.drawLine(QPointF(x, axis + 3), QPointF(x, axis + 7));
        if (x - last_label_x >= 46.0) {
          painter.setPen(QColor(199, 166, 109));
          painter.drawText(
              QRectF(x - 30, axis + 7, 60, 13), Qt::AlignCenter, S::format_year(year));
          last_label_x = x;
        }
        last_year = year;
      }
      const bool mission = event != nullptr && !event->mission.isEmpty();
      const bool applied = slot <= current;
      const bool is_current = slot == current;
      const double r = is_current ? 7.0 : mission ? 5.5 : 3.8;
      QColor fill = slot == 0 ? QColor(244, 231, 200)
                    : mission ? (applied ? QColor(206, 64, 52) : QColor(110, 50, 44))
                              : (applied ? QColor(212, 161, 90) : QColor(90, 72, 50));
      if (is_current) {
        painter.setPen(QPen(QColor(232, 201, 139), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(x, axis), r + 3.5, r + 3.5);
      }
      painter.setPen(QPen(QColor(20, 14, 8), 1.0));
      painter.setBrush(fill);
      painter.drawEllipse(QPointF(x, axis), r, r);
      m_hits.append(QRectF(x - 7, axis - 10, 14, 20));
    }
    const auto* event = m_document->current_event();
    font.setPointSizeF(9.0);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(244, 231, 200));
    painter.drawText(
        QRectF(left, 2, right - left, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        event != nullptr
            ? QStringLiteral("%1  —  %2").arg(event->date_label(), event->name)
            : QStringLiteral("%1  —  opening situation")
                  .arg(m_document->definition().start_label));
  }

  void mousePressEvent(QMouseEvent* event) override {
    const int slot = slot_at(event->position());
    if (slot >= 0) {
      m_document->set_timeline_index(slot);
    }
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    const int slot = slot_at(event->position());
    const auto& events = m_document->definition().events;
    if (slot > 0 && slot <= events.size()) {
      const auto& e = events[slot - 1];
      QToolTip::showText(
          event->globalPosition().toPoint(),
          QStringLiteral("<b>%1</b><br>%2%3")
              .arg(e.name.toHtmlEscaped(),
                   e.date_label(),
                   e.mission.isEmpty()
                       ? QString()
                       : QStringLiteral("<br>mission: %1").arg(e.mission)),
          this);
    } else {
      QToolTip::hideText();
    }
  }

private:
  [[nodiscard]] auto slot_at(QPointF p) const -> int {
    int best = -1;
    double best_d = 1e9;
    for (int i = 0; i < m_hits.size(); ++i) {
      const double d = std::abs(m_hits[i].center().x() - p.x());
      if (d < best_d &&
          d <= std::max(7.0,
                        m_hits.size() > 1
                            ? (m_hits[1].center().x() - m_hits[0].center().x()) * 0.5
                            : 7.0)) {
        best_d = d;
        best = i;
      }
    }
    return best;
  }

  WorldDocument* m_document;
  QVector<QRectF> m_hits;
};

CampaignWorkspace::CampaignWorkspace(QWidget* parent)
    : QMainWindow(parent) {
  setWindowFlags(Qt::Widget);
  setDockNestingEnabled(true);
  setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);
  m_document = new WorldDocument(this);
  m_canvas = new WorldCanvas(m_document, this);
  setCentralWidget(m_canvas);
  build_toolbar();
  build_explorer();
  build_inspector();
  build_timeline();
  build_problems();

  connect(m_canvas, &WorldCanvas::status_message, this, [this](const QString& message) {
    emit feedback(message, true);
  });
  connect(m_canvas, &WorldCanvas::cursor_moved, this, &CampaignWorkspace::cursor_text);
  connect(m_canvas, &WorldCanvas::path_requested, this, &CampaignWorkspace::show_path);
  connect(m_canvas,
          &WorldCanvas::context_requested,
          this,
          &CampaignWorkspace::show_context_menu);
  connect(m_canvas,
          &WorldCanvas::delete_requested,
          this,
          &CampaignWorkspace::delete_entity);
  connect(m_canvas, &WorldCanvas::tool_changed, this, [this](WorldCanvas::Tool tool) {
    if (tool == WorldCanvas::Tool::Select) {
      m_tools->actions().first()->setChecked(true);
    }
  });
  connect(m_canvas, &WorldCanvas::activated, this, [this](const QString& id) {
    const auto* site = m_document->definition().site(id);
    if (site != nullptr && !site->battlefield.empty()) {
      open_battlefield_for(id);
    } else {
      m_canvas->focus_on(id);
    }
  });
  connect(m_canvas, &WorldCanvas::layers_changed, this, [this](unsigned layers) {
    m_rebuilding = true;
    for (int i = 0; i < m_layers->count(); ++i) {
      auto* item = m_layers->item(i);
      item->setCheckState((layers & item->data(k_id_role).toUInt()) != 0U
                              ? Qt::Checked
                              : Qt::Unchecked);
    }
    m_rebuilding = false;
  });
  connect(m_document, &WorldDocument::changed, this, [this]() {
    refresh_inspector();
    refresh_problems();
    refresh_timeline();
    const auto counts = std::make_tuple(m_document->definition().sites.size(),
                                        m_document->definition().routes.size(),
                                        m_document->definition().regions.size());
    if (counts != m_outline_counts) {
      m_outline_counts = counts;
      refresh_query_sites();
      refresh_outline();
    }
    QMetaObject::invokeMethod(
        this, [this]() { refresh_properties(); }, Qt::QueuedConnection);
    emit summary_changed();
  });
  connect(m_document, &WorldDocument::selection_changed, this, [this]() {
    refresh_inspector();
    refresh_properties();
    refresh_outline_selection();
    emit summary_changed();
  });
  connect(m_document, &WorldDocument::timeline_changed, this, [this]() {
    refresh_outline();
    refresh_timeline();
    refresh_properties();
    emit summary_changed();
  });
  connect(m_document, &WorldDocument::loaded, this, [this]() {
    refresh_query_sites();
    refresh_outline();
    refresh_properties();
    refresh_problems();
    emit summary_changed();
  });
  connect(m_document, &WorldDocument::modified_changed, this, [this]() {
    emit summary_changed();
  });
}

auto CampaignWorkspace::has_world() const -> bool {
  return !m_document->definition().id.isEmpty();
}

auto CampaignWorkspace::open_world(const QString& path) -> bool {
  QString error;
  if (!m_document->load(path, &error)) {
    QMessageBox::warning(this,
                         QStringLiteral("Campaign"),
                         QStringLiteral("Could not open %1:\n%2").arg(path, error));
    return false;
  }
  emit feedback(
      QStringLiteral("Opened %1 — %2, %3, %4.")
          .arg(m_document->definition().name,
               plural(static_cast<int>(m_document->definition().sites.size()),
                      QStringLiteral("site"),
                      QStringLiteral("sites")),
               plural(static_cast<int>(m_document->definition().routes.size()),
                      QStringLiteral("route"),
                      QStringLiteral("routes")),
               plural(static_cast<int>(m_document->definition().events.size()),
                      QStringLiteral("event"),
                      QStringLiteral("events"))),
      true);
  return true;
}

void CampaignWorkspace::select_entity(const QString& id) {
  m_document->select(id);
  m_canvas->focus_on(id);
}

void CampaignWorkspace::set_timeline(const QString& event_id) {
  const auto& events = m_document->definition().events;
  for (int i = 0; i < events.size(); ++i) {
    if (events[i].id == event_id) {
      m_document->set_timeline_index(i + 1);
      return;
    }
  }
}

void CampaignWorkspace::undo() {
  if (m_document->can_undo()) {
    m_document->undo();
    emit feedback(QStringLiteral("Undone."), true);
  }
}

void CampaignWorkspace::redo() {
  if (m_document->can_redo()) {
    m_document->redo();
    emit feedback(QStringLiteral("Redone."), true);
  }
}

auto CampaignWorkspace::save() -> bool {
  if (m_document->path().isEmpty()) {
    return save_as();
  }
  QString error;
  if (!m_document->save(&error)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), error);
    return false;
  }
  const auto& report = m_document->report();
  emit feedback(QStringLiteral("Saved %1 — %2, %3.")
                    .arg(QFileInfo(m_document->path()).fileName(),
                         plural(report.errors(),
                                QStringLiteral("error"),
                                QStringLiteral("errors")),
                         plural(report.warnings(),
                                QStringLiteral("warning"),
                                QStringLiteral("warnings"))),
                report.errors() == 0);
  return true;
}

auto CampaignWorkspace::save_as() -> bool {
  const QString path =
      QFileDialog::getSaveFileName(this,
                                   QStringLiteral("Save Campaign World"),
                                   m_document->path(),
                                   QStringLiteral("Campaign world (*.json)"));
  if (path.isEmpty()) {
    return false;
  }
  QString error;
  if (!m_document->save_as(path, &error)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), error);
    return false;
  }
  emit feedback(QStringLiteral("Saved %1.").arg(path), true);
  return true;
}

auto CampaignWorkspace::maybe_save() -> bool {
  if (!m_document->is_modified()) {
    return true;
  }
  const auto answer = QMessageBox::question(
      this,
      QStringLiteral("Unsaved campaign changes"),
      QStringLiteral("The campaign world has unsaved changes. Save them first?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (answer == QMessageBox::Cancel) {
    return false;
  }
  return answer == QMessageBox::Discard || save();
}

void CampaignWorkspace::show_problems() {
  if (auto* dock = findChild<QDockWidget*>(QStringLiteral("campaign_problems"))) {
    dock->show();
    dock->raise();
  }
}

void CampaignWorkspace::validate_now() {
  refresh_problems();
  show_problems();
  const auto& report = m_document->report();
  emit feedback(report.errors() == 0 && report.warnings() == 0
                    ? QStringLiteral("The campaign world passes every check.")
                    : QStringLiteral("%1 and %2 — see Problems.")
                          .arg(plural(report.errors(),
                                      QStringLiteral("error"),
                                      QStringLiteral("errors")),
                               plural(report.warnings(),
                                      QStringLiteral("warning"),
                                      QStringLiteral("warnings"))),
                report.errors() == 0);
}

void CampaignWorkspace::complete_network() {
  const int added = m_document->complete_network();
  emit feedback(
      added == 0
          ? QStringLiteral("The road network is already complete.")
          : QStringLiteral("Added %1 tagged 'auto' — review them in Problems "
                           "and the map. Ctrl+Z undoes.")
                .arg(plural(added, QStringLiteral("route"), QStringLiteral("routes"))),
      true);
}

auto CampaignWorkspace::site_for_battlefield(
    const QString& map_path, const QString& mission_id) const -> QString {
  const auto& def = m_document->definition();
  if (!mission_id.isEmpty()) {
    if (const auto* site = def.site_for_mission(mission_id)) {
      return site->id;
    }
  }
  const QString file = QFileInfo(map_path).fileName();
  if (file.isEmpty()) {
    return {};
  }
  for (const auto& site : def.sites) {
    if (QFileInfo(site.battlefield.map).fileName() == file) {
      return site.id;
    }
  }
  return {};
}

void CampaignWorkspace::build_toolbar() {
  auto* bar = addToolBar(QStringLiteral("Campaign tools"));
  bar->setObjectName(QStringLiteral("campaign_tools"));
  bar->setMovable(false);
  bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_tools = new QActionGroup(this);
  const auto add_tool = [&](const QString& text,
                            WorldCanvas::Tool tool,
                            const QString& key,
                            const QString& tip) {
    auto* action = bar->addAction(text);
    action->setCheckable(true);
    action->setShortcut(QKeySequence(key));
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    action->setToolTip(
        QStringLiteral("<b>%1</b> <span style='color:#9C8158'>%2</span><br>%3")
            .arg(text, key, tip));
    m_tools->addAction(action);
    addAction(action);
    connect(action, &QAction::triggered, this, [this, tool]() {
      m_canvas->set_tool(tool);
    });
    return action;
  };
  add_tool(QStringLiteral("Select"),
           WorldCanvas::Tool::Select,
           QStringLiteral("S"),
           QStringLiteral(
               "Select, drag sites and route bends. Space+drag or middle-drag pans."))
      ->setChecked(true);
  bar->addSeparator();
  add_tool(QStringLiteral("Add Site"),
           WorldCanvas::Tool::AddSite,
           QStringLiteral("N"),
           QStringLiteral("Click the map to place a site of the chosen kind. Hold "
                          "Shift to place several."));
  m_site_kind = new QComboBox(this);
  m_site_kind->setToolTip(QStringLiteral("Kind of site to place"));
  for (const auto& kind : S::all_site_kind_names()) {
    m_site_kind->addItem(title_case(kind), kind);
  }
  m_site_kind->setCurrentIndex(1);
  connect(m_site_kind, &QComboBox::currentIndexChanged, this, [this]() {
    m_canvas->set_new_site_kind(
        *S::site_kind_from_string(m_site_kind->currentData().toString()));
    m_tools->actions().at(1)->trigger();
  });
  bar->addWidget(m_site_kind);
  bar->addSeparator();
  add_tool(
      QStringLiteral("Add Route"),
      WorldCanvas::Tool::AddRoute,
      QStringLiteral("R"),
      QStringLiteral("Click two sites to join them; keep clicking to chain a road."));
  m_route_kind = new QComboBox(this);
  m_route_kind->setToolTip(QStringLiteral("Kind of route to lay"));
  for (const auto& kind : S::all_route_kind_names()) {
    m_route_kind->addItem(title_case(kind), kind);
  }
  m_route_kind->setCurrentIndex(1);
  connect(m_route_kind, &QComboBox::currentIndexChanged, this, [this]() {
    m_canvas->set_new_route_kind(
        *S::route_kind_from_string(m_route_kind->currentData().toString()));
    m_tools->actions().at(2)->trigger();
  });
  bar->addWidget(m_route_kind);
  add_tool(QStringLiteral("Bend"),
           WorldCanvas::Tool::Waypoint,
           QStringLiteral("B"),
           QStringLiteral(
               "Click the map to bend the selected route. Drag a bend to move it; "
               "double-click it to remove it."));
  bar->addSeparator();
  add_tool(QStringLiteral("Measure March"),
           WorldCanvas::Tool::PathQuery,
           QStringLiteral("M"),
           QStringLiteral("Click two sites to see the cheapest march between them."));
  bar->addSeparator();
  auto* complete = bar->addAction(QStringLiteral("Complete Network"));
  complete->setToolTip(QStringLiteral("Give every unconnected site a road and join "
                                      "every stranded group of sites to the main "
                                      "network. New routes are tagged 'auto'."));
  connect(complete, &QAction::triggered, this, &CampaignWorkspace::complete_network);
  auto* fit = bar->addAction(QStringLiteral("Fit"));
  fit->setShortcut(QKeySequence(QStringLiteral("Home")));
  fit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  addAction(fit);
  connect(fit, &QAction::triggered, m_canvas, &WorldCanvas::fit_all);
}

void CampaignWorkspace::build_explorer() {
  auto* dock = new QDockWidget(QStringLiteral("Explorer"), this);
  dock->setObjectName(QStringLiteral("campaign_explorer"));
  dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  auto* tabs = new QTabWidget(dock);
  tabs->setDocumentMode(true);
  m_explorer_tabs = tabs;

  auto* outline_page = new QWidget(tabs);
  auto* outline_layout = new QVBoxLayout(outline_page);
  outline_layout->setContentsMargins(6, 6, 6, 6);
  m_outline_filter = new QLineEdit(outline_page);
  m_outline_filter->setPlaceholderText(QStringLiteral("Filter sites, routes, armies…"));
  m_outline_filter->setClearButtonEnabled(true);
  outline_layout->addWidget(m_outline_filter);
  m_outline = new QTreeWidget(outline_page);
  m_outline->setHeaderHidden(true);
  m_outline->setUniformRowHeights(true);
  m_outline->setExpandsOnDoubleClick(false);
  outline_layout->addWidget(m_outline, 1);
  connect(m_outline_filter,
          &QLineEdit::textChanged,
          this,
          &CampaignWorkspace::apply_outline_filter);
  connect(m_outline, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
    const QString id = item->data(0, k_id_role).toString();
    if (id.startsWith(QStringLiteral("event:"))) {
      set_timeline(id.mid(6));
    } else if (!id.isEmpty()) {
      select_entity(id);
    }
  });
  tabs->addTab(outline_page, QStringLiteral("Outline"));

  m_layers = new QListWidget(tabs);
  m_layers->setFrameShape(QFrame::NoFrame);
  const QVector<std::tuple<WorldCanvas::Layer, QString, QString>> layers = {
      {WorldCanvas::Relief,
       QStringLiteral("Relief"),
       QStringLiteral("Hill shading from the heightmap")},
      {WorldCanvas::Control,
       QStringLiteral("Faction control"),
       QStringLiteral("Region colours; hatched when contested")},
      {WorldCanvas::Borders, QStringLiteral("Region borders"), QString()},
      {WorldCanvas::Districts,
       QStringLiteral("Districts"),
       QStringLiteral("Apulia, Campania, Samnium, the Alps")},
      {WorldCanvas::Routes, QStringLiteral("Roads & sea lanes"), QString()},
      {WorldCanvas::Sites, QStringLiteral("Settlements & sites"), QString()},
      {WorldCanvas::Ports,
       QStringLiteral("Ports"),
       QStringLiteral("Anchor marks on harbours")},
      {WorldCanvas::Labels, QStringLiteral("Labels"), QString()},
      {WorldCanvas::Armies,
       QStringLiteral("Armies"),
       QStringLiteral("Banners with strength and supply")},
      {WorldCanvas::Movements,
       QStringLiteral("Campaign movements"),
       QStringLiteral("Marches made in the current event")},
      {WorldCanvas::Supply,
       QStringLiteral("Supply lines"),
       QStringLiteral("Each army's line to its depot")},
      {WorldCanvas::Resources,
       QStringLiteral("Resources"),
       QStringLiteral("Food, manpower, timber, stone, iron")},
      {WorldCanvas::Chokepoints,
       QStringLiteral("Chokepoints"),
       QStringLiteral("Sites and routes whose loss splits the network")},
      {WorldCanvas::Objectives,
       QStringLiteral("Objectives"),
       QStringLiteral("Where armies are marching")},
      {WorldCanvas::Issues,
       QStringLiteral("Problems"),
       QStringLiteral("Rings and dashes on invalid data")},
      {WorldCanvas::Legend,
       QStringLiteral("Legend"),
       QStringLiteral("Faction and route key")},
  };
  for (const auto& [layer, name, tip] : layers) {
    auto* item = new QListWidgetItem(name, m_layers);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setCheckState((m_canvas->layers() & layer) != 0U ? Qt::Checked
                                                           : Qt::Unchecked);
    item->setData(k_id_role, static_cast<unsigned>(layer));
    item->setToolTip(tip);
  }
  connect(m_layers, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    if (!m_rebuilding) {
      m_canvas->set_layer(
          static_cast<WorldCanvas::Layer>(item->data(k_id_role).toUInt()),
          item->checkState() == Qt::Checked);
    }
  });
  tabs->addTab(m_layers, QStringLiteral("Layers"));

  auto* analysis = new QWidget(tabs);
  auto* layout = new QVBoxLayout(analysis);
  layout->setContentsMargins(6, 6, 6, 6);
  auto* form = new QFormLayout();
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_query_from = no_insert_combo(analysis);
  m_query_to = no_insert_combo(analysis);
  m_query_faction = new QComboBox(analysis);
  m_query_safe = new QCheckBox(QStringLiteral("Avoid hostile ground"), analysis);
  m_query_sea = new QCheckBox(QStringLiteral("Allow sea lanes"), analysis);
  m_query_sea->setChecked(true);
  m_query_days = new QDoubleSpinBox(analysis);
  m_query_days->setRange(0.5, 120.0);
  m_query_days->setValue(7.0);
  m_query_days->setSuffix(QStringLiteral(" days"));
  form->addRow(QStringLiteral("From"), m_query_from);
  form->addRow(QStringLiteral("To"), m_query_to);
  form->addRow(QStringLiteral("Marching as"), m_query_faction);
  form->addRow(QString(), m_query_safe);
  form->addRow(QString(), m_query_sea);
  form->addRow(QStringLiteral("Within"), m_query_days);
  layout->addLayout(form);
  auto* row = new QHBoxLayout();
  auto* path = new QPushButton(QStringLiteral("March"), analysis);
  path->setToolTip(QStringLiteral("Cheapest march from → to"));
  auto* routes = new QPushButton(QStringLiteral("Alternatives"), analysis);
  routes->setToolTip(QStringLiteral("How many independent routes join from and to"));
  auto* reach = new QPushButton(QStringLiteral("Reach"), analysis);
  reach->setToolTip(
      QStringLiteral("Every site reachable from 'From' within the day budget"));
  row->addWidget(path);
  row->addWidget(routes);
  row->addWidget(reach);
  layout->addLayout(row);
  auto* network =
      new QPushButton(QStringLiteral("Isolation, threats && chokepoints"), analysis);
  layout->addWidget(network);
  connect(path, &QPushButton::clicked, this, &CampaignWorkspace::run_path_query);
  connect(
      routes, &QPushButton::clicked, this, &CampaignWorkspace::run_route_count_query);
  connect(reach, &QPushButton::clicked, this, [this]() { run_reach_query(); });
  connect(network, &QPushButton::clicked, this, &CampaignWorkspace::run_network_report);
  m_query_result = make_browser(analysis);
  connect(m_query_result,
          &QTextBrowser::anchorClicked,
          this,
          &CampaignWorkspace::handle_link);
  layout->addWidget(m_query_result, 1);
  tabs->addTab(analysis, QStringLiteral("Analysis"));

  dock->setWidget(tabs);
  dock->setMinimumWidth(260);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  resizeDocks({dock}, {290}, Qt::Horizontal);
}

void CampaignWorkspace::build_inspector() {
  auto* dock = new QDockWidget(QStringLiteral("Inspector"), this);
  dock->setObjectName(QStringLiteral("campaign_inspector"));
  dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  auto* split = new QSplitter(Qt::Vertical, dock);
  auto* top = new QWidget(split);
  auto* top_layout = new QVBoxLayout(top);
  top_layout->setContentsMargins(8, 6, 8, 4);
  m_inspector = make_browser(top);
  connect(
      m_inspector, &QTextBrowser::anchorClicked, this, &CampaignWorkspace::handle_link);
  top_layout->addWidget(m_inspector, 1);
  auto* actions = new QHBoxLayout();
  m_open_button =
      tool_button(QStringLiteral("Open Battlefield"),
                  QStringLiteral("Edit this site's battlefield map (and its mission) — "
                                 "or double-click the site"),
                  top);
  m_generate_button = tool_button(
      QStringLiteral("Generate Battlefield"),
      QStringLiteral("Prefill the map generator from this strategic situation"),
      top);
  m_supply_button = tool_button(QStringLiteral("Show Supply"),
                                QStringLiteral("Draw the supply line on the map"),
                                top);
  actions->addWidget(m_open_button);
  actions->addWidget(m_generate_button);
  actions->addWidget(m_supply_button);
  top_layout->addLayout(actions);
  connect(m_open_button, &QToolButton::clicked, this, [this]() {
    open_battlefield_for(m_document->selection());
  });
  connect(m_generate_button, &QToolButton::clicked, this, [this]() {
    generate_battlefield_for(m_document->selection());
  });
  connect(m_supply_button, &QToolButton::clicked, this, [this]() {
    show_supply_for(m_document->selection());
  });
  m_properties = new QWidget();
  m_properties_form = new QFormLayout(m_properties);
  m_properties_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_properties_form->setContentsMargins(10, 6, 10, 6);
  auto* scroll = new QScrollArea(split);
  scroll->setWidget(m_properties);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  split->addWidget(top);
  split->addWidget(scroll);
  split->setStretchFactor(0, 3);
  split->setStretchFactor(1, 2);
  dock->setWidget(split);
  dock->setMinimumWidth(340);
  addDockWidget(Qt::RightDockWidgetArea, dock);
  resizeDocks({dock}, {380}, Qt::Horizontal);
}

void CampaignWorkspace::build_timeline() {
  auto* dock = new QDockWidget(QStringLiteral("Timeline"), this);
  dock->setObjectName(QStringLiteral("campaign_timeline"));
  dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  auto* body = new QWidget(dock);
  auto* layout = new QVBoxLayout(body);
  layout->setContentsMargins(6, 4, 6, 4);
  layout->setSpacing(4);
  auto* row = new QHBoxLayout();
  const auto nav = [&](const QString& text,
                       const QString& tip,
                       const QString& key,
                       const std::function<void()>& step) {
    auto* button = new QToolButton(body);
    button->setText(text);
    button->setToolTip(QStringLiteral("%1 (%2)").arg(tip, key));
    button->setAutoRaise(true);
    if (!key.isEmpty()) {
      auto* action = new QAction(this);
      action->setShortcut(QKeySequence(key));
      action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
      connect(action, &QAction::triggered, this, step);
      addAction(action);
    }
    connect(button, &QToolButton::clicked, this, step);
    row->addWidget(button);
  };
  nav(QStringLiteral("⏮"),
      QStringLiteral("Opening situation"),
      QStringLiteral("Ctrl+["),
      [this]() { m_document->set_timeline_index(0); });
  nav(QStringLiteral("◀"),
      QStringLiteral("Previous event"),
      QStringLiteral("["),
      [this]() { m_document->set_timeline_index(m_document->timeline_index() - 1); });
  m_strip = new ChronologyStrip(m_document, body);
  row->addWidget(m_strip, 1);
  nav(QStringLiteral("▶"), QStringLiteral("Next event"), QStringLiteral("]"), [this]() {
    m_document->set_timeline_index(m_document->timeline_index() + 1);
  });
  nav(QStringLiteral("⏭"),
      QStringLiteral("End of the war"),
      QStringLiteral("Ctrl+]"),
      [this]() {
        m_document->set_timeline_index(
            static_cast<int>(m_document->definition().events.size()));
      });
  layout->addLayout(row);
  auto* split = new QSplitter(Qt::Horizontal, body);
  m_event_list = new QListWidget(split);
  m_event_list->setFrameShape(QFrame::NoFrame);
  m_event_detail = make_browser(split);
  connect(m_event_detail,
          &QTextBrowser::anchorClicked,
          this,
          &CampaignWorkspace::handle_link);
  split->addWidget(m_event_list);
  split->addWidget(m_event_detail);
  split->setStretchFactor(0, 2);
  split->setStretchFactor(1, 3);
  layout->addWidget(split, 1);
  body->setMinimumHeight(200);
  connect(m_event_list, &QListWidget::currentRowChanged, this, [this](int index) {
    if (!m_rebuilding && index >= 0) {
      m_document->set_timeline_index(index);
    }
  });
  dock->setWidget(body);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  resizeDocks({dock}, {210}, Qt::Vertical);
}

void CampaignWorkspace::build_problems() {
  auto* dock = new QDockWidget(QStringLiteral("Problems"), this);
  dock->setObjectName(QStringLiteral("campaign_problems"));
  dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  auto* body = new QWidget(dock);
  auto* layout = new QVBoxLayout(body);
  layout->setContentsMargins(6, 4, 6, 4);
  auto* row = new QHBoxLayout();
  m_issue_summary = new QLabel(body);
  row->addWidget(m_issue_summary, 1);
  const auto filter = [&](const QString& text) {
    auto* button = new QToolButton(body);
    button->setText(text);
    button->setCheckable(true);
    button->setChecked(true);
    button->setAutoRaise(true);
    connect(button, &QToolButton::toggled, this, [this]() { refresh_problems(); });
    row->addWidget(button);
    return button;
  };
  m_show_errors = filter(QStringLiteral("Errors"));
  m_show_warnings = filter(QStringLiteral("Warnings"));
  m_show_notes = filter(QStringLiteral("Notes"));
  layout->addLayout(row);
  m_issues = new QTreeWidget(body);
  m_issues->setColumnCount(3);
  m_issues->setHeaderLabels(
      {QStringLiteral("Check"), QStringLiteral("Where"), QStringLiteral("Problem")});
  m_issues->setRootIsDecorated(false);
  m_issues->setAlternatingRowColors(true);
  m_issues->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  const auto jump = [this](QTreeWidgetItem* item) {
    const QString id = item->data(0, k_id_role).toString();
    if (m_document->definition().kind_of(id) == S::EntityKind::Event) {
      set_timeline(id);
    } else if (!id.isEmpty()) {
      select_entity(id);
    }
  };
  connect(m_issues, &QTreeWidget::itemClicked, this, jump);
  connect(m_issues, &QTreeWidget::itemActivated, this, jump);
  layout->addWidget(m_issues, 1);
  dock->setWidget(body);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  tabifyDockWidget(findChild<QDockWidget*>(QStringLiteral("campaign_timeline")), dock);
  findChild<QDockWidget*>(QStringLiteral("campaign_timeline"))->raise();
}

void CampaignWorkspace::refresh_outline() {
  const auto& def = m_document->definition();
  m_rebuilding = true;
  m_outline->clear();
  const auto group = [this](const QString& title, int count) {
    auto* item = new QTreeWidgetItem(
        m_outline, {QStringLiteral("%1  (%2)").arg(title).arg(count)});
    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    item->setFlags(Qt::ItemIsEnabled);
    return item;
  };
  auto* regions =
      group(QStringLiteral("Regions"), static_cast<int>(def.regions.size()));
  for (const auto& region : def.regions) {
    auto* item = new QTreeWidgetItem(regions, {region.name});
    item->setData(0, k_id_role, region.id);
    item->setToolTip(0, region.historical_name);
    for (const auto* site : def.sites_in(region.id)) {
      auto* child = new QTreeWidgetItem(
          item,
          {QStringLiteral("%1%2").arg(
              site->name,
              site->battlefield.empty() ? QString() : QStringLiteral("  ⚔"))});
      child->setData(0, k_id_role, site->id);
      child->setToolTip(0,
                        QStringLiteral("%1 · %2").arg(
                            title_case(S::to_string(site->kind)), site->id));
    }
  }
  auto* routes = group(QStringLiteral("Routes"), static_cast<int>(def.routes.size()));
  for (const auto& route : def.routes) {
    auto* item = new QTreeWidgetItem(routes, {def.display_name(route.id)});
    item->setData(0, k_id_role, route.id);
    item->setToolTip(0, title_case(S::to_string(route.kind)));
  }
  auto* armies = group(QStringLiteral("Armies"),
                       static_cast<int>(m_document->state().armies.size()));
  for (const auto& army : m_document->state().armies) {
    auto* item = new QTreeWidgetItem(armies, {army.name});
    item->setData(0, k_id_role, army.id);
  }
  auto* events = group(QStringLiteral("Events"), static_cast<int>(def.events.size()));
  for (const auto& event : def.events) {
    auto* item = new QTreeWidgetItem(
        events, {QStringLiteral("%1 — %2").arg(event.date_label(), event.name)});
    item->setData(0, k_id_role, QStringLiteral("event:%1").arg(event.id));
  }
  regions->setExpanded(true);
  m_rebuilding = false;
  apply_outline_filter(m_outline_filter->text());
}

void CampaignWorkspace::apply_outline_filter(const QString& text) {
  const QString needle = text.trimmed();
  std::function<bool(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) -> bool {
    bool any_child = false;
    for (int i = 0; i < item->childCount(); ++i) {
      any_child = visit(item->child(i)) || any_child;
    }
    const bool self =
        needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive) ||
        item->data(0, k_id_role).toString().contains(needle, Qt::CaseInsensitive);
    const bool show = self || any_child;
    item->setHidden(!show);
    if (!needle.isEmpty() && any_child) {
      item->setExpanded(true);
    }
    return show;
  };
  for (int i = 0; i < m_outline->topLevelItemCount(); ++i) {
    visit(m_outline->topLevelItem(i));
  }
}

void CampaignWorkspace::refresh_outline_selection() {
  const QString id = m_document->selection();
  const auto matches =
      m_outline->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
  const QSignalBlocker blocker(m_outline);
  m_outline->clearSelection();
  for (auto* item : matches) {
    if (!id.isEmpty() && item->data(0, k_id_role).toString() == id) {
      item->setSelected(true);
      m_outline->scrollToItem(item);
      break;
    }
  }
}

auto CampaignWorkspace::link(const QString& id) const -> QString {
  return QStringLiteral("<a href='sel:%1'>%2</a>")
      .arg(id, m_document->definition().display_name(id).toHtmlEscaped());
}

void CampaignWorkspace::handle_link(const QUrl& url) {
  const QString text = url.toString();
  if (text.startsWith(QStringLiteral("sel:"))) {
    select_entity(text.mid(4));
  } else if (text.startsWith(QStringLiteral("event:"))) {
    set_timeline(text.mid(6));
  } else if (text.startsWith(QStringLiteral("path:"))) {
    const QStringList parts = text.mid(5).split(QLatin1Char('|'));
    if (parts.size() == 2) {
      show_path(parts[0], parts[1]);
    }
  } else if (text.startsWith(QStringLiteral("open:"))) {
    open_battlefield_for(text.mid(5));
  } else if (text.startsWith(QStringLiteral("reach:"))) {
    run_reach_query(text.mid(6), 7.0);
  }
}

auto CampaignWorkspace::issues_html(const QString& id) const -> QString {
  QString out;
  for (const auto& issue : m_document->report().for_entity(id)) {
    if (issue.severity == S::Severity::Info && issue.entity != id) {
      continue;
    }
    out += QStringLiteral("<div class='%1'>%2 %3</div>")
               .arg(issue.severity == S::Severity::Error     ? QStringLiteral("bad")
                    : issue.severity == S::Severity::Warning ? QStringLiteral("warn")
                                                             : QStringLiteral("dim"),
                    issue.severity == S::Severity::Error ? QStringLiteral("✖")
                                                         : QStringLiteral("⚠"),
                    issue.message.toHtmlEscaped());
  }
  return out.isEmpty() ? QString() : QStringLiteral("<h4>Problems</h4>") + out;
}

auto CampaignWorkspace::site_html(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const auto* site = def.site(id);
  const QString owner = S::site_controller(def, state, id);
  QString html =
      QStringLiteral(
          "<h2>%1</h2><div class='dim'>%2%3 in %4%5 · held by <b>%6</b></div>")
          .arg(site->name.toHtmlEscaped(),
               site->latin_name.isEmpty() || site->latin_name == site->name
                   ? QString()
                   : QStringLiteral("<i>%1</i> · ")
                         .arg(site->latin_name.toHtmlEscaped()),
               title_case(S::to_string(site->kind)),
               link(site->region),
               site->district.isEmpty()
                   ? QString()
                   : QStringLiteral(" (%1)").arg(title_case(site->district)),
               def.display_name(owner).toHtmlEscaped());
  if (!site->battlefield.empty()) {
    html += QStringLiteral("<p><a href='open:%1'>⚔ Open battlefield%2 →</a></p>")
                .arg(id,
                     site->battlefield.mission.isEmpty()
                         ? QString()
                         : QStringLiteral(" and mission <i>%1</i>")
                               .arg(site->battlefield.mission));
  }
  html += QStringLiteral("<table><tr>");
  html += QStringLiteral("<td valign='top'><h4>Economy</h4><table>");
  for (const S::Resource resource : S::k_all_resources) {
    html += QStringLiteral("<tr><td>%1</td><td style='color:#E8C98B'>%2</td></tr>")
                .arg(title_case(S::to_string(resource)),
                     meter(site->resources.get(resource)));
  }
  html += QStringLiteral("</table></td><td valign='top'><h4>Military</h4><table>");
  const auto site_state = state.sites.find(id);
  html += QStringLiteral("<tr><td>Walls</td><td>%1</td></tr>")
              .arg(meter(S::site_fortification(def, state, id))
                       .left(S::k_fortification_max));
  html += QStringLiteral("<tr><td>People</td><td>%1</td></tr>")
              .arg(title_case(S::to_string(site->population)));
  html += QStringLiteral("<tr><td>Port</td><td>%1%2</td></tr>")
              .arg(title_case(S::to_string(site->port)),
                   site->has_port() && !S::port_usable(def, state, id)
                       ? QStringLiteral(" <span class='bad'>closed</span>")
                       : QString());
  if (site_state != state.sites.end() && site_state->second.garrison > 0) {
    html += QStringLiteral("<tr><td>Garrison</td><td>%1</td></tr>")
                .arg(number(site_state->second.garrison));
  }
  html += QStringLiteral("</table></td></tr></table>");
  html += QStringLiteral("<h4>Roads</h4>");
  for (const auto* route : def.routes_at(id)) {
    const S::RouteStatus status = S::route_status(state, route->id);
    html +=
        QStringLiteral(
            "%1 <span class='dim'>by</span> %2 <span class='dim'>· %3 d%4</span><br>")
            .arg(link(route->other_end(id)), link(route->id))
            .arg(def.route_days(*route), 0, 'f', 1)
            .arg(status == S::RouteStatus::Open
                     ? QString()
                     : QStringLiteral(" · <span class='warn'>%1</span>")
                           .arg(S::to_string(status)));
  }
  if (def.routes_at(id).isEmpty()) {
    html += QStringLiteral("<span class='bad'>No roads — use Add Route (R).</span>");
  }
  const auto here = state.armies_at(id);
  if (!here.isEmpty()) {
    html += QStringLiteral("<h4>Armies here</h4>");
    for (const auto* army : here) {
      html += QStringLiteral("%1 <span class='dim'>%2</span><br>")
                  .arg(link(army->id), number(army->soldiers()));
    }
  }
  const QStringList supplied = S::armies_supplied_by(def, state, id);
  if (!supplied.isEmpty()) {
    html += QStringLiteral("<h4>Feeds</h4>");
    for (const auto& army : supplied) {
      html += link(army) + QStringLiteral("<br>");
    }
  }
  const S::StrategicGraph graph(def, state);
  QString nearest;
  double nearest_days = 1e9;
  for (const auto& army : state.armies) {
    if (!S::are_hostile(def, army.faction, owner)) {
      continue;
    }
    const S::Path path =
        graph.shortest_path(army.location, id, S::TraversalRules::march(army.faction));
    if (path.found && path.days < nearest_days) {
      nearest_days = path.days;
      nearest = army.id;
    }
  }
  if (!nearest.isEmpty()) {
    html += QStringLiteral("<h4>Threat</h4>%1 can arrive in <b>%2 days</b> · <a "
                           "href='path:%3|%4'>show march</a>")
                .arg(link(nearest))
                .arg(nearest_days, 0, 'f', 1)
                .arg(state.army(nearest)->location, id);
  }
  QStringList events;
  for (const auto& event : def.events) {
    if (event.site == id) {
      events.append(
          QStringLiteral("<a href='event:%1'>%2</a> <span class='dim'>%3</span>")
              .arg(event.id, event.name.toHtmlEscaped(), event.date_label()));
    }
  }
  if (!events.isEmpty()) {
    html += QStringLiteral("<h4>History</h4>") + events.join(QStringLiteral("<br>"));
  }
  return html + issues_html(id);
}

auto CampaignWorkspace::route_html(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  const auto* route = def.route(id);
  const S::StrategicGraph graph(def, m_document->state());
  S::TraversalRules without = S::TraversalRules::anything();
  without.excluded_routes = {id};
  const int stranded =
      graph.cut_off_by(without) - graph.cut_off_by(S::TraversalRules::anything());
  const int alternatives = graph.route_count(route->from, route->to, without);
  QString html = QStringLiteral("<h2>%1</h2><div class='dim'>%2 · %3 → %4</div>")
                     .arg(def.display_name(id).toHtmlEscaped(),
                          title_case(S::to_string(route->kind)),
                          link(route->from),
                          link(route->to));
  html += QStringLiteral("<h4>March</h4><b>%1 days</b> · %2 km on the road (%3 km as "
                         "the crow flies) · "
                         "carries %4/5")
              .arg(def.route_days(*route), 0, 'f', 1)
              .arg(def.route_length_km(*route), 0, 'f', 0)
              .arg(def.route_geodesic_km(*route), 0, 'f', 0)
              .arg(route->throughput);
  html +=
      QStringLiteral("<h4>Status</h4>%1%2")
          .arg(title_case(S::to_string(S::route_status(m_document->state(), id))),
               route->chokepoint ? QStringLiteral(" · <b>chokepoint</b>") : QString());
  html +=
      QStringLiteral("<h4>Network</h4>%1<br>%2")
          .arg(stranded > 0
                   ? QStringLiteral("<span class='warn'>Cutting it strands %1.</span>")
                         .arg(plural(
                             stranded, QStringLiteral("site"), QStringLiteral("sites")))
                   : QStringLiteral("Cutting it strands nothing."),
               alternatives == 0
                   ? QStringLiteral("No other way between its ends.")
                   : QStringLiteral("%1 between its ends.")
                         .arg(plural(alternatives,
                                     QStringLiteral("other route"),
                                     QStringLiteral("other independent routes"))));
  if (!route->waypoints.isEmpty()) {
    html +=
        QStringLiteral("<p class='dim'>%1 — drag to move, double-click to remove.</p>")
            .arg(plural(static_cast<int>(route->waypoints.size()),
                        QStringLiteral("bend"),
                        QStringLiteral("bends")));
  }
  return html + issues_html(id);
}

auto CampaignWorkspace::army_html(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const auto* army = state.army(id);
  const auto* commander = def.commander(army->commander);
  QString html =
      QStringLiteral("<h2>%1</h2><div class='dim'>%2 · led by %3 · at %4</div>")
          .arg(army->name.toHtmlEscaped(),
               def.display_name(army->faction),
               commander != nullptr ? commander->name.toHtmlEscaped()
                                    : QStringLiteral("nobody"),
               link(army->location));
  html += QStringLiteral("<h4>Strength</h4><b>%1</b> foot · <b>%2</b> horse%3 <span "
                         "class='dim'>(%4)</span>")
              .arg(number(army->infantry),
                   number(army->cavalry),
                   army->elephants > 0
                       ? QStringLiteral(" · <b>%1</b> %2")
                             .arg(army->elephants)
                             .arg(army->elephants == 1 ? QStringLiteral("elephant")
                                                       : QStringLiteral("elephants"))
                       : QString(),
                   S::to_string(army->band()));
  if (const auto* supply = m_document->supply_for(id)) {
    const QString css =
        supply->level == S::SupplyLevel::Supplied ? QStringLiteral("good")
        : supply->level == S::SupplyLevel::CutOff ? QStringLiteral("bad")
                                                  : QStringLiteral("warn");
    html += QStringLiteral("<h4>Supply</h4><span class='%1'><b>%2</b></span> — %3")
                .arg(css,
                     title_case(S::to_string(supply->level)),
                     supply->reason.toHtmlEscaped());
    if (supply->path.sites.size() > 1) {
      QStringList stops;
      for (const auto& stop : supply->path.sites) {
        stops.append(link(stop));
      }
      html += QStringLiteral("<br>") + stops.join(QStringLiteral(" → "));
    }
  }
  if (!army->objective.isEmpty()) {
    const S::Path path = S::StrategicGraph(def, state)
                             .shortest_path(army->location,
                                            army->objective,
                                            S::TraversalRules::march(army->faction));
    html +=
        QStringLiteral("<h4>Objective</h4>%1 — %2")
            .arg(link(army->objective),
                 path.found ? QStringLiteral("<a href='path:%1|%2'>%3 days away</a>")
                                  .arg(army->location, army->objective)
                                  .arg(path.days, 0, 'f', 1)
                            : QStringLiteral("<span class='bad'>unreachable</span>"));
  }
  const auto reach =
      S::StrategicGraph(def, state)
          .reachable(army->location, S::TraversalRules::march(army->faction), 7.0);
  html += QStringLiteral("<h4>Within a week</h4>%1 · <a href='reach:%2'>show</a>")
              .arg(plural(static_cast<int>(reach.size()) - 1,
                          QStringLiteral("site"),
                          QStringLiteral("sites")),
                   army->location);
  return html + issues_html(id);
}

auto CampaignWorkspace::region_html(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const auto* region = def.region(id);
  QString html =
      QStringLiteral("<h2>%1</h2><div class='dim'>%2 · %3 · importance %4/5</div>")
          .arg(region->name.toHtmlEscaped(),
               region->historical_name.toHtmlEscaped(),
               S::to_string(region->terrain))
          .arg(region->importance);
  html += QStringLiteral("<h4>Control</h4>Held by <b>%1</b>%2")
              .arg(def.display_name(S::region_controller(def, state, id)),
                   S::region_is_contested(def, state, id)
                       ? QStringLiteral(" · <span class='bad'>contested</span>")
                       : QString());
  S::ResourceProfile total;
  html += QStringLiteral("<h4>Sites</h4>");
  for (const auto* site : def.sites_in(id)) {
    for (const S::Resource resource : S::k_all_resources) {
      total.set(resource, total.get(resource) + site->resources.get(resource));
    }
    html += QStringLiteral("%1 <span class='dim'>%2</span><br>")
                .arg(link(site->id),
                     def.display_name(S::site_controller(def, state, site->id)));
  }
  QStringList parts;
  for (const S::Resource resource : S::k_all_resources) {
    parts.append(QStringLiteral("%1 %2")
                     .arg(title_case(S::to_string(resource)))
                     .arg(total.get(resource)));
  }
  html += QStringLiteral("<h4>Output</h4>") + parts.join(QStringLiteral(" · "));
  if (!region->adjacent.isEmpty()) {
    QStringList neighbours;
    for (const auto& other : region->adjacent) {
      neighbours.append(link(other));
    }
    html += QStringLiteral("<h4>Borders</h4>") + neighbours.join(QStringLiteral(", "));
  }
  return html + issues_html(id);
}

auto CampaignWorkspace::overview_html() const -> QString {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  if (def.id.isEmpty()) {
    return QStringLiteral(
        "<h2>No campaign world</h2><p>Open one from File ▸ Open Campaign World.</p>");
  }
  QString html = QStringLiteral("<h2>%1</h2><div class='dim'>%2</div>")
                     .arg(def.name.toHtmlEscaped(),
                          m_document->current_event() != nullptr
                              ? m_document->current_event()->date_label()
                              : def.start_label);
  html += QStringLiteral("<h4>Armies</h4><table>");
  for (const auto& army : state.armies) {
    const auto* supply = m_document->supply_for(army.id);
    html += QStringLiteral("<tr><td>%1</td><td>%2</td><td class='dim'>%3</td></tr>")
                .arg(link(army.id),
                     number(army.soldiers()),
                     supply != nullptr ? S::to_string(supply->level) : QString());
  }
  html += QStringLiteral("</table>");
  const auto isolated = S::isolated_sites(def, state);
  if (!isolated.isEmpty()) {
    html += QStringLiteral("<h4>Cut off from home</h4>");
    for (const auto& entry : isolated) {
      html += QStringLiteral("%1 <span class='dim'>cannot reach</span> %2<br>")
                  .arg(link(entry.site), link(entry.capital));
    }
  }
  const auto encounters = S::suggest_encounters(def, state);
  if (!encounters.isEmpty()) {
    html += QStringLiteral("<h4>Likely battles</h4>");
    for (const auto& encounter : encounters.mid(0, 6)) {
      html += QStringLiteral("%1 — %2 vs %3<br>")
                  .arg(link(encounter.site),
                       link(encounter.attacker_army),
                       link(encounter.defender_army));
    }
  }
  html += QStringLiteral(
      "<p class='dim'>Click anything on the map. Right-click for actions; "
      "double-click a ⚔ site to edit its battlefield.</p>");
  return html;
}

auto CampaignWorkspace::inspector_html(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  if (def.site(id) != nullptr) {
    return site_html(id);
  }
  if (def.route(id) != nullptr) {
    return route_html(id);
  }
  if (m_document->state().army(id) != nullptr) {
    return army_html(id);
  }
  if (def.region(id) != nullptr) {
    return region_html(id);
  }
  return overview_html();
}

void CampaignWorkspace::refresh_inspector() {
  const QString id = m_document->selection();
  const int scroll = m_inspector->verticalScrollBar() != nullptr
                         ? m_inspector->verticalScrollBar()->value()
                         : 0;
  m_inspector->setHtml(inspector_html(id));
  if (m_inspector->verticalScrollBar() != nullptr) {
    m_inspector->verticalScrollBar()->setValue(scroll);
  }
  const auto* site = m_document->definition().site(id);
  m_open_button->setEnabled(site != nullptr && !site->battlefield.empty());
  m_generate_button->setEnabled(site != nullptr);
  m_supply_button->setEnabled(m_document->state().army(id) != nullptr ||
                              site != nullptr);
}

void CampaignWorkspace::refresh_properties() {
  m_rebuilding = true;
  while (m_properties_form->rowCount() > 0) {
    m_properties_form->removeRow(0);
  }
  const QString id = m_document->selection();
  const auto& def = m_document->definition();
  if (def.site(id) != nullptr) {
    add_site_form(m_properties_form, id);
  } else if (def.route(id) != nullptr) {
    add_route_form(m_properties_form, id);
  } else if (def.region(id) != nullptr) {
    add_region_form(m_properties_form, id);
  } else if (m_document->state().army(id) != nullptr) {
    add_army_form(m_properties_form, id);
  } else {
    m_properties_form->addRow(
        new QLabel(QStringLiteral("<span style='color:#9C8158'>Select a site, "
                                  "route, region or army to edit it.</span>")));
  }
  m_rebuilding = false;
}

void CampaignWorkspace::add_site_form(QFormLayout* form, const QString& id) {
  const auto& def = m_document->definition();
  const S::Site site = *def.site(id);
  auto apply = [this, id](const std::function<void(S::Site&)>& change) {
    if (m_rebuilding) {
      return;
    }
    const auto* current = m_document->definition().site(id);
    if (current == nullptr) {
      return;
    }
    S::Site copy = *current;
    change(copy);
    m_document->update_site(copy);
  };
  form->addRow(new QLabel(
      QStringLiteral("<b>Properties</b> <span style='color:#9C8158'>%1</span>")
          .arg(id)));
  const auto text_field =
      [&](const QString& label,
          const QString& value,
          const std::function<void(S::Site&, const QString&)>& setter,
          const QString& placeholder = {}) {
        auto* edit = new QLineEdit(value);
        edit->setPlaceholderText(placeholder);
        connect(edit, &QLineEdit::editingFinished, this, [apply, edit, setter]() {
          apply([&](S::Site& s) { setter(s, edit->text().trimmed()); });
        });
        form->addRow(label, edit);
      };
  const auto choice = [&](const QString& label,
                          const QVector<std::pair<QString, QString>>& items,
                          const QString& current,
                          const std::function<void(S::Site&, const QString&)>& setter) {
    auto* box = new QComboBox();
    for (const auto& [value, text] : items) {
      box->addItem(text, value);
    }
    box->setCurrentIndex(std::max(0, box->findData(current)));
    connect(box, &QComboBox::currentIndexChanged, this, [apply, box, setter]() {
      apply([&](S::Site& s) { setter(s, box->currentData().toString()); });
    });
    form->addRow(label, box);
  };
  const auto names = [](const QStringList& values) {
    QVector<std::pair<QString, QString>> out;
    for (const auto& value : values) {
      out.append({value, title_case(value)});
    }
    return out;
  };
  text_field(QStringLiteral("Name"), site.name, [](S::Site& s, const QString& v) {
    s.name = v;
  });
  text_field(QStringLiteral("Latin"),
             site.latin_name,
             [](S::Site& s, const QString& v) { s.latin_name = v; });
  choice(QStringLiteral("Kind"),
         names(S::all_site_kind_names()),
         S::to_string(site.kind),
         [](S::Site& s, const QString& v) { s.kind = *S::site_kind_from_string(v); });
  QVector<std::pair<QString, QString>> regions;
  for (const auto& region : def.regions) {
    regions.append({region.id, region.name});
  }
  choice(
      QStringLiteral("Region"), regions, site.region, [](S::Site& s, const QString& v) {
        s.region = v;
        s.district.clear();
      });
  QVector<std::pair<QString, QString>> districts{{QString(), QStringLiteral("—")}};
  if (const auto* region = def.region(site.region)) {
    for (const auto& district : region->districts) {
      districts.append({district.id, district.name});
    }
  }
  choice(QStringLiteral("District"),
         districts,
         site.district,
         [](S::Site& s, const QString& v) { s.district = v; });
  choice(QStringLiteral("Population"),
         names({QStringLiteral("none"),
                QStringLiteral("hamlet"),
                QStringLiteral("small"),
                QStringLiteral("medium"),
                QStringLiteral("large"),
                QStringLiteral("metropolis")}),
         S::to_string(site.population),
         [](S::Site& s, const QString& v) {
           s.population = *S::population_from_string(v);
         });
  choice(
      QStringLiteral("Port"),
      names({QStringLiteral("none"), QStringLiteral("minor"), QStringLiteral("major")}),
      S::to_string(site.port),
      [](S::Site& s, const QString& v) { s.port = *S::port_class_from_string(v); });
  choice(QStringLiteral("Terrain"),
         names(S::all_terrain_names()),
         S::to_string(site.terrain),
         [](S::Site& s, const QString& v) { s.terrain = *S::terrain_from_string(v); });
  const auto spin = [&](const QString& label,
                        int value,
                        int max,
                        const std::function<void(S::Site&, int)>& setter) {
    auto* box = new QSpinBox();
    box->setRange(0, max);
    box->setValue(value);
    connect(box, &QSpinBox::valueChanged, this, [apply, setter](int v) {
      apply([&](S::Site& s) { setter(s, v); });
    });
    form->addRow(label, box);
  };
  spin(QStringLiteral("Walls"),
       site.fortification,
       S::k_fortification_max,
       [](S::Site& s, int v) { s.fortification = v; });
  for (const S::Resource resource : S::k_all_resources) {
    spin(title_case(S::to_string(resource)),
         site.resources.get(resource),
         S::k_resource_max,
         [resource](S::Site& s, int v) { s.resources.set(resource, v); });
  }
  text_field(
      QStringLiteral("Battlefield map"),
      site.battlefield.map,
      [](S::Site& s, const QString& v) { s.battlefield.map = v; },
      QStringLiteral("assets/maps/…json"));
  text_field(
      QStringLiteral("Mission"),
      site.battlefield.mission,
      [](S::Site& s, const QString& v) { s.battlefield.mission = v; },
      QStringLiteral("mission id"));
  QVector<std::pair<QString, QString>> presets{
      {QString(), QStringLiteral("Automatic")}};
  for (const auto& preset : S::known_generator_presets()) {
    presets.append({preset, title_case(preset)});
  }
  choice(QStringLiteral("Generator preset"),
         presets,
         site.battlefield.preset,
         [](S::Site& s, const QString& v) { s.battlefield.preset = v; });
  text_field(QStringLiteral("Tags"),
             site.tags.join(QStringLiteral(", ")),
             [](S::Site& s, const QString& v) {
               s.tags.clear();
               for (const auto& part : v.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                 s.tags.append(part.trimmed());
               }
             });
  if (m_document->at_start()) {
    auto* owner = new QComboBox();
    owner->addItem(QStringLiteral("Region's controller"), QString());
    owner->addItem(QStringLiteral("Neutral"),
                   QString::fromLatin1(S::k_neutral_faction));
    for (const auto& faction : def.factions) {
      owner->addItem(faction.name, faction.id);
    }
    const auto it = def.initial_state.sites.find(id);
    owner->setCurrentIndex(std::max(0,
                                    owner->findData(it != def.initial_state.sites.end()
                                                        ? it->second.controller
                                                        : QString())));
    connect(owner, &QComboBox::currentIndexChanged, this, [this, id, owner]() {
      if (!m_rebuilding) {
        m_document->set_initial_controller(id, owner->currentData().toString());
      }
    });
    form->addRow(QStringLiteral("Held in %1 by")
                     .arg(def.start_label.section(QLatin1Char(','), 0, 0)),
                 owner);
  } else {
    form->addRow(new QLabel(
        QStringLiteral("<span style='color:#9C8158'>Ownership changes come from "
                       "events. Rewind the timeline (Ctrl+[) to edit the "
                       "opening situation.</span>")));
  }
}

void CampaignWorkspace::add_route_form(QFormLayout* form, const QString& id) {
  const S::Route route = *m_document->definition().route(id);
  auto apply = [this, id](const std::function<void(S::Route&)>& change) {
    if (m_rebuilding) {
      return;
    }
    const auto* current = m_document->definition().route(id);
    if (current == nullptr) {
      return;
    }
    S::Route copy = *current;
    change(copy);
    m_document->update_route(copy);
  };
  form->addRow(new QLabel(
      QStringLiteral("<b>Properties</b> <span style='color:#9C8158'>%1</span>")
          .arg(id)));
  auto* name = new QLineEdit(route.name);
  name->setPlaceholderText(QStringLiteral("e.g. Via Appia"));
  connect(name, &QLineEdit::editingFinished, this, [apply, name]() {
    apply([&](S::Route& r) { r.name = name->text().trimmed(); });
  });
  form->addRow(QStringLiteral("Name"), name);
  auto* kind = new QComboBox();
  for (const auto& n : S::all_route_kind_names()) {
    kind->addItem(title_case(n), n);
  }
  kind->setCurrentIndex(std::max(0, kind->findData(S::to_string(route.kind))));
  connect(kind, &QComboBox::currentIndexChanged, this, [apply, kind]() {
    apply([&](S::Route& r) {
      r.kind = *S::route_kind_from_string(kind->currentData().toString());
    });
  });
  form->addRow(QStringLiteral("Kind"), kind);
  auto* throughput = new QSpinBox();
  throughput->setRange(1, S::k_throughput_max);
  throughput->setValue(route.throughput);
  throughput->setToolTip(QStringLiteral("How much supply the route carries (1–5)"));
  connect(throughput, &QSpinBox::valueChanged, this, [apply](int v) {
    apply([&](S::Route& r) { r.throughput = v; });
  });
  form->addRow(QStringLiteral("Throughput"), throughput);
  auto* length = new QDoubleSpinBox();
  length->setRange(0.0, 5000.0);
  length->setDecimals(0);
  length->setSpecialValueText(QStringLiteral("Measured"));
  length->setSuffix(QStringLiteral(" km"));
  length->setValue(route.length_km);
  connect(length, &QDoubleSpinBox::editingFinished, this, [apply, length]() {
    apply([&](S::Route& r) { r.length_km = length->value(); });
  });
  form->addRow(QStringLiteral("Length"), length);
  auto* choke = new QCheckBox(QStringLiteral("Chokepoint"));
  choke->setChecked(route.chokepoint);
  connect(choke, &QCheckBox::toggled, this, [apply](bool on) {
    apply([&](S::Route& r) { r.chokepoint = on; });
  });
  form->addRow(QString(), choke);
  if (m_document->at_start()) {
    auto* status = new QComboBox();
    for (const auto& s : {QStringLiteral("open"),
                          QStringLiteral("contested"),
                          QStringLiteral("blocked")}) {
      status->addItem(title_case(s), s);
    }
    status->setCurrentIndex(
        std::max(0,
                 status->findData(S::to_string(
                     S::route_status(m_document->definition().initial_state, id)))));
    connect(status, &QComboBox::currentIndexChanged, this, [this, id, status]() {
      if (!m_rebuilding) {
        m_document->set_initial_route_status(
            id, *S::route_status_from_string(status->currentData().toString()));
      }
    });
    form->addRow(QStringLiteral("Opening status"), status);
  }
  auto* straighten = new QPushButton(QStringLiteral("Straighten"));
  straighten->setEnabled(!route.waypoints.isEmpty());
  connect(straighten, &QPushButton::clicked, this, [this, id]() {
    m_document->clear_waypoints(id);
  });
  form->addRow(QString(), straighten);
}

void CampaignWorkspace::add_region_form(QFormLayout* form, const QString& id) {
  const auto& def = m_document->definition();
  form->addRow(new QLabel(
      QStringLiteral("<b>Properties</b> <span style='color:#9C8158'>%1</span>")
          .arg(id)));
  auto* importance = new QSpinBox();
  importance->setRange(1, S::k_importance_max);
  importance->setValue(def.region(id)->importance);
  connect(importance, &QSpinBox::valueChanged, this, [this, id](int v) {
    if (m_rebuilding) {
      return;
    }
    S::Region copy = *m_document->definition().region(id);
    copy.importance = v;
    m_document->update_region(copy);
  });
  form->addRow(QStringLiteral("Importance"), importance);
  if (!m_document->at_start()) {
    return;
  }
  auto* owner = new QComboBox();
  owner->addItem(QStringLiteral("Neutral"), QString::fromLatin1(S::k_neutral_faction));
  for (const auto& faction : def.factions) {
    owner->addItem(faction.name, faction.id);
  }
  owner->setCurrentIndex(
      std::max(0, owner->findData(S::region_controller(def, def.initial_state, id))));
  connect(owner, &QComboBox::currentIndexChanged, this, [this, id, owner]() {
    if (!m_rebuilding) {
      m_document->set_initial_controller(id, owner->currentData().toString());
    }
  });
  form->addRow(QStringLiteral("Held at the start by"), owner);
}

void CampaignWorkspace::add_army_form(QFormLayout* form, const QString& id) {
  const auto& def = m_document->definition();
  form->addRow(new QLabel(
      QStringLiteral("<b>Properties</b> <span style='color:#9C8158'>%1</span>")
          .arg(id)));
  if (!m_document->at_start() || def.initial_state.army(id) == nullptr) {
    form->addRow(new QLabel(
        QStringLiteral("<span style='color:#9C8158'>Armies are moved by events. "
                       "Rewind the timeline (Ctrl+[) to change the opening "
                       "deployment.</span>")));
    return;
  }
  auto* location = no_insert_combo(nullptr);
  for (const auto& site : def.sites) {
    location->addItem(site.name, site.id);
  }
  location->setCurrentIndex(
      std::max(0, location->findData(def.initial_state.army(id)->location)));
  connect(location, &QComboBox::currentIndexChanged, this, [this, id, location]() {
    if (!m_rebuilding) {
      m_document->set_initial_army_location(id, location->currentData().toString());
    }
  });
  form->addRow(QStringLiteral("Starts at"), location);
}

void CampaignWorkspace::refresh_timeline() {
  m_rebuilding = true;
  const auto& def = m_document->definition();
  if (m_event_list->count() != def.events.size() + 1) {
    m_event_list->clear();
    m_event_list->addItem(
        QStringLiteral("%1 — opening situation").arg(def.start_label));
    for (const auto& event : def.events) {
      auto* item = new QListWidgetItem(
          QStringLiteral("%1 — %2").arg(event.date_label(), event.name));
      if (!event.mission.isEmpty()) {
        item->setForeground(QColor(232, 201, 139));
        item->setToolTip(QStringLiteral("Mission %1").arg(event.mission));
      }
      m_event_list->addItem(item);
    }
  }
  m_event_list->setCurrentRow(m_document->timeline_index());
  m_event_list->scrollToItem(m_event_list->currentItem(),
                             QAbstractItemView::PositionAtCenter);
  m_rebuilding = false;
  m_strip->update();
  QString html;
  if (const auto* event = m_document->current_event()) {
    html += QStringLiteral("<h2>%1</h2><div class='dim'>%2%3</div><p>%4</p>")
                .arg(event->name.toHtmlEscaped(),
                     event->date_label(),
                     event->mission.isEmpty()
                         ? QString()
                         : QStringLiteral(" · mission <a href='open:%1'>%2</a>")
                               .arg(def.site_for_mission(event->mission) != nullptr
                                        ? def.site_for_mission(event->mission)->id
                                        : QString(),
                                    event->mission),
                     event->summary.toHtmlEscaped());
    const auto changes =
        S::diff_states(def, m_document->previous_state(), m_document->state());
    html += QStringLiteral("<h4>Changes</h4><table>");
    for (const auto& change : changes) {
      const bool entity = def.kind_of(change.entity) != S::EntityKind::None;
      html += QStringLiteral("<tr><td>%1</td><td class='dim'>%2</td><td "
                             "class='dim'>%3</td><td>→ <b>%4</b></td></tr>")
                  .arg(entity ? link(change.entity) : change.entity.toHtmlEscaped(),
                       change.field,
                       (change.before.isEmpty() ? QStringLiteral("–")
                                                : def.display_name(change.before))
                           .toHtmlEscaped(),
                       (change.after.isEmpty() ? QStringLiteral("–")
                                               : def.display_name(change.after))
                           .toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
  } else {
    html = QStringLiteral(
               "<h2>%1</h2><p class='dim'>The opening situation. Step through the war "
               "with [ and ], or click the timeline. Each event is applied to the "
               "world deterministically; what it changes is listed here.</p>")
               .arg(def.start_label.toHtmlEscaped());
  }
  m_event_detail->setHtml(html);
}

void CampaignWorkspace::refresh_problems() {
  const auto& report = m_document->report();
  m_issues->clear();
  for (const auto& issue : report.issues) {
    const bool visible =
        (issue.severity == S::Severity::Error && m_show_errors->isChecked()) ||
        (issue.severity == S::Severity::Warning && m_show_warnings->isChecked()) ||
        (issue.severity == S::Severity::Info && m_show_notes->isChecked());
    if (!visible) {
      continue;
    }
    auto* item = new QTreeWidgetItem(m_issues);
    item->setText(0,
                  QStringLiteral("%1  %2").arg(
                      issue.severity == S::Severity::Error     ? QStringLiteral("✖")
                      : issue.severity == S::Severity::Warning ? QStringLiteral("⚠")
                                                               : QStringLiteral("ℹ"),
                      title_case(issue.code)));
    item->setText(1, m_document->definition().display_name(issue.entity));
    item->setText(2, issue.message);
    const QString help = S::validation_code_help(issue.code);
    item->setToolTip(2, help.isEmpty() ? issue.message : help);
    item->setForeground(0,
                        issue.severity == S::Severity::Error ? QColor(255, 138, 122)
                        : issue.severity == S::Severity::Warning
                            ? QColor(245, 192, 96)
                            : QColor(159, 196, 232));
    item->setData(0, k_id_role, issue.entity);
  }
  m_issues->resizeColumnToContents(0);
  m_issues->resizeColumnToContents(1);
  int notes = 0;
  for (const auto& issue : report.issues) {
    notes += issue.severity == S::Severity::Info ? 1 : 0;
  }
  m_show_errors->setText(QStringLiteral("✖ %1").arg(report.errors()));
  m_show_warnings->setText(QStringLiteral("⚠ %1").arg(report.warnings()));
  m_show_notes->setText(QStringLiteral("ℹ %1").arg(notes));
  m_issue_summary->setText(
      report.errors() == 0 && report.warnings() == 0
          ? QStringLiteral("<span style='color:#8fdc9f'>✔ Every check passes.</span>")
          : QStringLiteral("Click a problem to find it on the map."));
  if (auto* dock = findChild<QDockWidget*>(QStringLiteral("campaign_problems"))) {
    dock->setWindowTitle(
        report.errors() + report.warnings() == 0
            ? QStringLiteral("Problems")
            : QStringLiteral("Problems (%1)").arg(report.errors() + report.warnings()));
  }
}

void CampaignWorkspace::refresh_query_sites() {
  const auto& def = m_document->definition();
  for (auto* box : {m_query_from, m_query_to}) {
    const QString current = box->currentData().toString();
    const QSignalBlocker blocker(box);
    box->clear();
    QVector<const S::Site*> sorted;
    for (const auto& site : def.sites) {
      sorted.append(&site);
    }
    std::sort(sorted.begin(), sorted.end(), [](const S::Site* a, const S::Site* b) {
      return a->name < b->name;
    });
    for (const auto* site : sorted) {
      box->addItem(site->name, site->id);
    }
    box->setCurrentIndex(std::max(0, box->findData(current)));
  }
  const QString faction = m_query_faction->currentData().toString();
  m_query_faction->clear();
  m_query_faction->addItem(QStringLiteral("Anyone"), QString());
  for (const auto& entry : def.factions) {
    m_query_faction->addItem(entry.name, entry.id);
  }
  m_query_faction->setCurrentIndex(std::max(0, m_query_faction->findData(faction)));
}

void CampaignWorkspace::show_path(const QString& from, const QString& to) {
  m_query_from->setCurrentIndex(std::max(0, m_query_from->findData(from)));
  m_query_to->setCurrentIndex(std::max(0, m_query_to->findData(to)));
  if (auto* tabs = qobject_cast<QTabWidget*>(m_explorer_tabs)) {
    tabs->setCurrentIndex(2);
  }
  run_path_query();
}

void CampaignWorkspace::run_path_query() {
  const auto& def = m_document->definition();
  S::TraversalRules rules;
  rules.faction = m_query_faction->currentData().toString();
  rules.avoid_hostile = m_query_safe->isChecked();
  rules.allow_sea = m_query_sea->isChecked();
  const QString from = m_query_from->currentData().toString();
  const QString to = m_query_to->currentData().toString();
  const S::Path path =
      S::StrategicGraph(def, m_document->state()).shortest_path(from, to, rules);
  m_canvas->clear_reach();
  if (!path.found) {
    m_canvas->clear_highlight_path();
    m_query_result->setHtml(
        QStringLiteral("<span class='bad'><b>No route</b></span> from %1 to %2 "
                       "under these rules.")
            .arg(link(from), link(to)));
    return;
  }
  m_canvas->set_highlight_path(path);
  QString html =
      QStringLiteral("<h2>%1 days</h2><div class='dim'>%2 km · %3 · narrowest road "
                     "carries %4/5</div><ol>")
          .arg(path.days, 0, 'f', 1)
          .arg(path.km, 0, 'f', 0)
          .arg(plural(static_cast<int>(path.routes.size()),
                      QStringLiteral("leg"),
                      QStringLiteral("legs")) +
               (path.sea_legs > 0 ? QStringLiteral(", %1 by sea").arg(path.sea_legs)
                                  : QString()))
          .arg(path.min_throughput);
  for (int i = 0; i < path.routes.size(); ++i) {
    html += QStringLiteral("<li>%1 <span class='dim'>by</span> %2</li>")
                .arg(link(path.sites[i + 1]), link(path.routes[i]));
  }
  m_query_result->setHtml(html + QStringLiteral("</ol>"));
  emit feedback(QStringLiteral("March from %1 to %2: %3 days. Esc clears.")
                    .arg(def.display_name(from), def.display_name(to))
                    .arg(path.days, 0, 'f', 1),
                true);
}

void CampaignWorkspace::run_route_count_query() {
  S::TraversalRules rules;
  rules.faction = m_query_faction->currentData().toString();
  rules.avoid_hostile = m_query_safe->isChecked();
  rules.allow_sea = m_query_sea->isChecked();
  const QString from = m_query_from->currentData().toString();
  const QString to = m_query_to->currentData().toString();
  const int count = S::StrategicGraph(m_document->definition(), m_document->state())
                        .route_count(from, to, rules);
  m_query_result->setHtml(
      QStringLiteral("<h2>%1</h2><div class='dim'>independent routes from %2 to %3. "
                     "An enemy must cut %1 to separate them.</div>")
          .arg(count)
          .arg(link(from), link(to)));
}

void CampaignWorkspace::run_reach_query(const QString& from_override,
                                        double days_override) {
  const auto& def = m_document->definition();
  S::TraversalRules rules;
  rules.faction = m_query_faction->currentData().toString();
  rules.avoid_hostile = m_query_safe->isChecked();
  rules.allow_sea = m_query_sea->isChecked();
  const QString from =
      from_override.isEmpty() ? m_query_from->currentData().toString() : from_override;
  const double days = days_override > 0 ? days_override : m_query_days->value();
  const auto reach =
      S::StrategicGraph(def, m_document->state()).reachable(from, rules, days);
  m_canvas->clear_highlight_path();
  m_canvas->set_reach(reach, days);
  QVector<std::pair<double, QString>> sorted;
  for (const auto& [site, d] : reach) {
    sorted.append({d, site});
  }
  std::sort(sorted.begin(), sorted.end());
  QString html =
      QStringLiteral("<h2>%1</h2><div class='dim'>within %2 days of %3</div><table>")
          .arg(plural(static_cast<int>(reach.size()),
                      QStringLiteral("site"),
                      QStringLiteral("sites")))
          .arg(days)
          .arg(link(from));
  for (const auto& [d, site] : sorted) {
    html += QStringLiteral("<tr><td>%1</td><td class='dim'>%2 d</td></tr>")
                .arg(link(site))
                .arg(d, 0, 'f', 1);
  }
  m_query_result->setHtml(html + QStringLiteral("</table>"));
  if (auto* tabs = qobject_cast<QTabWidget*>(m_explorer_tabs)) {
    tabs->setCurrentIndex(2);
  }
}

void CampaignWorkspace::run_network_report() {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const S::StrategicGraph graph(def, state);
  QString html = QStringLiteral("<h4>Cut off from home</h4>");
  const auto isolated = S::isolated_sites(def, state);
  if (isolated.isEmpty()) {
    html += QStringLiteral(
        "<span class='dim'>None — every settlement can reach its capital.</span>");
  }
  for (const auto& entry : isolated) {
    html += QStringLiteral("%1 <span class='dim'>cannot reach</span> %2<br>")
                .arg(link(entry.site), link(entry.capital));
  }
  html += QStringLiteral("<h4>Threatened routes</h4>");
  const auto threats = S::threatened_routes(def, state, m_query_days->value());
  if (threats.isEmpty()) {
    html += QStringLiteral("<span class='dim'>None within %1 days.</span>")
                .arg(m_query_days->value());
  }
  for (const auto& threat : threats.mid(0, 14)) {
    html += QStringLiteral("%1 <span class='dim'>— %2 in %3 d</span><br>")
                .arg(link(threat.route), link(threat.army))
                .arg(threat.days, 0, 'f', 1);
  }
  html += QStringLiteral("<h4>Chokepoints</h4>");
  QStringList sites;
  for (const auto& site : graph.chokepoint_sites(S::TraversalRules::anything())) {
    sites.append(link(site));
  }
  html += sites.join(QStringLiteral(", "));
  html += QStringLiteral("<h4>Overland chokepoints</h4>");
  QStringList land;
  for (const auto& site : graph.chokepoint_sites(S::TraversalRules::land_only())) {
    land.append(link(site));
  }
  html += land.join(QStringLiteral(", "));
  m_query_result->setHtml(html);
  if (auto* tabs = qobject_cast<QTabWidget*>(m_explorer_tabs)) {
    tabs->setCurrentIndex(2);
  }
}

void CampaignWorkspace::open_battlefield_for(const QString& id) {
  const auto* site = m_document->definition().site(id);
  if (site == nullptr || site->battlefield.empty()) {
    emit feedback(
        QStringLiteral("This site has no battlefield yet — use Generate Battlefield."),
        false);
    return;
  }
  const QString map =
      site->battlefield.map.isEmpty()
          ? QString()
          : S::resolve_asset_path(m_document->root(), site->battlefield.map);
  emit open_battlefield_requested(map, site->battlefield.mission, id);
}

void CampaignWorkspace::generate_battlefield_for(const QString& id) {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  if (def.site(id) == nullptr) {
    return;
  }
  const S::Encounter encounter = S::encounter_at(def, state, id);
  const S::EncounterContext context =
      S::encounter_context(def, state, encounter, S::current_season(def, state));
  emit generate_battlefield_requested(S::generation_request(def, state, context),
                                      QStringLiteral("%1 — %2, %3 attacking %4")
                                          .arg(context.site_name,
                                               context.kind,
                                               def.display_name(context.attacker),
                                               def.display_name(context.defender)),
                                      id);
}

void CampaignWorkspace::show_supply_for(const QString& id) {
  const auto& def = m_document->definition();
  QStringList armies;
  if (m_document->state().army(id) != nullptr) {
    armies = {id};
  } else if (def.site(id) != nullptr) {
    armies = S::armies_supplied_by(def, m_document->state(), id);
  }
  m_canvas->set_layer(WorldCanvas::Supply, true);
  if (armies.size() == 1) {
    if (const auto* report = m_document->supply_for(armies.first());
        report != nullptr && report->path.found) {
      m_canvas->set_highlight_path(report->path);
    }
  }
}

void CampaignWorkspace::delete_entity(const QString& id) {
  const auto& def = m_document->definition();
  const QString name = def.display_name(id);
  const S::EntityKind kind = def.kind_of(id);
  if (kind != S::EntityKind::Site && kind != S::EntityKind::Route) {
    if (!id.isEmpty()) {
      emit feedback(QStringLiteral("Only sites and routes can be deleted here."),
                    false);
    }
    return;
  }
  const int roads =
      kind == S::EntityKind::Site ? static_cast<int>(def.routes_at(id).size()) : 0;
  if (m_document->remove(id)) {
    emit feedback(
        roads > 0
            ? QStringLiteral("Deleted %1 and its %2. Ctrl+Z restores them.")
                  .arg(name,
                       plural(roads, QStringLiteral("road"), QStringLiteral("roads")))
            : QStringLiteral("Deleted %1. Ctrl+Z restores it.").arg(name),
        true);
  }
}

void CampaignWorkspace::show_context_menu(const QString& id,
                                          const S::LonLat& position,
                                          const QPoint& global) {
  const auto& def = m_document->definition();
  QMenu menu(this);
  const auto* site = def.site(id);
  const auto* route = def.route(id);
  const auto* army = m_document->state().army(id);
  const auto* region = def.region(id);
  if (site != nullptr) {
    menu.addSection(site->name);
    if (!site->battlefield.empty()) {
      auto* open = menu.addAction(QStringLiteral("Open Battlefield"),
                                  this,
                                  [this, id]() { open_battlefield_for(id); });
      QFont bold = open->font();
      bold.setBold(true);
      open->setFont(bold);
    }
    menu.addAction(QStringLiteral("Generate Battlefield"), this, [this, id]() {
      generate_battlefield_for(id);
    });
    menu.addSeparator();
    menu.addAction(QStringLiteral("March From Here…"), this, [this, id]() {
      m_query_from->setCurrentIndex(std::max(0, m_query_from->findData(id)));
      m_canvas->set_tool(WorldCanvas::Tool::PathQuery);
      m_tools->actions().at(4)->setChecked(true);
      emit feedback(QStringLiteral("Now click the destination."), true);
    });
    menu.addAction(QStringLiteral("Reach in 7 Days"), this, [this, id]() {
      run_reach_query(id, 7.0);
    });
    menu.addAction(
        QStringLiteral("Show Supply"), this, [this, id]() { show_supply_for(id); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Connect With a Road…"), this, [this]() {
      m_tools->actions().at(2)->trigger();
      emit feedback(QStringLiteral("Click this site, then the site to connect."), true);
    });
    if (m_document->at_start()) {
      auto* owner = menu.addMenu(QStringLiteral("Held at the Start By"));
      owner->addAction(QStringLiteral("Region's Controller"), this, [this, id]() {
        m_document->set_initial_controller(id, {});
      });
      owner->addAction(QStringLiteral("Neutral"), this, [this, id]() {
        m_document->set_initial_controller(id,
                                           QString::fromLatin1(S::k_neutral_faction));
      });
      for (const auto& faction : def.factions) {
        owner->addAction(faction.name, this, [this, id, f = faction.id]() {
          m_document->set_initial_controller(id, f);
        });
      }
    }
    menu.addSeparator();
    menu.addAction(
        QStringLiteral("Delete Site"), this, [this, id]() { delete_entity(id); });
  } else if (route != nullptr) {
    menu.addSection(def.display_name(id));
    menu.addAction(QStringLiteral("Add a Bend Here"), this, [this, id, position]() {
      m_document->add_waypoint(id, position);
    });
    if (!route->waypoints.isEmpty()) {
      menu.addAction(QStringLiteral("Straighten"), this, [this, id]() {
        m_document->clear_waypoints(id);
      });
    }
    auto* choke = menu.addAction(QStringLiteral("Chokepoint"));
    choke->setCheckable(true);
    choke->setChecked(route->chokepoint);
    connect(choke, &QAction::toggled, this, [this, id](bool on) {
      S::Route copy = *m_document->definition().route(id);
      copy.chokepoint = on;
      m_document->update_route(copy);
    });
    if (m_document->at_start()) {
      auto* status = menu.addMenu(QStringLiteral("Opening Status"));
      for (const auto value :
           {S::RouteStatus::Open, S::RouteStatus::Contested, S::RouteStatus::Blocked}) {
        auto* action = status->addAction(
            title_case(S::to_string(value)), this, [this, id, value]() {
              m_document->set_initial_route_status(id, value);
            });
        action->setCheckable(true);
        action->setChecked(S::route_status(def.initial_state, id) == value);
      }
    }
    menu.addSeparator();
    menu.addAction(
        QStringLiteral("Delete Route"), this, [this, id]() { delete_entity(id); });
  } else if (army != nullptr) {
    menu.addSection(army->name);
    menu.addAction(
        QStringLiteral("Show Supply"), this, [this, id]() { show_supply_for(id); });
    if (!army->objective.isEmpty()) {
      menu.addAction(QStringLiteral("Show March to Objective"),
                     this,
                     [this, from = army->location, to = army->objective]() {
                       show_path(from, to);
                     });
    }
    menu.addAction(QStringLiteral("Reach in 7 Days"),
                   this,
                   [this, from = army->location]() { run_reach_query(from, 7.0); });
  } else {
    if (region != nullptr) {
      menu.addSection(region->name);
      if (m_document->at_start()) {
        auto* owner = menu.addMenu(QStringLiteral("Held at the Start By"));
        owner->addAction(QStringLiteral("Neutral"), this, [this, id]() {
          m_document->set_initial_controller(id,
                                             QString::fromLatin1(S::k_neutral_faction));
        });
        for (const auto& faction : def.factions) {
          owner->addAction(faction.name, this, [this, id, f = faction.id]() {
            m_document->set_initial_controller(id, f);
          });
        }
      }
    }
    auto* add = menu.addMenu(QStringLiteral("Add Site Here"));
    for (const auto& kind : S::all_site_kind_names()) {
      add->addAction(title_case(kind), this, [this, kind, position]() {
        m_document->add_site(position, *S::site_kind_from_string(kind));
      });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("Fit Map"), m_canvas, &WorldCanvas::fit_all);
  }
  menu.exec(global);
}

} // namespace MapEditor
