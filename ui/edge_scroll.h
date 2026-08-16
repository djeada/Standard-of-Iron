#ifndef SOI_UI_EDGE_SCROLL_H
#define SOI_UI_EDGE_SCROLL_H

#include <QObject>
#include <QPointF>
#include <QQmlEngine>

namespace Ui::EdgeScrollGeometry {

inline constexpr double k_base_horizontal_zone = 12.0;
inline constexpr double k_base_vertical_zone = 10.0;
inline constexpr double k_min_zone = 4.0;

struct Vector {
  double dx = 0.0;
  double dz = 0.0;

  [[nodiscard]] auto is_zero() const -> bool { return dx == 0.0 && dz == 0.0; }
};

[[nodiscard]] auto horizontal_zone(double sensitivity, double ui_scale) -> double;
[[nodiscard]] auto vertical_zone(double sensitivity, double ui_scale) -> double;

[[nodiscard]] auto vector_at(double x,
                             double y,
                             double width,
                             double height,
                             double sensitivity,
                             double ui_scale) -> Vector;

} // namespace Ui::EdgeScrollGeometry

class EdgeScroll : public QObject {
  Q_OBJECT

  Q_PROPERTY(qreal baseHorizontalZone READ base_horizontal_zone CONSTANT)
  Q_PROPERTY(qreal baseVerticalZone READ base_vertical_zone CONSTANT)

public:
  explicit EdgeScroll(QObject* parent = nullptr)
      : QObject(parent) {}

  static EdgeScroll* instance();
  static EdgeScroll* create(QQmlEngine* engine, QJSEngine* script_engine);

  [[nodiscard]] static qreal base_horizontal_zone();
  [[nodiscard]] static qreal base_vertical_zone();

  Q_INVOKABLE [[nodiscard]] static qreal horizontalZone(qreal sensitivity,
                                                        qreal ui_scale);
  Q_INVOKABLE [[nodiscard]] static qreal verticalZone(qreal sensitivity,
                                                      qreal ui_scale);

  Q_INVOKABLE [[nodiscard]] static QPointF vector(
      qreal x, qreal y, qreal width, qreal height, qreal sensitivity, qreal ui_scale);
};

#endif
