#include "icon_art.h"

#include <QHash>
#include <QPainter>
#include <QPolygonF>
#include <QRectF>
#include <QStringView>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace Ui::IconArt {

namespace {

using S = Stroke;

auto fill(QString path, Tone tone) -> Stroke {
  return Stroke{std::move(path), tone, true, 0.0F};
}

auto line(QString path, Tone tone, float width) -> Stroke {
  return Stroke{std::move(path), tone, false, width};
}

auto pick_head() -> Stroke {
  return fill(QStringLiteral("M9.4 2.8 Q15.2 1.4 21 5.6 L19.2 8.4 Q14.6 4.8 10.4 5.8 "
                             "Z"),
              Tone::Metal);
}

auto pick_haft() -> Stroke {
  return line(QStringLiteral("M13.8 5.2 L6.4 19.8"), Tone::Metal, 2.4F);
}

auto boulder_body(Tone tone) -> Stroke {
  return fill(QStringLiteral("M2.6 21.6 L5.2 16.2 L10.4 14.4 L16.4 16.8 L18.2 21.6 Z"),
              tone);
}

auto build_catalog() -> std::vector<Art> {
  std::vector<Art> catalog;

  const auto add = [&catalog](const char* id, std::vector<Stroke> strokes) {
    catalog.push_back(Art{QString::fromLatin1(id), std::move(strokes)});
  };

  add("idle",
      {line(QStringLiteral("M12 4 L17.6 6.4 L20 12 L17.6 17.6 L12 20 L6.4 17.6 L4 12 "
                           "L6.4 6.4 Z"),
            Tone::Metal,
            2.0F),
       fill(QStringLiteral("M12 9.6 L14.4 12 L12 14.4 L9.6 12 Z"), Tone::Ink)});

  add("move",
      {line(QStringLiteral("M4 12 L13 12"), Tone::Metal, 3.0F),
       fill(QStringLiteral("M12.4 6.4 L20.4 12 L12.4 17.6 Z"), Tone::Metal),
       line(QStringLiteral("M6 12 L11 12"), Tone::Ink, 1.0F)});

  add("attack",
      {line(QStringLiteral("M4.8 19.2 L19.2 4.8"), Tone::Metal, 3.0F),
       line(QStringLiteral("M19.2 19.2 L4.8 4.8"), Tone::Metal, 3.0F),
       line(QStringLiteral("M7.6 16.4 L16.4 7.6"), Tone::Ink, 1.1F),
       fill(QStringLiteral("M12 9.4 L14.6 12 L12 14.6 L9.4 12 Z"), Tone::Ember)});

  add("patrol",
      {line(QStringLiteral("M5 8.4 L17.4 8.4"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M16.2 5.2 L21 8.4 L16.2 11.6 Z"), Tone::Metal),
       line(QStringLiteral("M19 15.6 L6.6 15.6"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M7.8 12.4 L3 15.6 L7.8 18.8 Z"), Tone::Metal)});

  add("guard",
      {fill(QStringLiteral("M12 2.8 L20 6 L20 12.4 Q20 18.4 12 21.6 Q4 18.4 4 12.4 "
                           "L4 6 Z"),
            Tone::Metal),
       line(QStringLiteral("M12 6 L12 18.4"), Tone::Ink, 1.2F),
       fill(QStringLiteral("M12 9.6 L14.4 12 L12 14.4 L9.6 12 Z"), Tone::Ember)});

  add("hold",
      {line(QStringLiteral("M12 3.6 L12 17"), Tone::Metal, 2.8F),
       line(QStringLiteral("M7 8.4 L17 8.4"), Tone::Metal, 2.4F),
       line(QStringLiteral("M4.6 20.4 L19.4 20.4"), Tone::Metal, 3.0F),
       fill(QStringLiteral("M12 1.6 L14.2 3.8 L12 6 L9.8 3.8 Z"), Tone::Ember)});

  add("divide",
      {line(QStringLiteral("M11.2 12 L4.8 12"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M3 12 L8 8 L8 16 Z"), Tone::Metal),
       line(QStringLiteral("M12.8 12 L19.2 12"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M21 12 L16 8 L16 16 Z"), Tone::Metal),
       line(QStringLiteral("M12 4.2 L12 8"), Tone::Ember, 2.0F),
       line(QStringLiteral("M12 16 L12 19.8"), Tone::Ember, 2.0F)});

  add("join",
      {line(QStringLiteral("M3.2 7 L9.4 12"), Tone::Metal, 2.2F),
       line(QStringLiteral("M3.2 17 L9.4 12"), Tone::Metal, 2.2F),
       fill(QStringLiteral("M8 8 L13.2 12 L8 16 Z"), Tone::Metal),
       line(QStringLiteral("M20.8 7 L14.6 12"), Tone::Metal, 2.2F),
       line(QStringLiteral("M20.8 17 L14.6 12"), Tone::Metal, 2.2F),
       fill(QStringLiteral("M16 8 L10.8 12 L16 16 Z"), Tone::Metal),
       fill(QStringLiteral("M10 10 L14 10 L14 14 L10 14 Z"), Tone::Ember)});

  add("construct",
      {line(QStringLiteral("M4 20.4 L4 8 L12 3.4 L20 8 L20 20.4"), Tone::Metal, 2.4F),
       line(QStringLiteral("M4 13.2 L20 13.2"), Tone::Metal, 2.0F),
       line(QStringLiteral("M12 5.4 L12 13.2"), Tone::Ink, 1.1F),
       fill(QStringLiteral("M12 15.4 L14.2 17.6 L12 19.8 L9.8 17.6 Z"), Tone::Ember)});

  add("repair",
      {fill(QStringLiteral("M2.8 12.4 L21.2 12.4 L18.6 16.4 L14 16.4 L14 20.6 L10 "
                           "20.6 L10 16.4 L5.4 16.4 Z"),
            Tone::Metal),
       fill(QStringLiteral("M6.6 20.2 L17.4 20.2 L17.4 22.2 L6.6 22.2 Z"), Tone::Metal),
       line(QStringLiteral("M4.4 11.8 L11.2 6.6"), Tone::Metal, 2.6F),
       fill(QStringLiteral("M11 2.2 L19.4 6.4 L17.2 10.4 L8.8 6.2 Z"), Tone::Metal),
       fill(QStringLiteral("M16.4 9 L18.6 11.2 L16.4 13.4 L14.2 11.2 Z"),
            Tone::Ember)});

  add("dismantle",
      {fill(QStringLiteral("M4 12.4 L20 12.4 L20 20.4 L4 20.4 Z"), Tone::Metal),
       fill(QStringLiteral("M9 14.6 L15 14.6 L15 18.2 L9 18.2 Z"), Tone::Ink),
       line(QStringLiteral("M12 2 L12 6.6"), Tone::Ember, 2.2F),
       fill(QStringLiteral("M8.4 5.8 L15.6 5.8 L12 10.4 Z"), Tone::Ember)});

  add("chop_wood",
      {fill(QStringLiteral("M2.6 16.4 L14.8 16.4 L14.8 21.6 L2.6 21.6 Z"),
            Tone::Timber),
       line(QStringLiteral("M4.8 19 L12.6 19"), Tone::Ink, 1.1F),
       line(QStringLiteral("M5.4 20.2 L13.6 6.6"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M13 3 L17.6 5.6 Q22.2 9 17.6 12.4 L13 9.8 Q15.2 6.4 13 3 "
                           "Z"),
            Tone::Metal)});

  add("mine_stone",
      {pick_head(),
       pick_haft(),
       boulder_body(Tone::Stone),
       line(QStringLiteral("M7.6 21.6 L9 17.6 L13.6 16.4"), Tone::Ink, 1.1F)});

  add("mine_iron",
      {pick_head(),
       pick_haft(),
       boulder_body(Tone::Iron),
       fill(QStringLiteral("M7.4 17.8 L8.9 19.3 L7.4 20.8 L5.9 19.3 Z"), Tone::Ember),
       fill(QStringLiteral("M13.6 17.6 L15 19 L13.6 20.4 L12.2 19 Z"), Tone::Ember)});

  add("harvest_grain",
      {line(QStringLiteral("M6 21.6 L8.6 9.4"), Tone::Timber, 1.6F),
       line(QStringLiteral("M12 21.6 L12.6 8.6"), Tone::Timber, 1.6F),
       line(QStringLiteral("M18 21.6 L16.4 9.4"), Tone::Timber, 1.6F),
       fill(QStringLiteral("M8.6 9.6 L10.4 6.4 L8.6 2.6 L6.8 6.4 Z"), Tone::Ember),
       fill(QStringLiteral("M12.6 8.8 L14.4 5.6 L12.6 1.8 L10.8 5.6 Z"), Tone::Ember),
       fill(QStringLiteral("M16.4 9.6 L18.2 6.4 L16.4 2.6 L14.6 6.4 Z"), Tone::Ember),
       line(QStringLiteral("M3.4 20 L20.6 20"), Tone::Ink, 1.1F)});

  add("slaughter_sheep",
      {fill(QStringLiteral("M4.2 18.6 L6.2 11.4 L14.6 10.2 L18.8 13.6 L16.4 18.6 Z"),
            Tone::Ember),
       fill(QStringLiteral("M14.6 10.2 L17.4 5.6 L20.6 6.4 L18.8 13.6 Z"), Tone::Ember),
       line(QStringLiteral("M3.2 5.2 L11.8 13.8"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M2.2 3.4 L6.4 3.2 L4.4 6.6 Z"), Tone::Metal),
       line(QStringLiteral("M6.6 18.6 L16.4 18.6"), Tone::Ink, 1.1F)});

  add("auto_gather",
      {pick_head(),
       pick_haft(),
       line(QStringLiteral("M4.4 12 Q4.4 4.8 11.6 4.8 Q16.4 4.8 18.6 8.4"),
            Tone::Ember,
            2.0F),
       fill(QStringLiteral("M15.4 3.2 L20.6 6.2 L15.4 9.2 Z"), Tone::Ember)});

  add("deliver",
      {fill(QStringLiteral("M4.4 10.2 L12.4 10.2 L12.4 19.6 L4.4 19.6 Z"), Tone::Metal),
       line(QStringLiteral("M4.4 13.6 L12.4 13.6"), Tone::Ink, 1.1F),
       line(QStringLiteral("M13.8 14.9 L17.6 14.9"), Tone::Ember, 2.2F),
       fill(QStringLiteral("M16.6 11.4 L21.2 14.9 L16.6 18.4 Z"), Tone::Ember)});

  add("heal",
      {fill(QStringLiteral("M9.6 3.8 L14.4 3.8 L14.4 9.6 L20.2 9.6 L20.2 14.4 L14.4 "
                           "14.4 L14.4 20.2 L9.6 20.2 L9.6 14.4 L3.8 14.4 L3.8 9.6 "
                           "L9.6 9.6 Z"),
            Tone::Metal),
       fill(QStringLiteral("M12 9.4 L14.6 12 L12 14.6 L9.4 12 Z"), Tone::Ember)});

  add("train",
      {fill(QStringLiteral("M4.4 14.6 Q4.4 4 12 4 Q19.6 4 19.6 14.6 Z"), Tone::Metal),
       fill(QStringLiteral("M2.8 14.4 L21.2 14.4 L21.2 17.4 L2.8 17.4 Z"), Tone::Metal),
       fill(QStringLiteral("M5.8 17.4 L9.8 17.4 L9.8 21.6 L5.8 21.6 Z"), Tone::Metal),
       fill(QStringLiteral("M14.2 17.4 L18.2 17.4 L18.2 21.6 L14.2 21.6 Z"),
            Tone::Metal),
       fill(QStringLiteral("M10.2 1.4 L13.8 1.4 L13.8 4.6 L10.2 4.6 Z"), Tone::Ember),
       line(QStringLiteral("M12 6.2 L12 14.2"), Tone::Ink, 1.4F)});

  add("blocked",
      {line(QStringLiteral("M8 3.8 L16 3.8 L20.2 8 L20.2 16 L16 20.2 L8 20.2 L3.8 16 "
                           "L3.8 8 Z"),
            Tone::Metal,
            2.4F),
       line(QStringLiteral("M7.2 16.8 L16.8 7.2"), Tone::Ember, 2.6F)});

  add("collect",
      {pick_head(),
       pick_haft(),
       fill(QStringLiteral("M1.8 21.6 L6 15.4 L9.4 21.6 Z"), Tone::Timber),
       fill(QStringLiteral("M9 21.6 L13.8 15 L18.8 21.6 Z"), Tone::Stone)});

  add("formation",
      {fill(QStringLiteral("M4 5.4 L20 5.4 L20 8.8 L4 8.8 Z"), Tone::Metal),
       fill(QStringLiteral("M4 10.4 L20 10.4 L20 13.8 L4 13.8 Z"), Tone::Metal),
       fill(QStringLiteral("M4 15.4 L20 15.4 L20 18.8 L4 18.8 Z"), Tone::Metal),
       line(QStringLiteral("M9.4 5.4 L9.4 18.8"), Tone::Ink, 1.0F),
       line(QStringLiteral("M14.6 5.4 L14.6 18.8"), Tone::Ink, 1.0F)});

  add("rally",
      {line(QStringLiteral("M7 2.8 L7 21.4"), Tone::Metal, 2.4F),
       fill(QStringLiteral("M7.6 4 L19.4 7.2 L7.6 11.2 Z"), Tone::Ember),
       fill(QStringLiteral("M4 20 L10 20 L10 22.2 L4 22.2 Z"), Tone::Metal)});

  add("stop",
      {fill(QStringLiteral("M6 6 L18 6 L18 18 L6 18 Z"), Tone::Metal),
       line(QStringLiteral("M9 9 L15 15"), Tone::Ink, 1.2F),
       line(QStringLiteral("M15 9 L9 15"), Tone::Ink, 1.2F)});

  add("run",
      {fill(QStringLiteral("M3.4 4.8 L10.4 12 L3.4 19.2 L7.4 19.2 L14.4 12 L7.4 4.8 Z"),
            Tone::Metal),
       fill(QStringLiteral("M10.6 4.8 L17.6 12 L10.6 19.2 L14.6 19.2 L21.6 12 L14.6 "
                           "4.8 Z"),
            Tone::Ember)});

  add("aura",
      {line(QStringLiteral("M12 2.6 L18.6 5.4 L21.4 12 L18.6 18.6 L12 21.4 L5.4 18.6 "
                           "L2.6 12 L5.4 5.4 Z"),
            Tone::Ember,
            2.0F),
       line(QStringLiteral("M12 7.4 L15.2 8.8 L16.6 12 L15.2 15.2 L12 16.6 L8.8 15.2 "
                           "L7.4 12 L8.8 8.8 Z"),
            Tone::Metal,
            1.8F),
       fill(QStringLiteral("M12 10.4 L13.6 12 L12 13.6 L10.4 12 Z"), Tone::Ember)});

  add("gate",
      {fill(QStringLiteral("M3.8 20.4 L3.8 9.2 Q12 2.2 20.2 9.2 L20.2 20.4 Z"),
            Tone::Metal),
       fill(QStringLiteral("M8 20.4 L8 12.2 Q12 8.6 16 12.2 L16 20.4 Z"), Tone::Ink),
       line(QStringLiteral("M12 10 L12 20.4"), Tone::Metal, 1.4F)});

  add("wood",
      {fill(QStringLiteral("M3.4 8.6 L20.6 8.6 L20.6 15.4 L3.4 15.4 Z"), Tone::Timber),
       line(QStringLiteral("M7 12 L17 12"), Tone::Ink, 1.1F)});

  add("stone",
      {fill(QStringLiteral("M4 19.4 L7 10.2 L13 8 L20 12.2 L20 19.4 Z"), Tone::Stone),
       line(QStringLiteral("M9.4 19.4 L11 13 L16.4 11.6"), Tone::Ink, 1.0F)});

  add("iron",
      {fill(QStringLiteral("M4 19.4 L7 10.2 L13 8 L20 12.2 L20 19.4 Z"), Tone::Iron),
       fill(QStringLiteral("M9 14.2 L10.6 15.8 L9 17.4 L7.4 15.8 Z"), Tone::Ember),
       fill(QStringLiteral("M14.8 13.2 L16.2 14.6 L14.8 16 L13.4 14.6 Z"),
            Tone::Ember)});

  add("food",
      {line(QStringLiteral("M8.4 20.6 L10.2 9.8"), Tone::Timber, 1.5F),
       line(QStringLiteral("M12 20.6 L12 8.6"), Tone::Timber, 1.5F),
       line(QStringLiteral("M15.6 20.6 L13.8 9.8"), Tone::Timber, 1.5F),
       fill(QStringLiteral("M10.2 10 L11.8 6.8 L10.2 3.4 L8.6 6.8 Z"), Tone::Ember),
       fill(QStringLiteral("M12 8.8 L13.6 5.6 L12 2.2 L10.4 5.6 Z"), Tone::Ember),
       fill(QStringLiteral("M13.8 10 L15.4 6.8 L13.8 3.4 L12.2 6.8 Z"), Tone::Ember),
       line(QStringLiteral("M6.2 15.2 L17.8 15.2"), Tone::Ink, 1.1F)});

  add("gold",
      {fill(QStringLiteral("M4.4 12.6 L19.6 12.6 L19.6 18.4 L4.4 18.4 Z"), Tone::Gold),
       fill(QStringLiteral("M7.2 6.6 L16.8 6.6 L16.8 12.2 L7.2 12.2 Z"), Tone::Gold),
       line(QStringLiteral("M4.4 15.4 L19.6 15.4"), Tone::Ink, 1.0F)});

  add("difficulty_easy",
      {fill(QStringLiteral("M6.9 10.05 L17.99 11.61 Q17.3 19.39 11.11 20.34 "
                           "Q5.42 17.72 6.9 10.05 Z"),
            Tone::Edge),
       fill(QStringLiteral("M5.68 10.08 L8.65 10.5 L8.21 15.08 Q6.92 17.12 5.44 16.11 "
                           "Q4.84 13.2 5.68 10.08 Z"),
            Tone::Gold),
       fill(QStringLiteral(
                "M19.15 11.98 L16.18 11.56 L15.34 16.09 Q16.03 18.4 17.72 17.83 "
                "Q19.1 15.2 19.15 11.98 Z"),
            Tone::Gold),
       line(QStringLiteral("M8.39 13.09 Q9.48 14.66 10.97 13.45"), Tone::Ink, 1.0F),
       line(QStringLiteral("M13.14 13.76 Q14.24 15.32 15.72 14.12"), Tone::Ink, 1.0F),
       fill(QStringLiteral(
                "M11.6 17.58 Q11.54 18.01 11.23 18.28 Q10.93 18.55 10.56 18.49 "
                "Q10.19 18.44 9.97 18.1 Q9.75 17.76 9.82 17.33 "
                "Q9.88 16.9 10.18 16.63 Q10.48 16.36 10.85 16.41 "
                "Q11.22 16.47 11.44 16.81 Q11.66 17.15 11.6 17.58 Z"),
            Tone::Ink),
       fill(QStringLiteral("M5.6 10.68 Q6.32 2.7 13.48 3.5 Q20.58 4.7 19.07 12.57 Z"),
            Tone::Gold),
       fill(QStringLiteral("M4.78 9.35 L20.23 11.52 L19.52 13.64 L4.87 11.58 Z"),
            Tone::Iron),
       line(QStringLiteral("M8.64 6.26 Q10.11 4.44 12.74 4.41"), Tone::Metal, 0.9F),
       fill(QStringLiteral(
                "M7.5 10.84 Q7.47 11.05 7.3 11.17 Q7.14 11.3 6.93 11.27 "
                "Q6.73 11.24 6.6 11.07 Q6.48 10.91 6.51 10.7 Q6.53 10.5 6.7 10.37 "
                "Q6.86 10.25 7.07 10.28 Q7.28 10.31 7.4 10.47 Q7.52 10.64 7.5 10.84 "
                "Z"),
            Tone::Ink),
       fill(QStringLiteral(
                "M12.84 11.59 Q12.81 11.8 12.65 11.92 Q12.48 12.05 12.28 12.02 "
                "Q12.07 11.99 11.95 11.83 Q11.82 11.66 11.85 11.45 "
                "Q11.88 11.25 12.05 11.13 Q12.21 11 12.42 11.03 "
                "Q12.62 11.06 12.75 11.22 Q12.87 11.39 12.84 11.59 Z"),
            Tone::Ink),
       fill(QStringLiteral("M18.19 12.35 Q18.16 12.55 18 12.68 Q17.83 12.8 17.63 12.77 "
                           "Q17.42 12.74 17.3 12.58 Q17.17 12.41 17.2 12.21 "
                           "Q17.23 12 17.39 11.88 Q17.56 11.75 17.76 11.78 "
                           "Q17.97 11.81 18.09 11.97 Q18.22 12.14 18.19 12.35 Z"),
            Tone::Ink),
       fill(QStringLiteral(
                "M14.21 4.01 Q13.02 1.02 9.37 1.11 Q5.49 1.37 4.17 5.02 "
                "Q3.75 6.58 4.86 7.34 Q6.35 3.92 9.97 4.02 Q11.98 4.1 12.88 4.83 Z"),
            Tone::Ember),
       fill(QStringLiteral(
                "M14.71 4.08 Q14.66 4.41 14.31 4.6 Q13.96 4.78 13.51 4.72 "
                "Q13.06 4.66 12.77 4.38 Q12.48 4.1 12.53 3.77 Q12.57 3.45 12.93 3.26 "
                "Q13.28 3.07 13.73 3.13 Q14.18 3.2 14.47 3.48 Q14.75 3.75 14.71 4.08 "
                "Z"),
            Tone::Iron),
       fill(QStringLiteral(
                "M16.9 17.4 Q16.9 18.1 16.4 18.6 Q15.9 19.1 15.2 19.1 "
                "Q14.5 19.1 14 18.6 Q13.5 18.1 13.5 17.4 Q13.5 16.7 14 16.2 "
                "Q14.5 15.7 15.2 15.7 Q15.9 15.7 16.4 16.2 Q16.9 16.7 16.9 17.4 Z"),
            Tone::Ember),
       fill(QStringLiteral("M15.1 16.8 Q15.1 17.01 14.95 17.15 Q14.81 17.3 14.6 17.3 "
                           "Q14.39 17.3 14.25 17.15 Q14.1 17.01 14.1 16.8 "
                           "Q14.1 16.59 14.25 16.45 Q14.39 16.3 14.6 16.3 "
                           "Q14.81 16.3 14.95 16.45 Q15.1 16.59 15.1 16.8 Z"),
            Tone::Metal),
       line(QStringLiteral("M17.8 1.8 L21 1.8 L17.8 4.8 L21 4.8"), Tone::Metal, 1.1F),
       line(QStringLiteral("M20.4 6.6 L22.2 6.6 L20.4 8.4 L22.2 8.4"),
            Tone::Metal,
            0.9F)});

  add("difficulty_normal",
      {fill(QStringLiteral(
                "M21.9 9.61 Q21.38 5.91 18.56 3.45 Q15.74 1 12 1 Q8.26 1 5.44 3.45 "
                "Q2.62 5.91 2.1 9.61 L7.05 10.3 Q7.31 8.45 8.72 7.23 Q10.13 6 12 6 "
                "Q13.87 6 15.28 7.23 Q16.69 8.45 16.95 10.3 Z"),
            Tone::Ember),
       line(QStringLiteral("M18.95 8.47 L20.74 7.82"), Tone::Ink, 0.5F),
       line(QStringLiteral("M18.13 6.86 L19.71 5.8"), Tone::Ink, 0.5F),
       line(QStringLiteral("M16.95 5.5 L18.22 4.09"), Tone::Ink, 0.5F),
       line(QStringLiteral("M15.47 4.47 L16.37 2.79"), Tone::Ink, 0.5F),
       line(QStringLiteral("M13.79 3.82 L14.25 1.98"), Tone::Ink, 0.5F),
       line(QStringLiteral("M12 3.6 L12 1.7"), Tone::Ink, 0.5F),
       line(QStringLiteral("M10.21 3.82 L9.75 1.98"), Tone::Ink, 0.5F),
       line(QStringLiteral("M8.53 4.47 L7.63 2.79"), Tone::Ink, 0.5F),
       line(QStringLiteral("M7.05 5.5 L5.78 4.09"), Tone::Ink, 0.5F),
       line(QStringLiteral("M5.87 6.86 L4.29 5.8"), Tone::Ink, 0.5F),
       line(QStringLiteral("M5.05 8.47 L3.26 7.82"), Tone::Ink, 0.5F),
       fill(QStringLiteral("M7 10.6 L17 10.6 Q17.4 18.4 12 20.4 Q6.6 18.4 7 10.6 Z"),
            Tone::Edge),
       fill(QStringLiteral("M4.2 11.8 Q4 3.4 12 3.2 Q20 3.4 19.8 11.8 Z"), Tone::Ink),
       fill(QStringLiteral("M5 11.2 Q4.8 4.2 12 4 Q19.2 4.2 19 11.2 Z"), Tone::Gold),
       line(QStringLiteral("M7.4 8 Q8.4 5.8 11 5.3"), Tone::Metal, 0.9F),
       fill(QStringLiteral("M4.2 10.2 L19.8 10.2 L19.8 11.8 L4.2 11.8 Z"), Tone::Iron),
       fill(QStringLiteral("M11.2 4.2 L12.8 4.2 L12.8 10.2 L11.2 10.2 Z"), Tone::Iron),
       fill(QStringLiteral("M4.8 11.4 L7.8 11.4 Q8.4 14.6 8 17.8 Q6.4 19.4 5.2 18 "
                           "Q4.4 14.6 4.8 11.4 Z"),
            Tone::Gold),
       fill(
           QStringLiteral("M19.2 11.4 L16.2 11.4 Q15.6 14.6 16 17.8 Q17.6 19.4 18.8 18 "
                          "Q19.6 14.6 19.2 11.4 Z"),
           Tone::Gold),
       fill(QStringLiteral(
                "M6.9 14.8 Q6.9 15.01 6.75 15.15 Q6.61 15.3 6.4 15.3 "
                "Q6.19 15.3 6.05 15.15 Q5.9 15.01 5.9 14.8 Q5.9 14.59 6.05 14.45 "
                "Q6.19 14.3 6.4 14.3 Q6.61 14.3 6.75 14.45 Q6.9 14.59 6.9 14.8 Z"),
            Tone::Ink),
       fill(QStringLiteral("M18.1 14.8 Q18.1 15.01 17.95 15.15 Q17.81 15.3 17.6 15.3 "
                           "Q17.39 15.3 17.25 15.15 Q17.1 15.01 17.1 14.8 "
                           "Q17.1 14.59 17.25 14.45 Q17.39 14.3 17.6 14.3 "
                           "Q17.81 14.3 17.95 14.45 Q18.1 14.59 18.1 14.8 Z"),
            Tone::Ink),
       line(QStringLiteral("M8.8 12.9 L11 13.1"), Tone::Ink, 1.1F),
       line(QStringLiteral("M13 12.6 Q14.2 11.9 15.4 12.5"), Tone::Ink, 1.1F),
       fill(QStringLiteral(
                "M10.75 14.6 Q10.75 14.89 10.53 15.09 Q10.31 15.3 10 15.3 "
                "Q9.69 15.3 9.47 15.09 Q9.25 14.89 9.25 14.6 Q9.25 14.31 9.47 14.11 "
                "Q9.69 13.9 10 13.9 Q10.31 13.9 10.53 14.11 Q10.75 14.31 10.75 14.6 "
                "Z"),
            Tone::Ink),
       fill(QStringLiteral(
                "M15 14.4 Q15 14.75 14.77 15 Q14.53 15.25 14.2 15.25 "
                "Q13.87 15.25 13.63 15 Q13.4 14.75 13.4 14.4 Q13.4 14.05 13.63 13.8 "
                "Q13.87 13.55 14.2 13.55 Q14.53 13.55 14.77 13.8 Q15 14.05 15 14.4 Z"),
            Tone::Ink),
       line(QStringLiteral("M10.2 17.6 Q12.4 18.4 14.4 16.9"), Tone::Ink, 1.0F)});

  add("difficulty_hard",
      {fill(QStringLiteral(
                "M6.6 7.6 Q3 6.8 2.2 3.4 Q2 2 2.8 1.4 Q3.4 4.2 6 4.8 Q7.4 5.2 8 6 Z"),
            Tone::Edge),
       fill(QStringLiteral("M17.4 7.6 Q21 6.8 21.8 3.4 Q22 2 21.2 1.4 Q20.6 4.2 18 4.8 "
                           "Q16.6 5.2 16 6 Z"),
            Tone::Edge),
       line(QStringLiteral("M3.4 4.6 L4.4 3.6"), Tone::Timber, 0.6F),
       line(QStringLiteral("M20.6 4.6 L19.6 3.6"), Tone::Timber, 0.6F),
       line(QStringLiteral("M4.8 5.8 L5.6 4.6"), Tone::Timber, 0.6F),
       line(QStringLiteral("M19.2 5.8 L18.4 4.6"), Tone::Timber, 0.6F),
       fill(QStringLiteral(
                "M5.2 12 Q4.6 4 12 3.6 Q19.4 4 18.8 12 Q19 17.2 17.2 21.2 L13.2 21.2 "
                "L13.2 17 L10.8 17 L10.8 21.2 L6.8 21.2 Q5 17.2 5.2 12 Z"),
            Tone::Iron),
       line(QStringLiteral("M7.8 7.4 Q9 5.2 11.6 4.8"), Tone::Metal, 0.9F),
       fill(QStringLiteral(
                "M6.4 11 L11.2 13 L11.2 17.4 L12.8 17.4 L12.8 13 L17.6 11 L17.4 13.8 "
                "L13.2 15 L10.8 15 L6.6 13.8 Z"),
            Tone::Ink),
       fill(QStringLiteral("M8 12.4 L10.4 13.4 L10.2 14.4 L8.2 13.8 Z"), Tone::Ember),
       fill(QStringLiteral("M16 12.4 L13.6 13.4 L13.8 14.4 L15.8 13.8 Z"), Tone::Ember),
       fill(QStringLiteral("M10.8 17 L13.2 17 L13.2 21.2 L10.8 21.2 Z"), Tone::Ink),
       line(QStringLiteral("M11.3 17.6 L11.3 20.6"), Tone::Metal, 0.45F),
       line(QStringLiteral("M12 17.6 L12 20.6"), Tone::Metal, 0.45F),
       line(QStringLiteral("M12.7 17.6 L12.7 20.6"), Tone::Metal, 0.45F),
       fill(QStringLiteral("M11.2 10.6 L12.8 10.6 L12.4 12.6 L11.6 12.6 Z"),
            Tone::Metal),
       fill(QStringLiteral(
                "M8.3 17.6 Q8.3 17.97 8.04 18.24 Q7.77 18.5 7.4 18.5 "
                "Q7.03 18.5 6.76 18.24 Q6.5 17.97 6.5 17.6 Q6.5 17.23 6.76 16.96 "
                "Q7.03 16.7 7.4 16.7 Q7.77 16.7 8.04 16.96 Q8.3 17.23 8.3 17.6 Z"),
            Tone::Gold),
       fill(QStringLiteral("M17.5 17.6 Q17.5 17.97 17.24 18.24 Q16.97 18.5 16.6 18.5 "
                           "Q16.23 18.5 15.96 18.24 Q15.7 17.97 15.7 17.6 "
                           "Q15.7 17.23 15.96 16.96 Q16.23 16.7 16.6 16.7 "
                           "Q16.97 16.7 17.24 16.96 Q17.5 17.23 17.5 17.6 Z"),
            Tone::Gold),
       fill(QStringLiteral("M11 3.8 Q12 1.2 13 3.8 Z"), Tone::Ember)});

  add("difficulty_very_hard",
      {fill(QStringLiteral("M6.8 9.6 Q5.2 14 4.6 17.6 Q4.2 20.4 4.8 22.4 L11.6 22.4 "
                           "Q10.2 19.6 10.6 16.6 Q11 13.8 13.8 11.8 Z"),
            Tone::Metal),
       fill(QStringLiteral(
                "M15.2 8.6 Q15.2 10.38 13.85 11.64 Q12.51 12.9 10.6 12.9 "
                "Q8.69 12.9 7.35 11.64 Q6 10.38 6 8.6 Q6 6.82 7.35 5.56 "
                "Q8.69 4.3 10.6 4.3 Q12.51 4.3 13.85 5.56 Q15.2 6.82 15.2 8.6 Z"),
            Tone::Metal),
       line(QStringLiteral("M7 15.4 Q6.4 13.6 7.8 11.8"), Tone::Edge, 0.8F),
       fill(QStringLiteral(
                "M4.4 17.2 Q7.8 15.8 11.4 16.2 L11 18.8 Q7.6 18.4 4.2 19.8 Z"),
            Tone::Iron),
       fill(QStringLiteral("M4.6 17.6 L2.2 17.4 L4.4 19 Z"), Tone::Iron),
       fill(QStringLiteral(
                "M6.45 17.8 Q6.45 17.99 6.32 18.12 Q6.19 18.25 6 18.25 "
                "Q5.81 18.25 5.68 18.12 Q5.55 17.99 5.55 17.8 Q5.55 17.61 5.68 17.48 "
                "Q5.81 17.35 6 17.35 Q6.19 17.35 6.32 17.48 Q6.45 17.61 6.45 17.8 Z"),
            Tone::Metal),
       fill(QStringLiteral(
                "M8.65 17.3 Q8.65 17.49 8.52 17.62 Q8.39 17.75 8.2 17.75 "
                "Q8.01 17.75 7.88 17.62 Q7.75 17.49 7.75 17.3 Q7.75 17.11 7.88 16.98 "
                "Q8.01 16.85 8.2 16.85 Q8.39 16.85 8.52 16.98 Q8.65 17.11 8.65 17.3 "
                "Z"),
            Tone::Metal),
       fill(QStringLiteral(
                "M10.65 17.3 Q10.65 17.49 10.52 17.62 Q10.39 17.75 10.2 17.75 "
                "Q10.01 17.75 9.88 17.62 Q9.75 17.49 9.75 17.3 "
                "Q9.75 17.11 9.88 16.98 Q10.01 16.85 10.2 16.85 "
                "Q10.39 16.85 10.52 16.98 Q10.65 17.11 10.65 17.3 Z"),
            Tone::Metal),
       fill(QStringLiteral("M14.2 9 L21 8.8 L20.4 11.6 L14.4 10.8 Z"), Tone::Ink),
       fill(QStringLiteral("M14.8 10 Q17.2 9 19.4 10.2 Q17.2 10.8 14.8 10.6 Z"),
            Tone::Ember),
       fill(QStringLiteral("M13.8 6.2 Q18.6 5 22.2 7.4 Q22.6 8.6 21.4 8.9 L14.4 9.3 Z"),
            Tone::Gold),
       fill(QStringLiteral(
                "M14.4 10.6 L20.6 11.4 Q20.9 12.4 19.8 12.7 Q16.4 12.8 13.8 11.8 Z"),
            Tone::Gold),
       fill(
           QStringLiteral("M15.6 9.2 L16.1 9.9 L16.6 9.2 L17.1 9.9 L17.6 9.2 L18.1 9.9 "
                          "L18.6 9.2 Z"),
           Tone::Metal),
       fill(QStringLiteral("M13.6 5.4 Q14.8 4.6 15.4 6.2 L14.4 7.4 Z"), Tone::Ink),
       fill(QStringLiteral("M6 8.6 Q5.8 3.6 10.6 3.2 Q14.6 3.2 15.4 6.6 L15.2 7.8 "
                           "Q10.8 6.8 6 8.6 Z"),
            Tone::Iron),
       fill(QStringLiteral("M6.1 5.7 Q6.6 3.5 3.2 1.8 Q7.4 4.1 8.3 5.1 Z"), Tone::Iron),
       fill(QStringLiteral("M8.5 4.4 Q9 2.4 7.4 0.9 Q9.8 3 10.7 3.8 Z"), Tone::Iron),
       fill(QStringLiteral("M11.1 4.1 Q11.6 2.2 11.8 0.8 Q12.4 2.8 13.3 3.5 Z"),
            Tone::Iron),
       line(QStringLiteral("M6.4 8.4 Q10.8 6.6 15 7.6"), Tone::Metal, 0.6F),
       fill(QStringLiteral(
                "M8.65 6 Q8.65 6.19 8.52 6.32 Q8.39 6.45 8.2 6.45 "
                "Q8.01 6.45 7.88 6.32 Q7.75 6.19 7.75 6 Q7.75 5.81 7.88 5.68 "
                "Q8.01 5.55 8.2 5.55 Q8.39 5.55 8.52 5.68 Q8.65 5.81 8.65 6 Z"),
            Tone::Ink),
       fill(QStringLiteral(
                "M11.45 5.2 Q11.45 5.39 11.32 5.52 Q11.19 5.65 11 5.65 "
                "Q10.81 5.65 10.68 5.52 Q10.55 5.39 10.55 5.2 Q10.55 5.01 10.68 4.88 "
                "Q10.81 4.75 11 4.75 Q11.19 4.75 11.32 4.88 Q11.45 5.01 11.45 5.2 Z"),
            Tone::Ink),
       fill(QStringLiteral("M11 8.6 L14.2 8.4 Q14 10.2 12.6 10.2 Q11.2 10 11 8.6 Z"),
            Tone::Ember),
       fill(QStringLiteral(
                "M13.45 9.1 Q13.45 9.29 13.32 9.42 Q13.19 9.55 13 9.55 "
                "Q12.81 9.55 12.68 9.42 Q12.55 9.29 12.55 9.1 Q12.55 8.91 12.68 8.78 "
                "Q12.81 8.65 13 8.65 Q13.19 8.65 13.32 8.78 Q13.45 8.91 13.45 9.1 Z"),
            Tone::Ink),
       line(QStringLiteral("M21.2 13.4 L22.6 15"), Tone::Ember, 1.0F),
       line(QStringLiteral("M19.4 14.2 L19.8 16.2"), Tone::Ember, 1.0F),
       line(QStringLiteral("M22.8 10.4 L22.8 10.4"), Tone::Ember, 1.0F)});

  return catalog;
}

auto catalog() -> const std::vector<Art>& {
  static const std::vector<Art> k_catalog = build_catalog();
  return k_catalog;
}

auto index() -> const QHash<QString, const Art*>& {
  static const QHash<QString, const Art*> k_index = [] {
    QHash<QString, const Art*> map;
    for (const Art& art : catalog()) {
      map.insert(art.id, &art);
    }
    return map;
  }();
  return k_index;
}

auto aliases() -> const QHash<QString, QString>& {
  static const QHash<QString, QString> k_aliases = {
      {QStringLiteral("build"), QStringLiteral("construct")},
      {QStringLiteral("defense"), QStringLiteral("guard")},
      {QStringLiteral("cut_tree"), QStringLiteral("chop_wood")},
      {QStringLiteral("collect_stone"), QStringLiteral("mine_stone")},
      {QStringLiteral("collect_iron_ore"), QStringLiteral("mine_iron")},
      {QStringLiteral("harvest_grain"), QStringLiteral("harvest_grain")},
      {QStringLiteral("repair_structure"), QStringLiteral("repair")},
      {QStringLiteral("dismantle_structure"), QStringLiteral("dismantle")},
      {QStringLiteral("unavailable"), QStringLiteral("blocked")},
  };
  return k_aliases;
}

struct PathCursor {
  QStringView text;
  int position{0};

  auto skip_separators() -> void {
    while (position < text.size() &&
           (text.at(position).isSpace() || text.at(position) == QLatin1Char(','))) {
      ++position;
    }
  }

  auto at_end() -> bool {
    skip_separators();
    return position >= text.size();
  }

  auto next_command() -> QChar {
    skip_separators();
    if (position >= text.size()) {
      return {};
    }
    return text.at(position++);
  }

  auto next_number(bool& ok) -> qreal {
    skip_separators();
    const int start = position;
    if (position < text.size() && (text.at(position) == QLatin1Char('-') ||
                                   text.at(position) == QLatin1Char('+'))) {
      ++position;
    }
    while (position < text.size() &&
           (text.at(position).isDigit() || text.at(position) == QLatin1Char('.'))) {
      ++position;
    }
    if (position == start) {
      ok = false;
      return 0.0;
    }
    return text.mid(start, position - start).toDouble(&ok);
  }
};

} // namespace

auto Palette::color_for(Tone tone) const -> QColor {
  switch (tone) {
  case Tone::Ink:
    return ink;
  case Tone::Metal:
    return metal;
  case Tone::Edge:
    return edge;
  case Tone::Ember:
    return ember;
  case Tone::Timber:
    return timber;
  case Tone::Stone:
    return stone;
  case Tone::Iron:
    return iron;
  case Tone::Gold:
    return gold;
  }
  return metal;
}

auto tone_id(Tone tone) -> QString {
  switch (tone) {
  case Tone::Ink:
    return QStringLiteral("ink");
  case Tone::Metal:
    return QStringLiteral("metal");
  case Tone::Edge:
    return QStringLiteral("edge");
  case Tone::Ember:
    return QStringLiteral("ember");
  case Tone::Timber:
    return QStringLiteral("timber");
  case Tone::Stone:
    return QStringLiteral("stone");
  case Tone::Iron:
    return QStringLiteral("iron");
  case Tone::Gold:
    return QStringLiteral("gold");
  }
  return QStringLiteral("metal");
}

auto tone_from_id(const QString& id) -> Tone {
  if (id == QStringLiteral("ink")) {
    return Tone::Ink;
  }
  if (id == QStringLiteral("edge")) {
    return Tone::Edge;
  }
  if (id == QStringLiteral("ember")) {
    return Tone::Ember;
  }
  if (id == QStringLiteral("timber")) {
    return Tone::Timber;
  }
  if (id == QStringLiteral("stone")) {
    return Tone::Stone;
  }
  if (id == QStringLiteral("iron")) {
    return Tone::Iron;
  }
  if (id == QStringLiteral("gold")) {
    return Tone::Gold;
  }
  return Tone::Metal;
}

auto resolve_id(const QString& id) -> QString {
  const auto alias = aliases().constFind(id);
  return alias != aliases().constEnd() ? alias.value() : id;
}

auto find(const QString& id) -> const Art* {
  const auto resolved = resolve_id(id);
  const auto match = index().constFind(resolved);
  return match != index().constEnd() ? match.value() : nullptr;
}

auto ids() -> QStringList {
  QStringList result;
  result.reserve(static_cast<int>(catalog().size()));
  for (const Art& art : catalog()) {
    result.append(art.id);
  }
  return result;
}

auto build_path(const QString& path_data,
                qreal scale,
                qreal offset_x,
                qreal offset_y) -> QPainterPath {
  QPainterPath path;
  PathCursor cursor{QStringView(path_data), 0};
  const auto point = [&](qreal x, qreal y) {
    return QPointF(offset_x + x * scale, offset_y + y * scale);
  };

  while (!cursor.at_end()) {
    const QChar command = cursor.next_command();
    bool ok = true;
    if (command == QLatin1Char('M') || command == QLatin1Char('L')) {
      const qreal x = cursor.next_number(ok);
      const qreal y = cursor.next_number(ok);
      if (!ok) {
        break;
      }
      if (command == QLatin1Char('M')) {
        path.moveTo(point(x, y));
      } else {
        path.lineTo(point(x, y));
      }
    } else if (command == QLatin1Char('Q')) {
      const qreal cx = cursor.next_number(ok);
      const qreal cy = cursor.next_number(ok);
      const qreal x = cursor.next_number(ok);
      const qreal y = cursor.next_number(ok);
      if (!ok) {
        break;
      }
      path.quadTo(point(cx, cy), point(x, y));
    } else if (command == QLatin1Char('Z') || command == QLatin1Char('z')) {
      path.closeSubpath();
    } else {
      break;
    }
  }
  return path;
}

auto default_palette() -> Palette {
  Palette palette;
  palette.ink = QColor(QStringLiteral("#140f0a"));
  palette.metal = QColor(QStringLiteral("#d7cbb2"));
  palette.edge = QColor(QStringLiteral("#fff1cf"));
  palette.ember = QColor(QStringLiteral("#e0a542"));
  palette.timber = QColor(QStringLiteral("#8a5a32"));
  palette.stone = QColor(QStringLiteral("#8e8b86"));
  palette.iron = QColor(QStringLiteral("#7f8a96"));
  palette.gold = QColor(QStringLiteral("#d9a441"));
  return palette;
}

void paint(QPainter& painter,
           const QString& id,
           const QRectF& rect,
           const Palette& palette,
           qreal opacity) {
  const Art* art = find(id);
  if (art == nullptr || rect.isEmpty()) {
    return;
  }

  const qreal size = std::min(rect.width(), rect.height());
  const qreal scale = size / static_cast<qreal>(k_design_grid);
  const qreal offset_x = rect.x() + (rect.width() - size) * 0.5;
  const qreal offset_y = rect.y() + (rect.height() - size) * 0.5;

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setOpacity(painter.opacity() * opacity);
  for (const Stroke& stroke : art->strokes) {
    const QPainterPath path = build_path(stroke.path, scale, offset_x, offset_y);
    const QColor color = palette.color_for(stroke.tone);
    if (stroke.filled) {
      painter.setPen(Qt::NoPen);
      painter.fillPath(path, color);
    } else {
      QPen pen(color);
      pen.setWidthF(std::max(1.0, static_cast<qreal>(stroke.width) * scale));
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);
    }
  }
  painter.restore();
}

} // namespace Ui::IconArt

IconArtLibrary::IconArtLibrary(QObject* parent)
    : QObject(parent) {
}

bool IconArtLibrary::has(const QString& id) {
  return Ui::IconArt::find(id) != nullptr;
}

QStringList IconArtLibrary::ids() {
  return Ui::IconArt::ids();
}

QString IconArtLibrary::resolve(const QString& id) {
  return Ui::IconArt::resolve_id(id);
}

QVariantList IconArtLibrary::strokes(const QString& id) {
  QVariantList result;
  const Ui::IconArt::Art* art = Ui::IconArt::find(id);
  if (art == nullptr) {
    return result;
  }

  constexpr qreal k_flatten_scale = 256.0;
  for (const Ui::IconArt::Stroke& stroke : art->strokes) {
    const QPainterPath path = Ui::IconArt::build_path(
        stroke.path, k_flatten_scale / Ui::IconArt::k_design_grid, 0.0, 0.0);
    QVariantList subpaths;
    for (const QPolygonF& polygon : path.toSubpathPolygons()) {
      QVariantList points;
      points.reserve(polygon.size() * 2);
      for (const QPointF& vertex : polygon) {
        points.append(vertex.x() / k_flatten_scale);
        points.append(vertex.y() / k_flatten_scale);
      }
      if (points.size() >= 4) {
        subpaths.append(QVariant(points));
      }
    }
    if (subpaths.isEmpty()) {
      continue;
    }

    QVariantMap entry;
    entry[QStringLiteral("tone")] = Ui::IconArt::tone_id(stroke.tone);
    entry[QStringLiteral("filled")] = stroke.filled;
    entry[QStringLiteral("width")] = static_cast<double>(stroke.width) /
                                     static_cast<double>(Ui::IconArt::k_design_grid);
    entry[QStringLiteral("subpaths")] = subpaths;
    result.append(entry);
  }
  return result;
}

auto IconArtLibrary::create(QQmlEngine* engine,
                            QJSEngine* script_engine) -> IconArtLibrary* {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)

  static IconArtLibrary library;
  QQmlEngine::setObjectOwnership(&library, QQmlEngine::CppOwnership);
  return &library;
}
