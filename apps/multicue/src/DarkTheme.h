#pragma once

namespace mc::app {

// Applies the application's default dark palette. Called once at startup,
// before any window is constructed, per the project brief's "dark theme by
// default" requirement.
void applyDarkTheme();

} // namespace mc::app
