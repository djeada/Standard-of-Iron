#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

class Theme : public QObject {
  Q_OBJECT

  Q_PROPERTY(QColor bg READ bg CONSTANT)
  Q_PROPERTY(QColor bgShade READ bgShade CONSTANT)
  Q_PROPERTY(QColor dim READ dim CONSTANT)

  Q_PROPERTY(QColor panelBase READ panelBase CONSTANT)
  Q_PROPERTY(QColor panelBr READ panelBr CONSTANT)
  Q_PROPERTY(QColor panelBorder READ panelBorder CONSTANT)

  Q_PROPERTY(QColor cardBase READ cardBase CONSTANT)
  Q_PROPERTY(QColor cardBaseA READ cardBaseA CONSTANT)
  Q_PROPERTY(QColor cardBaseB READ cardBaseB CONSTANT)
  Q_PROPERTY(QColor cardBorder READ cardBorder CONSTANT)

  Q_PROPERTY(QColor hover READ hover CONSTANT)
  Q_PROPERTY(QColor hoverBg READ hoverBg CONSTANT)
  Q_PROPERTY(QColor selected READ selected CONSTANT)
  Q_PROPERTY(QColor selectedBg READ selectedBg CONSTANT)
  Q_PROPERTY(QColor selectedBr READ selectedBr CONSTANT)

  Q_PROPERTY(QColor thumbBr READ thumbBr CONSTANT)
  Q_PROPERTY(QColor border READ border CONSTANT)

  Q_PROPERTY(QColor textMain READ textMain CONSTANT)
  Q_PROPERTY(QColor textBright READ textBright CONSTANT)
  Q_PROPERTY(QColor textSub READ textSub CONSTANT)
  Q_PROPERTY(QColor textSubLite READ textSubLite CONSTANT)
  Q_PROPERTY(QColor textDim READ textDim CONSTANT)
  Q_PROPERTY(QColor textHint READ textHint CONSTANT)

  Q_PROPERTY(QColor accent READ accent CONSTANT)
  Q_PROPERTY(QColor accentBright READ accentBright CONSTANT)

  Q_PROPERTY(QColor addColor READ addColor CONSTANT)
  Q_PROPERTY(QColor removeColor READ removeColor CONSTANT)

  Q_PROPERTY(int spacingTiny READ spacingTiny CONSTANT)
  Q_PROPERTY(int spacingSmall READ spacingSmall CONSTANT)
  Q_PROPERTY(int spacingMedium READ spacingMedium CONSTANT)
  Q_PROPERTY(int spacingLarge READ spacingLarge CONSTANT)
  Q_PROPERTY(int spacingXLarge READ spacingXLarge CONSTANT)

  Q_PROPERTY(int radiusSmall READ radiusSmall CONSTANT)
  Q_PROPERTY(int radiusMedium READ radiusMedium CONSTANT)
  Q_PROPERTY(int radiusLarge READ radiusLarge CONSTANT)
  Q_PROPERTY(int radiusPanel READ radiusPanel CONSTANT)

  Q_PROPERTY(int animFast READ animFast CONSTANT)
  Q_PROPERTY(int animNormal READ animNormal CONSTANT)
  Q_PROPERTY(int animSlow READ animSlow CONSTANT)

  Q_PROPERTY(int fontSizeTiny READ fontSizeTiny CONSTANT)
  Q_PROPERTY(int fontSizeSmall READ fontSizeSmall CONSTANT)
  Q_PROPERTY(int fontSizeMedium READ fontSizeMedium CONSTANT)
  Q_PROPERTY(int fontSizeLarge READ fontSizeLarge CONSTANT)
  Q_PROPERTY(int fontSizeTitle READ fontSizeTitle CONSTANT)
  Q_PROPERTY(int fontSizeHero READ fontSizeHero CONSTANT)

  Q_PROPERTY(QVariantList playerColors READ playerColors CONSTANT)
  Q_PROPERTY(QVariantList teamIcons READ teamIcons CONSTANT)
  Q_PROPERTY(QVariantList factions READ factions CONSTANT)
  Q_PROPERTY(QVariantMap unitIcons READ unitIcons CONSTANT)

public:
  static auto instance() -> Theme *;
  static auto create(QQmlEngine *engine, QJSEngine *scriptEngine) -> Theme *;

  [[nodiscard]] static QColor bg() { return {"#071018"}; }
  [[nodiscard]] static QColor bgShade() { return {"#061214"}; }
  [[nodiscard]] static QColor dim() { return {0, 0, 0, 115}; }

  [[nodiscard]] static QColor panelBase() { return {"#071018"}; }
  [[nodiscard]] static QColor panelBr() { return {"#0f2430"}; }
  [[nodiscard]] static QColor panelBorder() { return {"#0f2430"}; }

  [[nodiscard]] static QColor cardBase() { return {"#061214"}; }
  [[nodiscard]] static QColor cardBaseA() { return {"#061214AA"}; }
  [[nodiscard]] static QColor cardBaseB() { return {"#061214"}; }
  [[nodiscard]] static QColor cardBorder() { return {"#12323a"}; }

  [[nodiscard]] static QColor hover() { return {"#184c7a"}; }
  [[nodiscard]] static QColor hoverBg() { return {"#184c7a"}; }
  [[nodiscard]] static QColor selected() { return {"#1f8bf5"}; }
  [[nodiscard]] static QColor selectedBg() { return {"#1f8bf5"}; }
  [[nodiscard]] static QColor selectedBr() { return {"#1b74d1"}; }

  [[nodiscard]] static QColor thumbBr() { return {"#2A4E56"}; }
  [[nodiscard]] static QColor border() { return {"#0f2b34"}; }

  [[nodiscard]] static QColor textMain() { return {"#eaf6ff"}; }
  [[nodiscard]] static QColor textBright() { return {"#dff0ff"}; }
  [[nodiscard]] static QColor textSub() { return {"#86a7b6"}; }
  [[nodiscard]] static QColor textSubLite() { return {"#79a6b7"}; }
  [[nodiscard]] static QColor textDim() { return {"#4f6a75"}; }
  [[nodiscard]] static QColor textHint() { return {"#2a5e6e"}; }

  [[nodiscard]] static QColor accent() { return {"#9fd9ff"}; }
  [[nodiscard]] static QColor accentBright() { return {"#d0e8ff"}; }

  [[nodiscard]] static QColor addColor() { return {"#3A9CA8"}; }
  [[nodiscard]] static QColor removeColor() { return {"#D04040"}; }

  [[nodiscard]] static int spacingTiny() { return 4; }
  [[nodiscard]] static int spacingSmall() { return 8; }
  [[nodiscard]] static int spacingMedium() { return 12; }
  [[nodiscard]] static int spacingLarge() { return 16; }
  [[nodiscard]] static int spacingXLarge() { return 20; }

  [[nodiscard]] static int radiusSmall() { return 4; }
  [[nodiscard]] static int radiusMedium() { return 6; }
  [[nodiscard]] static int radiusLarge() { return 8; }
  [[nodiscard]] static int radiusPanel() { return 14; }

  [[nodiscard]] static int animFast() { return 120; }
  [[nodiscard]] static int animNormal() { return 160; }
  [[nodiscard]] static int animSlow() { return 200; }

  [[nodiscard]] static int fontSizeTiny() { return 11; }
  [[nodiscard]] static int fontSizeSmall() { return 12; }
  [[nodiscard]] static int fontSizeMedium() { return 14; }
  [[nodiscard]] static int fontSizeLarge() { return 16; }
  [[nodiscard]] static int fontSizeTitle() { return 18; }
  [[nodiscard]] static int fontSizeHero() { return 28; }

  [[nodiscard]] auto playerColors() const -> QVariantList;
  [[nodiscard]] auto teamIcons() const -> QVariantList;
  [[nodiscard]] auto factions() const -> QVariantList;
  [[nodiscard]] auto unitIcons() const -> QVariantMap;

private:
  explicit Theme(QObject *parent = nullptr);
  static Theme *m_instance;
};

#endif