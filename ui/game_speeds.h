#ifndef SOI_UI_GAME_SPEEDS_H
#define SOI_UI_GAME_SPEEDS_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

class GameSpeeds : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QVariantList options READ options CONSTANT)
  Q_PROPERTY(qreal minimum READ minimum CONSTANT)
  Q_PROPERTY(qreal maximum READ maximum CONSTANT)

public:
  explicit GameSpeeds(QObject* parent = nullptr)
      : QObject(parent) {}

  static GameSpeeds* instance();
  static GameSpeeds* create(QQmlEngine* engine, QJSEngine* script_engine);

  [[nodiscard]] static QVariantList options();
  [[nodiscard]] static qreal minimum();
  [[nodiscard]] static qreal maximum();

  Q_INVOKABLE [[nodiscard]] static qreal sanitized(qreal speed);
  Q_INVOKABLE [[nodiscard]] static qreal stepped(qreal speed, int direction);
  Q_INVOKABLE [[nodiscard]] static int index_of(qreal speed);
  Q_INVOKABLE [[nodiscard]] static QString label(qreal speed);
  Q_INVOKABLE [[nodiscard]] static QStringList labels();
};

#endif
