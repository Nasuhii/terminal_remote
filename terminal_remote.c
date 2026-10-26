#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define TAG "TerminalRemoteApp"

#define HID_BT_KEYS_STORAGE_PATH EXT_PATH("apps_data/hid_ble/.bt_hid.keys")
#define ANKI_PRESETS_PATH        EXT_PATH("apps_data/anki_remote/presets.dat")
#define SETTINGS_DIR_PATH        EXT_PATH("apps_data/terminal_remote")
#define SETTINGS_FILE_PATH       EXT_PATH("apps_data/terminal_remote/settings.dat")

#define NUM_FLIPPER_BUTTONS 6
#define PRESET_NAME_MAX_LEN 9
#define MAX_PRESETS         4

#define MOD_CTRL_BIT  (1 << 0)
#define MOD_SHIFT_BIT (1 << 1)
#define MOD_ALT_BIT   (1 << 2)
#define MOD_GUI_BIT   (1 << 3)

#define PRESET_FILE_MAGIC     0x4152504DUL // 'ARPM'
#define PRESET_FILE_VERSION   0x0001
#define SETTINGS_FILE_MAGIC   0x54525345UL // 'TRSE'
#define SETTINGS_FILE_VERSION 2

#define NUM_TABS 3

typedef struct {
    char name[6];
    char shell_title[20];
    char prompt[18];
    char shortcut[8];
    uint8_t keycode;
} TabConfig;

typedef enum {
    RoleProfileStandard = 0,
    RoleProfilePentest,
    RoleProfileAiData,
    RoleProfileDevops,
    RoleProfileCount,
} RoleProfile;

typedef enum {
    SceneConnection = 0,
    SceneController,
    SceneMenu,
    SceneMouse,
    SceneRoleProfileSelect,
    ScenePresetSelect,
    SceneQuickMacros,
    SceneSettings,
    SceneCheatSheet,
} AppScene;

typedef enum {
    BackActionFromPreset = 0, // Uses preset definition (Ctrl+C for Preset 3)
    BackActionForceCtrlC,     // Always sends Ctrl+C
    BackActionEscape,         // Always sends ESC
    BackActionCtrlD,          // Always sends Ctrl+D
    BackActionPanicLock,      // Panic Key: Lock screen immediately
    BackActionMenuOnly,       // Directly opens menu
    BackActionCount,
} BackAction;

typedef enum {
    FlipperButtonUp = 0,
    FlipperButtonDown,
    FlipperButtonLeft,
    FlipperButtonRight,
    FlipperButtonOk,
    FlipperButtonBack,
} FlipperButton;

typedef struct {
    uint8_t keycode;
    uint8_t modifiers;
} KeyMapping;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint8_t preset_count;
    uint8_t active_index;
    uint8_t led_brightness;
    uint8_t reserved[11];
} AnkiPresetFileHeader;

typedef struct __attribute__((packed)) {
    char name[10];
    KeyMapping keymap[NUM_FLIPPER_BUTTONS];
} AnkiPresetDiskEntry;

typedef struct {
    char name[PRESET_NAME_MAX_LEN + 1];
    KeyMapping keymap[NUM_FLIPPER_BUTTONS];
    char label[NUM_FLIPPER_BUTTONS][8];
} AppPreset;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t active_preset_index;
    uint8_t back_action;
    bool haptics_enabled;
    bool led_enabled;
    uint8_t key_delay_ms;
    uint8_t tab1_name_idx;
    uint8_t tab2_name_idx;
    uint8_t role_profile;
} TerminalRemoteSettings;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    Bt* bt;
    FuriHalBleProfileBase* ble_hid_profile;

    AppScene current_scene;
    bool running;
    bool connected;

    // Presets
    AppPreset presets[MAX_PRESETS];
    uint8_t preset_count;
    uint8_t active_preset_index;

    // Tabs & Roles
    RoleProfile role_profile;
    uint8_t role_select_index;
    uint8_t active_tab; // 0..NUM_TABS - 1
    uint8_t tab1_name_idx;
    uint8_t tab2_name_idx;
    char tab_last_action[NUM_TABS][28];
    uint32_t keystroke_count;
    bool cursor_blink;

    // Settings
    BackAction back_action;
    bool haptics_enabled;
    bool led_enabled;
    uint8_t key_delay_ms;

    // Menu States
    uint8_t menu_index;
    uint8_t preset_select_index;
    uint8_t macro_index;
    uint8_t settings_index;

    // Button states for visual feedback
    bool btn_up;
    bool btn_down;
    bool btn_left;
    bool btn_right;
    bool btn_ok;
    bool btn_back;

    char last_action[28];
    char status_toast[28];
} TerminalRemoteApp;

typedef enum {
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} TerminalRemoteEvent;

static const char* const TAB1_NAMES[] = {"1:MAIN", "1:BASH", "1:DEV", "1:SSH"};
static const char* const TAB2_NAMES[] = {"2:WORK", "2:LOGS", "2:HTOP", "2:AUX"};

static const TabConfig ROLE_TABS[RoleProfileCount][NUM_TABS] = {
    [RoleProfileStandard] = {
        {
            .name = "MAIN",
            .shell_title = "> pts/1 [main] ~",
            .prompt = "nasuhi@main:~$",
            .shortcut = "Alt+1",
            .keycode = HID_KEYBOARD_1,
        },
        {
            .name = "WORK",
            .shell_title = "> pts/2 [work] ~",
            .prompt = "nasuhi@work:~$",
            .shortcut = "Alt+2",
            .keycode = HID_KEYBOARD_2,
        },
        {
            .name = "LOGS",
            .shell_title = "> pts/3 [logs] ~",
            .prompt = "nasuhi@logs:~$",
            .shortcut = "Alt+3",
            .keycode = HID_KEYBOARD_3,
        },
    },
    [RoleProfilePentest] = {
        {
            .name = "RECON",
            .shell_title = "> nmap / rustscan",
            .prompt = "root@kali:~/recon#",
            .shortcut = "Alt+1",
            .keycode = HID_KEYBOARD_1,
        },
        {
            .name = "EXPLO",
            .shell_title = "> msfconsole v6",
            .prompt = "msf6 (exploit) >",
            .shortcut = "Alt+2",
            .keycode = HID_KEYBOARD_2,
        },
        {
            .name = "REVSH",
            .shell_title = "> nc listener :4444",
            .prompt = "connect back ~$",
            .shortcut = "Alt+3",
            .keycode = HID_KEYBOARD_3,
        },
    },
    [RoleProfileAiData] = {
        {
            .name = "TRAIN",
            .shell_title = "> python train.py",
            .prompt = "(pytorch) epoch:~$",
            .shortcut = "Alt+1",
            .keycode = HID_KEYBOARD_1,
        },
        {
            .name = "HTOP",
            .shell_title = "> watch nvidia-smi",
            .prompt = "gpu:0 [78C 96%] ~$",
            .shortcut = "Alt+2",
            .keycode = HID_KEYBOARD_2,
        },
        {
            .name = "LAB",
            .shell_title = "> jupyter notebook",
            .prompt = "jupyter@local:~$",
            .shortcut = "Alt+3",
            .keycode = HID_KEYBOARD_3,
        },
    },
    [RoleProfileDevops] = {
        {
            .name = "DOCKR",
            .shell_title = "> docker compose",
            .prompt = "docker@host:~$",
            .shortcut = "Alt+1",
            .keycode = HID_KEYBOARD_1,
        },
        {
            .name = "K8S",
            .shell_title = "> kubectl get pods",
            .prompt = "k8s-cluster:~$",
            .shortcut = "Alt+2",
            .keycode = HID_KEYBOARD_2,
        },
        {
            .name = "BUILD",
            .shell_title = "> cargo / make",
            .prompt = "ci-runner@prod:~$",
            .shortcut = "Alt+3",
            .keycode = HID_KEYBOARD_3,
        },
    },
};

static const char* const ROLE_NAMES[RoleProfileCount] = {
    "Standart Dev",
    "Siber Guvenlik",
    "Yapay Zeka & Veri",
    "DevOps & Bulut"
};

#define TERMINAL_TABS (ROLE_TABS[app->role_profile])

// -----------------------------------------------------------------------------
// Helper: Key labels
// -----------------------------------------------------------------------------

static void resolve_key_label(KeyMapping km, char* out, size_t out_len) {
    if(km.modifiers & MOD_CTRL_BIT) {
        if(km.keycode == HID_KEYBOARD_C) {
            snprintf(out, out_len, "^C");
            return;
        } else if(km.keycode == HID_KEYBOARD_D) {
            snprintf(out, out_len, "^D");
            return;
        } else if(km.keycode == HID_KEYBOARD_Z) {
            snprintf(out, out_len, "^Z");
            return;
        } else if(km.keycode == HID_KEYBOARD_L) {
            snprintf(out, out_len, "^L");
            return;
        } else if(km.keycode >= HID_KEYBOARD_A && km.keycode <= HID_KEYBOARD_Z) {
            snprintf(out, out_len, "^%c", 'A' + (km.keycode - HID_KEYBOARD_A));
            return;
        }
    }

    if(km.modifiers & MOD_ALT_BIT) {
        if(km.keycode >= HID_KEYBOARD_1 && km.keycode <= HID_KEYBOARD_9) {
            snprintf(out, out_len, "A%d", 1 + (km.keycode - HID_KEYBOARD_1));
            return;
        }
    }

    switch(km.keycode) {
    case HID_KEYBOARD_UP_ARROW:
        snprintf(out, out_len, "^");
        break;
    case HID_KEYBOARD_DOWN_ARROW:
        snprintf(out, out_len, "v");
        break;
    case HID_KEYBOARD_LEFT_ARROW:
        snprintf(out, out_len, "<");
        break;
    case HID_KEYBOARD_RIGHT_ARROW:
        snprintf(out, out_len, ">");
        break;
    case HID_KEYBOARD_RETURN:
        snprintf(out, out_len, "OK");
        break;
    case HID_KEYBOARD_ESCAPE:
        snprintf(out, out_len, "ESC");
        break;
    case HID_KEYBOARD_TAB:
        snprintf(out, out_len, "TAB");
        break;
    case HID_KEYBOARD_SPACEBAR:
        snprintf(out, out_len, "SPC");
        break;
    case HID_KEYBOARD_DELETE:
        snprintf(out, out_len, "BKSP");
        break;
    case HID_KEYBOARD_DELETE_FORWARD:
        snprintf(out, out_len, "DEL");
        break;
    default:
        if(km.keycode >= HID_KEYBOARD_1 && km.keycode <= HID_KEYBOARD_9) {
            snprintf(out, out_len, "%d", 1 + (km.keycode - HID_KEYBOARD_1));
        } else if(km.keycode >= HID_KEYBOARD_A && km.keycode <= HID_KEYBOARD_Z) {
            snprintf(out, out_len, "%c", 'A' + (km.keycode - HID_KEYBOARD_A));
        } else if(km.keycode == 0) {
            snprintf(out, out_len, "-");
        } else {
            snprintf(out, out_len, "k%02X", km.keycode);
        }
        break;
    }
}

static void update_preset_labels(AppPreset* p) {
    for(uint8_t i = 0; i < NUM_FLIPPER_BUTTONS; ++i) {
        resolve_key_label(p->keymap[i], p->label[i], sizeof(p->label[i]));
    }
}

// -----------------------------------------------------------------------------
// BLE HID Helper
// -----------------------------------------------------------------------------

static void terminal_remote_send_key(TerminalRemoteApp* app, KeyMapping mapping, bool is_press) {
    if(!app->connected || !app->ble_hid_profile) return;
    if(mapping.keycode == 0 && mapping.modifiers == 0) return;

    if(is_press) {
        uint16_t key = mapping.keycode;
        if(mapping.modifiers & MOD_CTRL_BIT)  key |= KEY_MOD_LEFT_CTRL;
        if(mapping.modifiers & MOD_SHIFT_BIT) key |= KEY_MOD_LEFT_SHIFT;
        if(mapping.modifiers & MOD_ALT_BIT)   key |= KEY_MOD_LEFT_ALT;
        if(mapping.modifiers & MOD_GUI_BIT)   key |= KEY_MOD_LEFT_GUI;

        ble_profile_hid_kb_press(app->ble_hid_profile, key);
        app->keystroke_count++;
    } else {
        ble_profile_hid_kb_release_all(app->ble_hid_profile);
    }
}

static void terminal_remote_send_raw_key(TerminalRemoteApp* app, uint8_t keycode, uint8_t modifiers) {
    if(!app->connected || !app->ble_hid_profile) return;
    KeyMapping km = {.keycode = keycode, .modifiers = modifiers};
    terminal_remote_send_key(app, km, true);
    furi_delay_ms(app->key_delay_ms > 0 ? app->key_delay_ms : 25);
    terminal_remote_send_key(app, km, false);
}

static void terminal_remote_send_panic(TerminalRemoteApp* app) {
    if(!app->connected || !app->ble_hid_profile) return;

    // Send Super (GUI) + L to lock screen (Linux & Windows)
    ble_profile_hid_kb_press(app->ble_hid_profile, KEY_MOD_LEFT_GUI | HID_KEYBOARD_L);
    furi_delay_ms(30);
    ble_profile_hid_kb_release_all(app->ble_hid_profile);

    // RGB Red Flash + Haptic Vibration
    if(app->led_enabled) {
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightBlue, 0);
        furi_hal_light_set(LightGreen, 0);
    }
    if(app->notifications) {
        notification_message(app->notifications, &sequence_double_vibro);
    }
    furi_delay_ms(150);
    if(app->led_enabled) {
        furi_hal_light_set(LightRed, 0);
        if(app->connected) furi_hal_light_set(LightBlue, 255);
    }

    strlcpy(app->last_action, "[!] PANIC: WORKSTATION LOCKED", sizeof(app->last_action));
    strlcpy(app->status_toast, "PANIC: Locked!", sizeof(app->status_toast));
}

static void terminal_remote_type_string(TerminalRemoteApp* app, const char* str) {
    if(!app->connected || !app->ble_hid_profile || !str) return;
    for(size_t i = 0; str[i] != '\0'; i++) {
        uint8_t c = (uint8_t)str[i];
        if(c >= sizeof(hid_asciimap) / sizeof(hid_asciimap[0])) continue;
        uint16_t key = hid_asciimap[c];
        if(key != HID_KEYBOARD_NONE) {
            ble_profile_hid_kb_press(app->ble_hid_profile, key);
            furi_delay_ms(12);
            ble_profile_hid_kb_release_all(app->ble_hid_profile);
            furi_delay_ms(8);
        }
    }
}

// -----------------------------------------------------------------------------
// Preset & Settings Management
// -----------------------------------------------------------------------------

static void terminal_remote_setup_default_presets(TerminalRemoteApp* app) {
    app->preset_count = 4;

    // Preset 0: Terminal Remote (Preset 3 mapping from Anki Remote)
    snprintf(app->presets[0].name, sizeof(app->presets[0].name), "Preset 3");
    app->presets[0].keymap[FlipperButtonUp]    = (KeyMapping){.keycode = HID_KEYBOARD_UP_ARROW, .modifiers = 0};
    app->presets[0].keymap[FlipperButtonDown]  = (KeyMapping){.keycode = HID_KEYBOARD_DOWN_ARROW, .modifiers = 0};
    app->presets[0].keymap[FlipperButtonLeft]  = (KeyMapping){.keycode = HID_KEYBOARD_1, .modifiers = MOD_ALT_BIT};
    app->presets[0].keymap[FlipperButtonRight] = (KeyMapping){.keycode = HID_KEYBOARD_2, .modifiers = MOD_ALT_BIT};
    app->presets[0].keymap[FlipperButtonOk]    = (KeyMapping){.keycode = HID_KEYBOARD_RETURN, .modifiers = 0};
    app->presets[0].keymap[FlipperButtonBack]  = (KeyMapping){.keycode = HID_KEYBOARD_C, .modifiers = MOD_CTRL_BIT};
    update_preset_labels(&app->presets[0]);

    // Preset 1: Numbers & Keys (Preset 2 mapping from Anki Remote)
    snprintf(app->presets[1].name, sizeof(app->presets[1].name), "Preset 2");
    app->presets[1].keymap[FlipperButtonUp]    = (KeyMapping){.keycode = HID_KEYBOARD_1, .modifiers = 0};
    app->presets[1].keymap[FlipperButtonDown]  = (KeyMapping){.keycode = HID_KEYBOARD_2, .modifiers = 0};
    app->presets[1].keymap[FlipperButtonLeft]  = (KeyMapping){.keycode = HID_KEYBOARD_3, .modifiers = 0};
    app->presets[1].keymap[FlipperButtonRight] = (KeyMapping){.keycode = HID_KEYBOARD_4, .modifiers = 0};
    app->presets[1].keymap[FlipperButtonOk]    = (KeyMapping){.keycode = HID_KEYBOARD_SPACEBAR, .modifiers = 0};
    app->presets[1].keymap[FlipperButtonBack]  = (KeyMapping){.keycode = HID_KEYBOARD_DELETE_FORWARD, .modifiers = MOD_CTRL_BIT};
    update_preset_labels(&app->presets[1]);

    // Preset 2: Standard Navigation (Default)
    snprintf(app->presets[2].name, sizeof(app->presets[2].name), "Default");
    app->presets[2].keymap[FlipperButtonUp]    = (KeyMapping){.keycode = HID_KEYBOARD_UP_ARROW, .modifiers = 0};
    app->presets[2].keymap[FlipperButtonDown]  = (KeyMapping){.keycode = HID_KEYBOARD_DOWN_ARROW, .modifiers = 0};
    app->presets[2].keymap[FlipperButtonLeft]  = (KeyMapping){.keycode = HID_KEYBOARD_LEFT_ARROW, .modifiers = 0};
    app->presets[2].keymap[FlipperButtonRight] = (KeyMapping){.keycode = HID_KEYBOARD_RIGHT_ARROW, .modifiers = 0};
    app->presets[2].keymap[FlipperButtonOk]    = (KeyMapping){.keycode = HID_KEYBOARD_RETURN, .modifiers = 0};
    app->presets[2].keymap[FlipperButtonBack]  = (KeyMapping){.keycode = HID_KEYBOARD_ESCAPE, .modifiers = 0};
    update_preset_labels(&app->presets[2]);

    // Preset 3: Vim / Dev Navigation
    snprintf(app->presets[3].name, sizeof(app->presets[3].name), "Vim Dev");
    app->presets[3].keymap[FlipperButtonUp]    = (KeyMapping){.keycode = HID_KEYBOARD_K, .modifiers = 0};
    app->presets[3].keymap[FlipperButtonDown]  = (KeyMapping){.keycode = HID_KEYBOARD_J, .modifiers = 0};
    app->presets[3].keymap[FlipperButtonLeft]  = (KeyMapping){.keycode = HID_KEYBOARD_H, .modifiers = 0};
    app->presets[3].keymap[FlipperButtonRight] = (KeyMapping){.keycode = HID_KEYBOARD_L, .modifiers = 0};
    app->presets[3].keymap[FlipperButtonOk]    = (KeyMapping){.keycode = HID_KEYBOARD_RETURN, .modifiers = 0};
    app->presets[3].keymap[FlipperButtonBack]  = (KeyMapping){.keycode = HID_KEYBOARD_ESCAPE, .modifiers = 0};
    update_preset_labels(&app->presets[3]);

    app->active_preset_index = 0; // Default to Preset 3 (Terminal)
}

static void terminal_remote_load_anki_presets(TerminalRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, ANKI_PRESETS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        AnkiPresetFileHeader hdr = {0};
        if(storage_file_read(file, &hdr, sizeof(hdr)) == sizeof(hdr) &&
           hdr.magic == PRESET_FILE_MAGIC && hdr.version == PRESET_FILE_VERSION) {
            
            uint8_t count = (hdr.preset_count > MAX_PRESETS) ? MAX_PRESETS : hdr.preset_count;
            for(uint8_t i = 0; i < count; ++i) {
                AnkiPresetDiskEntry entry = {0};
                if(storage_file_read(file, &entry, sizeof(entry)) == sizeof(entry)) {
                    snprintf(app->presets[i].name, sizeof(app->presets[i].name), "%s", entry.name);
                    memcpy(app->presets[i].keymap, entry.keymap, sizeof(entry.keymap));
                    
                    if(strcmp(entry.name, "Preset 3") == 0 && app->presets[i].keymap[FlipperButtonBack].keycode == 0) {
                        app->presets[i].keymap[FlipperButtonBack] = (KeyMapping){.keycode = HID_KEYBOARD_C, .modifiers = MOD_CTRL_BIT};
                    }
                    update_preset_labels(&app->presets[i]);
                }
            }
            app->preset_count = count;

            for(uint8_t i = 0; i < app->preset_count; ++i) {
                if(strcmp(app->presets[i].name, "Preset 3") == 0) {
                    app->active_preset_index = i;
                    break;
                }
            }
            FURI_LOG_I(TAG, "Loaded %d presets from %s", count, ANKI_PRESETS_PATH);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void terminal_remote_save_settings(TerminalRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_DIR_PATH);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        TerminalRemoteSettings s = {
            .magic = SETTINGS_FILE_MAGIC,
            .version = SETTINGS_FILE_VERSION,
            .active_preset_index = app->active_preset_index,
            .back_action = (uint8_t)app->back_action,
            .haptics_enabled = app->haptics_enabled,
            .led_enabled = app->led_enabled,
            .key_delay_ms = app->key_delay_ms,
            .tab1_name_idx = app->tab1_name_idx,
            .tab2_name_idx = app->tab2_name_idx,
            .role_profile = (uint8_t)app->role_profile,
        };
        storage_file_write(file, &s, sizeof(s));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void terminal_remote_load_settings(TerminalRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SETTINGS_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        TerminalRemoteSettings s = {0};
        if(storage_file_read(file, &s, sizeof(s)) == sizeof(s) &&
           s.magic == SETTINGS_FILE_MAGIC) {
            if(s.active_preset_index < app->preset_count) {
                app->active_preset_index = s.active_preset_index;
            }
            if(s.back_action < BackActionCount) {
                app->back_action = (BackAction)s.back_action;
            }
            app->haptics_enabled = s.haptics_enabled;
            app->led_enabled = s.led_enabled;
            if(s.key_delay_ms >= 10 && s.key_delay_ms <= 100) {
                app->key_delay_ms = s.key_delay_ms;
            }
            if(s.tab1_name_idx < 4) app->tab1_name_idx = s.tab1_name_idx;
            if(s.tab2_name_idx < 4) app->tab2_name_idx = s.tab2_name_idx;
            if(s.role_profile < RoleProfileCount) {
                app->role_profile = (RoleProfile)s.role_profile;
            }
            FURI_LOG_I(TAG, "Settings loaded from %s", SETTINGS_FILE_PATH);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// -----------------------------------------------------------------------------
// Scene Rendering Functions
// -----------------------------------------------------------------------------

static void draw_scene_connection(Canvas* canvas, TerminalRemoteApp* app) {
    // Top Bar
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ BLUETOOTH BAGLANTISI ]");
    canvas_set_color(canvas, ColorBlack);

    // Frame
    canvas_draw_rframe(canvas, 2, 14, 124, 38, 3);

    // Bluetooth Rune (x: 14, y: 20 to y: 38)
    canvas_draw_line(canvas, 14, 20, 14, 38);
    canvas_draw_line(canvas, 14, 20, 20, 25);
    canvas_draw_line(canvas, 20, 25, 8, 33);
    canvas_draw_line(canvas, 8, 25, 20, 33);
    canvas_draw_line(canvas, 20, 33, 14, 38);

    if(app->connected) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 26, "CIHAZA BAGLI");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 28, 37, "BLE HID Aktif (Hazir)");
        canvas_draw_str(canvas, 28, 47, "[OK] ile Konsola Girin");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 26, "BAGLANTI BEKLENIYOR");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 28, 37, "Cihaz: Flipper Bluetooth");
        canvas_draw_str(canvas, 28, 47, "PC/Telefondan Baglanin");
    }

    // Bottom Bar
    canvas_draw_line(canvas, 0, 54, 128, 54);
    canvas_set_font(canvas, FontSecondary);
    if(app->connected) {
        canvas_draw_str(canvas, 3, 62, "OK: Konsol");
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "BK: Menu");
    } else {
        canvas_draw_str(canvas, 3, 62, "OK: Konsola Gec");
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "BK: Cikis");
    }
}

static void draw_scene_controller(Canvas* canvas, TerminalRemoteApp* app) {
    // -------------------------------------------------------------------------
    // 1. Sleek Minimalist Tab Header (y: 0..11)
    // -------------------------------------------------------------------------
    // Left: Minimalist Bluetooth icon
    if(app->connected) {
        canvas_draw_disc(canvas, 5, 5, 2);
        canvas_draw_str(canvas, 10, 8, "BLE");
    } else {
        canvas_draw_circle(canvas, 5, 5, 2);
        canvas_draw_str(canvas, 10, 8, "WAIT");
    }

    // Center: 3 Tab Pills (MAIN, WORK, LOGS)
    for(uint8_t t = 0; t < NUM_TABS; ++t) {
        uint8_t px = 28 + (t * 26);
        uint8_t py = 1;
        uint8_t pw = 25;
        uint8_t ph = 9;

        if(app->active_tab == t) {
            canvas_draw_rbox(canvas, px, py, pw, ph, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, px + 12, py + ph - 2, AlignCenter, AlignBottom, TERMINAL_TABS[t].name);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, px, py, pw, ph, 2);
            canvas_draw_str_aligned(canvas, px + 12, py + ph - 2, AlignCenter, AlignBottom, TERMINAL_TABS[t].name);
        }
    }

    // Right: Keystroke activity counter
    char cnt_str[10];
    snprintf(cnt_str, sizeof(cnt_str), "#%lu", (unsigned long)(app->keystroke_count % 1000));
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, cnt_str);

    // -------------------------------------------------------------------------
    // 2. Full-Width Minimalist Terminal Shell (x: 1, y: 12, w: 126, h: 51)
    // -------------------------------------------------------------------------
    canvas_draw_rframe(canvas, 1, 12, 126, 51, 2);
    
    // Subheader divider line
    canvas_draw_line(canvas, 2, 22, 125, 22);

    // Terminal session title
    canvas_draw_str(canvas, 5, 20, TERMINAL_TABS[app->active_tab].shell_title);

    // Keycap badge for active shortcut (Alt+1 / Alt+2 / Alt+3)
    canvas_draw_rframe(canvas, 94, 13, 30, 8, 1);
    canvas_draw_str_aligned(canvas, 109, 20, AlignCenter, AlignBottom, TERMINAL_TABS[app->active_tab].shortcut);

    // Interactive Action Feedback vs Idle Prompt
    bool any_pressed = app->btn_up || app->btn_down || app->btn_left || app->btn_right || app->btn_ok || app->btn_back;

    if(any_pressed) {
        // High-contrast inverted action banner
        canvas_draw_rbox(canvas, 5, 26, 118, 14, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignBottom, app->last_action);
        canvas_set_color(canvas, ColorBlack);
    } else {
        // Clean prompt line
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 6, 32, TERMINAL_TABS[app->active_tab].prompt);

        // Active tab's last command
        const char* last = app->tab_last_action[app->active_tab];
        char cmd_line[36];
        if(last[0] != '\0') {
            snprintf(cmd_line, sizeof(cmd_line), "> %s", last);
        } else {
            snprintf(cmd_line, sizeof(cmd_line), "> ready");
        }
        canvas_draw_str(canvas, 6, 42, cmd_line);

        // Blinking cursor
        if(app->cursor_blink) {
            size_t w = canvas_string_width(canvas, cmd_line);
            canvas_draw_box(canvas, 8 + w, 35, 4, 7);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Bottom Minimalist Symbol Strip
    // -------------------------------------------------------------------------
    canvas_draw_line(canvas, 3, 51, 124, 51);

    canvas_draw_str(canvas, 5, 60, "<:Tab-");
    canvas_draw_str(canvas, 33, 60, "^:Up");
    canvas_draw_str(canvas, 56, 60, "OK:Ret");
    canvas_draw_str(canvas, 83, 60, "v:Dn");
    canvas_draw_str(canvas, 102, 60, ">:Tab+");
}

static void draw_scene_mouse(Canvas* canvas, TerminalRemoteApp* app) {
    // Header
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ D-PAD MOUSE / SUNUM ]");
    canvas_set_color(canvas, ColorBlack);

    // Frame
    canvas_draw_rframe(canvas, 2, 14, 124, 38, 3);

    // Crosshair target in center
    canvas_draw_circle(canvas, 64, 33, 14);
    canvas_draw_circle(canvas, 64, 33, 4);
    canvas_draw_line(canvas, 64, 16, 64, 50);
    canvas_draw_line(canvas, 47, 33, 81, 33);

    // Status / Mode Info
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, 24, "MOD: MOUSE");
    canvas_draw_str(canvas, 6, 34, "ADIM: 14px");
    if(app->btn_ok) {
        canvas_draw_str(canvas, 86, 34, "[TIK!]");
    }

    // Bottom Bar
    canvas_draw_line(canvas, 0, 54, 128, 54);
    canvas_draw_str(canvas, 3, 62, "OK: Sol Tik | OK+: Cift");
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "BK: Menu");
}

static void draw_scene_role_select(Canvas* canvas, TerminalRemoteApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ CALISMA PROFILI ]");
    canvas_set_color(canvas, ColorBlack);

    for(uint8_t i = 0; i < RoleProfileCount; ++i) {
        uint8_t y = 23 + (i * 9);
        char line[32];
        const char* active_tag = (i == app->role_profile) ? " [AKTIF]" : "";
        snprintf(line, sizeof(line), "%d. %s%s", i + 1, ROLE_NAMES[i], active_tag);

        if(app->role_select_index == i) {
            canvas_draw_box(canvas, 2, y - 7, 124, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, line);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, line);
        }
    }

    // Tab preview of selected profile
    const TabConfig* tabs = ROLE_TABS[app->role_select_index];
    char preview[48];
    snprintf(preview, sizeof(preview), "T1:%s  T2:%s  T3:%s", tabs[0].name, tabs[1].name, tabs[2].name);

    canvas_draw_rframe(canvas, 1, 44, 126, 9, 1);
    canvas_draw_str(canvas, 3, 51, preview);

    canvas_draw_line(canvas, 0, 55, 127, 55);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Aktif Et  |  BK: Menu");
}

static void draw_scene_menu(Canvas* canvas, TerminalRemoteApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ TERMINAL HUB ]");
    canvas_set_color(canvas, ColorBlack);

    char p_str[28];
    snprintf(p_str, sizeof(p_str), "Preset: %s", app->presets[app->active_preset_index].name);

    char bt_label[32];
    snprintf(bt_label, sizeof(bt_label), "Bluetooth: %s", app->connected ? "Bagli [OK]" : "Baglan [Bekliyor]");

    char role_label[32];
    snprintf(role_label, sizeof(role_label), "Mod: %s", ROLE_NAMES[app->role_profile]);

    typedef struct {
        const char* badge;
        const char* label;
    } MenuRow;

    const MenuRow items[] = {
        {.badge = ">_",  .label = "Terminal Console"},
        {.badge = "BT",  .label = bt_label},
        {.badge = "(!)", .label = "PANIC: Kilit (Super+L)"},
        {.badge = "::",  .label = role_label},
        {.badge = "><",  .label = "D-Pad Mouse / Sunum"},
        {.badge = "#P",  .label = p_str},
        {.badge = ">>",  .label = "Quick Macros & Payloads"},
        {.badge = "-o-", .label = "Settings & Remap"},
        {.badge = "(?)", .label = "Cheat Sheet / Help"},
        {.badge = "[X]", .label = "Exit Terminal Remote"},
    };
    const uint8_t count = sizeof(items) / sizeof(items[0]);
    const uint8_t visible = 4;

    uint8_t scroll_offset = 0;
    if(app->menu_index >= visible) {
        scroll_offset = app->menu_index - visible + 1;
    }

    for(uint8_t v = 0; v < visible; ++v) {
        uint8_t i = scroll_offset + v;
        if(i >= count) break;
        uint8_t y = 15 + (v * 12);

        if(app->menu_index == i) {
            canvas_draw_rbox(canvas, 2, y - 1, 119, 11, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 5, y + 8, items[i].badge);
            canvas_draw_str(canvas, 25, y + 8, items[i].label);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, 3, y - 1, 18, 10, 1);
            canvas_draw_str_aligned(canvas, 12, y + 7, AlignCenter, AlignBottom, items[i].badge);
            canvas_draw_str(canvas, 25, y + 8, items[i].label);
        }
    }

    elements_scrollbar_pos(canvas, 124, 14, 48, app->menu_index, count);
}

static void draw_scene_preset_select(Canvas* canvas, TerminalRemoteApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ SELECT PRESET ]");
    canvas_set_color(canvas, ColorBlack);

    for(uint8_t i = 0; i < app->preset_count; ++i) {
        uint8_t y = 23 + (i * 9);
        char line[32];
        const char* active_tag = (i == app->active_preset_index) ? " [ACTIVE]" : "";
        snprintf(line, sizeof(line), "%d. %s%s", i + 1, app->presets[i].name, active_tag);

        if(app->preset_select_index == i) {
            canvas_draw_box(canvas, 2, y - 7, 124, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, line);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, line);
        }
    }

    // Keymap Preview
    AppPreset* p = &app->presets[app->preset_select_index];
    char preview[48];
    snprintf(preview, sizeof(preview), "U:%s D:%s L:%s R:%s OK:%s B:%s",
        p->label[FlipperButtonUp],
        p->label[FlipperButtonDown],
        p->label[FlipperButtonLeft],
        p->label[FlipperButtonRight],
        p->label[FlipperButtonOk],
        p->label[FlipperButtonBack]);

    canvas_draw_rframe(canvas, 1, 44, 126, 9, 1);
    canvas_draw_str(canvas, 3, 51, preview);

    canvas_draw_line(canvas, 0, 55, 127, 55);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Activate  |  BK: Menu");
}

static void draw_scene_quick_macros(Canvas* canvas, TerminalRemoteApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ QUICK MACROS & PAYLOADS ]");
    canvas_set_color(canvas, ColorBlack);

    const char* macros[] = {
        "1. [!] PANIC: Kilit (Super+L)",
        "2. ^C  SIGINT (Interrupt)",
        "3. ^D  EOF (Exit Session)",
        "4. ^L  Clear Screen",
        "5. ^Z  SIGTSTP (Suspend)",
        "6. ^R  Reverse History Search",
        "7. TAB Auto-complete",
        "8. [Net]  ip a | grep inet",
        "9. [Git]  git status -s",
        "10.[Dock] docker ps --format table",
        "11.[Port] ss -tulnp",
        "12.[Htop] htop || top",
        "13.[Rev]  nc -lvnp 4444",
    };
    uint8_t count = sizeof(macros) / sizeof(macros[0]);

    for(uint8_t i = 0; i < 4; ++i) {
        uint8_t idx = (app->macro_index < 4) ? i : (i + (app->macro_index - 3));
        if(idx >= count) break;
        uint8_t y = 24 + (i * 8);

        if(app->macro_index == idx) {
            canvas_draw_box(canvas, 2, y - 7, 120, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, macros[idx]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, macros[idx]);
        }
    }

    elements_scrollbar(canvas, app->macro_index, count);

    canvas_draw_line(canvas, 0, 54, 127, 54);
    if(app->status_toast[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, app->status_toast);
    } else {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Send Keystroke | BK: Menu");
    }
}

static void draw_scene_settings(Canvas* canvas, TerminalRemoteApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "[ SETTINGS ]");
    canvas_set_color(canvas, ColorBlack);

    const char* back_names[] = {"Preset ^C", "Force ^C", "Escape", "^D (EOF)", "Panic (Lock)", "Menu Only"};
    char line1[32];
    char line2[32];
    char line3[32];
    char line4[32];
    char line5[32];
    char line6[32];
    char line7[32] = "7. [Save Settings to SD]";
    char line8[32] = "8. [Reload presets.dat]";
    char line9[32];

    snprintf(line1, sizeof(line1), "1. Back Key: <%s>", back_names[app->back_action]);
    snprintf(line2, sizeof(line2), "2. Tab 1:    <%s>", TAB1_NAMES[app->tab1_name_idx]);
    snprintf(line3, sizeof(line3), "3. Tab 2:    <%s>", TAB2_NAMES[app->tab2_name_idx]);
    snprintf(line4, sizeof(line4), "4. Vibro:    <%s>", app->haptics_enabled ? "ON" : "OFF");
    snprintf(line5, sizeof(line5), "5. Blue LED: <%s>", app->led_enabled ? "ON" : "OFF");
    snprintf(line6, sizeof(line6), "6. Delay:    <%dms>", app->key_delay_ms);
    snprintf(line9, sizeof(line9), "9. Role:     <%s>", ROLE_NAMES[app->role_profile]);

    const char* lines[] = {line1, line2, line3, line4, line5, line6, line7, line8, line9};
    uint8_t count = sizeof(lines) / sizeof(lines[0]);

    for(uint8_t i = 0; i < 4; ++i) {
        uint8_t idx = (app->settings_index < 4) ? i : (i + (app->settings_index - 3));
        if(idx >= count) break;
        uint8_t y = 24 + (i * 8);

        if(app->settings_index == idx) {
            canvas_draw_box(canvas, 2, y - 7, 120, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, y, lines[idx]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, y, lines[idx]);
        }
    }

    elements_scrollbar(canvas, app->settings_index, count);

    canvas_draw_line(canvas, 0, 54, 127, 54);
    if(app->status_toast[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, app->status_toast);
    } else {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK: Toggle/Save | BK: Menu");
    }
}

static void draw_scene_cheatsheet(Canvas* canvas, TerminalRemoteApp* app) {
    AppPreset* p = &app->presets[app->active_preset_index];
    char title[32];
    snprintf(title, sizeof(title), "[ %s GUIDE ]", p->name);
    
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, title);
    canvas_set_color(canvas, ColorBlack);

    char u[64], d[64], l[64], r[64], ok[64], bk[64];
    snprintf(u, sizeof(u), "UP:    [^] %-4s (Hist Prev)", p->label[FlipperButtonUp]);
    snprintf(d, sizeof(d), "DOWN:  [v] %-4s (Hist Next)", p->label[FlipperButtonDown]);
    snprintf(l, sizeof(l), "LEFT:  [<] %-4s (Prev Tab)", p->label[FlipperButtonLeft]);
    snprintf(r, sizeof(r), "RIGHT: [>] %-4s (Next Tab)", p->label[FlipperButtonRight]);
    snprintf(ok, sizeof(ok), "OK:    [o] %-4s (Execute)", p->label[FlipperButtonOk]);
    snprintf(bk, sizeof(bk), "BACK:  [x] %-4s (Short:Send | Hold:Menu)", p->label[FlipperButtonBack]);

    canvas_draw_str(canvas, 3, 20, u);
    canvas_draw_str(canvas, 3, 27, d);
    canvas_draw_str(canvas, 3, 34, l);
    canvas_draw_str(canvas, 3, 41, r);
    canvas_draw_str(canvas, 3, 48, ok);
    canvas_draw_str(canvas, 3, 54, bk);

    canvas_draw_line(canvas, 0, 56, 127, 56);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "BK: Return to Hub");
}

static void terminal_remote_draw_callback(Canvas* canvas, void* context) {
    TerminalRemoteApp* app = (TerminalRemoteApp*)context;
    furi_assert(app);

    canvas_clear(canvas);

    switch(app->current_scene) {
    case SceneConnection:
        draw_scene_connection(canvas, app);
        break;
    case SceneController:
        draw_scene_controller(canvas, app);
        break;
    case SceneMenu:
        draw_scene_menu(canvas, app);
        break;
    case SceneMouse:
        draw_scene_mouse(canvas, app);
        break;
    case SceneRoleProfileSelect:
        draw_scene_role_select(canvas, app);
        break;
    case ScenePresetSelect:
        draw_scene_preset_select(canvas, app);
        break;
    case SceneQuickMacros:
        draw_scene_quick_macros(canvas, app);
        break;
    case SceneSettings:
        draw_scene_settings(canvas, app);
        break;
    case SceneCheatSheet:
        draw_scene_cheatsheet(canvas, app);
        break;
    }
}

// -----------------------------------------------------------------------------
// Input Handlers
// -----------------------------------------------------------------------------

static void handle_controller_input(TerminalRemoteApp* app, InputEvent* event) {
    // Hold BACK always opens the Hub Menu
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        app->btn_back = false;
        app->current_scene = SceneMenu;
        app->menu_index = 0;
        return;
    }

    AppPreset* p = &app->presets[app->active_preset_index];

    // Handle Back key
    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            app->btn_back = true;
        } else if(event->type == InputTypeRelease) {
            app->btn_back = false;
        } else if(event->type == InputTypeShort) {
            app->btn_back = false;

            if(app->back_action == BackActionMenuOnly) {
                app->current_scene = SceneMenu;
                return;
            }

            KeyMapping km = {0};
            const char* act_str = NULL;

            if(app->back_action == BackActionPanicLock) {
                terminal_remote_send_panic(app);
                return;
            } else if(app->back_action == BackActionFromPreset) {
                km = p->keymap[FlipperButtonBack];
                act_str = "[^C] SIGINT (Break)";
            } else if(app->back_action == BackActionForceCtrlC) {
                km = (KeyMapping){.keycode = HID_KEYBOARD_C, .modifiers = MOD_CTRL_BIT};
                act_str = "[^C] SIGINT (Break)";
            } else if(app->back_action == BackActionEscape) {
                km = (KeyMapping){.keycode = HID_KEYBOARD_ESCAPE, .modifiers = 0};
                act_str = "[ESC] Escape Key";
            } else if(app->back_action == BackActionCtrlD) {
                km = (KeyMapping){.keycode = HID_KEYBOARD_D, .modifiers = MOD_CTRL_BIT};
                act_str = "[^D] EOF (Logout)";
            }

            if(act_str) {
                strlcpy(app->last_action, act_str, sizeof(app->last_action));
                strlcpy(app->tab_last_action[app->active_tab], act_str, sizeof(app->tab_last_action[0]));
            }
            if(app->haptics_enabled && app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
            if(km.keycode == HID_KEYBOARD_C && (km.modifiers & MOD_CTRL_BIT) && app->led_enabled) {
                furi_hal_light_set(LightRed, 255);
                furi_delay_ms(40);
                furi_hal_light_set(LightRed, 0);
                if(app->connected) furi_hal_light_set(LightBlue, 255);
            }
            terminal_remote_send_raw_key(app, km.keycode, km.modifiers);
        }
        return;
    }

    // Handle Left / Right (Tab Switching as dedicated pages)
    if(event->key == InputKeyLeft && event->type == InputTypePress) {
        app->btn_left = true;
        app->active_tab = (app->active_tab + NUM_TABS - 1) % NUM_TABS;
        snprintf(
            app->last_action,
            sizeof(app->last_action),
            "[<] T%d:%.5s (%.5s)",
            app->active_tab + 1,
            TERMINAL_TABS[app->active_tab].name,
            TERMINAL_TABS[app->active_tab].shortcut);
        strlcpy(
            app->tab_last_action[app->active_tab],
            "Switched tab",
            sizeof(app->tab_last_action[0]));
        if(app->haptics_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        if(app->led_enabled) {
            furi_hal_light_set(LightGreen, 255);
            furi_delay_ms(30);
            furi_hal_light_set(LightGreen, 0);
            if(app->connected) furi_hal_light_set(LightBlue, 255);
        }
        terminal_remote_send_raw_key(app, TERMINAL_TABS[app->active_tab].keycode, MOD_ALT_BIT);
        return;
    } else if(event->key == InputKeyLeft && event->type == InputTypeRelease) {
        app->btn_left = false;
        return;
    }

    if(event->key == InputKeyRight && event->type == InputTypePress) {
        app->btn_right = true;
        app->active_tab = (app->active_tab + 1) % NUM_TABS;
        snprintf(
            app->last_action,
            sizeof(app->last_action),
            "[>] T%d:%.5s (%.5s)",
            app->active_tab + 1,
            TERMINAL_TABS[app->active_tab].name,
            TERMINAL_TABS[app->active_tab].shortcut);
        strlcpy(
            app->tab_last_action[app->active_tab],
            "Switched tab",
            sizeof(app->tab_last_action[0]));
        if(app->haptics_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        if(app->led_enabled) {
            furi_hal_light_set(LightGreen, 255);
            furi_delay_ms(30);
            furi_hal_light_set(LightGreen, 0);
            if(app->connected) furi_hal_light_set(LightBlue, 255);
        }
        terminal_remote_send_raw_key(app, TERMINAL_TABS[app->active_tab].keycode, MOD_ALT_BIT);
        return;
    } else if(event->key == InputKeyRight && event->type == InputTypeRelease) {
        app->btn_right = false;
        return;
    }

    // Handle Up, Down, OK
    bool is_press = (event->type == InputTypePress);
    bool is_release = (event->type == InputTypeRelease);

    if(is_press || is_release) {
        KeyMapping mapping = {0};
        const char* action_str = NULL;

        switch(event->key) {
        case InputKeyUp:
            app->btn_up = is_press;
            mapping = p->keymap[FlipperButtonUp];
            action_str = "[^] HISTORY PREV";
            break;
        case InputKeyDown:
            app->btn_down = is_press;
            mapping = p->keymap[FlipperButtonDown];
            action_str = "[v] HISTORY NEXT";
            break;
        case InputKeyOk:
            app->btn_ok = is_press;
            mapping = p->keymap[FlipperButtonOk];
            action_str = "[OK] ENTER / EXEC";
            break;
        default:
            break;
        }

        if(is_press) {
            if(action_str) {
                strlcpy(app->last_action, action_str, sizeof(app->last_action));
                strlcpy(app->tab_last_action[app->active_tab], action_str, sizeof(app->tab_last_action[0]));
            }
            if(app->haptics_enabled && app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
            terminal_remote_send_key(app, mapping, true);
        } else if(is_release) {
            terminal_remote_send_key(app, mapping, false);
        }
    }
}

static void handle_connection_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    if(event->key == InputKeyOk) {
        app->current_scene = SceneController;
    } else if(event->key == InputKeyBack) {
        if(app->connected) {
            app->current_scene = SceneMenu;
        } else {
            app->running = false;
        }
    }
}

static void handle_mouse_input(TerminalRemoteApp* app, InputEvent* event) {
    if(!app->connected || !app->ble_hid_profile) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->current_scene = SceneMenu;
        }
        return;
    }

    const int8_t step = 14;

    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            ble_profile_hid_mouse_move(app->ble_hid_profile, 0, -step);
            break;
        case InputKeyDown:
            ble_profile_hid_mouse_move(app->ble_hid_profile, 0, step);
            break;
        case InputKeyLeft:
            ble_profile_hid_mouse_move(app->ble_hid_profile, -step, 0);
            break;
        case InputKeyRight:
            ble_profile_hid_mouse_move(app->ble_hid_profile, step, 0);
            break;
        case InputKeyOk:
            app->btn_ok = true;
            ble_profile_hid_mouse_press(app->ble_hid_profile, 1);
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRelease) {
        if(event->key == InputKeyOk) {
            app->btn_ok = false;
            ble_profile_hid_mouse_release(app->ble_hid_profile, 1);
        }
    } else if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            app->current_scene = SceneMenu;
        }
    } else if(event->type == InputTypeLong) {
        if(event->key == InputKeyOk) {
            // Double Click
            ble_profile_hid_mouse_press(app->ble_hid_profile, 1);
            furi_delay_ms(20);
            ble_profile_hid_mouse_release(app->ble_hid_profile, 1);
            furi_delay_ms(60);
            ble_profile_hid_mouse_press(app->ble_hid_profile, 1);
            furi_delay_ms(20);
            ble_profile_hid_mouse_release(app->ble_hid_profile, 1);
        }
    }
}

static void handle_role_select_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    switch(event->key) {
    case InputKeyUp:
        if(app->role_select_index > 0) app->role_select_index--;
        else app->role_select_index = RoleProfileCount - 1;
        break;
    case InputKeyDown:
        if(app->role_select_index < RoleProfileCount - 1) app->role_select_index++;
        else app->role_select_index = 0;
        break;
    case InputKeyOk:
        app->role_profile = (RoleProfile)app->role_select_index;
        terminal_remote_save_settings(app);
        snprintf(app->last_action, sizeof(app->last_action), "Mod: %s", ROLE_NAMES[app->role_profile]);
        if(app->haptics_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        app->current_scene = SceneController;
        break;
    case InputKeyBack:
        app->current_scene = SceneMenu;
        break;
    default:
        break;
    }
}

static void handle_menu_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    switch(event->key) {
    case InputKeyUp:
        if(app->menu_index > 0) app->menu_index--;
        else app->menu_index = 9;
        break;
    case InputKeyDown:
        if(app->menu_index < 9) app->menu_index++;
        else app->menu_index = 0;
        break;
    case InputKeyOk:
        switch(app->menu_index) {
        case 0: // Controller
            app->current_scene = SceneController;
            break;
        case 1: // Bluetooth Connection / Status
            app->current_scene = SceneConnection;
            break;
        case 2: // Panic Key (Super+L)
            terminal_remote_send_panic(app);
            app->current_scene = SceneController;
            break;
        case 3: // Role Profile Select
            app->current_scene = SceneRoleProfileSelect;
            app->role_select_index = (uint8_t)app->role_profile;
            break;
        case 4: // D-Pad Mouse Mode
            app->current_scene = SceneMouse;
            break;
        case 5: // Preset Select
            app->current_scene = ScenePresetSelect;
            app->preset_select_index = app->active_preset_index;
            break;
        case 6: // Quick Macros & Payloads
            app->current_scene = SceneQuickMacros;
            app->status_toast[0] = '\0';
            break;
        case 7: // Settings
            app->current_scene = SceneSettings;
            app->status_toast[0] = '\0';
            break;
        case 8: // Cheat sheet
            app->current_scene = SceneCheatSheet;
            break;
        case 9: // Exit
            app->running = false;
            break;
        }
        break;
    case InputKeyBack:
        app->current_scene = SceneController;
        break;
    default:
        break;
    }
}

static void handle_preset_select_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    switch(event->key) {
    case InputKeyUp:
        if(app->preset_select_index > 0) app->preset_select_index--;
        else app->preset_select_index = app->preset_count - 1;
        break;
    case InputKeyDown:
        if(app->preset_select_index < app->preset_count - 1) app->preset_select_index++;
        else app->preset_select_index = 0;
        break;
    case InputKeyOk:
        app->active_preset_index = app->preset_select_index;
        terminal_remote_save_settings(app);
        snprintf(app->last_action, sizeof(app->last_action), "Switched to %s", app->presets[app->active_preset_index].name);
        if(app->haptics_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        app->current_scene = SceneController;
        break;
    case InputKeyBack:
        app->current_scene = SceneMenu;
        break;
    default:
        break;
    }
}

static void handle_quick_macros_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    switch(event->key) {
    case InputKeyUp:
        if(app->macro_index > 0) app->macro_index--;
        else app->macro_index = 12;
        break;
    case InputKeyDown:
        if(app->macro_index < 12) app->macro_index++;
        else app->macro_index = 0;
        break;
    case InputKeyOk:
        switch(app->macro_index) {
        case 0: // Panic Lock
            terminal_remote_send_panic(app);
            break;
        case 1: // ^C
            terminal_remote_send_raw_key(app, HID_KEYBOARD_C, MOD_CTRL_BIT);
            strlcpy(app->status_toast, "Sent: ^C (SIGINT)", sizeof(app->status_toast));
            break;
        case 2: // ^D
            terminal_remote_send_raw_key(app, HID_KEYBOARD_D, MOD_CTRL_BIT);
            strlcpy(app->status_toast, "Sent: ^D (EOF)", sizeof(app->status_toast));
            break;
        case 3: // ^L
            terminal_remote_send_raw_key(app, HID_KEYBOARD_L, MOD_CTRL_BIT);
            strlcpy(app->status_toast, "Sent: ^L (Clear)", sizeof(app->status_toast));
            break;
        case 4: // ^Z
            terminal_remote_send_raw_key(app, HID_KEYBOARD_Z, MOD_CTRL_BIT);
            strlcpy(app->status_toast, "Sent: ^Z (Suspend)", sizeof(app->status_toast));
            break;
        case 5: // ^R
            terminal_remote_send_raw_key(app, HID_KEYBOARD_R, MOD_CTRL_BIT);
            strlcpy(app->status_toast, "Sent: ^R (Search)", sizeof(app->status_toast));
            break;
        case 6: // TAB
            terminal_remote_send_raw_key(app, HID_KEYBOARD_TAB, 0);
            strlcpy(app->status_toast, "Sent: TAB", sizeof(app->status_toast));
            break;
        case 7: // Net IP
            terminal_remote_type_string(app, "ip a | grep inet\n");
            strlcpy(app->status_toast, "Sent: ip a", sizeof(app->status_toast));
            break;
        case 8: // Git status
            terminal_remote_type_string(app, "git status -s\n");
            strlcpy(app->status_toast, "Sent: git status", sizeof(app->status_toast));
            break;
        case 9: // Docker ps
            terminal_remote_type_string(app, "docker ps --format \"table {{.Names}}\\t{{.Status}}\"\n");
            strlcpy(app->status_toast, "Sent: docker ps", sizeof(app->status_toast));
            break;
        case 10: // Port scan
            terminal_remote_type_string(app, "ss -tulnp\n");
            strlcpy(app->status_toast, "Sent: ss -tulnp", sizeof(app->status_toast));
            break;
        case 11: // Htop
            terminal_remote_type_string(app, "htop || top\n");
            strlcpy(app->status_toast, "Sent: htop", sizeof(app->status_toast));
            break;
        case 12: // Revshell listener
            terminal_remote_type_string(app, "nc -lvnp 4444\n");
            strlcpy(app->status_toast, "Sent: nc listener", sizeof(app->status_toast));
            break;
        }
        if(app->haptics_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    case InputKeyBack:
        app->current_scene = SceneMenu;
        break;
    default:
        break;
    }
}

static void handle_settings_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort) return;

    switch(event->key) {
    case InputKeyUp:
        if(app->settings_index > 0) app->settings_index--;
        else app->settings_index = 8;
        break;
    case InputKeyDown:
        if(app->settings_index < 8) app->settings_index++;
        else app->settings_index = 0;
        break;
    case InputKeyOk:
        switch(app->settings_index) {
        case 0: // Back key mode
            app->back_action = (app->back_action + 1) % BackActionCount;
            break;
        case 1: // Tab 1 Name
            app->tab1_name_idx = (app->tab1_name_idx + 1) % 4;
            break;
        case 2: // Tab 2 Name
            app->tab2_name_idx = (app->tab2_name_idx + 1) % 4;
            break;
        case 3: // Haptics
            app->haptics_enabled = !app->haptics_enabled;
            if(app->haptics_enabled && app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
            break;
        case 4: // Blue LED
            app->led_enabled = !app->led_enabled;
            if(app->connected && app->led_enabled) {
                furi_hal_light_set(LightBlue, 255);
            } else {
                furi_hal_light_set(LightBlue, 0);
            }
            break;
        case 5: // Key delay
            if(app->key_delay_ms == 25) app->key_delay_ms = 50;
            else if(app->key_delay_ms == 50) app->key_delay_ms = 15;
            else app->key_delay_ms = 25;
            break;
        case 6: // Save to SD
            terminal_remote_save_settings(app);
            strlcpy(app->status_toast, "Settings Saved to SD!", sizeof(app->status_toast));
            if(app->haptics_enabled && app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
            break;
        case 7: // Reload presets
            terminal_remote_load_anki_presets(app);
            strlcpy(app->status_toast, "Presets Reloaded!", sizeof(app->status_toast));
            break;
        case 8: // Role Profile
            app->role_profile = (RoleProfile)((app->role_profile + 1) % RoleProfileCount);
            terminal_remote_save_settings(app);
            snprintf(app->status_toast, sizeof(app->status_toast), "Role: %s", ROLE_NAMES[app->role_profile]);
            break;
        }
        break;
    case InputKeyBack:
        app->current_scene = SceneMenu;
        break;
    default:
        break;
    }
}

static void handle_cheatsheet_input(TerminalRemoteApp* app, InputEvent* event) {
    if(event->type == InputTypeShort && (event->key == InputKeyBack || event->key == InputKeyOk)) {
        app->current_scene = SceneMenu;
    }
}

static void terminal_remote_input_handler(TerminalRemoteApp* app, InputEvent* event) {
    switch(app->current_scene) {
    case SceneConnection:
        handle_connection_input(app, event);
        break;
    case SceneController:
        handle_controller_input(app, event);
        break;
    case SceneMenu:
        handle_menu_input(app, event);
        break;
    case SceneMouse:
        handle_mouse_input(app, event);
        break;
    case SceneRoleProfileSelect:
        handle_role_select_input(app, event);
        break;
    case ScenePresetSelect:
        handle_preset_select_input(app, event);
        break;
    case SceneQuickMacros:
        handle_quick_macros_input(app, event);
        break;
    case SceneSettings:
        handle_settings_input(app, event);
        break;
    case SceneCheatSheet:
        handle_cheatsheet_input(app, event);
        break;
    }
}

static void terminal_remote_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = (FuriMessageQueue*)context;
    TerminalRemoteEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void terminal_remote_connection_status_callback(BtStatus status, void* context) {
    TerminalRemoteApp* app = (TerminalRemoteApp*)context;
    if(!app) return;
    bool was_connected = app->connected;
    app->connected = (status == BtStatusConnected);
    if(app->connected) {
        if(app->led_enabled) {
            furi_hal_light_set(LightBlue, 255);
        }
        // Auto-switch to controller if waiting in connection scene!
        if(!was_connected && app->current_scene == SceneConnection) {
            app->current_scene = SceneController;
            if(app->haptics_enabled && app->notifications) {
                notification_message(app->notifications, &sequence_single_vibro);
            }
        }
    } else {
        if(app->led_enabled) {
            furi_hal_light_set(LightBlue, 0);
        }
    }
    if(app->view_port) {
        view_port_update(app->view_port);
    }
}

// -----------------------------------------------------------------------------
// App Lifecycle
// -----------------------------------------------------------------------------

static TerminalRemoteApp* terminal_remote_app_alloc() {
    TerminalRemoteApp* app = malloc(sizeof(TerminalRemoteApp));
    furi_check(app);
    memset(app, 0, sizeof(TerminalRemoteApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(TerminalRemoteEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->current_scene = SceneConnection; // Start with connection screen if not connected
    app->back_action = BackActionFromPreset;
    app->haptics_enabled = true;
    app->led_enabled = true;
    app->key_delay_ms = 25;
    app->active_tab = 0;
    app->tab1_name_idx = 0;
    app->tab2_name_idx = 0;
    app->cursor_blink = true;

    // Viewport setup
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, terminal_remote_draw_callback, app);
    view_port_input_callback_set(app->view_port, terminal_remote_input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Initialize presets
    terminal_remote_setup_default_presets(app);
    terminal_remote_load_anki_presets(app);
    terminal_remote_load_settings(app);

    strlcpy(app->last_action, "Ready: Tab 1 Active", sizeof(app->last_action));
    for(uint8_t t = 0; t < NUM_TABS; ++t) {
        snprintf(app->tab_last_action[t], sizeof(app->tab_last_action[0]), "Ready: %.6s", TERMINAL_TABS[t].name);
    }

    app->running = true;
    app->connected = false;

    // Start BLE HID Profile
    app->bt = furi_record_open(RECORD_BT);
    bt_keys_storage_set_storage_path(app->bt, HID_BT_KEYS_STORAGE_PATH);
    bt_set_status_changed_callback(app->bt, terminal_remote_connection_status_callback, app);
    app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid, NULL);
    furi_check(app->ble_hid_profile);
    furi_hal_bt_start_advertising();

    return app;
}

static void terminal_remote_app_free(TerminalRemoteApp* app) {
    furi_assert(app);

    // Stop BLE
    if(app->bt) {
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        furi_check(bt_profile_restore_default(app->bt));
        bt_keys_storage_set_default_path(app->bt);
        furi_record_close(RECORD_BT);
        app->bt = NULL;
    }

    // Free GUI
    if(app->view_port) {
        view_port_enabled_set(app->view_port, false);
        gui_remove_view_port(app->gui, app->view_port);
        view_port_free(app->view_port);
        app->view_port = NULL;
    }

    if(app->notifications) {
        notification_internal_message(app->notifications, &sequence_reset_blue);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }

    if(app->event_queue) {
        furi_message_queue_free(app->event_queue);
        app->event_queue = NULL;
    }

    free(app);
}

// -----------------------------------------------------------------------------
// Entry Point
// -----------------------------------------------------------------------------

int32_t terminal_remote_app(void* p) {
    UNUSED(p);
    TerminalRemoteApp* app = terminal_remote_app_alloc();
    TerminalRemoteEvent event;

    while(app->running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, furi_ms_to_ticks(500));
        if(status == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                terminal_remote_input_handler(app, &event.input);
            }
        } else if(status == FuriStatusErrorTimeout) {
            // Blink terminal cursor every 500ms
            app->cursor_blink = !app->cursor_blink;
        }
        view_port_update(app->view_port);
    }

    terminal_remote_app_free(app);
    return 0;
}
