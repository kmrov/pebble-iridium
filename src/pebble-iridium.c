#include <pebble.h>

static void second_handler(struct tm *tick_time, TimeUnits units_changed);
static void minute_handler(struct tm *tick_time, TimeUnits units_changed);

#define KEY_NUM 0
#define KEY_IRIDIUM_TIME 1
#define KEY_IRIDIUM_BRIGHTNESS 2
#define KEY_IRIDIUM_AZIMUTH 3

static Window *s_main_window;

static TextLayer *s_time_layer;

static TextLayer *s_countdown_layer;

static TextLayer *s_iridium_layers[4];

struct flare {
    time_t time;
    char brightness[8];
    char azimuth[8];

    char text[32];
};

struct flare flares[4];
bool flares_present = false;

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    static char time_buffer[] = "00:00:00";
    static char brightness_buffer[] = "-0.0";
    static char azimuth_buffer[] = "000000";

    time_t iridium_time = 0;

    int num = 0;

    Tuple *t = dict_read_first(iterator);

    while(t != NULL) {
        switch(t->key) {
            case KEY_NUM:
                num = (int) t->value->int32;
            break;
            case KEY_IRIDIUM_TIME:
                iridium_time = t->value->int32;
                strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", localtime(&iridium_time));
            break;
            case KEY_IRIDIUM_BRIGHTNESS:
                snprintf(brightness_buffer, sizeof(brightness_buffer), "%s", t->value->cstring);
            break;
            case KEY_IRIDIUM_AZIMUTH:
                snprintf(azimuth_buffer, sizeof(azimuth_buffer), "%s\u00B0", t->value->cstring);
            break;
            default:
                APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
            break;
        }
        t = dict_read_next(iterator);
    }

    flares[num].time = iridium_time;
    strncpy(flares[num].brightness, brightness_buffer, sizeof(brightness_buffer));
    strncpy(flares[num].azimuth, azimuth_buffer, sizeof(azimuth_buffer));

    snprintf(flares[num].text, sizeof(flares[num].text), "%s (%s) %s", time_buffer, brightness_buffer, azimuth_buffer);

    flares_present = true;

    text_layer_set_text(s_iridium_layers[num], flares[num].text);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! %d", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void main_window_load(Window *window) {
    window_set_background_color(window, GColorBlack);

    s_time_layer = text_layer_create(GRect(0, 4, 144, 50));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_text(s_time_layer, "00:00");

    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

    s_countdown_layer = text_layer_create(GRect(0, 38, 144, 50));
    text_layer_set_background_color(s_countdown_layer, GColorClear);
    text_layer_set_text_color(s_countdown_layer, GColorWhite);
    text_layer_set_text(s_countdown_layer, "");

    text_layer_set_font(s_countdown_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);

    for (int i=0; i<4; i++) {
        s_iridium_layers[i] = text_layer_create(GRect(0, 72+(22*i), 144, 22));
        text_layer_set_background_color(s_iridium_layers[i], GColorClear);
        text_layer_set_text_color(s_iridium_layers[i], GColorWhite);
        text_layer_set_font(s_iridium_layers[i], fonts_get_system_font(FONT_KEY_GOTHIC_18));
        text_layer_set_text_alignment(s_iridium_layers[i], GTextAlignmentCenter);
    }
    text_layer_set_text(s_iridium_layers[0], "Loading...");

    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_countdown_layer));

    for (int i=0; i<4; i++) {
        layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_iridium_layers[i]));
    }
}

static void update_iridium() {
    DictionaryIterator *iter;

    APP_LOG(APP_LOG_LEVEL_INFO, "Updating iridium...");

    app_message_outbox_begin(&iter);

    dict_write_uint8(iter, 0, 0);

    app_message_outbox_send();
}

static void update_time() {
    time_t timestamp = time(NULL);
    struct tm *tick_time = localtime(&timestamp);

    static char buffer[] = "00:00";

    if(clock_is_24h_style() == true) {
        strftime(buffer, sizeof(buffer), "%H:%M", tick_time);
    } else {
        strftime(buffer, sizeof(buffer), "%I:%M", tick_time);
    }

    text_layer_set_text(s_time_layer, buffer);
}

static void second_handler(struct tm *tick_time, TimeUnits units_changed) {
    time_t timestamp = time(NULL);
    time_t diff = flares[0].time - timestamp;
    static char buffer[] = "-00:00";

    if (tick_time->tm_sec == 0) {
        update_time();
    }

    if (diff < 0)
    {
        text_layer_set_text(s_countdown_layer, "");
        tick_timer_service_subscribe(MINUTE_UNIT, minute_handler);
    }
    else
    {
        strftime(buffer, sizeof(buffer), "%M:%S", localtime(&diff));
        text_layer_set_text(s_countdown_layer, buffer);
        if (diff == 10)
        {
            vibes_double_pulse();
        }
    }
}

static void minute_handler(struct tm *tick_time, TimeUnits units_changed) {
    time_t timestamp = time(NULL);
    time_t diff = flares[0].time - timestamp;

    update_time();

    if (flares_present) {
        if ((timestamp > flares[0].time) || (tick_time->tm_min == 0)) {
            update_iridium();
        }

        if ((flares[0].time - timestamp < 60*5) && (flares[0].time - timestamp > 0))
        {
            tick_timer_service_subscribe(SECOND_UNIT, second_handler);
            vibes_double_pulse();
        }
    }
}

static void main_window_unload(Window *window) {
    for (int i=0; i<4; i++) {
        text_layer_destroy(s_iridium_layers[i]);
    }
    text_layer_destroy(s_time_layer);
}

static void init() {
    s_main_window = window_create();

    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });

    window_stack_push(s_main_window, true);

    update_time();

    tick_timer_service_subscribe(MINUTE_UNIT, minute_handler);

    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}