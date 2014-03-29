/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MENU_COMMON_H__
#define MENU_COMMON_H__

#include "../general.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../performance.h"
#include "../core_info.h"

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../gfx/fonts/fonts.h"

#define MENU_TEXTURE_FULLSCREEN false

#ifndef __cplusplus
#include <stdbool.h>
#else
extern "C" {
#endif

#include "../file_list.h"

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,
   RGUI_FILE_USE_DIRECTORY,
   RGUI_SETTINGS,
   RGUI_START_SCREEN,

   // settings options are done here too
   RGUI_SETTINGS_OPEN_FILEBROWSER,
   RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE,
   RGUI_SETTINGS_CORE,
   RGUI_SETTINGS_DEFERRED_CORE,
   RGUI_SETTINGS_SAVE_CONFIG,
   RGUI_SETTINGS_CORE_OPTIONS,
   RGUI_SETTINGS_PATH_OPTIONS,
   RGUI_SETTINGS_REWIND_ENABLE,
   RGUI_SETTINGS_REWIND_GRANULARITY,
   RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT,
   RGUI_SETTINGS_SRAM_AUTOSAVE,
   RGUI_SETTINGS_SAVESTATE_SAVE,
   RGUI_SETTINGS_SAVESTATE_LOAD,
   RGUI_SETTINGS_DISK_OPTIONS,
   RGUI_SETTINGS_DISK_INDEX,
   RGUI_SETTINGS_DISK_APPEND,
   RGUI_SETTINGS_DRIVER_VIDEO,
   RGUI_SETTINGS_DRIVER_AUDIO,
   RGUI_SETTINGS_DRIVER_INPUT,
   RGUI_SETTINGS_DRIVER_CAMERA,
   RGUI_SETTINGS_SCREENSHOT,
   RGUI_SETTINGS_GPU_SCREENSHOT,
   RGUI_SCREENSHOT_DIR_PATH,
   RGUI_BROWSER_DIR_PATH,
   RGUI_SAVESTATE_DIR_PATH,
   RGUI_SAVEFILE_DIR_PATH,
   RGUI_LIBRETRO_DIR_PATH,
   RGUI_LIBRETRO_INFO_DIR_PATH,
   RGUI_CONFIG_DIR_PATH,
   RGUI_OVERLAY_DIR_PATH,
   RGUI_SYSTEM_DIR_PATH,
   RGUI_SETTINGS_RESTART_GAME,
   RGUI_SETTINGS_AUDIO_MUTE,
   RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA,
   RGUI_SETTINGS_AUDIO_VOLUME_LEVEL,
   RGUI_SETTINGS_CUSTOM_BGM_CONTROL_ENABLE,
   RGUI_SETTINGS_RSOUND_SERVER_IP_ADDRESS,
   RGUI_SETTINGS_ZIP_EXTRACT,
   RGUI_SETTINGS_DEBUG_TEXT,
   RGUI_SETTINGS_RESTART_EMULATOR,
   RGUI_SETTINGS_RESUME_GAME,
   RGUI_SETTINGS_QUIT_RARCH,

   // Match up with libretro order for simplicity.
   RGUI_SETTINGS_BIND_BEGIN,
   RGUI_SETTINGS_BIND_B = RGUI_SETTINGS_BIND_BEGIN,
   RGUI_SETTINGS_BIND_Y,
   RGUI_SETTINGS_BIND_SELECT,
   RGUI_SETTINGS_BIND_START,
   RGUI_SETTINGS_BIND_UP,
   RGUI_SETTINGS_BIND_DOWN,
   RGUI_SETTINGS_BIND_LEFT,
   RGUI_SETTINGS_BIND_RIGHT,
   RGUI_SETTINGS_BIND_A,
   RGUI_SETTINGS_BIND_X,
   RGUI_SETTINGS_BIND_L,
   RGUI_SETTINGS_BIND_R,
   RGUI_SETTINGS_BIND_L2,
   RGUI_SETTINGS_BIND_R2,
   RGUI_SETTINGS_BIND_L3,
   RGUI_SETTINGS_BIND_R3,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS,
   RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS,
   RGUI_SETTINGS_BIND_LAST = RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS,
   RGUI_SETTINGS_BIND_MENU_TOGGLE = RGUI_SETTINGS_BIND_BEGIN + RARCH_MENU_TOGGLE,
   RGUI_SETTINGS_CUSTOM_BIND,
   RGUI_SETTINGS_CUSTOM_BIND_ALL,
   RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL,

   RGUI_LAKKA_LAUNCH,

   RGUI_SETTINGS_CORE_OPTION_NONE = 0xffff,
   RGUI_SETTINGS_CORE_OPTION_START = 0x10000
} rgui_file_type_t;

typedef enum
{
   RGUI_ACTION_UP,
   RGUI_ACTION_DOWN,
   RGUI_ACTION_LEFT,
   RGUI_ACTION_RIGHT,
   RGUI_ACTION_OK,
   RGUI_ACTION_CANCEL,
   RGUI_ACTION_REFRESH,
   RGUI_ACTION_START,
   RGUI_ACTION_MESSAGE,
   RGUI_ACTION_SCROLL_DOWN,
   RGUI_ACTION_SCROLL_UP,
   RGUI_ACTION_MAPPING_PREVIOUS,
   RGUI_ACTION_MAPPING_NEXT,
   RGUI_ACTION_NOOP
} rgui_action_t;

#define RGUI_MAX_BUTTONS 32
#define RGUI_MAX_AXES 32
#define RGUI_MAX_HATS 4
struct rgui_bind_state_port
{
   bool buttons[RGUI_MAX_BUTTONS];
   int16_t axes[RGUI_MAX_AXES];
   uint16_t hats[RGUI_MAX_HATS];
};

struct rgui_bind_axis_state
{
   // Default axis state.
   int16_t rested_axes[RGUI_MAX_AXES];
   // Locked axis state. If we configured an axis, avoid having the same axis state trigger something again right away.
   int16_t locked_axes[RGUI_MAX_AXES];
};

struct rgui_bind_state
{
   struct retro_keybind *target;
   unsigned begin;
   unsigned last;
   unsigned player;
   struct rgui_bind_state_port state[MAX_PLAYERS];
   struct rgui_bind_axis_state axis_state[MAX_PLAYERS];
   bool skip;
};

void menu_poll_bind_get_rested_axes(struct rgui_bind_state *state);
void menu_poll_bind_state(struct rgui_bind_state *state);
bool menu_poll_find_trigger(struct rgui_bind_state *state, struct rgui_bind_state *new_state);

void lakka_draw(void *data);

typedef struct
{
   uint64_t old_input_state;
   uint64_t trigger_state;
   bool do_held;

   unsigned delay_timer;
   unsigned delay_count;

   file_list_t *menu_stack;
   file_list_t *selection_buf;
   size_t selection_ptr;
   bool msg_force;

   core_info_list_t *core_info;
   bool defer_core;
   char deferred_path[PATH_MAX];

   // Quick jumping indices with L/R.
   // Rebuilt when parsing directory.
   size_t scroll_indices[2 * (26 + 2) + 1];
   unsigned scroll_indices_size;
   unsigned scroll_accel;

   char base_path[PATH_MAX];
   char default_glslp[PATH_MAX];
   char default_cgp[PATH_MAX];

   const uint8_t *font;
   bool alloc_font;

   char libretro_dir[PATH_MAX];
   struct retro_system_info info;
   bool load_no_rom;

   unsigned current_pad;

   rarch_time_t last_time; // Used to throttle RGUI in case VSync is broken.

   struct rgui_bind_state binds;
   struct
   {
      const char **buffer;
      const char *label;
      bool display;
   } keyboard;
} rgui_handle_t;

extern rgui_handle_t *rgui;

typedef struct
{
   char*  name;
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   struct font_output_list out;
} menu_subitem;

typedef struct
{
   char*  name;
   char*  rom;
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   int    active_subitem;
   int num_subitems;
   menu_subitem *subitems;
   struct font_output_list out;
} menu_item;

typedef struct
{
   char*  name;
   char*  libretro;
   GLuint icon;
   float  alpha;
   float  zoom;
   int    active_item;
   int    num_items;
   menu_item *items;
   struct font_output_list out;
} menu_category;

void menu_init(void);
bool menu_iterate(void);
void menu_free(void);

int rgui_input_postprocess(void *data, uint64_t old_state);

void menu_parse_and_resolve(void *data, unsigned menu_type);

bool load_menu_game(void);
extern void load_menu_game_new_core(void);

bool menu_replace_config(const char *path);

bool menu_save_new_config(void);

int menu_settings_toggle_setting(void *data, unsigned setting, unsigned action, unsigned menu_type);
int menu_set_settings(void *data, unsigned setting, unsigned action);
void menu_set_settings_label(char *type_str, size_t type_str_size, unsigned *w, unsigned type);

void menu_populate_entries(void *data, unsigned menu_type);
unsigned menu_type_is(unsigned type);

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

#ifdef __cplusplus
}
#endif

#endif
