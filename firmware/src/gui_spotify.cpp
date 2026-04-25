#include "gui_spotify.h"
#include <esp_heap_caps.h>

// Le pointeur TFT est stocké globalement *seulement pour le callback LVGL*
static TFT_eSPI* global_tft_ptr = nullptr;
static lv_indev_drv_t indev_drv; // Driver pour le tactile
static lv_indev_t* indev_touchpad;

// Callback de dessin matériel pour LVGL v8
static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (global_tft_ptr) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        global_tft_ptr->startWrite();
        global_tft_ptr->setAddrWindow(area->x1, area->y1, w, h);
        global_tft_ptr->pushColors((uint16_t *)&color_p->full, w * h, true);
        global_tft_ptr->endWrite();
    }
    lv_disp_flush_ready(disp_drv);
}

// Callback de lecture tactile pour LVGL v8
static void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    uint16_t touchX = 0, touchY = 0;
    bool touched = false;

    if (global_tft_ptr) {
        touched = global_tft_ptr->getTouch(&touchX, &touchY);
        if (touched) {
            Serial.printf("Tactile capté : X=%d, Y=%d\n", touchX, touchY);
        }
    }

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        // Inversion ou mapping manuel si nécessaire
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

void GuiSpotify::init(TFT_eSPI* tft) {
    _tft = tft;
    global_tft_ptr = tft;

    // Configuration de la calibration tactile par défaut (peut nécessiter un ajustement)
    uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
    _tft->setTouch(calData);

    // Allocation dynamique d'un buffer de 1/10 d'écran (assez pour un ST7789 sur ESP32)
    buf1 = (lv_color_t*)heap_caps_malloc(screenWidth * 24 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    draw_buf = (lv_disp_draw_buf_t*)malloc(sizeof(lv_disp_draw_buf_t));
    disp_drv = (lv_disp_drv_t*)malloc(sizeof(lv_disp_drv_t));

    lv_disp_draw_buf_init(draw_buf, buf1, NULL, screenWidth * 24);

    lv_disp_drv_init(disp_drv);
    disp_drv->hor_res = screenWidth;
    disp_drv->ver_res = screenHeight;
    disp_drv->flush_cb = my_disp_flush;
    disp_drv->draw_buf = draw_buf;
    disp = lv_disp_drv_register(disp_drv);

    // Initialisation du pilote tactile
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

static void play_event_cb(lv_event_t * e) {
    GuiSpotify * ui = (GuiSpotify *)lv_event_get_user_data(e);
    if (ui) {
        ui->isPlayBtnClicked = true;
    }
}

void GuiSpotify::buildInterface() {
    // Style de base - Thème Sombre
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0); // Couleur Spotify Dark

    // Titre de la chanson
    title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "Chargement...");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 40);

    // Artiste
    artist_label = lv_label_create(scr);
    lv_label_set_text(artist_label, "Artiste Inconnu");
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0xB3B3B3), 0); // Gris clair
    lv_obj_align(artist_label, LV_ALIGN_TOP_MID, 0, 70);

    // Barre de progression
    progress_bar = lv_bar_create(scr);
    lv_obj_set_size(progress_bar, 260, 6);
    lv_obj_align(progress_bar, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR); // Vert Spotify
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x535353), LV_PART_MAIN);

    // Temps
    time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align_to(time_label, progress_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Bouton Play/Pause (On utilise un simple label texte pour simuler une icône ici)
    play_btn = lv_btn_create(scr);
    lv_obj_set_size(play_btn, 60, 60);
    lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_white(), 0);
    lv_obj_align(play_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(play_btn, play_event_cb, LV_EVENT_CLICKED, this);

    play_btn_label = lv_label_create(play_btn);
    lv_label_set_text(play_btn_label, "||"); // "||" pour pause, ">" pour play
    lv_obj_set_style_text_color(play_btn_label, lv_color_black(), 0);
    lv_obj_center(play_btn_label);
}

void GuiSpotify::updateTitle(const char* title, const char* artist) {
    if(title_label) lv_label_set_text(title_label, title);
    if(artist_label) lv_label_set_text(artist_label, artist);
}

void GuiSpotify::updateProgress(uint32_t current_time, uint32_t total_time) {
    if(total_time > 0 && progress_bar) {
        int pct = (current_time * 100) / total_time;
        lv_bar_set_value(progress_bar, pct, LV_ANIM_OFF);

        char time_str[32];
        sprintf(time_str, "%lu:%02lu / %lu:%02lu",
            current_time/60, current_time%60,
            total_time/60, total_time%60);
        lv_label_set_text(time_label, time_str);
    }
}

void GuiSpotify::togglePlayPauseIcon(bool isPlaying) {
    if(play_btn_label) {
        lv_label_set_text(play_btn_label, isPlaying ? "||" : ">");
    }
}

void GuiSpotify::deinit() {
    // Nettoyage critique pour libérer la RAM avant le retour à l'IA
    // Il faut nettoyer les objets AVANT de supprimer le display
    lv_obj_clean(lv_scr_act());

    if(disp) {
        lv_disp_remove(disp);
    }
    if (buf1) free(buf1);
    if (draw_buf) free(draw_buf);
    if (disp_drv) free(disp_drv);

    global_tft_ptr = nullptr;
}
