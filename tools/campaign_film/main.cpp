#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QProcess>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QThread>

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>

#include "campaign_film_overlay.h"
#include "campaign_film_spec.h"
#include "ui/brand_fonts.h"
#include "ui/campaign_map_film.h"
#include "ui/campaign_map_view.h"

namespace {

using namespace CampaignFilm;

auto log_line(const QString& text) -> void {
  std::fprintf(stderr, "[campaign_film] %s\n", qUtf8Printable(text));
  std::fflush(stderr);
}

class Encoder {
public:
  auto open(const QString& path, int width, int height, int fps, const QString& codec)
      -> bool {
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
      log_line(QStringLiteral("ffmpeg not found; no clip will be written"));
      return false;
    }
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                     QStringLiteral("error"),        QStringLiteral("-y"),
                     QStringLiteral("-f"),           QStringLiteral("rawvideo"),
                     QStringLiteral("-pixel_format"), QStringLiteral("rgba"),
                     QStringLiteral("-video_size"),
                     QStringLiteral("%1x%2").arg(width).arg(height),
                     QStringLiteral("-framerate"),   QString::number(fps),
                     QStringLiteral("-i"),           QStringLiteral("-"),
                     QStringLiteral("-an")};
    if (codec == QStringLiteral("prores")) {
      args << QStringLiteral("-c:v") << QStringLiteral("prores_ks")
           << QStringLiteral("-profile:v") << QStringLiteral("3")
           << QStringLiteral("-pix_fmt") << QStringLiteral("yuv422p10le");
    } else {
      args << QStringLiteral("-c:v") << QStringLiteral("libx264")
           << QStringLiteral("-preset") << QStringLiteral("slow") << QStringLiteral("-crf")
           << QStringLiteral("10") << QStringLiteral("-pix_fmt")
           << QStringLiteral("yuv420p") << QStringLiteral("-movflags")
           << QStringLiteral("+faststart");
    }
    args << path;
    m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_process.start(ffmpeg, args);
    m_open = m_process.waitForStarted(10000);
    return m_open;
  }

  auto write(const QImage& frame) -> bool {
    if (!m_open) {
      return false;
    }
    const QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
    const qint64 bytes = static_cast<qint64>(rgba.sizeInBytes());
    if (m_process.write(reinterpret_cast<const char*>(rgba.constBits()), bytes) != bytes) {
      return false;
    }
    while (m_process.bytesToWrite() > bytes * 3) {
      if (!m_process.waitForBytesWritten(30000)) {
        return false;
      }
    }
    return true;
  }

  auto close() -> bool {
    if (!m_open) {
      return false;
    }
    m_process.closeWriteChannel();
    m_process.waitForFinished(-1);
    m_open = false;
    return m_process.exitStatus() == QProcess::NormalExit && m_process.exitCode() == 0;
  }

private:
  QProcess m_process;
  bool m_open = false;
};

auto read_cpu_temperature() -> std::optional<int> {
  const QDir hwmon(QStringLiteral("/sys/class/hwmon"));
  for (const QString& entry : hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QFile name(hwmon.filePath(entry + QStringLiteral("/name")));
    if (!name.open(QIODevice::ReadOnly) ||
        name.readAll().trimmed() != QByteArrayLiteral("coretemp")) {
      continue;
    }
    QFile temp(hwmon.filePath(entry + QStringLiteral("/temp1_input")));
    if (temp.open(QIODevice::ReadOnly)) {
      bool ok = false;
      const int milli = temp.readAll().trimmed().toInt(&ok);
      if (ok) {
        return milli / 1000;
      }
    }
  }
  return std::nullopt;
}

auto read_gpu_temperature() -> std::optional<int> {
  const QString smi = QStandardPaths::findExecutable(QStringLiteral("nvidia-smi"));
  if (smi.isEmpty()) {
    return std::nullopt;
  }
  QProcess process;
  process.start(smi, {QStringLiteral("--query-gpu=temperature.gpu"),
                      QStringLiteral("--format=csv,noheader,nounits")});
  if (!process.waitForFinished(3000)) {
    return std::nullopt;
  }
  bool ok = false;
  const int value = process.readAllStandardOutput().trimmed().split('\n').value(0).toInt(&ok);
  return ok ? std::optional<int>(value) : std::nullopt;
}

void thermal_guard(int frame) {
  const auto cpu = read_cpu_temperature();
  const auto gpu = read_gpu_temperature();
  const bool hot = (cpu && *cpu > 88) || (gpu && *gpu > 83);
  if (!hot) {
    return;
  }
  log_line(QStringLiteral("frame %1: CPU %2 C / GPU %3 C, pausing to cool")
               .arg(frame)
               .arg(cpu.value_or(-1))
               .arg(gpu.value_or(-1)));
  for (;;) {
    QThread::sleep(5);
    const auto c = read_cpu_temperature();
    const auto g = read_gpu_temperature();
    if ((!c || *c < 78) && (!g || *g < 75)) {
      log_line(QStringLiteral("cooled to CPU %1 C / GPU %2 C, resuming")
                   .arg(c.value_or(-1))
                   .arg(g.value_or(-1)));
      return;
    }
  }
}

auto write_json(const QString& path, const QJsonObject& object) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    log_line(QStringLiteral("cannot write %1").arg(path));
    return false;
  }
  file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
  return true;
}

auto round3(double value) -> double { return std::round(value * 1000.0) / 1000.0; }

auto values_json(const QMap<QString, double>& values) -> QJsonObject {
  QJsonObject object;
  for (auto it = values.begin(); it != values.end(); ++it) {
    object.insert(it.key(), it.value());
  }
  return object;
}

auto build_timeline_json(const Spec& spec,
                         const March& march,
                         const Timeline& timeline,
                         const QString& clip_name,
                         int first_frame,
                         int end_frame) -> QJsonObject {
  QJsonObject root;
  root.insert(QStringLiteral("id"), spec.id);
  root.insert(QStringLiteral("title"), spec.title);
  root.insert(QStringLiteral("width"), spec.width);
  root.insert(QStringLiteral("height"), spec.height);
  root.insert(QStringLiteral("fps"), spec.fps);
  root.insert(QStringLiteral("supersample"), spec.supersample);
  root.insert(QStringLiteral("duration"), spec.duration);
  root.insert(QStringLiteral("frames"), spec.frame_count());
  root.insert(QStringLiteral("first_frame"), first_frame);
  root.insert(QStringLiteral("end_frame"), end_frame);
  root.insert(QStringLiteral("clip"), clip_name);
  root.insert(QStringLiteral("burn_text"), spec.burn_text);

  QJsonArray events;
  if (spec.route.enabled) {
    QJsonObject route;
    const auto [from, to] = timeline.route_window();
    route.insert(QStringLiteral("from"), spec.route.from);
    route.insert(QStringLiteral("to"), spec.route.to);
    route.insert(QStringLiteral("progress_from"), round3(from));
    route.insert(QStringLiteral("progress_to"), round3(to));
    QJsonArray stops;
    for (const auto& stop : march.stops) {
      const float progress = march.path.progress_at_raw_index(stop.index);
      if (progress < from - 1e-5F || progress > to + 1e-5F) {
        continue;
      }
      QJsonObject entry;
      entry.insert(QStringLiteral("id"), stop.id);
      entry.insert(QStringLiteral("name"), stop.name);
      entry.insert(QStringLiteral("date"), stop.date);
      entry.insert(QStringLiteral("kind"), stop.kind);
      entry.insert(QStringLiteral("progress"), round3(progress));
      entry.insert(QStringLiteral("local_progress"),
                   round3((progress - from) / std::max(1e-6F, to - from)));
      const auto arrival = timeline.arrival_time(progress);
      if (arrival) {
        entry.insert(QStringLiteral("arrival"), round3(*arrival));
        QJsonObject event;
        event.insert(QStringLiteral("time"), round3(*arrival));
        event.insert(QStringLiteral("type"), QStringLiteral("arrival"));
        event.insert(QStringLiteral("stop"), stop.id);
        event.insert(QStringLiteral("name"), stop.name);
        event.insert(QStringLiteral("date"), stop.date);
        events.push_back(event);
      }
      stops.push_back(entry);
    }
    route.insert(QStringLiteral("stops"), stops);
    root.insert(QStringLiteral("route"), route);
  }

  auto window_event = [&events](const QString& type, const QString& name, const Window& w,
                                float in) {
    QJsonObject event;
    event.insert(QStringLiteral("time"), round3(in));
    event.insert(QStringLiteral("type"), type + QStringLiteral("_in"));
    event.insert(QStringLiteral("name"), name);
    events.push_back(event);
    if (!std::isinf(w.out)) {
      QJsonObject out_event;
      out_event.insert(QStringLiteral("time"), round3(w.out));
      out_event.insert(QStringLiteral("type"), type + QStringLiteral("_out"));
      out_event.insert(QStringLiteral("name"), name);
      events.push_back(out_event);
    }
  };
  for (const auto& marker : spec.markers) {
    Window w = marker.window;
    w.in = timeline.marker_appear_time(marker);
    if (!std::isinf(marker.hold)) {
      w.out = w.in + marker.hold;
    }
    if (!std::isinf(w.in)) {
      window_event(QStringLiteral("marker"), marker.site, w, w.in);
    }
  }
  for (const auto& label : spec.labels) {
    window_event(QStringLiteral("label"), label.text, label.window, label.window.in);
  }
  for (const auto& stamp : spec.stamps) {
    window_event(QStringLiteral("stamp"), stamp.title, stamp.window, stamp.window.in);
  }
  for (const auto& region : spec.regions) {
    window_event(QStringLiteral("region"), region.id, region.window, region.window.in);
  }

  QJsonArray armies;
  for (const auto& army : spec.armies) {
    QJsonObject entry;
    entry.insert(QStringLiteral("id"), army.id);
    entry.insert(QStringLiteral("label"), army.label);
    entry.insert(QStringLiteral("side"), army.side);
    entry.insert(QStringLiteral("source"), army.source);
    entry.insert(QStringLiteral("interp"),
                 army.linear ? QStringLiteral("linear") : QStringLiteral("step"));
    QJsonArray keys;
    for (const auto& key : army.keys) {
      QJsonObject key_entry;
      const auto at = timeline.army_key_time(key);
      key_entry.insert(QStringLiteral("time"), at ? QJsonValue(round3(*at)) : QJsonValue());
      if (!key.at.isEmpty()) {
        key_entry.insert(QStringLiteral("at"), key.at);
      }
      key_entry.insert(QStringLiteral("values"), values_json(key.values));
      key_entry.insert(QStringLiteral("text"), format_army_line(key.values));
      keys.push_back(key_entry);
      if (at) {
        QJsonObject event;
        event.insert(QStringLiteral("time"), round3(*at));
        event.insert(QStringLiteral("type"), QStringLiteral("army"));
        event.insert(QStringLiteral("name"), army.id);
        event.insert(QStringLiteral("text"), format_army_line(key.values));
        events.push_back(event);
      }
    }
    entry.insert(QStringLiteral("keys"), keys);
    armies.push_back(entry);
  }
  root.insert(QStringLiteral("armies"), armies);

  QJsonArray camera;
  for (const auto& key : spec.camera) {
    QJsonObject entry;
    entry.insert(QStringLiteral("time"), key.time);
    entry.insert(QStringLiteral("distance"), key.distance);
    entry.insert(QStringLiteral("yaw"), key.yaw);
    entry.insert(QStringLiteral("pitch"), key.pitch);
    entry.insert(QStringLiteral("fov"), key.fov);
    camera.push_back(entry);
  }
  root.insert(QStringLiteral("camera_keys"), camera);

  QVariantList sorted = events.toVariantList();
  std::stable_sort(sorted.begin(), sorted.end(), [](const QVariant& a, const QVariant& b) {
    return a.toMap().value(QStringLiteral("time")).toDouble() <
           b.toMap().value(QStringLiteral("time")).toDouble();
  });
  root.insert(QStringLiteral("events"), QJsonArray::fromVariantList(sorted));
  QJsonArray warnings;
  for (const QString& warning : timeline.warnings()) {
    warnings.push_back(warning);
  }
  root.insert(QStringLiteral("warnings"), warnings);
  return root;
}

auto pick_family(const QStringList& registered,
                 const QStringList& preference,
                 const QString& fallback) -> QString {
  for (const QString& candidate : preference) {
    if (registered.contains(candidate)) {
      return candidate;
    }
  }
  return fallback;
}

} // namespace

auto main(int argc, char** argv) -> int {
  qputenv("QT_OPENGL", "desktop");
  qputenv("QSG_RHI_BACKEND", "opengl");
  qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("campaign_map_film"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Films the campaign map from a camera/route spec "
                     "(docs/CAMPAIGN_MAP_FILM.md)."));
  parser.addHelpOption();
  const QCommandLineOption spec_option(QStringLiteral("spec"),
                                       QStringLiteral("Film spec JSON."),
                                       QStringLiteral("file"));
  const QCommandLineOption out_option(QStringLiteral("out"),
                                      QStringLiteral("Output root directory."),
                                      QStringLiteral("dir"),
                                      QStringLiteral("artifacts/campaign_film"));
  const QCommandLineOption width_option(QStringLiteral("width"),
                                        QStringLiteral("Override output width."),
                                        QStringLiteral("px"));
  const QCommandLineOption height_option(QStringLiteral("height"),
                                         QStringLiteral("Override output height."),
                                         QStringLiteral("px"));
  const QCommandLineOption fps_option(QStringLiteral("fps"),
                                      QStringLiteral("Override frame rate."),
                                      QStringLiteral("n"));
  const QCommandLineOption ss_option(QStringLiteral("supersample"),
                                     QStringLiteral("Override supersampling factor."),
                                     QStringLiteral("n"));
  const QCommandLineOption start_option(QStringLiteral("start"),
                                        QStringLiteral("First second to render."),
                                        QStringLiteral("s"));
  const QCommandLineOption end_option(QStringLiteral("end"),
                                      QStringLiteral("Last second to render."),
                                      QStringLiteral("s"));
  const QCommandLineOption stills_option(
      QStringLiteral("stills"),
      QStringLiteral("Comma-separated times; writes PNG stills instead of a clip."),
      QStringLiteral("t1,t2"));
  const QCommandLineOption frames_option(QStringLiteral("frames"),
                                         QStringLiteral("Also write every frame as PNG."));
  const QCommandLineOption no_clip_option(QStringLiteral("no-clip"),
                                          QStringLiteral("Do not encode a clip."));
  const QCommandLineOption no_text_option(
      QStringLiteral("no-text"),
      QStringLiteral("Draw markers without names, labels or stamps."));
  const QCommandLineOption armies_option(
      QStringLiteral("draw-armies"),
      QStringLiteral("Debug: draw the exported army figures at their anchors."));
  const QCommandLineOption codec_option(QStringLiteral("codec"),
                                        QStringLiteral("h264 (default) or prores."),
                                        QStringLiteral("name"),
                                        QStringLiteral("h264"));
  const QCommandLineOption validate_option(
      QStringLiteral("validate-only"),
      QStringLiteral("Parse the spec, write timeline.json and exit without rendering."));
  const QCommandLineOption march_option(QStringLiteral("march"),
                                        QStringLiteral("hannibal_path.json to read."),
                                        QStringLiteral("file"),
                                        QStringLiteral("assets/campaign_map/hannibal_path.json"));
  const QCommandLineOption catalog_option(QStringLiteral("catalog"),
                                          QStringLiteral("Region catalogue JSON."),
                                          QStringLiteral("file"),
                                          QStringLiteral("tools/campaign_film/regions.json"));
  const QCommandLineOption bounds_option(QStringLiteral("bounds"),
                                         QStringLiteral("Map bounds JSON."),
                                         QStringLiteral("file"),
                                         QStringLiteral("tools/map_pipeline/map_bounds.json"));
  const QCommandLineOption no_guard_option(QStringLiteral("no-thermal-guard"),
                                           QStringLiteral("Do not pause on hot CPU/GPU."));
  parser.addOptions({spec_option, out_option, width_option, height_option, fps_option,
                     ss_option, start_option, end_option, stills_option, frames_option,
                     no_clip_option, no_text_option, armies_option, codec_option,
                     validate_option, march_option, catalog_option, bounds_option,
                     no_guard_option});
  parser.process(app);

  if (!parser.isSet(spec_option)) {
    log_line(QStringLiteral("--spec is required"));
    return 2;
  }

  QString error;
  MapBounds bounds;
  if (const auto object = load_json_object(parser.value(bounds_option), &error)) {
    bounds.lon_min = object->value(QStringLiteral("lon_min")).toDouble(bounds.lon_min);
    bounds.lon_max = object->value(QStringLiteral("lon_max")).toDouble(bounds.lon_max);
    bounds.lat_min = object->value(QStringLiteral("lat_min")).toDouble(bounds.lat_min);
    bounds.lat_max = object->value(QStringLiteral("lat_max")).toDouble(bounds.lat_max);
  } else {
    log_line(error);
    return 2;
  }
  const auto march_object = load_json_object(parser.value(march_option), &error);
  if (!march_object) {
    log_line(error);
    return 2;
  }
  const auto march = parse_march(*march_object, &error);
  if (!march) {
    log_line(error);
    return 2;
  }
  std::optional<Catalog> catalog;
  if (QFileInfo::exists(parser.value(catalog_option))) {
    const auto catalog_object = load_json_object(parser.value(catalog_option), &error);
    if (catalog_object) {
      catalog = parse_catalog(*catalog_object, bounds, &error);
    }
    if (!catalog) {
      log_line(error);
      return 2;
    }
  }
  const auto spec_object = load_json_object(parser.value(spec_option), &error);
  if (!spec_object) {
    log_line(error);
    return 2;
  }
  LoadContext context;
  context.march = &*march;
  context.catalog = catalog ? &*catalog : nullptr;
  context.bounds = bounds;
  auto parsed = parse_spec(*spec_object, context, &error);
  if (!parsed) {
    log_line(QStringLiteral("spec: %1").arg(error));
    return 2;
  }
  Spec spec = *parsed;
  if (parser.isSet(width_option)) {
    spec.width = parser.value(width_option).toInt();
  }
  if (parser.isSet(height_option)) {
    spec.height = parser.value(height_option).toInt();
  }
  if (parser.isSet(fps_option)) {
    spec.fps = std::max(1, parser.value(fps_option).toInt());
  }
  if (parser.isSet(ss_option)) {
    spec.supersample = std::clamp(parser.value(ss_option).toInt(), 1, 4);
  }
  const bool draw_text = spec.burn_text && !parser.isSet(no_text_option);

  const Timeline timeline(spec, *march);
  for (const QString& warning : timeline.warnings()) {
    log_line(QStringLiteral("warning: %1").arg(warning));
  }

  const int total_frames = spec.frame_count();
  int first_frame = 0;
  int end_frame = total_frames;
  if (parser.isSet(start_option)) {
    first_frame = std::clamp(
        static_cast<int>(std::lround(parser.value(start_option).toDouble() * spec.fps)), 0,
        total_frames - 1);
  }
  if (parser.isSet(end_option)) {
    end_frame = std::clamp(
        static_cast<int>(std::lround(parser.value(end_option).toDouble() * spec.fps)),
        first_frame + 1, total_frames);
  }
  std::vector<float> still_times;
  if (parser.isSet(stills_option)) {
    for (const QString& part :
         parser.value(stills_option).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      still_times.push_back(part.toFloat());
    }
  }

  const QString out_dir = QDir(parser.value(out_option)).filePath(spec.id);
  QDir().mkpath(out_dir);
  const QString clip_name =
      spec.id + (parser.value(codec_option) == QStringLiteral("prores")
                     ? QStringLiteral(".mov")
                     : QStringLiteral(".mp4"));
  write_json(QDir(out_dir).filePath(QStringLiteral("timeline.json")),
             build_timeline_json(spec, *march, timeline, clip_name, first_frame, end_frame));
  log_line(QStringLiteral("%1: %2x%3 @ %4 fps, supersample %5, %6 s (%7 frames), out %8")
               .arg(spec.id)
               .arg(spec.width)
               .arg(spec.height)
               .arg(spec.fps)
               .arg(spec.supersample)
               .arg(spec.duration)
               .arg(total_frames)
               .arg(out_dir));
  if (parser.isSet(validate_option)) {
    return 0;
  }

  QOpenGLContext gl_context;
  gl_context.setFormat(format);
  if (!gl_context.create()) {
    log_line(QStringLiteral("cannot create an OpenGL context (needs a display, e.g. "
                            "DISPLAY=:0)"));
    return 3;
  }
  QOffscreenSurface surface;
  surface.setFormat(gl_context.format());
  surface.create();
  if (!gl_context.makeCurrent(&surface)) {
    log_line(QStringLiteral("cannot make the OpenGL context current"));
    return 3;
  }
  const QString renderer_name = QString::fromLatin1(
      reinterpret_cast<const char*>(gl_context.functions()->glGetString(GL_RENDERER)));
  log_line(QStringLiteral("rendering on %1").arg(renderer_name));
  if (renderer_name.contains(QStringLiteral("llvmpipe"), Qt::CaseInsensitive) &&
      !qEnvironmentVariableIsSet("SOI_PROMO_ALLOW_LOW_QUALITY")) {
    log_line(QStringLiteral("refusing to film on a software renderer "
                            "(set SOI_PROMO_ALLOW_LOW_QUALITY for tests)"));
    return 3;
  }

  const QSize internal(spec.width * spec.supersample, spec.height * spec.supersample);
  QQuickRenderControl control;
  QQuickWindow window(&control);
  window.setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(&gl_context));
  window.setColor(Qt::black);
  if (!control.initialize()) {
    log_line(QStringLiteral("QQuickRenderControl failed to initialise"));
    return 3;
  }
  QOpenGLFramebufferObject target(internal, QOpenGLFramebufferObject::CombinedDepthStencil);
  if (!target.isValid()) {
    log_line(QStringLiteral("cannot allocate a %1x%2 render target")
                 .arg(internal.width())
                 .arg(internal.height()));
    return 3;
  }
  window.setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(target.texture(), internal));
  window.setGeometry(0, 0, internal.width(), internal.height());
  window.contentItem()->setSize(internal);

  auto* view = new CampaignMapView();
  view->setParentItem(window.contentItem());
  view->setSize(internal);
  view->set_film_route(march->points);

  CampaignMapFilm::TerrainHeightField heights;
  heights.load_default();

  const QStringList registered = Ui::BrandFonts::register_bundled();
  OverlayFonts fonts;
  fonts.display = pick_family(registered, {QStringLiteral("Standard Iron Display")},
                              QStringLiteral("serif"));
  fonts.text = pick_family(registered, {QStringLiteral("EB Garamond")},
                           QStringLiteral("serif"));

  const float pixel_scale = static_cast<float>(internal.height()) / spec.reference_height;
  OverlayOptions overlay_options;
  overlay_options.pixel_scale = pixel_scale;
  overlay_options.draw_text = draw_text;
  overlay_options.draw_armies = parser.isSet(armies_option);

  auto render_frame = [&](float time, FrameEval* eval_out) -> QImage {
    const FrameEval eval = timeline.evaluate(time);
    view->set_film_state(timeline.frame_state(eval));
    gl_context.makeCurrent(&surface);
    control.polishItems();
    control.beginFrame();
    control.sync();
    control.render();
    control.endFrame();
    QImage image = target.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);

    CampaignMapFilm::CameraPose pose = eval.camera;
    pose.target_height = heights.height_at(pose.target) * spec.terrain_height_scale;
    const QMatrix4x4 mvp = CampaignMapFilm::view_projection(
        static_cast<float>(internal.width()), static_cast<float>(internal.height()), pose);
    const Projector project = [&](const QVector2D& uv) -> std::optional<QPointF> {
      const auto projected = CampaignMapFilm::project(
          mvp,
          CampaignMapFilm::world_point(uv, heights.height_at(uv) * spec.terrain_height_scale),
          static_cast<float>(internal.width()), static_cast<float>(internal.height()));
      if (!projected.in_front) {
        return std::nullopt;
      }
      return projected.pixel;
    };
    paint_overlay(image, spec, eval, project, fonts, overlay_options);
    if (spec.supersample > 1) {
      image = image.scaled(spec.width, spec.height, Qt::IgnoreAspectRatio,
                           Qt::SmoothTransformation);
    }
    if (eval_out != nullptr) {
      *eval_out = eval;
    }
    return image;
  };

  for (int warm = 0; warm < 3; ++warm) {
    (void)render_frame(first_frame / static_cast<float>(spec.fps), nullptr);
  }

  const bool guard = !parser.isSet(no_guard_option);
  QElapsedTimer timer;
  timer.start();

  if (!still_times.empty()) {
    const QString still_dir = QDir(out_dir).filePath(QStringLiteral("stills"));
    QDir().mkpath(still_dir);
    for (const float t : still_times) {
      const QImage image = render_frame(t, nullptr);
      const QString path =
          QDir(still_dir).filePath(QStringLiteral("still_%1.png").arg(t, 6, 'f', 2, QLatin1Char('0')));
      image.save(path);
      log_line(QStringLiteral("still %1 s -> %2").arg(t).arg(path));
    }
    delete view;
    return 0;
  }

  const bool keep_frames = parser.isSet(frames_option);
  const QString frame_dir = QDir(out_dir).filePath(QStringLiteral("frames"));
  if (keep_frames) {
    QDir().mkpath(frame_dir);
  }
  Encoder encoder;
  bool encoding = false;
  if (!parser.isSet(no_clip_option)) {
    encoding = encoder.open(QDir(out_dir).filePath(clip_name), spec.width, spec.height,
                            spec.fps, parser.value(codec_option));
  }

  QJsonArray frames_json;
  for (int frame = first_frame; frame < end_frame; ++frame) {
    if (guard && (frame - first_frame) % 30 == 0) {
      thermal_guard(frame);
    }
    const float time = static_cast<float>(frame) / static_cast<float>(spec.fps);
    FrameEval eval;
    const QImage image = render_frame(time, &eval);
    if (encoding && !encoder.write(image)) {
      log_line(QStringLiteral("ffmpeg stopped accepting frames"));
      encoding = false;
    }
    if (keep_frames) {
      image.save(QDir(frame_dir).filePath(QStringLiteral("frame_%1.png").arg(frame, 6, 10,
                                                                              QLatin1Char('0'))));
    }

    CampaignMapFilm::CameraPose pose = eval.camera;
    pose.target_height = heights.height_at(pose.target) * spec.terrain_height_scale;
    const QMatrix4x4 mvp = CampaignMapFilm::view_projection(
        static_cast<float>(spec.width), static_cast<float>(spec.height), pose);
    auto point_json = [&](const QVector2D& uv) {
      QJsonObject point;
      const auto projected = CampaignMapFilm::project(
          mvp,
          CampaignMapFilm::world_point(uv, heights.height_at(uv) * spec.terrain_height_scale),
          static_cast<float>(spec.width), static_cast<float>(spec.height));
      point.insert(QStringLiteral("x"), round3(projected.pixel.x()));
      point.insert(QStringLiteral("y"), round3(projected.pixel.y()));
      point.insert(QStringLiteral("on_screen"),
                   projected.in_front && projected.pixel.x() >= 0 &&
                       projected.pixel.y() >= 0 && projected.pixel.x() <= spec.width &&
                       projected.pixel.y() <= spec.height);
      return point;
    };
    QJsonObject entry;
    entry.insert(QStringLiteral("frame"), frame);
    entry.insert(QStringLiteral("time"), round3(time));
    QJsonObject head = point_json(eval.head_uv);
    head.insert(QStringLiteral("progress"), round3(eval.route_head));
    entry.insert(QStringLiteral("head"), head);
    QJsonArray markers;
    for (const auto& value : eval.markers) {
      QJsonObject m = point_json(spec.markers[value.index].uv);
      m.insert(QStringLiteral("site"), spec.markers[value.index].site);
      m.insert(QStringLiteral("alpha"), round3(value.alpha));
      markers.push_back(m);
    }
    entry.insert(QStringLiteral("markers"), markers);
    QJsonArray labels;
    for (const auto& value : eval.labels) {
      QJsonObject l = point_json(value.uv);
      l.insert(QStringLiteral("text"), spec.labels[value.index].text);
      l.insert(QStringLiteral("alpha"), round3(value.alpha));
      labels.push_back(l);
    }
    entry.insert(QStringLiteral("labels"), labels);
    QJsonArray stamps;
    for (const auto& value : eval.stamps) {
      QJsonObject st;
      st.insert(QStringLiteral("title"), spec.stamps[value.index].title);
      st.insert(QStringLiteral("subtitle"), spec.stamps[value.index].subtitle);
      st.insert(QStringLiteral("alpha"), round3(value.alpha));
      stamps.push_back(st);
    }
    entry.insert(QStringLiteral("stamps"), stamps);
    QJsonArray armies;
    for (std::size_t i = 0; i < eval.armies.size(); ++i) {
      const auto& value = eval.armies[i];
      QJsonObject a = point_json(value.uv);
      a.insert(QStringLiteral("id"), value.id);
      a.insert(QStringLiteral("label"), spec.armies[i].label);
      a.insert(QStringLiteral("alpha"), round3(value.alpha));
      a.insert(QStringLiteral("values"), values_json(value.values));
      a.insert(QStringLiteral("text"), format_army_line(value.values));
      armies.push_back(a);
    }
    entry.insert(QStringLiteral("armies"), armies);
    frames_json.push_back(entry);

    if ((frame - first_frame) % 30 == 0) {
      const double elapsed = timer.elapsed() / 1000.0;
      log_line(QStringLiteral("frame %1/%2 (%3 s elapsed)")
                   .arg(frame + 1)
                   .arg(end_frame)
                   .arg(elapsed, 0, 'f', 1));
    }
  }

  QJsonObject overlays;
  overlays.insert(QStringLiteral("id"), spec.id);
  overlays.insert(QStringLiteral("width"), spec.width);
  overlays.insert(QStringLiteral("height"), spec.height);
  overlays.insert(QStringLiteral("fps"), spec.fps);
  overlays.insert(QStringLiteral("coordinates"),
                  QStringLiteral("output pixels, origin top-left"));
  overlays.insert(QStringLiteral("frames"), frames_json);
  write_json(QDir(out_dir).filePath(QStringLiteral("overlays.json")), overlays);

  if (encoding) {
    if (encoder.close()) {
      log_line(QStringLiteral("clip -> %1").arg(QDir(out_dir).filePath(clip_name)));
    } else {
      log_line(QStringLiteral("ffmpeg failed to finish the clip"));
      delete view;
      return 4;
    }
  }
  log_line(QStringLiteral("done: %1 frames in %2 s")
               .arg(end_frame - first_frame)
               .arg(timer.elapsed() / 1000.0, 0, 'f', 1));
  delete view;
  return 0;
}
