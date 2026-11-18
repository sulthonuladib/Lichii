// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariantList>

class QtConfig;

namespace Horizon {

QVariantList ReadEmulatorSettings(const QString& category, const QString& program_id);
bool WriteEmulatorSetting(QtConfig& config, const QString& key, const QVariant& value,
                          const QString& program_id);
bool RemoveEmulatorOverrides(const QString& program_id);
qulonglong ParseProgramId(const QString& program_id);

} // namespace Horizon
