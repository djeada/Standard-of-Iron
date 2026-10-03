#include "app/bootstrap/data_paths.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTextStream>

#include <string_view>

#include "app/core/app_identity.h"
#include "app/core/user_settings.h"
#include "game/systems/persistence/save_load_service.h"

namespace App::Bootstrap {

auto data_paths_requested_from_argv(int argc, char* argv[]) -> bool {
  for (int index = 1; index < argc; ++index) {
    if (argv[index] != nullptr &&
        std::string_view(argv[index]) == "--print-data-paths") {
      return true;
    }
  }
  return false;
}

auto print_data_paths(int argc, char* argv[]) -> int {
  QCoreApplication const app(argc, argv);
  App::Core::apply_application_identity();

  QSettings const settings = App::Core::UserSettings::open();
  QTextStream out(stdout);
  out << "SOI_APPLICATION_ID=" << QCoreApplication::applicationName() << '\n'
      << "SOI_SAVES_DIR=" << Game::Systems::SaveLoadService::saves_directory() << '\n'
      << "SOI_SAVE_DATABASE=" << Game::Systems::SaveLoadService::database_path() << '\n'
      << "SOI_EXPORTS_DIR=" << Game::Systems::SaveLoadService::exports_directory()
      << '\n'
      << "SOI_SETTINGS_FILE=" << settings.fileName() << '\n';
  out.flush();
  return 0;
}

} // namespace App::Bootstrap
