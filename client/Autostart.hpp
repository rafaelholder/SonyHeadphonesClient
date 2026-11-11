#pragma once
// "Start with Windows": a value in HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// pointing to this executable with --tray. Per-user, no admin rights, removable from
// Task Manager > Startup apps. Off until the user ticks it in the tray menu.

extern bool autostartSupported();
// True only when the Run value exists AND points to the running executable.
extern bool autostartIsEnabled();
extern bool autostartSetEnabled(bool enabled);
