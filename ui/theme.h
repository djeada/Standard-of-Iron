#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QObject>
#include <QQmlEngine>

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

public:
  static Theme *instance();
  static Theme *create(QQmlEngine *engine, QJSEngine *scriptEngine);

  QColor bg() const { return QColor("#071018"); }
  QColor bgShade() const { return QColor("#061214"); }
  QColor dim() const { return QColor(0, 0, 0, 0.45 * 255); }

  QColor panelBase() const { return QColor("#071018"); }
  QColor panelBr() const { return QColor("#0f2430"); }
  QColor panelBorder() const { return QColor("#0f2430"); }

  QColor cardBase() const { return QColor("#061214"); }
  QColor cardBaseA() const { return QColor("#061214AA"); }
  QColor cardBaseB() const { return QColor("#061214"); }
  QColor cardBorder() const { return QColor("#12323a"); }

  QColor hover() const { return QColor("#184c7a"); }
  QColor hoverBg() const { return QColor("#184c7a"); }
  QColor selected() const { return QColor("#1f8bf5"); }
  QColor selectedBg() const { return QColor("#1f8bf5"); }
  QColor selectedBr() const { return QColor("#1b74d1"); }

  QColor thumbBr() const { return QColor("#2A4E56"); }
  QColor border() const { return QColor("#0f2b34"); }

  QColor textMain() const { return QColor("#eaf6ff"); }
  QColor textBright() const { return QColor("#dff0ff"); }
  QColor textSub() const { return QColor("#86a7b6"); }
  QColor textSubLite() const { return QColor("#79a6b7"); }
  QColor textDim() const { return QColor("#4f6a75"); }
  QColor textHint() const { return QColor("#2a5e6e"); }

  QColor accent() const { return QColor("#9fd9ff"); }
  QColor accentBright() const { return QColor("#d0e8ff"); }

  QColor addColor() const { return QColor("#3A9CA8"); }
  QColor removeColor() const { return QColor("#D04040"); }

  int spacingTiny() const { return 4; }
  int spacingSmall() const { return 8; }
  int spacingMedium() const { return 12; }
  int spacingLarge() const { return 16; }
  int spacingXLarge() const { return 20; }

  int radiusSmall() const { return 4; }
  int radiusMedium() const { return 6; }
  int radiusLarge() const { return 8; }
  int radiusPanel() const { return 14; }

  int animFast() const { return 120; }
  int animNormal() const { return 160; }
  int animSlow() const { return 200; }

  int fontSizeTiny() const { return 11; }
  int fontSizeSmall() const { return 12; }
  int fontSizeMedium() const { return 14; }
  int fontSizeLarge() const { return 16; }
  int fontSizeTitle() const { return 18; }
  int fontSizeHero() const { return 28; }

private:
  explicit Theme(QObject *parent = nullptr);
  static Theme *m_instance;
};

#endif