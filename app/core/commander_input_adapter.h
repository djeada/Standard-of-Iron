#pragma once

#include <QObject>

class GameEngine;

class CommanderInputAdapter final : public QObject {
  Q_OBJECT

public:
  explicit CommanderInputAdapter(GameEngine* engine, QObject* parent = nullptr);

  Q_INVOKABLE void key_down(int key, int modifiers = 0);
  Q_INVOKABLE void key_up(int key, int modifiers = 0);
  Q_INVOKABLE void primary_action_down();
  Q_INVOKABLE void primary_action_up();
  Q_INVOKABLE void secondary_action_down();
  Q_INVOKABLE void secondary_action_up();
  Q_INVOKABLE void trigger_rally();
  Q_INVOKABLE void dodge();
  Q_INVOKABLE void jump();
  Q_INVOKABLE void cycle_lock_on();
  Q_INVOKABLE void special_action();
  Q_INVOKABLE void center_mouse(qreal center_sx, qreal center_sy);
  Q_INVOKABLE void toggle_mode();

private:
  GameEngine* m_engine = nullptr;
};
