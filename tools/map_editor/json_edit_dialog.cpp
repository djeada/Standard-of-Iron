#include "json_edit_dialog.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include "hill_projection_widget.h"
#include "hill_projection_model.h"
#include "map_json_keys.h"

namespace MapEditor {

JsonEditDialog::JsonEditDialog(const QString& title,
                               const QJsonObject& json,
                               bool enable_hill_projection,
                               QWidget* parent)
    : QDialog(parent)
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

  QJsonDocument doc(json);
  m_editor->setPlainText(doc.toJson(QJsonDocument::Indented));
  m_model_json = json;
  m_result = json;

  if (m_enable_hill_projection) {
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* json_panel = new QWidget(splitter);
    json_panel->setMinimumWidth(220);
    json_panel->setMaximumWidth(420);
    auto* json_layout = new QVBoxLayout(json_panel);
    json_layout->setContentsMargins(0, 0, 0, 0);
    json_layout->addWidget(m_editor);

    auto* projection_panel = new QFrame(splitter);
    auto* projection_layout = new QVBoxLayout(projection_panel);
    projection_layout->setContentsMargins(8, 8, 8, 8);
    projection_layout->setSpacing(8);

    auto* projection_title = new QLabel("Hill top projection", projection_panel);
    projection_title->setObjectName("panelTitle");
    projection_layout->addWidget(projection_title);

    auto* marker_row = new QWidget(projection_panel);
    auto* marker_layout = new QHBoxLayout(marker_row);
    marker_layout->setContentsMargins(0, 0, 0, 0);
    marker_layout->setSpacing(6);
    marker_layout->addWidget(new QLabel("Marker:", marker_row));
    m_hill_marker_button = new QPushButton("Hill", marker_row);
    m_hill_marker_button->setCheckable(true);
    m_entrance_marker_button = new QPushButton("Entrance", marker_row);
    m_entrance_marker_button->setCheckable(true);
    m_nothing_marker_button = new QPushButton("Nothing", marker_row);
    m_nothing_marker_button->setCheckable(true);
    m_entrance_marker_button->setChecked(true);
    marker_layout->addWidget(m_hill_marker_button);
    marker_layout->addWidget(m_entrance_marker_button);
    marker_layout->addWidget(m_nothing_marker_button);
    marker_layout->addStretch(1);
    projection_layout->addWidget(marker_row);

    auto* marker_group = new QButtonGroup(projection_panel);
    marker_group->setExclusive(true);
    marker_group->addButton(m_hill_marker_button);
    marker_group->addButton(m_entrance_marker_button);
    marker_group->addButton(m_nothing_marker_button);
    connect(m_hill_marker_button, &QPushButton::clicked, this, [this]() {
      if (m_hill_projection != nullptr) {
        m_hill_projection->set_edit_layer(HillProjectionWidget::EditLayer::Hill);
      }
    });
    connect(m_entrance_marker_button, &QPushButton::clicked, this, [this]() {
      if (m_hill_projection != nullptr) {
        m_hill_projection->set_edit_layer(HillProjectionWidget::EditLayer::Entrance);
      }
    });
    connect(m_nothing_marker_button, &QPushButton::clicked, this, [this]() {
      if (m_hill_projection != nullptr) {
        m_hill_projection->set_edit_layer(HillProjectionWidget::EditLayer::Nothing);
      }
    });

    m_projection_hint_label = new QLabel(projection_panel);
    m_projection_hint_label->setWordWrap(true);
    projection_layout->addWidget(m_projection_hint_label);

    m_hill_projection = new HillProjectionWidget(projection_panel);
    m_hill_projection->set_edit_layer(HillProjectionWidget::EditLayer::Entrance);
    projection_layout->addWidget(m_hill_projection, 1);

    splitter->addWidget(json_panel);
    splitter->addWidget(projection_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 6);
    splitter->setSizes({280, 1400});
    layout->addWidget(splitter, 1);
  } else {
    layout->addWidget(m_editor, 1);
  }

  connect(m_editor, &QPlainTextEdit::textChanged, this, &JsonEditDialog::validate_json);
  if (m_hill_projection != nullptr) {
    connect(m_hill_projection,
            &HillProjectionWidget::projection_changed,
            this,
            &JsonEditDialog::on_projection_entrances_changed);
  }

  auto* button_layout = new QHBoxLayout();
  auto* cancel_button = new QPushButton("Cancel", this);
  m_ok_button = new QPushButton("OK", this);
  m_ok_button->setDefault(true);
  m_ok_button->setProperty("primary", true);

  button_layout->addStretch();
  button_layout->addWidget(cancel_button);
  button_layout->addWidget(m_ok_button);
  layout->addLayout(button_layout);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_ok_button, &QPushButton::clicked, this, &JsonEditDialog::on_accepted);

  validate_json();
  setWindowState(windowState() | Qt::WindowFullScreen);
}

void JsonEditDialog::validate_json() {
  if (m_syncing_editor) {
    return;
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  m_is_valid = (error.error == QJsonParseError::NoError && doc.isObject());
  m_ok_button->setEnabled(m_is_valid);

  if (!m_is_valid) {
    m_editor->setStyleSheet("QPlainTextEdit { border: 2px solid red; }");
  } else {
    m_editor->setStyleSheet("");
    m_model_json = doc.object();
    m_result = m_model_json;
  }

  update_projection_state();
}

void JsonEditDialog::sync_editor_from_model() {
  if (m_editor == nullptr) {
    return;
  }

  const QString json_text =
      QJsonDocument(m_model_json).toJson(QJsonDocument::Indented);
  if (m_editor->toPlainText() == json_text) {
    return;
  }

  m_syncing_editor = true;
  {
    QSignalBlocker blocker(m_editor);
    m_editor->setPlainText(json_text);
  }
  m_syncing_editor = false;
  validate_json();
}

void JsonEditDialog::update_projection_state() {
  if (m_hill_projection == nullptr) {
    return;
  }

  if (!m_is_valid) {
    m_projection_hint_label->setText(
        "Projection disabled until JSON is valid.\n"
        "Fix syntax errors in JSON to continue editing entrances.");
    m_hill_projection->setEnabled(false);
    if (m_hill_marker_button != nullptr) {
      m_hill_marker_button->setEnabled(false);
    }
    if (m_entrance_marker_button != nullptr) {
      m_entrance_marker_button->setEnabled(false);
    }
    if (m_nothing_marker_button != nullptr) {
      m_nothing_marker_button->setEnabled(false);
    }
    return;
  }

  const QString terrain_type =
      m_model_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  if (terrain_type != QStringLiteral("hill")) {
    m_projection_hint_label->setText(
        "Projection is only active for terrain with \"type\": \"hill\".");
    m_hill_projection->setEnabled(false);
    if (m_hill_marker_button != nullptr) {
      m_hill_marker_button->setEnabled(false);
    }
    if (m_entrance_marker_button != nullptr) {
      m_entrance_marker_button->setEnabled(false);
    }
    if (m_nothing_marker_button != nullptr) {
      m_nothing_marker_button->setEnabled(false);
    }
    return;
  }

  m_projection_hint_label->setText(
      "Grid max: 80 x 80 cells\n"
      "Select marker: Hill, Entrance, or Nothing\n"
      "Left drag: paint selected marker\n"
      "Right drag: erase selected marker\n"
      "Adjacent entrance cells are saved as one JSON entry");
  m_hill_projection->setEnabled(true);
  if (m_hill_marker_button != nullptr) {
    m_hill_marker_button->setEnabled(true);
  }
  if (m_entrance_marker_button != nullptr) {
    m_entrance_marker_button->setEnabled(true);
  }
  if (m_nothing_marker_button != nullptr) {
    m_nothing_marker_button->setEnabled(true);
  }

  if (m_syncing_projection) {
    return;
  }
  m_syncing_projection = true;
  m_hill_projection->set_hill_json(m_model_json);
  m_syncing_projection = false;
}

void JsonEditDialog::apply_projection_to_model_json() {
  if (m_hill_projection == nullptr) {
    return;
  }
  const HillProjection::Model model = HillProjection::build_model(m_model_json);
  m_model_json = HillProjection::apply_projection_to_hill_json(
      m_model_json, model, m_hill_projection->hill_cells(), m_hill_projection->entrance_cells());
  m_result = m_model_json;
}

void JsonEditDialog::on_projection_entrances_changed() {
  if (!m_is_valid || m_syncing_projection) {
    return;
  }

  m_syncing_projection = true;
  apply_projection_to_model_json();
  sync_editor_from_model();
  m_syncing_projection = false;
}

void JsonEditDialog::on_accepted() {
  if (m_is_valid) {
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
