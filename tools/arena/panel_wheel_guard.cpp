#include "panel_wheel_guard.h"

#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QObject>
#include <QWidget>

namespace Arena::Panels {

namespace {

class WheelGuard : public QObject {
public:
  using QObject::QObject;

  auto eventFilter(QObject* watched, QEvent* event) -> bool override {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (event->type() != QEvent::Wheel || widget == nullptr || widget->hasFocus()) {
      return QObject::eventFilter(watched, event);
    }

    event->ignore();
    return true;
  }
};

void guard_one(QObject* guard, QWidget* widget) {
  widget->setFocusPolicy(Qt::StrongFocus);
  widget->installEventFilter(guard);
}

} // namespace

void guard_wheel_edits(QWidget* panel) {
  if (panel == nullptr) {
    return;
  }

  auto* guard = new WheelGuard(panel);
  for (auto* slider : panel->findChildren<QAbstractSlider*>()) {
    guard_one(guard, slider);
  }
  for (auto* spin_box : panel->findChildren<QAbstractSpinBox*>()) {
    guard_one(guard, spin_box);
  }
  for (auto* combo_box : panel->findChildren<QComboBox*>()) {
    guard_one(guard, combo_box);
  }
}

} // namespace Arena::Panels
