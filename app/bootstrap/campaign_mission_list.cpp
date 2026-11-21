#include "app/bootstrap/campaign_mission_list.h"

#include <QCoreApplication>
#include <QTextStream>

#include <string_view>

#include "game/mission/campaign_mission_index.h"

namespace App::Bootstrap {

auto campaign_mission_list_requested_from_argv(int argc, char* argv[]) -> bool {
  for (int index = 1; index < argc; ++index) {
    if (argv[index] != nullptr &&
        std::string_view(argv[index]) == "--list-campaign-missions") {
      return true;
    }
  }
  return false;
}

auto print_campaign_mission_list(int argc, char* argv[]) -> int {
  QCoreApplication const app(argc, argv);

  // Only the references go to stdout so scripts can read it line by line;
  // the loaders' own log lines and every problem go to stderr.
  const auto index = Game::Mission::load_campaign_mission_index();
  QTextStream out(stdout);
  for (const QString& reference : index.references) {
    out << reference << '\n';
  }
  out.flush();

  if (index.ok()) {
    return 0;
  }
  QTextStream err(stderr);
  for (const QString& problem : index.problems) {
    err << "--list-campaign-missions: " << problem << '\n';
  }
  err << "--list-campaign-missions: campaign data could not be loaded completely\n";
  err.flush();
  return 1;
}

} // namespace App::Bootstrap
