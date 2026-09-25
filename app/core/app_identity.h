#pragma once

#include <QCoreApplication>
#include <QString>

namespace App::Core {

// QStandardPaths::AppDataLocation is built from this name, so it decides the
// directory every player's saves live in:
//
//   Windows  %APPDATA%/standard_of_iron/saves
//   Linux    ~/.local/share/standard_of_iron/saves
//   macOS    ~/Library/Application Support/standard_of_iron/saves
//
// Before it was pinned, Qt inferred the name: the executable's file name, or
// CFBundleName inside a macOS bundle. Renaming the binary, or giving the bundle
// a display name, would have silently moved every save. The value is the
// executable name the game already shipped with, so existing saves stay put.
// Steam Auto-Cloud is configured against these exact paths
// (steam/README.md), so changing the name strands every cloud save.
//
// The organisation name stays empty on purpose: Qt inserts it as an extra
// directory level between the data root and the application name.
inline constexpr char k_application_id[] = "standard_of_iron";

inline void apply_application_identity() {
  QCoreApplication::setOrganizationName(QString());
  QCoreApplication::setApplicationName(QString::fromLatin1(k_application_id));
}

} // namespace App::Core
