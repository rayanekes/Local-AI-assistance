#include "ui_spotify.h"
#include <Arduino.h>

// Global pointers to UI elements
static lv_obj_t* ui_spotify_screen = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* play_btn = nullptr;
static lv_obj_t* play_btn_label = nullptr;
static lv_obj_t* slider = nullptr;

static bool current_playing_state = false;

// Event callbacks for controls
static void play_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        current_playing_state = !current_playing_state;
        ui_spotify_set_playing(current_playing_state);
        // Normally, this would also signal the audio player to pause/resume
    }
}

void ui_spotify_init(void) {
    if (ui_spotify_screen != nullptr) {
        // UI already initialized
        return;
    }

    // Create a new screen
    ui_spotify_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_spotify_screen, lv_color_hex(0x121212), LV_PART_MAIN); // Spotify dark background
    lv_obj_set_style_bg_opa(ui_spotify_screen, 255, LV_PART_MAIN);

    // Album art placeholder
    lv_obj_t* album_art = lv_obj_create(ui_spotify_screen);
    lv_obj_set_size(album_art, 150, 150);
    lv_obj_align(album_art, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(album_art, lv_color_hex(0x282828), LV_PART_MAIN); // Dark grey placeholder
    lv_obj_set_style_border_width(album_art, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(album_art, 10, LV_PART_MAIN);

    // Track title label
    title_label = lv_label_create(ui_spotify_screen);
    lv_label_set_text(title_label, "Unknown Title");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, LV_PART_MAIN); // Assuming montserrat 16 is available
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 190);

    // Progress slider
    slider = lv_slider_create(ui_spotify_screen);
    lv_obj_set_size(slider, 200, 5);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 220);
    lv_slider_set_value(slider, 30, LV_ANIM_OFF); // Demo progress
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x535353), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1DB954), LV_PART_INDICATOR); // Spotify green
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB); // Hide knob slightly

    // Play/Pause button
    play_btn = lv_btn_create(ui_spotify_screen);
    lv_obj_set_size(play_btn, 60, 60);
    lv_obj_align(play_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x1DB954), LV_PART_MAIN);
    lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_ALL, NULL);

    play_btn_label = lv_label_create(play_btn);
    lv_label_set_text(play_btn_label, LV_SYMBOL_PLAY);
    lv_obj_center(play_btn_label);

    // Prev button
    lv_obj_t* prev_btn = lv_btn_create(ui_spotify_screen);
    lv_obj_set_size(prev_btn, 40, 40);
    lv_obj_align_to(prev_btn, play_btn, LV_ALIGN_OUT_LEFT_MID, -20, 0);
    lv_obj_set_style_radius(prev_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x121212), LV_PART_MAIN); // Match bg
    lv_obj_set_style_border_width(prev_btn, 0, LV_PART_MAIN);
    lv_obj_t* prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(prev_label);

    // Next button
    lv_obj_t* next_btn = lv_btn_create(ui_spotify_screen);
    lv_obj_set_size(next_btn, 40, 40);
    lv_obj_align_to(next_btn, play_btn, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_set_style_radius(next_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x121212), LV_PART_MAIN);
    lv_obj_set_style_border_width(next_btn, 0, LV_PART_MAIN);
    lv_obj_t* next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(next_label);

    // Load the screen
    lv_scr_load(ui_spotify_screen);
}

void ui_spotify_set_title(const char* title) {
    if (title_label != nullptr && title != nullptr) {
        lv_label_set_text(title_label, title);
    }
}

void ui_spotify_set_playing(bool is_playing) {
    current_playing_state = is_playing;
    if (play_btn_label != nullptr) {
        if (is_playing) {
            lv_label_set_text(play_btn_label, LV_SYMBOL_PAUSE);
        } else {
            lv_label_set_text(play_btn_label, LV_SYMBOL_PLAY);
        }
    }
}

void ui_spotify_teardown(void) {
    if (ui_spotify_screen != nullptr) {
        // Switch back to a basic default screen before deleting
        lv_obj_t* default_scr = lv_obj_create(NULL);
        lv_scr_load(default_scr);

        // Delete the spotify screen and all its children to free RAM
        lv_obj_del(ui_spotify_screen);

        // Reset pointers to prevent dangling references
        ui_spotify_screen = nullptr;
        title_label = nullptr;
        play_btn = nullptr;
        play_btn_label = nullptr;
        slider = nullptr;
    }
}
