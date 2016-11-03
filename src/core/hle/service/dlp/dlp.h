// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

namespace Service {

class Interface;

namespace DLP {

void IsChild(Interface* self);

/// Initializes the DLP services.
void Init();

/// Shuts down the DLP services.
void Shutdown();

} // namespace DLP
} // namespace Service
