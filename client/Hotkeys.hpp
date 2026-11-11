#pragma once
// System-wide keyboard shortcuts (Windows: RegisterHotKey). They work with any window focused,
// including while the client is hidden in the tray.
//
// Ctrl+Alt+H  next noise mode (cancelling -> ambient -> off -> ...)
// Ctrl+Alt+J  show / hide the window
//
// Keys were picked so they don't collide with AltGr (= Ctrl+Alt) characters of the
// US-International layout used for pt-BR typing (e.g. AltGr+, = c-cedilla, AltGr+N = n-tilde).
struct SDL_Window;

struct HotkeysState
{
    bool nextMode{};
    bool toggleWindow{};
};

// Main thread, after the window exists. Returns which shortcuts were registered
// (a shortcut already taken by another app fails individually).
extern HotkeysState hotkeysInit(SDL_Window* window);
extern void hotkeysDestroy();
