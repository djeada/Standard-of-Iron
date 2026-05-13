#include "commander_input_adapter.h"

#include "game_engine.h"

CommanderInputAdapter::CommanderInputAdapter(GameEngine *engine,
                                             QObject *parent)
    : QObject(parent), m_engine(engine) {}

void CommanderInputAdapter::key_down(int key, int modifiers) {
  if (m_engine != nullptr) {
    m_engine->commander_key_down(key, modifiers);
  }
}

void CommanderInputAdapter::key_up(int key, int modifiers) {
  if (m_engine != nullptr) {
    m_engine->commander_key_up(key, modifiers);
  }
}

void CommanderInputAdapter::primary_action_down() {
  if (m_engine != nullptr) {
    m_engine->commander_primary_action_down();
  }
}

void CommanderInputAdapter::primary_action_up() {
  if (m_engine != nullptr) {
    m_engine->commander_primary_action_up();
  }
}

void CommanderInputAdapter::secondary_action_down() {
  if (m_engine != nullptr) {
    m_engine->commander_secondary_action_down();
  }
}

void CommanderInputAdapter::secondary_action_up() {
  if (m_engine != nullptr) {
    m_engine->commander_secondary_action_up();
  }
}

void CommanderInputAdapter::trigger_rally() {
  if (m_engine != nullptr) {
    m_engine->commander_trigger_rally();
  }
}

void CommanderInputAdapter::dodge() {
  if (m_engine != nullptr) {
    m_engine->commander_dodge();
  }
}

void CommanderInputAdapter::cycle_lock_on() {
  if (m_engine != nullptr) {
    m_engine->commander_cycle_lock_on();
  }
}

void CommanderInputAdapter::special_action() {
  if (m_engine != nullptr) {
    m_engine->commander_special_action();
  }
}

void CommanderInputAdapter::center_mouse(qreal center_sx, qreal center_sy) {
  if (m_engine != nullptr) {
    m_engine->commander_center_mouse(center_sx, center_sy);
  }
}

void CommanderInputAdapter::toggle_mode() {
  if (m_engine != nullptr) {
    m_engine->toggle_commander_control_mode();
  }
}
