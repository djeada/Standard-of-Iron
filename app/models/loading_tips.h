#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <random>
#include <vector>

class LoadingTips : public QObject {
  Q_OBJECT

  Q_PROPERTY(int count READ count NOTIFY tips_changed)

public:
  explicit LoadingTips(QObject* parent = nullptr);

  static auto instance() -> LoadingTips*;
  static auto create(QQmlEngine* engine, QJSEngine* script_engine) -> LoadingTips*;

  [[nodiscard]] int count();

  Q_INVOKABLE [[nodiscard]] QString next();

  Q_INVOKABLE void reseed(quint32 seed);

  [[nodiscard]] QStringList source_texts();

  void load_from_json(const QByteArray& payload);

signals:
  void tips_changed();

private:
  void ensure_loaded();
  void refill_deck();

  std::vector<QString> m_sources;
  std::vector<int> m_deck;
  std::mt19937 m_rng;
  int m_last_drawn = -1;
  bool m_loaded = false;
};
