#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QString>

#include <gtest/gtest.h>

#include "systems/save_format.h"
#include "systems/save_storage.h"

using namespace Game::Systems;

namespace {

auto decode(const QByteArray& png) -> QImage {
  QImage image;
  image.loadFromData(png, "PNG");
  return image;
}

} // namespace

TEST(SavePreviewTest, EncodePreviewDownscalesAndStaysDecodable) {
  QImage frame(1920, 1080, QImage::Format_RGBA8888);
  frame.fill(QColor(12, 34, 56));

  const QByteArray png = Save::encode_preview(frame);
  ASSERT_FALSE(png.isEmpty());

  const QImage decoded = decode(png);
  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.width(), Save::k_preview_width);
  EXPECT_EQ(decoded.height(), 180);
  EXPECT_EQ(decoded.pixelColor(decoded.width() / 2, decoded.height() / 2),
            QColor(12, 34, 56));

  EXPECT_LT(png.size(), 256 * 1024);
}

TEST(SavePreviewTest, SmallFrameIsNotUpscaled) {
  QImage frame(64, 48, QImage::Format_RGBA8888);
  frame.fill(Qt::red);

  const QImage decoded = decode(Save::encode_preview(frame));
  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(decoded.width(), 64);
  EXPECT_EQ(decoded.height(), 48);
}

TEST(SavePreviewTest, NullFrameEncodesToNothing) {
  EXPECT_TRUE(Save::encode_preview(QImage()).isEmpty());
  EXPECT_TRUE(Save::encode_preview(QImage(8, 8, QImage::Format_RGBA8888), 0).isEmpty());
}

TEST(SavePreviewTest, FramebufferReadbackIsStoredAndReadBack) {
  QOffscreenSurface surface;
  surface.create();
  if (!surface.isValid()) {
    GTEST_SKIP() << "No offscreen surface available";
  }

  QOpenGLContext context;
  if (!context.create() || !context.makeCurrent(&surface)) {
    GTEST_SKIP() << "No OpenGL context available in this environment";
  }

  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::Depth);
  QOpenGLFramebufferObject fbo(QSize(640, 360), format);
  ASSERT_TRUE(fbo.isValid());
  ASSERT_TRUE(fbo.bind());

  auto* functions = context.functions();
  functions->glViewport(0, 0, 640, 360);
  functions->glClearColor(0.25F, 0.5F, 0.75F, 1.0F);
  functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  functions->glFinish();

  const QImage frame = fbo.toImage();
  ASSERT_TRUE(fbo.release());
  ASSERT_FALSE(frame.isNull());
  EXPECT_EQ(frame.size(), QSize(640, 360));

  const QByteArray png = Save::encode_preview(frame);
  ASSERT_FALSE(png.isEmpty());

  SaveStorage storage(":memory:");
  QString error;
  ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();

  Save::Record record;
  record.slot_name = QStringLiteral("preview_slot");
  record.title = QStringLiteral("Preview Slot");
  record.map_name = QStringLiteral("Test Map");
  record.map_path = QStringLiteral("assets/maps/test.json");
  record.mode = QStringLiteral("skirmish");
  record.difficulty = QStringLiteral("normal");
  record.world = Save::pack(QByteArray("{\"entities\":[]}"));
  ASSERT_TRUE(storage.write_slot(record, &error)) << error.toStdString();
  ASSERT_TRUE(storage.update_screenshot("preview_slot", png, &error))
      << error.toStdString();

  Save::Record loaded;
  ASSERT_TRUE(storage.read_slot("preview_slot", loaded, &error)) << error.toStdString();
  EXPECT_EQ(loaded.screenshot, png);

  const QImage decoded = decode(loaded.screenshot);
  ASSERT_FALSE(decoded.isNull());
  const QColor centre = decoded.pixelColor(decoded.width() / 2, decoded.height() / 2);
  EXPECT_NEAR(centre.red(), 64, 3);
  EXPECT_NEAR(centre.green(), 128, 3);
  EXPECT_NEAR(centre.blue(), 191, 3);

  context.doneCurrent();
}
