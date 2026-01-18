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
  Q_PROPERTY(QColor accentBr READ accentBr CONSTANT)

  Q_PROPERTY(QColor addColor READ addColor CONSTANT)
  Q_PROPERTY(QColor removeColor READ removeColor CONSTANT)

  Q_PROPERTY(QColor dangerBg READ dangerBg CONSTANT)
  Q_PROPERTY(QColor dangerBr READ dangerBr CONSTANT)

  Q_PROPERTY(QColor successBg READ successBg CONSTANT)
  Q_PROPERTY(QColor successBr READ successBr CONSTANT)
  Q_PROPERTY(QColor successText READ successText CONSTANT)
  Q_PROPERTY(QColor disabledBg READ disabledBg CONSTANT)

  Q_PROPERTY(QColor infoBg READ infoBg CONSTANT)
  Q_PROPERTY(QColor infoBr READ infoBr CONSTANT)
  Q_PROPERTY(QColor infoText READ infoText CONSTANT)

  Q_PROPERTY(QColor warningText READ warningText CONSTANT)

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
  Q_PROPERTY(QVariantMap nationEmblems READ nationEmblems CONSTANT)

public:
  static auto instance() -> Theme *;
  static auto create(QQmlEngine *engine, QJSEngine *scriptEngine) -> Theme *;

  [[nodiscard]] static auto bg() -> QColor { return {"#071018"}; }
  [[nodiscard]] static auto bgShade() -> QColor { return {"#061214"}; }
  [[nodiscard]] static auto dim() -> QColor { return {0, 0, 0, 115}; }

  [[nodiscard]] static auto panelBase() -> QColor { return {"#071018"}; }
  [[nodiscard]] static auto panelBr() -> QColor { return {"#0f2430"}; }
  [[nodiscard]] static auto panelBorder() -> QColor { return {"#0f2430"}; }

  [[nodiscard]] static auto cardBase() -> QColor { return {"#061214"}; }
  [[nodiscard]] static auto cardBaseA() -> QColor { return {"#061214AA"}; }
  [[nodiscard]] static auto cardBaseB() -> QColor { return {"#061214"}; }
  [[nodiscard]] static auto cardBorder() -> QColor { return {"#12323a"}; }

  [[nodiscard]] static auto hover() -> QColor { return {"#184c7a"}; }
  [[nodiscard]] static auto hoverBg() -> QColor { return {"#184c7a"}; }
  [[nodiscard]] static auto selected() -> QColor { return {"#1f8bf5"}; }
  [[nodiscard]] static auto selectedBg() -> QColor { return {"#1f8bf5"}; }
  [[nodiscard]] static auto selectedBr() -> QColor { return {"#1b74d1"}; }

  [[nodiscard]] static auto thumbBr() -> QColor { return {"#2A4E56"}; }
  [[nodiscard]] static auto border() -> QColor { return {"#0f2b34"}; }

  [[nodiscard]] static auto textMain() -> QColor { return {"#eaf6ff"}; }
  [[nodiscard]] static auto textBright() -> QColor { return {"#dff0ff"}; }
  [[nodiscard]] static auto textSub() -> QColor { return {"#86a7b6"}; }
  [[nodiscard]] static auto textSubLite() -> QColor { return {"#79a6b7"}; }
  [[nodiscard]] static auto textDim() -> QColor { return {"#4f6a75"}; }
  [[nodiscard]] static auto textHint() -> QColor { return {"#2a5e6e"}; }

  [[nodiscard]] static auto accent() -> QColor { return {"#9fd9ff"}; }
  [[nodiscard]] static auto accentBright() -> QColor { return {"#d0e8ff"}; }
  [[nodiscard]] static auto accentBr() -> QColor { return {"#7eb8db"}; }

  [[nodiscard]] static auto addColor() -> QColor { return {"#3A9CA8"}; }
  [[nodiscard]] static auto removeColor() -> QColor { return {"#D04040"}; }

  [[nodiscard]] static auto dangerBg() -> QColor { return {"#4a1e1e"}; }
  [[nodiscard]] static auto dangerBr() -> QColor { return {"#6b2d2d"}; }

  [[nodiscard]] static auto successBg() -> QColor { return {"#1e4a2c"}; }
  [[nodiscard]] static auto successBr() -> QColor { return {"#2d6b3f"}; }
  [[nodiscard]] static auto successText() -> QColor { return {"#8fdc9f"}; }
  [[nodiscard]] static auto disabledBg() -> QColor { return {"#1a2a32"}; }

  [[nodiscard]] static auto infoBg() -> QColor { return {"#1a3a5a"}; }
  [[nodiscard]] static auto infoBr() -> QColor { return {"#2a5a8a"}; }
  [[nodiscard]] static auto infoText() -> QColor { return {"#7ab8e8"}; }

  [[nodiscard]] static auto warningText() -> QColor { return {"#f5a623"}; }

  [[nodiscard]] static auto spacingTiny() -> int { return 4; }
  [[nodiscard]] static auto spacingSmall() -> int { return 8; }
  [[nodiscard]] static auto spacingMedium() -> int { return 12; }
  [[nodiscard]] static auto spacingLarge() -> int { return 16; }
  [[nodiscard]] static auto spacingXLarge() -> int { return 20; }

  [[nodiscard]] static auto radiusSmall() -> int { return 4; }
  [[nodiscard]] static auto radiusMedium() -> int { return 6; }
  [[nodiscard]] static auto radiusLarge() -> int { return 8; }
  [[nodiscard]] static auto radiusPanel() -> int { return 14; }

  [[nodiscard]] static auto animFast() -> int { return 120; }
  [[nodiscard]] static auto animNormal() -> int { return 160; }
  [[nodiscard]] static auto animSlow() -> int { return 200; }

  [[nodiscard]] static auto fontSizeTiny() -> int { return 11; }
  [[nodiscard]] static auto fontSizeSmall() -> int { return 12; }
  [[nodiscard]] static auto fontSizeMedium() -> int { return 14; }
  [[nodiscard]] static auto fontSizeLarge() -> int { return 16; }
  [[nodiscard]] static auto fontSizeTitle() -> int { return 18; }
  [[nodiscard]] static auto fontSizeHero() -> int { return 28; }

  [[nodiscard]] static auto playerColors() -> QVariantList;
  [[nodiscard]] static auto teamIcons() -> QVariantList;
  [[nodiscard]] static auto factions() -> QVariantList;
  [[nodiscard]] static auto unitIcons() -> QVariantMap;
  [[nodiscard]] static auto nationEmblems() -> QVariantMap;

private:
  explicit Theme(QObject *parent = nullptr);
  static Theme *m_instance;
};

#endif
