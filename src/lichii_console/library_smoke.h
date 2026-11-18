// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

namespace Horizon {
class ConsoleLibrary;
int RunLibraryChecks(ConsoleLibrary& library, const QString& temp_path);
} // namespace Horizon
