#include "app/bootstrap/windows_gl_probe.h"

#ifdef Q_OS_WIN

#include <QByteArray>
#include <QDebug>
#include <QString>

#include <cstdio>
#include <cstring>
#include <gl/gl.h>
#include <string_view>
#include <windows.h>

#include "render/gl/context_requirements.h"
#pragma comment(lib, "opengl32.lib")

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif

#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif

#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif

#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC hDC,
                                                         HGLRC hShareContext,
                                                         const int* attribList);

namespace App::Bootstrap {

namespace {

constexpr int k_required_gl_major = Render::GL::ContextRequirements::required.major;
constexpr int k_required_gl_minor = Render::GL::ContextRequirements::required.minor;

auto parse_opengl_version(const char* version, int* major, int* minor) -> bool {
  return version != nullptr && major != nullptr && minor != nullptr &&
         std::sscanf(version, "%d.%d", major, minor) == 2;
}

void capture_current_gl_info(NativeOpenGLProbeResult& result) {
  const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

  result.vendor =
      vendor != nullptr ? QString::fromLatin1(vendor) : QStringLiteral("<unknown>");
  result.renderer =
      renderer != nullptr ? QString::fromLatin1(renderer) : QStringLiteral("<unknown>");
  result.version =
      version != nullptr ? QString::fromLatin1(version) : QStringLiteral("<unknown>");
  result.major = 0;
  result.minor = 0;
  if (version != nullptr) {
    (void)parse_opengl_version(version, &result.major, &result.minor);
  }
}

auto opengl_version_supported(int major, int minor) -> bool {
  return major > k_required_gl_major ||
         (major == k_required_gl_major && minor >= k_required_gl_minor);
}

bool g_opengl_crashed = false;
LONG WINAPI crash_handler(EXCEPTION_POINTERS* exceptionInfo) {
  if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {

    char crash_log_path[MAX_PATH + 32] = {};
    const DWORD temp_length = GetTempPathA(MAX_PATH, crash_log_path);
    if (temp_length == 0 || temp_length > MAX_PATH) {
      crash_log_path[0] = '\0';
    }
    strcat_s(
        crash_log_path, sizeof(crash_log_path), "standard_of_iron_opengl_crash.txt");
    FILE* crash_log = nullptr;
    if (fopen_s(&crash_log, crash_log_path, "w") != 0) {
      crash_log = nullptr;
    }
    if (crash_log) {
      fprintf(crash_log, "OpenGL/Qt rendering crash detected (Access Violation)\n");
      fprintf(crash_log, "Try running with: run_debug_softwaregl.cmd\n");
      fprintf(crash_log, "Or set environment variable: QT_OPENGL=software\n");
      fclose(crash_log);
    }

    qCritical() << "=== CRASH DETECTED ===";
    qCritical() << "OpenGL rendering failed. This usually means:";
    qCritical() << "1. Graphics drivers are outdated";
    qCritical() << "2. Running in a VM with incomplete OpenGL support";
    qCritical() << "3. GPU doesn't support required OpenGL version";
    qCritical() << "";
    qCritical() << "To fix: Run run_debug_softwaregl.cmd instead";
    qCritical() << "Or set: set QT_OPENGL=software";

    g_opengl_crashed = true;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

auto software_requested_from_argv(int argc, char* argv[]) -> bool {
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg =
        argv[index] != nullptr ? std::string_view(argv[index]) : std::string_view();
    if (arg == "-s" || arg == "--force-software" || arg == "--quality=none" ||
        arg == "--quality=software") {
      return true;
    }
    if (arg == "--quality" && index + 1 < argc) {
      const std::string_view value = argv[index + 1] != nullptr
                                         ? std::string_view(argv[index + 1])
                                         : std::string_view();
      if (value == "none" || value == "software") {
        return true;
      }
    }
  }
  return false;
}

auto test_native_opengl() -> NativeOpenGLProbeResult {
  NativeOpenGLProbeResult result;

  WNDCLASSA wc = {};
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = "OpenGLTest";

  if (!RegisterClassA(&wc)) {
    return result;
  }

  HWND hwnd = CreateWindowExA(0,
                              "OpenGLTest",
                              "",
                              WS_OVERLAPPEDWINDOW,
                              0,
                              0,
                              1,
                              1,
                              nullptr,
                              nullptr,
                              wc.hInstance,
                              nullptr);
  if (!hwnd) {
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return result;
  }

  HDC hdc = GetDC(hwnd);
  if (!hdc) {
    DestroyWindow(hwnd);
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return result;
  }

  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int pixel_format = ChoosePixelFormat(hdc, &pfd);
  if (pixel_format != 0 && SetPixelFormat(hdc, pixel_format, &pfd)) {
    PIXELFORMATDESCRIPTOR chosen_pfd = {};
    if (DescribePixelFormat(hdc, pixel_format, sizeof(chosen_pfd), &chosen_pfd) != 0) {
      result.generic_software = (chosen_pfd.dwFlags & PFD_GENERIC_FORMAT) != 0 &&
                                (chosen_pfd.dwFlags & PFD_GENERIC_ACCELERATED) == 0;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc) {
      if (wglMakeCurrent(hdc, hglrc)) {
        capture_current_gl_info(result);

        auto* create_core_context = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));
        if (create_core_context != nullptr) {
          constexpr std::array probe_versions{
              Render::GL::ContextRequirements::preferred,
              Render::GL::ContextRequirements::Version{4, 4},
              Render::GL::ContextRequirements::Version{4, 3},
              Render::GL::ContextRequirements::apple_maximum,
              Render::GL::ContextRequirements::required,
          };
          for (const auto candidate : probe_versions) {
            const int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                                   candidate.major,
                                   WGL_CONTEXT_MINOR_VERSION_ARB,
                                   candidate.minor,
                                   WGL_CONTEXT_PROFILE_MASK_ARB,
                                   WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                   0};
            HGLRC core_ctx = create_core_context(hdc, nullptr, attribs);
            if (core_ctx == nullptr) {
              continue;
            }
            wglMakeCurrent(nullptr, nullptr);
            if (wglMakeCurrent(hdc, core_ctx)) {
              result.used_core_context = true;
              result.requested_major = candidate.major;
              result.requested_minor = candidate.minor;
              capture_current_gl_info(result);
            }
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(core_ctx);
            (void)wglMakeCurrent(hdc, hglrc);
            if (result.used_core_context) {
              break;
            }
          }
        }

        QByteArray vendor_bytes = result.vendor.toLocal8Bit();
        QByteArray renderer_bytes = result.renderer.toLocal8Bit();
        QByteArray version_bytes = result.version.toLocal8Bit();
        fprintf(stderr, "[OpenGL Test] Native context created successfully\n");
        fprintf(stderr, "[OpenGL Test] Vendor: %s\n", vendor_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Renderer: %s\n", renderer_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Version: %s\n", version_bytes.constData());
        if (result.used_core_context) {
          fprintf(stderr,
                  "[OpenGL Test] Probe context: %d.%d core\n",
                  result.requested_major,
                  result.requested_minor);
        } else {
          fprintf(stderr, "[OpenGL Test] Probe context: legacy\n");
        }
        if (result.generic_software) {
          fprintf(stderr, "[OpenGL Test] Pixel format is generic software rendering\n");
        }

        const bool microsoft_gdi =
            result.vendor.contains("Microsoft", Qt::CaseInsensitive) ||
            result.renderer.contains("GDI Generic", Qt::CaseInsensitive);
        const bool version_ok = result.used_core_context &&
                                opengl_version_supported(result.major, result.minor);
        result.supported = version_ok && !result.generic_software && !microsoft_gdi;
        if (!version_ok) {
          fprintf(stderr,
                  "[OpenGL Test] Rejected: requires OpenGL %d.%d Core, found %d.%d "
                  "%s\n",
                  k_required_gl_major,
                  k_required_gl_minor,
                  result.major,
                  result.minor,
                  result.used_core_context ? "Core" : "without a Core profile");
        }
        if (microsoft_gdi) {
          fprintf(
              stderr,
              "[OpenGL Test] Rejected: Microsoft GDI generic renderer is not usable "
              "for the 3D renderer\n");
        }

        wglMakeCurrent(nullptr, nullptr);
      }
      wglDeleteContext(hglrc);
    }
  }

  ReleaseDC(hwnd, hdc);
  DestroyWindow(hwnd);
  UnregisterClassA("OpenGLTest", wc.hInstance);

  return result;
}

void install_opengl_crash_handler() {
  SetUnhandledExceptionFilter(crash_handler);
}

auto opengl_crash_detected() -> bool {
  return g_opengl_crashed;
}

} // namespace App::Bootstrap

#endif
