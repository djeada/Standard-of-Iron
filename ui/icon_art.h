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

// The command and activity iconography, drawn from vector outlines rather than
// shipped as bitmaps.
//
// One geometry source feeds three surfaces that must not drift: the QML HUD
// draws it on a Canvas, the overhead markers reuse the same shapes at a smaller
// size, and the arena viewport paints it with QPainter. Bitmaps could not do
// that job here anyway -- the previous icons were 30x30 PNGs, which is already
// soft at 1x and mush on a high-DPI display.
namespace Ui::IconArt {

// Every shape names a role rather than a colour so a caller can retint the whole
// set for a disabled button, an interrupted order or the high-contrast theme
// without the artwork knowing anything about the palette.
enum class Tone : std::uint8_t {
  Ink = 0,   // cut lines and shadowed recesses
  Metal = 1, // the primary form
  Edge = 2,  // struck highlight
  Ember = 3, // the live, energetic accent
  Timber = 4,
  Stone = 5,
  Iron = 6,
  Gold = 7,
};

struct Stroke {
  // Absolute-coordinate path over a 24x24 design grid. Only M, L, Q and Z.
  QString path;
  Tone tone{Tone::Metal};
  bool filled{true};
  // Grid units, so the line keeps its weight relative to the icon at any size.
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

// Nullptr when nothing is registered under `id`; callers fall back to a glyph.
[[nodiscard]] auto find(const QString& id) -> const Art*;
[[nodiscard]] auto ids() -> QStringList;

// Resolves aliases such as "build" -> "construct" before looking the art up, so
// HUD action ids and activity ids can share one drawing.
[[nodiscard]] auto resolve_id(const QString& id) -> QString;

[[nodiscard]] auto build_path(const QString& path_data,
                              qreal scale,
                              qreal offset_x,
                              qreal offset_y) -> QPainterPath;

[[nodiscard]] auto default_palette() -> Palette;

// Paints the icon centred in `rect`, preserving the square design grid.
void paint(QPainter& painter,
           const QString& id,
           const QRectF& rect,
           const Palette& palette,
           qreal opacity = 1.0);

} // namespace Ui::IconArt

// QML face of the library. Registered as a singleton so a Canvas delegate can
// ask for the polylines it needs to stroke without reimplementing the path
// grammar in JavaScript.
class IconArtLibrary : public QObject {
  Q_OBJECT

public:
  explicit IconArtLibrary(QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] static bool has(const QString& id);
  Q_INVOKABLE [[nodiscard]] static QStringList ids();
  Q_INVOKABLE [[nodiscard]] static QString resolve(const QString& id);

  // [{ tone: "metal", filled: true, width: 1.6, subpaths: [[x, y, x, y, ...]] }]
  // Coordinates are already normalised to 0..1 so the caller only multiplies by
  // its own pixel size.
  Q_INVOKABLE [[nodiscard]] static QVariantList strokes(const QString& id);

  static auto create(QQmlEngine* engine, QJSEngine* script_engine) -> IconArtLibrary*;
};

#endif // UI_ICON_ART_H
