#pragma once

#include <QString>
#include <QVariantMap>

namespace App::Economy {

[[nodiscard]] auto unit_profile(const QString& unit_type,
                                const QString& nation_id) -> QVariantMap;

} // namespace App::Economy
