#ifndef UI_SPOTIFY_H
#define UI_SPOTIFY_H

#include <lvgl.h>

// Initialize the Spotify-style UI
void ui_spotify_init(void);

// Update the track title
void ui_spotify_set_title(const char* title);

// Update the playback state (true = playing, false = paused)
void ui_spotify_set_playing(bool is_playing);

// Unload and completely deallocate the UI from RAM
void ui_spotify_teardown(void);

#endif // UI_SPOTIFY_H
