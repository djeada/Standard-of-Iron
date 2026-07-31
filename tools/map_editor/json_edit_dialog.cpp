#include "json_edit_dialog.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

#include "hill_projection_model.h"
#include "hill_projection_widget.h"
#include "map_json_keys.h"
#include "mountain_projection_widget.h"
#include "terrain_projection_widget.h"

namespace MapEditor {

JsonEditDialog::JsonEditDialog(const QString& title,
                               const QJsonObject& json,
                               bool enable_hill_projection,
                               JsonSchema schema,
                               QWidget* parent,
                               HillProjection::MapContext map_context)
    : QDialog(parent)
    , m_schema(std::move(schema))
    , m_map_context(map_context)
    , m_enable_hill_projection(enable_hill_projection) {
  setup_ui(title, json);
}

void JsonEditDialog::setup_ui(const QString& title, const QJsonObject& json) {
  setWindowTitle(title);
  resize(1280, 720);

  auto* layout = new QVBoxLayout(this);

  auto* label =
      new QLabel("Edit JSON properties (changes will be saved to map):", this);
  label->setWordWrap(true);
  layout->addWidget(label);

  m_editor = new QPlainTextEdit(this);
  m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_editor->setPlaceholderText("{\n  \"type\": \"...\"\n}");
  m_editor->setTabStopDistance(
      4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));

  QJsonDocument const doc(json);
  m_editor->setPlainText(doc.toJson(QJsonDocument::Indented));
  m_opening_json = json;
  m_model_json = json;
  m_result = json;

  if (m_enable_hill_projection) {
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* json_panel = new QSplitter(Qt::Vertical, splitter);
    json_panel->setMinimumWidth(320);
    json_panel->setMaximumWidth(560);
    json_panel->addWidget(m_editor);
    json_panel->addWidget(build_schema_panel(json_panel));
    json_panel->setStretchFactor(0, 3);
    json_panel->setStretchFactor(1, 4);

    auto* projection_panel = new QFrame(splitter);
    auto* projection_layout = new QVBoxLayout(projection_panel);
    projection_layout->setContentsMargins(8, 8, 8, 8);
    projection_layout->setSpacing(8);

    auto* projection_title = new QLabel("Terrain projection", projection_panel);
    projection_title->setObjectName("panelTitle");
    projection_layout->addWidget(projection_title);

    const QString terrain_type =
        json.value(MapJsonKeys::type).toString().trimmed().toLower();
    if (terrain_type == QStringLiteral("mountain")) {
      m_projection = new MountainProjectionWidget(projection_panel);
    } else {
      m_projection = new HillProjectionWidget(projection_panel);
    }
    m_projection->set_map_context(m_map_context);

    auto* marker_row = new QWidget(projection_panel);
    auto* marker_layout = new QHBoxLayout(marker_row);
    marker_layout->setContentsMargins(0, 0, 0, 0);
    marker_layout->setSpacing(6);
    marker_layout->addWidget(new QLabel("Marker:", marker_row));

    auto* marker_group = new QButtonGroup(projection_panel);
    marker_group->setExclusive(true);

    const auto defs = m_projection->layer_definitions();
    m_marker_buttons.reserve(defs.size());
    for (int i = 0; i < defs.size(); ++i) {
      auto* btn = new QPushButton(defs[i].first, marker_row);
      btn->setCheckable(true);
      marker_layout->addWidget(btn);
      marker_group->addButton(btn);
      m_marker_buttons.append(btn);
      connect(btn, &QPushButton::clicked, this, [this, i]() {
        if (m_projection != nullptr) {
          m_projection->set_active_layer(i);
        }
      });
    }

    marker_row->setVisible(!defs.isEmpty());

    const int entrance_idx = m_projection->entrance_layer_index();
    if (entrance_idx >= 0 && entrance_idx < m_marker_buttons.size()) {
      m_marker_buttons[entrance_idx]->setChecked(true);
    } else if (!m_marker_buttons.isEmpty()) {
      m_marker_buttons[0]->setChecked(true);
    }

    marker_layout->addStretch(1);

    marker_layout->addWidget(new QLabel("Brush:", marker_row));
    auto* brush_spin = new QSpinBox(marker_row);
    brush_spin->setRange(1, 12);
    brush_spin->setValue(m_projection->brush_size());
    brush_spin->setSuffix(" cells");
    brush_spin->setToolTip(
        "Width of the paint stroke in cells; drag across the grid to paint a whole "
        "run at once");
    marker_layout->addWidget(brush_spin);
    connect(brush_spin, &QSpinBox::valueChanged, this, [this](int value) {
      if (m_projection != nullptr) {
        m_projection->set_brush_size(value);
      }
    });

    if (entrance_idx >= 0) {
      m_clear_entrances_button = new QPushButton("Clear Entrances", marker_row);
      m_clear_entrances_button->setToolTip(
          "Drop every entrance from this hill; paint new ones on the outlined edge "
          "cells");
      marker_layout->addWidget(m_clear_entrances_button);
      connect(m_clear_entrances_button,
              &QPushButton::clicked,
              this,
              &JsonEditDialog::clear_entrances);
    }

    projection_layout->addWidget(marker_row);

    m_projection_hint_label = new QLabel(projection_panel);
    m_projection_hint_label->setWordWrap(true);
    projection_layout->addWidget(m_projection_hint_label);

    projection_layout->addWidget(m_projection, 1);

    splitter->addWidget(json_panel);
    splitter->addWidget(projection_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 6);
    splitter->setSizes({420, 1400});
    layout->addWidget(splitter, 1);
  } else {
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_editor);
    splitter->addWidget(build_schema_panel(splitter));
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({720, 520});
    layout->addWidget(splitter, 1);
  }

  connect(m_editor, &QPlainTextEdit::textChanged, this, &JsonEditDialog::validate_json);
  if (m_projection != nullptr) {
    connect(m_projection,
            &TerrainProjectionWidget::projection_changed,
            this,
            &JsonEditDialog::on_projection_entrances_changed);
    connect(m_projection,
            &TerrainProjectionWidget::entrance_rejected,
            this,
            [this](const QString& reason) {
              m_projection_warning = reason;
              refresh_projection_hint();
            });
  }

  auto* button_layout = new QHBoxLayout();
  m_restore_button = new QPushButton("Restore Opening Version", this);
  m_restore_button->setToolTip(
      "Put back the JSON exactly as it was when this editor opened, discarding every "
      "change made here");
  auto* cancel_button = new QPushButton("Cancel", this);
  m_ok_button = new QPushButton("OK", this);
  m_ok_button->setDefault(true);
  m_ok_button->setProperty("primary", true);

  button_layout->addWidget(m_restore_button);
  button_layout->addStretch();
  button_layout->addWidget(cancel_button);
  button_layout->addWidget(m_ok_button);
  layout->addLayout(button_layout);

  connect(m_restore_button,
          &QPushButton::clicked,
          this,
          &JsonEditDialog::restore_opening_json);
  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_ok_button, &QPushButton::clicked, this, &JsonEditDialog::on_accepted);

  validate_json();
  show_field_details(QString());
  setWindowState(windowState() | Qt::WindowFullScreen);
}

QWidget* JsonEditDialog::build_schema_panel(QWidget* parent) {
  auto* panel = new QFrame(parent);
  auto* panel_layout = new QVBoxLayout(panel);
  panel_layout->setContentsMargins(8, 8, 8, 8);
  panel_layout->setSpacing(6);

  auto* title = new QLabel(m_schema.title.isEmpty()
                               ? QStringLiteral("Valid keys")
                               : QStringLiteral("Valid keys — %1").arg(m_schema.title),
                           panel);
  title->setObjectName("panelTitle");
  panel_layout->addWidget(title);

  if (!m_schema.summary.isEmpty()) {
    auto* summary = new QLabel(m_schema.summary, panel);
    summary->setWordWrap(true);
    summary->setProperty("status", "muted");
    panel_layout->addWidget(summary);
  }

  m_schema_status_label = new QLabel(panel);
  m_schema_status_label->setWordWrap(true);
  panel_layout->addWidget(m_schema_status_label);

  m_schema_tree = new QTreeWidget(panel);
  m_schema_tree->setColumnCount(3);
  m_schema_tree->setHeaderLabels({"Key", "Type", "Default"});
  m_schema_tree->setRootIsDecorated(false);
  m_schema_tree->setUniformRowHeights(true);
  m_schema_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_schema_tree->setAlternatingRowColors(true);

  if (m_schema.is_empty()) {
    auto* empty = new QTreeWidgetItem(m_schema_tree);
    empty->setText(0, QStringLiteral("No schema available"));
    empty->setFirstColumnSpanned(true);
    empty->setFlags(Qt::NoItemFlags);
  }

  for (const JsonFieldSpec& field : m_schema.fields) {
    auto* item = new QTreeWidgetItem(m_schema_tree);
    item->setText(0, field.key);
    item->setText(1, field.type);
    item->setText(2, field.required ? QStringLiteral("required") : field.default_value);
    item->setData(0, Qt::UserRole, field.key);

    QString tip = field.description;
    if (!field.allowed.isEmpty()) {
      tip += QStringLiteral("\nOne of: %1").arg(field.allowed.join(", "));
    }
    for (int column = 0; column < 3; ++column) {
      item->setToolTip(column, tip);
    }

    if (field.required) {
      QFont bold = item->font(0);
      bold.setBold(true);
      item->setFont(0, bold);
    }
  }

  m_schema_tree->resizeColumnToContents(0);
  m_schema_tree->resizeColumnToContents(1);
  panel_layout->addWidget(m_schema_tree, 1);

  connect(m_schema_tree,
          &QTreeWidget::currentItemChanged,
          this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
            show_field_details(current != nullptr
                                   ? current->data(0, Qt::UserRole).toString()
                                   : QString());
          });
  connect(m_schema_tree,
          &QTreeWidget::itemDoubleClicked,
          this,
          [this](QTreeWidgetItem* item, int) {
            if (item != nullptr) {
              insert_key(item->data(0, Qt::UserRole).toString());
            }
          });

  m_schema_detail_label = new QLabel(panel);
  m_schema_detail_label->setWordWrap(true);
  m_schema_detail_label->setMinimumHeight(56);
  m_schema_detail_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_schema_detail_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  panel_layout->addWidget(m_schema_detail_label);

  auto* button_row = new QHBoxLayout();
  m_insert_key_button = new QPushButton("Insert Key", panel);
  m_insert_key_button->setToolTip(
      "Add the selected key with its default value (double-click a row does the same)");
  connect(m_insert_key_button, &QPushButton::clicked, this, [this]() {
    QTreeWidgetItem* item = m_schema_tree->currentItem();
    if (item != nullptr) {
      insert_key(item->data(0, Qt::UserRole).toString());
    }
  });
  button_row->addWidget(m_insert_key_button);

  m_insert_missing_button = new QPushButton("Add Missing Required", panel);
  m_insert_missing_button->setToolTip("Add every required key that is not present yet");
  connect(m_insert_missing_button,
          &QPushButton::clicked,
          this,
          &JsonEditDialog::insert_missing_required_keys);
  button_row->addWidget(m_insert_missing_button);
  button_row->addStretch(1);
  panel_layout->addLayout(button_row);

  return panel;
}

void JsonEditDialog::show_field_details(const QString& key) {
  if (m_schema_detail_label == nullptr) {
    return;
  }

  const JsonFieldSpec* field = m_schema.find(key);
  if (field == nullptr) {
    m_schema_detail_label->setText(
        m_schema.is_empty()
            ? QStringLiteral("This object has no documented schema; any key is kept "
                             "as-is.")
            : QStringLiteral("Select a key to see what it does."));
    return;
  }

  QString text = QStringLiteral("<b>%1</b> — %2").arg(field->key, field->description);
  if (!field->allowed.isEmpty()) {
    text += QStringLiteral("<br/>One of: <code>%1</code>")
                .arg(field->allowed.join(QStringLiteral(", ")));
  }
  if (!field->required && !field->default_value.isEmpty()) {
    text += QStringLiteral("<br/>Omitted → <code>%1</code>").arg(field->default_value);
  }
  m_schema_detail_label->setText(text);
}

void JsonEditDialog::insert_key(const QString& key) {
  const JsonFieldSpec* field = m_schema.find(key);
  if (field == nullptr || !m_is_valid) {
    return;
  }
  if (m_model_json.contains(key)) {
    show_field_details(key);
    return;
  }

  m_model_json[key] = field->placeholder;
  sync_editor_from_model();
}

void JsonEditDialog::insert_missing_required_keys() {
  if (!m_is_valid) {
    return;
  }

  bool changed = false;
  for (const JsonFieldSpec& field : m_schema.fields) {
    if (field.required && !m_model_json.contains(field.key)) {
      m_model_json[field.key] = field.placeholder;
      changed = true;
    }
  }

  if (changed) {
    sync_editor_from_model();
  }
}

void JsonEditDialog::update_schema_state() {
  if (m_schema_status_label == nullptr) {
    return;
  }

  const bool has_schema = !m_schema.is_empty();
  if (m_insert_key_button != nullptr) {
    m_insert_key_button->setEnabled(m_is_valid && has_schema);
  }
  if (m_insert_missing_button != nullptr) {
    m_insert_missing_button->setEnabled(m_is_valid && has_schema);
  }

  if (!m_is_valid) {
    m_schema_status_label->setText(
        QStringLiteral("<span style='color:#e06060;'>Invalid JSON — fix the syntax "
                       "before the keys can be checked.</span>"));
    return;
  }

  if (!has_schema) {
    m_schema_status_label->clear();
    return;
  }

  QStringList missing;
  bool has_required = false;
  for (const JsonFieldSpec& field : m_schema.fields) {
    has_required = has_required || field.required;
    if (field.required && !m_model_json.contains(field.key)) {
      missing << field.key;
    }
  }

  QStringList unknown;
  for (const QString& key : m_model_json.keys()) {
    if (m_schema.find(key) == nullptr) {
      unknown << key;
    }
  }

  QStringList lines;
  if (!missing.isEmpty()) {
    lines << QStringLiteral("<span style='color:#e0a860;'>Missing required: %1</span>")
                 .arg(missing.join(QStringLiteral(", ")));
  }
  if (!unknown.isEmpty()) {

    lines << QStringLiteral(
                 "<span style='color:#9aa0a6;'>Not read by the loader (kept as-is): "
                 "%1</span>")
                 .arg(unknown.join(QStringLiteral(", ")));
  }
  if (lines.isEmpty()) {
    lines
        << (has_required
                ? QStringLiteral(
                      "<span style='color:#7ec27e;'>All required keys present.</span>")
                : QStringLiteral("<span style='color:#7ec27e;'>Every key here is "
                                 "optional; each one overrides a default.</span>"));
  }

  m_schema_status_label->setText(lines.join(QStringLiteral("<br/>")));
}

void JsonEditDialog::validate_json() {
  if (m_syncing_editor) {
    return;
  }

  QJsonParseError error;
  QJsonDocument const doc =
      QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  m_is_valid = (error.error == QJsonParseError::NoError && doc.isObject());
  m_ok_button->setEnabled(m_is_valid);

  if (!m_is_valid) {
    m_editor->setStyleSheet("QPlainTextEdit { border: 2px solid red; }");
  } else {
    m_editor->setStyleSheet("");
    m_model_json = doc.object();
    m_result = m_model_json;
  }

  if (m_is_valid && model_has_entrances()) {
    m_entrances_cleared = false;
  }

  update_schema_state();
  update_projection_state();
  update_restore_state();
  update_entrance_button_state();
}

void JsonEditDialog::update_restore_state() {
  if (m_restore_button == nullptr) {
    return;
  }
  m_restore_button->setEnabled(!m_is_valid || m_model_json != m_opening_json);
}

bool JsonEditDialog::model_has_entrances() const {
  return !m_model_json.value(MapJsonKeys::entrances).toArray().isEmpty();
}

void JsonEditDialog::update_entrance_button_state() {
  if (m_clear_entrances_button == nullptr) {
    return;
  }
  const bool is_hill =
      m_is_valid &&
      m_model_json.value(MapJsonKeys::type).toString().trimmed().toLower() ==
          QStringLiteral("hill");
  m_clear_entrances_button->setEnabled(is_hill && model_has_entrances());
}

void JsonEditDialog::clear_entrances() {
  if (!m_is_valid || !model_has_entrances()) {
    return;
  }

  m_entrances_cleared = true;
  m_projection_warning.clear();
  m_model_json.remove(MapJsonKeys::entrances);
  m_result = m_model_json;
  sync_editor_from_model();
}

void JsonEditDialog::restore_opening_json() {
  m_projection_warning.clear();
  m_entrances_cleared = false;
  m_model_json = m_opening_json;
  m_result = m_opening_json;

  m_syncing_editor = true;
  {
    QSignalBlocker const blocker(m_editor);
    m_editor->setPlainText(
        QJsonDocument(m_opening_json).toJson(QJsonDocument::Indented));
  }
  m_syncing_editor = false;

  validate_json();
}

void JsonEditDialog::sync_editor_from_model() {
  if (m_editor == nullptr) {
    return;
  }

  const QString json_text = QJsonDocument(m_model_json).toJson(QJsonDocument::Indented);
  if (m_editor->toPlainText() == json_text) {
    return;
  }

  m_syncing_editor = true;
  {
    QSignalBlocker const blocker(m_editor);
    m_editor->setPlainText(json_text);
  }
  m_syncing_editor = false;
  validate_json();
}

void JsonEditDialog::update_projection_state() {
  if (m_projection == nullptr) {
    return;
  }

  const auto set_buttons_enabled = [this](bool enabled) {
    for (QPushButton* btn : m_marker_buttons) {
      if (btn != nullptr) {
        btn->setEnabled(enabled);
      }
    }
  };

  if (!m_is_valid) {
    m_projection_hint_label->setTextFormat(Qt::PlainText);
    m_projection_hint_label->setText(
        "Projection disabled until JSON is valid.\n"
        "Fix syntax errors in JSON to continue editing the terrain footprint.");
    m_projection->setEnabled(false);
    set_buttons_enabled(false);
    return;
  }

  const QString terrain_type =
      m_model_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  const bool is_mountain = terrain_type == QStringLiteral("mountain");
  if (terrain_type != QStringLiteral("hill") && !is_mountain) {
    m_projection_hint_label->setTextFormat(Qt::PlainText);
    m_projection_hint_label->setText(
        R"(Projection is only active for terrain with type "hill" or "mountain".)");
    m_projection->setEnabled(false);
    set_buttons_enabled(false);
    return;
  }

  m_projection->setEnabled(true);
  set_buttons_enabled(true);

  if (!m_syncing_projection) {
    m_syncing_projection = true;
    m_projection_warning.clear();
    m_projection->set_terrain_json(m_model_json);
    m_syncing_projection = false;
  }

  refresh_projection_hint();
}

void JsonEditDialog::refresh_projection_hint() {
  if (m_projection == nullptr || m_projection_hint_label == nullptr ||
      !m_projection->is_active()) {
    return;
  }

  const HillProjection::Model& model = m_projection->get_model();
  QStringList lines;
  lines << QStringLiteral("Grid: %1 x %2 cells (auto-fitted to the JSON footprint)")
               .arg(model.grid_width)
               .arg(model.grid_height);
  lines << QStringLiteral(
               "Left drag: paint selected marker (brush %1 cells wide, JSON updates "
               "when the stroke ends)")
               .arg(m_projection->brush_size());
  lines << QStringLiteral("Right drag: erase selected marker");
  if (model.is_mountain) {
    lines << QStringLiteral("Mountains stay entrance-free; rotation comes from JSON.");
  } else {
    lines << QStringLiteral(
                 "Entrances: %1-%2 ramps. Each connected blob is stored as one "
                 "{x, z, radius} entry centred on the hill edge, so painted cells "
                 "snap to the disc that entry describes.")
                 .arg(HillProjection::k_min_entrances)
                 .arg(HillProjection::k_max_entrances);
  }

  if (m_entrances_cleared) {
    lines << QStringLiteral(
        "<span style='color:#e0a860;'>Entrances cleared. The runtime still carves one "
        "default ramp on the hill's western face (rotated with the hill) until you "
        "paint new ones.</span>");
  } else {
    const QStringList issues = m_projection->issues();
    if (!issues.isEmpty()) {
      lines << QStringLiteral("<span style='color:#e0a860;'>%1</span>")
                   .arg(issues.join(QStringLiteral(" ")));
    }
  }
  if (!m_projection_warning.isEmpty()) {
    lines << QStringLiteral("<span style='color:#e06060;'>%1</span>")
                 .arg(m_projection_warning);
  }

  m_projection_hint_label->setTextFormat(Qt::RichText);
  m_projection_hint_label->setText(lines.join(QStringLiteral("<br/>")));
}

void JsonEditDialog::apply_projection_to_model_json() {
  if (m_projection == nullptr) {
    return;
  }

  const HillProjection::Model& model = m_projection->get_model();
  m_model_json = HillProjection::apply_projection_to_hill_json(
      m_model_json, model, m_projection->body_cells(), m_projection->entrance_cells());
  if (m_entrances_cleared) {
    m_model_json.remove(MapJsonKeys::entrances);
  }
  m_result = m_model_json;

  if (!model.is_mountain) {
    m_projection->set_entrance_cells(HillProjection::entrance_cells_from_json(
        model, m_model_json.value(MapJsonKeys::entrances).toArray()));
  }
}

void JsonEditDialog::on_projection_entrances_changed() {
  if (!m_is_valid || m_syncing_projection) {
    return;
  }

  if (!m_projection->entrance_cells().isEmpty()) {
    m_entrances_cleared = false;
  }

  m_syncing_projection = true;
  apply_projection_to_model_json();
  sync_editor_from_model();
  m_syncing_projection = false;
}

void JsonEditDialog::normalize_projection_entrances() {
  if (m_projection == nullptr || !m_projection->is_active() || m_entrances_cleared ||
      m_model_json == m_opening_json) {
    return;
  }

  const HillProjection::Model& model = m_projection->get_model();
  if (model.is_mountain) {
    return;
  }

  const QVector<QPoint> body = m_projection->body_cells();
  const QVector<QPoint> entrances = m_projection->entrance_cells();
  if (HillProjection::entrance_issues(model, body, entrances).isEmpty()) {
    return;
  }

  const QJsonArray normalized = HillProjection::entrances_from_cells(
      model, HillProjection::normalize_entrance_cells(model, body, entrances));
  if (normalized.isEmpty()) {
    m_model_json.remove(MapJsonKeys::entrances);
  } else {
    m_model_json[MapJsonKeys::entrances] = normalized;
  }
  m_result = m_model_json;
}

void JsonEditDialog::on_accepted() {
  if (m_is_valid) {
    normalize_projection_entrances();
    m_result = m_model_json;
    accept();
  } else {
    QMessageBox::warning(
        this, "Invalid JSON", "The JSON is not valid. Fix errors before saving.");
  }
}

QJsonObject JsonEditDialog::get_json() const {
  return m_result;
}

} // namespace MapEditor
