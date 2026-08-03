#ifndef UI_ICON_ART_H
#define UI_ICON_ART_H

#include <QColor>
#include <QObject>
#include <QPainterPath>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <vector>

class QPainter;
class QRectF;

namespace Ui::IconArt {

enum class Tone : std::uint8_t {
  Ink = 0,
  Metal = 1,
  Edge = 2,
  Ember = 3,
  Timber = 4,
  Stone = 5,
  Iron = 6,
  Gold = 7,
};

struct Stroke {

  QString path;
  Tone tone{Tone::Metal};
  bool filled{true};

  float width{1.6F};
};

struct Art {
  QString id;
  std::vector<Stroke> strokes;
};

struct Palette {
  QColor ink;
  QColor metal;
  QColor edge;
  QColor ember;
  QColor timber;
  QColor stone;
  QColor iron;
  QColor gold;

  [[nodiscard]] auto color_for(Tone tone) const -> QColor;
};

inline constexpr float k_design_grid = 24.0F;

[[nodiscard]] auto tone_id(Tone tone) -> QString;
[[nodiscard]] auto tone_from_id(const QString& id) -> Tone;

[[nodiscard]] auto find(const QString& id) -> const Art*;
[[nodiscard]] auto ids() -> QStringList;

[[nodiscard]] auto resolve_id(const QString& id) -> QString;

[[nodiscard]] auto build_path(const QString& path_data,
                              qreal scale,
                              qreal offset_x,
                              qreal offset_y) -> QPainterPath;

[[nodiscard]] auto default_palette() -> Palette;

void paint(QPainter& painter,
           const QString& id,
           const QRectF& rect,
           const Palette& palette,
           qreal opacity = 1.0);

} // namespace Ui::IconArt

class IconArtLibrary : public QObject {
  Q_OBJECT

public:
  explicit IconArtLibrary(QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] static bool has(const QString& id);
  Q_INVOKABLE [[nodiscard]] static QStringList ids();
  Q_INVOKABLE [[nodiscard]] static QString resolve(const QString& id);

  Q_INVOKABLE [[nodiscard]] static QVariantList strokes(const QString& id);

  static auto create(QQmlEngine* engine, QJSEngine* script_engine) -> IconArtLibrary*;
};

#endif
