#pragma once

#include <QCoreApplication>
#include <QString>

namespace App::Core {

inline constexpr char k_application_id[] = "standard_of_iron";

inline void apply_application_identity() {
  QCoreApplication::setOrganizationName(QString());
  QCoreApplication::setApplicationName(QString::fromLatin1(k_application_id));
}

} // namespace App::Core
