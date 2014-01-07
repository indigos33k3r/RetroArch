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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "menu_common.h"

#include "../../gfx/gfx_common.h"
#include "../../performance.h"
#include "../../driver.h"
#include "../../file.h"
#include "../../file_ext.h"
#include "../../input/input_common.h"
#include "../../input/keyboard_line.h"

#include "../../compat/posix_string.h"

 #include "../../gfx/fonts/bitmap.h"

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((rgui->width - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((rgui->height - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

rgui_handle_t *rgui;
const menu_ctx_driver_t *menu_ctx;

//forward decl
static int menu_iterate_func(void *data, unsigned action);

static void blit_line(rgui_handle_t *rgui,
      int x, int y, const char *message, bool green)
{
   int j, i;
   while (*message)
   {
      for (j = 0; j < FONT_HEIGHT; j++)
      {
         for (i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (rgui->font[FONT_OFFSET((unsigned char)*message) + offset] & rem);

            if (col)
            {
               rgui->frame_buf[(y + j) * (rgui->frame_buf_pitch >> 1) + (x + i)] = green ?
               (15 << 0) | (7 << 4) | (15 << 8) | (7 << 12) : 0xFFFF;
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
      message++;
   }
}

static uint16_t gray_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = 0;

   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = 1;
   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
}

static void fill_rect(uint16_t *buf, unsigned pitch,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   unsigned j, i;
   for (j = y; j < y + height; j++)
      for (i = x; i < x + width; i++)
         buf[j * (pitch >> 1) + i] = col(i, j);
}

static void render_background(rgui_handle_t *rgui)
{
   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, 0, rgui->width, rgui->height, gray_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, 0, rgui->width, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, rgui->height-5, rgui->width, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, 0, 5, rgui->height, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         rgui->width - 5, 0, 5, rgui->height, green_filler);
}

static void rgui_render(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (rgui->need_refresh && 
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   size_t begin = rgui->selection_ptr >= TERM_HEIGHT / 2 ?
      rgui->selection_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->selection_ptr + TERM_HEIGHT <= rgui->selection_buf->size ?
      rgui->selection_ptr + TERM_HEIGHT : rgui->selection_buf->size;
   
   // Do not scroll if all items are visible.
   if (rgui->selection_buf->size <= TERM_HEIGHT)
      begin = 0;

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_background(rgui);

   unsigned x, y;
   size_t i;

   x = TERM_START_X;
   y = TERM_START_Y;

   for (i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path = 0;
      unsigned type = 0;
      file_list_get_at_offset(rgui->selection_buf, i, &path, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == rgui->selection_ptr; // SEE

      strlcpy(entry_title_buf, path, sizeof(entry_title_buf));
      strlcpy(type_str_buf, type_str, sizeof(type_str_buf));

      if ((type == RGUI_FILE_PLAIN || type == RGUI_FILE_DIRECTORY))
         menu_ticker_line(entry_title_buf, TERM_WIDTH - (w + 1 + 2), g_extern.frame_count / 15, path, selected);
      else
         menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15, type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '*' : ' ',
            TERM_WIDTH - (w + 1 + 2), TERM_WIDTH - (w + 1 + 2),
            entry_title_buf,
            w,
            type_str_buf);

      blit_line(rgui, x, y, message, selected);
   }
}

unsigned menu_type_is(unsigned type)
{
   unsigned ret = 0;
   bool type_found;

   type_found = 
      type == RGUI_SETTINGS ||
      type == RGUI_SETTINGS_CORE_OPTIONS ||
      type == RGUI_SETTINGS_DISK_OPTIONS ||
      type == RGUI_SETTINGS_PATH_OPTIONS;

   if (type_found)
   {
      ret = RGUI_SETTINGS;
      return ret;
   }

   type_found = type == RGUI_BROWSER_DIR_PATH ||
      type == RGUI_SAVESTATE_DIR_PATH ||
      type == RGUI_LIBRETRO_DIR_PATH ||
      type == RGUI_LIBRETRO_INFO_DIR_PATH ||
      type == RGUI_CONFIG_DIR_PATH ||
      type == RGUI_SAVEFILE_DIR_PATH ||
      type == RGUI_OVERLAY_DIR_PATH ||
      type == RGUI_SCREENSHOT_DIR_PATH ||
      type == RGUI_SYSTEM_DIR_PATH;

   if (type_found)
   {
      ret = RGUI_FILE_DIRECTORY;
      return ret;
   }

   return ret;
}


void load_menu_game_prepare(void)
{
   if (*g_extern.fullpath || rgui->load_no_rom)
   {
      if (*g_extern.fullpath)
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

#ifdef RARCH_CONSOLE
      if (g_extern.system.no_game || *g_extern.fullpath)
#endif
   }

   // redraw RGUI frame
   rgui->old_input_state = rgui->trigger_state = 0;
   rgui->do_held = false;
   rgui->msg_force = true;

   if (menu_ctx)
      menu_iterate_func(rgui, RGUI_ACTION_NOOP);

   // Draw frame for loading message
   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, rgui->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   if (driver.video)
      rarch_render_cached_frame();

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);
}

static void menu_update_libretro_info(void)
{
   *rgui->libretro_dir = '\0';
#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   if (path_is_directory(g_settings.libretro))
      strlcpy(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
   else if (*g_settings.libretro)
   {
      fill_pathname_basedir(rgui->libretro_dir, g_settings.libretro, sizeof(rgui->libretro_dir));
#ifdef HAVE_DYNAMIC
      libretro_get_system_info(g_settings.libretro, &rgui->info, NULL);
#endif
   }

#ifndef HAVE_DYNAMIC
   retro_get_system_info(&rgui->info);
#endif

   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
}

bool load_menu_game(void)
{
   if (g_extern.main_is_init)
      rarch_main_deinit();

   struct rarch_main_wrap args = {0};

   args.verbose       = g_extern.verbose;
   args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
   args.sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
   args.state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
   args.rom_path      = *g_extern.fullpath ? g_extern.fullpath : NULL;
   args.libretro_path = *g_settings.libretro ? g_settings.libretro : NULL;
   args.no_rom        = rgui->load_no_rom;
   rgui->load_no_rom  = false;

   if (rarch_main_init_wrap(&args) == 0)
   {
      RARCH_LOG("rarch_main_init_wrap() succeeded.\n");
      // Update menu state which depends on config.
      menu_update_libretro_info();
      return true;
   }
   else
   {
      char name[PATH_MAX];
      char msg[PATH_MAX];
      fill_pathname_base(name, g_extern.fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      msg_queue_push(g_extern.msg_queue, msg, 1, 90);
      rgui->msg_force = true;
      RARCH_ERR("rarch_main_init_wrap() failed.\n");
      return false;
   }
}

void menu_init(void)
{
   if (!menu_ctx_init_first(&menu_ctx, ((void**)&rgui)))
   {
      RARCH_ERR("Could not initialize menu.\n");
      rarch_fail(1, "menu_init()");
   }

   strlcpy(rgui->base_path, g_settings.rgui_browser_directory, sizeof(rgui->base_path));
   rgui->menu_stack = (file_list_t*)calloc(1, sizeof(file_list_t));
   rgui->selection_buf = (file_list_t*)calloc(1, sizeof(file_list_t));
   file_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);
   rgui->selection_ptr = 0;
   rgui->push_start_screen = g_settings.rgui_show_start_screen;
   g_settings.rgui_show_start_screen = false;
   menu_populate_entries(rgui, RGUI_SETTINGS);

   rgui->trigger_state = 0;
   rgui->old_input_state = 0;
   rgui->do_held = false;
   rgui->frame_buf_show = true;
   rgui->current_pad = 0;

   menu_update_libretro_info();

   rgui->last_time = rarch_get_time_usec();
}

void menu_free(void)
{
   if (menu_ctx && menu_ctx->free)
      menu_ctx->free(rgui);

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   file_list_free(rgui->menu_stack);
   file_list_free(rgui->selection_buf);

   core_info_list_free(rgui->core_info);

   free(rgui);
}

void menu_ticker_line(char *buf, size_t len, unsigned index, const char *str, bool selected)
{
   size_t str_len = strlen(str);
   if (str_len <= len)
   {
      strlcpy(buf, str, len + 1);
      return;
   }

   if (!selected)
   {
      strlcpy(buf, str, len + 1 - 3);
      strlcat(buf, "...", len + 1);
   }
   else
   {
      // Wrap long strings in options with some kind of ticker line.
      unsigned ticker_period = 2 * (str_len - len) + 4;
      unsigned phase = index % ticker_period;

      unsigned phase_left_stop = 2;
      unsigned phase_left_moving = phase_left_stop + (str_len - len);
      unsigned phase_right_stop = phase_left_moving + 2;

      unsigned left_offset = phase - phase_left_stop;
      unsigned right_offset = (str_len - len) - (phase - phase_right_stop);

      // Ticker period: [Wait at left (2 ticks), Progress to right (type_len - w), Wait at right (2 ticks), Progress to left].
      if (phase < phase_left_stop)
         strlcpy(buf, str, len + 1);
      else if (phase < phase_left_moving)
         strlcpy(buf, str + left_offset, len + 1);
      else if (phase < phase_right_stop)
         strlcpy(buf, str + str_len - len, len + 1);
      else
         strlcpy(buf, str + right_offset, len + 1);
   }
}

#ifdef HAVE_MENU
static uint64_t menu_input(void)
{
   unsigned i;
   uint64_t input_state = 0;

#ifdef HW_RVL
   static const struct retro_keybind *binds[] = { g_settings.input.menu_binds };
#else
   static const struct retro_keybind *binds[] = { g_settings.input.binds[0] };
#endif

   for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_R2; i++)
   {
      input_state |= input_input_state_func(binds,
            0, RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
   }

   input_state |= input_key_pressed_func(RARCH_MENU_TOGGLE) ? (1ULL << RARCH_MENU_TOGGLE) : 0;

   rgui->trigger_state = input_state & ~rgui->old_input_state;

   rgui->do_held = (input_state & (
            (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_R)
            )) && !(input_state & (1ULL << RARCH_MENU_TOGGLE));

   return input_state;
}

// This only makes sense for PC so far.
// Consoles use set_keybind callbacks instead.
static int menu_custom_bind_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   (void)action; // Have to ignore action here. Only bind that should work here is Quit RetroArch or something like that.

   if (menu_ctx)
      rgui_render(rgui);

   char msg[256];
   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)", input_config_bind_map[rgui->binds.begin - RGUI_SETTINGS_BIND_BEGIN].desc);

   struct rgui_bind_state binds = rgui->binds;
   menu_poll_bind_state(&binds);

   if ((binds.skip && !rgui->binds.skip) || menu_poll_find_trigger(&rgui->binds, &binds))
   {
      binds.begin++;
      if (binds.begin <= binds.last)
         binds.target++;
      else
         file_list_pop(rgui->menu_stack, &rgui->selection_ptr);

      // Avoid new binds triggering things right away.
      rgui->trigger_state = 0;
      rgui->old_input_state = -1ULL;
   }
   rgui->binds = binds;
   return 0;
}

static int menu_settings_iterate(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   rgui->frame_buf_pitch = rgui->width * 2;
   unsigned type = 0;
   const char *label = NULL;
   if (action != RGUI_ACTION_REFRESH)
      file_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &label, &type);

   if (type == RGUI_SETTINGS_CORE)
   {
#if defined(HAVE_DYNAMIC)
      label = rgui->libretro_dir;
#elif defined(HAVE_LIBRETRO_MANAGEMENT)
      label = default_paths.core_dir;
#else
      label = ""; // Shouldn't happen ...
#endif
   }
   else if (type == RGUI_SETTINGS_DISK_APPEND)
      label = rgui->base_path;

   const char *dir = NULL;
   unsigned menu_type = 0;
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh)
      action = RGUI_ACTION_NOOP;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr > 0)
            rgui->selection_ptr--;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + 1 < rgui->selection_buf->size)
            rgui->selection_ptr++;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_LEFT:
      case RGUI_ACTION_RIGHT:
      case RGUI_ACTION_OK:
      case RGUI_ACTION_START:
         if ((type == RGUI_SETTINGS_OPEN_FILEBROWSER || type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE)
               && action == RGUI_ACTION_OK)
         {
            rgui->defer_core = type == RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE;
            file_list_push(rgui->menu_stack, rgui->base_path, RGUI_FILE_DIRECTORY, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else if ((menu_type_is(type) == RGUI_SETTINGS || type == RGUI_SETTINGS_CORE || type == RGUI_SETTINGS_DISK_APPEND) && action == RGUI_ACTION_OK)
         {
            file_list_push(rgui->menu_stack, label, type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         break;

      case RGUI_ACTION_REFRESH:
         rgui->selection_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }

   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh && !(menu_type == RGUI_FILE_DIRECTORY ||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_DISK_APPEND))
   {
      rgui->need_refresh = false;
      if (
            menu_type == RGUI_SETTINGS_DISK_OPTIONS
            )
         menu_populate_entries(rgui, menu_type);
      else
         menu_populate_entries(rgui, RGUI_SETTINGS);
   }

   if (menu_ctx)
      rgui_render(rgui);

   return 0;
}

static inline void menu_descend_alphabet(void *data, size_t *ptr_out)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == 0)
      return;
   size_t i = rgui->scroll_indices_size - 1;
   while (i && rgui->scroll_indices[i - 1] >= ptr)
      i--;
   *ptr_out = rgui->scroll_indices[i - 1];
}

static inline void menu_ascend_alphabet(void *data, size_t *ptr_out)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (!rgui->scroll_indices_size)
      return;
   size_t ptr = *ptr_out;
   if (ptr == rgui->scroll_indices[rgui->scroll_indices_size - 1])
      return;
   size_t i = 0;
   while (i < rgui->scroll_indices_size - 1 && rgui->scroll_indices[i + 1] <= ptr)
      i++;
   *ptr_out = rgui->scroll_indices[i + 1];
}

static void menu_flush_stack_type(void *data, unsigned final_type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   unsigned type;
   type = 0;
   rgui->need_refresh = true;
   file_list_get_last(rgui->menu_stack, NULL, &type);
   while (type != final_type)
   {
      file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
      file_list_get_last(rgui->menu_stack, NULL, &type);
   }
}

void load_menu_game_new_core(void)
{
#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
   libretro_get_system_info(g_settings.libretro, &rgui->info,
         &rgui->load_no_rom);

   g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
#else
   rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
}

static int menu_iterate_func(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const char *dir = 0;
   unsigned menu_type = 0;
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);
   int ret = 0;

   if (menu_ctx && menu_ctx->set_texture)
      menu_ctx->set_texture(rgui, false);

   if (menu_type_is(menu_type) == RGUI_SETTINGS)
      return menu_settings_iterate(rgui, action);
   else if (menu_type == RGUI_SETTINGS_CUSTOM_BIND)
      return menu_custom_bind_iterate(rgui, action);

   if (rgui->need_refresh && action != RGUI_ACTION_MESSAGE)
      action = RGUI_ACTION_NOOP;

   unsigned scroll_speed = (max(rgui->scroll_accel, 2) - 2) / 4 + 1;
   unsigned fast_scroll_speed = 4 + 4 * scroll_speed;

   switch (action)
   {
      case RGUI_ACTION_UP:
         if (rgui->selection_ptr >= scroll_speed)
            rgui->selection_ptr -= scroll_speed;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_DOWN:
         if (rgui->selection_ptr + scroll_speed < rgui->selection_buf->size)
            rgui->selection_ptr += scroll_speed;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_LEFT:
         if (rgui->selection_ptr > fast_scroll_speed)
            rgui->selection_ptr -= fast_scroll_speed;
         else
            rgui->selection_ptr = 0;
         break;

      case RGUI_ACTION_RIGHT:
         if (rgui->selection_ptr + fast_scroll_speed < rgui->selection_buf->size)
            rgui->selection_ptr += fast_scroll_speed;
         else
            rgui->selection_ptr = rgui->selection_buf->size - 1;
         break;

      case RGUI_ACTION_SCROLL_UP:
         menu_descend_alphabet(rgui, &rgui->selection_ptr);
         break;
      case RGUI_ACTION_SCROLL_DOWN:
         menu_ascend_alphabet(rgui, &rgui->selection_ptr);
         break;
      
      case RGUI_ACTION_CANCEL:
         if (rgui->menu_stack->size > 1)
         {
            rgui->need_refresh = true;
            file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         }
         break;

      case RGUI_ACTION_OK:
      {
         if (rgui->selection_buf->size == 0)
            return 0;

         const char *path = 0;
         unsigned type = 0;
         file_list_get_at_offset(rgui->selection_buf, rgui->selection_ptr, &path, &type);

         if (
               menu_type_is(type) == RGUI_FILE_DIRECTORY ||
               type == RGUI_SETTINGS_CORE ||
               type == RGUI_SETTINGS_DISK_APPEND ||
               type == RGUI_FILE_DIRECTORY)
         {
            char cat_path[PATH_MAX];
            fill_pathname_join(cat_path, dir, path, sizeof(cat_path));

            file_list_push(rgui->menu_stack, cat_path, type, rgui->selection_ptr);
            rgui->selection_ptr = 0;
            rgui->need_refresh = true;
         }
         else
         {
            if (menu_type == RGUI_SETTINGS_DEFERRED_CORE)
            {
               // FIXME: Add for consoles.
               strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
               strlcpy(g_extern.fullpath, rgui->deferred_path, sizeof(g_extern.fullpath));

               load_menu_game_new_core(); // HERE
               rgui->msg_force = true;
               ret = -1;
               menu_flush_stack_type(rgui, RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_CORE)
            {
               fill_pathname_join(g_settings.libretro, dir, path, sizeof(g_settings.libretro));
               libretro_free_system_info(&rgui->info);
               libretro_get_system_info(g_settings.libretro, &rgui->info,
                     &rgui->load_no_rom);

               // No ROM needed for this core, load game immediately.
               if (rgui->load_no_rom)
               {
                  g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
                  *g_extern.fullpath = '\0';
                  rgui->msg_force = true;
                  ret = -1;
               }

               menu_flush_stack_type(rgui, RGUI_SETTINGS);
            }
            else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
            {
               char image[PATH_MAX];
               fill_pathname_join(image, dir, path, sizeof(image));
               rarch_disk_control_append_image(image);

               g_extern.lifecycle_state |= 1ULL << MODE_GAME;

               menu_flush_stack_type(rgui, RGUI_SETTINGS);
               ret = -1;
            }
            else if (menu_type == RGUI_BROWSER_DIR_PATH)
            {
               strlcpy(g_settings.rgui_browser_directory, dir, sizeof(g_settings.rgui_browser_directory));
               strlcpy(rgui->base_path, dir, sizeof(rgui->base_path));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#ifdef HAVE_SCREENSHOTS
            else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
            {
               strlcpy(g_settings.screenshot_directory, dir, sizeof(g_settings.screenshot_directory));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
#endif
            else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
            {
               strlcpy(g_extern.savefile_dir, dir, sizeof(g_extern.savefile_dir));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
            {
               strlcpy(g_extern.savestate_dir, dir, sizeof(g_extern.savestate_dir));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
            {
               strlcpy(rgui->libretro_dir, dir, sizeof(rgui->libretro_dir));
               menu_init_core_info(rgui);
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_CONFIG_DIR_PATH)
            {
               strlcpy(g_settings.rgui_config_directory, dir, sizeof(g_settings.rgui_config_directory));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_LIBRETRO_INFO_DIR_PATH)
            {
               strlcpy(g_settings.libretro_info_path, dir, sizeof(g_settings.libretro_info_path));
               menu_init_core_info(rgui);
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else if (menu_type == RGUI_SYSTEM_DIR_PATH)
            {
               strlcpy(g_settings.system_directory, dir, sizeof(g_settings.system_directory));
               menu_flush_stack_type(rgui, RGUI_SETTINGS_PATH_OPTIONS);
            }
            else
            {
               if (rgui->defer_core)
               {
                  fill_pathname_join(rgui->deferred_path, dir, path, sizeof(rgui->deferred_path));

                  const core_info_t *info = NULL;
                  size_t supported = 0;
                  if (rgui->core_info)
                     core_info_list_get_supported_cores(rgui->core_info, rgui->deferred_path, &info, &supported);

                  if (supported == 1) // Can make a decision right now.
                  {
                     printf(g_extern.fullpath);
                     printf("---");
                     printf(g_settings.libretro);
                     printf("---");
                     printf(rgui->deferred_path);


                     strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/sonic3.smd", sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-genplus.so", sizeof(g_settings.libretro));

                     //libretro_free_system_info(&rgui->info);
                     /*libretro_get_system_info(g_settings.libretro, &rgui->info,
                           &rgui->load_no_rom);*/
                     g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);

                     menu_flush_stack_type(rgui, RGUI_SETTINGS);
                     rgui->msg_force = true;
                     ret = -1;
                  }
                  else // Present a selection.
                  {
                     file_list_push(rgui->menu_stack, rgui->libretro_dir, RGUI_SETTINGS_DEFERRED_CORE, rgui->selection_ptr);
                     rgui->selection_ptr = 0;
                     rgui->need_refresh = true;
                  }
               }
               else
               {
                  fill_pathname_join(g_extern.fullpath, dir, path, sizeof(g_extern.fullpath));
                  g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);

                  menu_flush_stack_type(rgui, RGUI_SETTINGS);
                  rgui->msg_force = true;
                  ret = -1;
               }
            }
         }
         break;
      }

      case RGUI_ACTION_REFRESH:
         rgui->selection_ptr = 0;
         rgui->need_refresh = true;
         break;

      case RGUI_ACTION_MESSAGE:
         rgui->msg_force = true;
         break;

      default:
         break;
   }


   // refresh values in case the stack changed
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (rgui->need_refresh && (menu_type == RGUI_FILE_DIRECTORY ||
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY || 
            menu_type == RGUI_SETTINGS_DEFERRED_CORE ||
            menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_DISK_APPEND))
   {
      rgui->need_refresh = false;
      menu_parse_and_resolve(rgui, menu_type);
   }

   if (menu_ctx && menu_ctx->iterate)
      menu_ctx->iterate(rgui, action);

   if (menu_ctx)
      rgui_render(rgui);

   return ret;
}

bool menu_iterate(void)
{
   rarch_time_t time, delta, target_msec, sleep_msec;
   unsigned action;
   static bool initial_held = true;
   static bool first_held = false;
   uint64_t input_state = 0;
   int input_entry_ret = 0;
   int ret;

   if (g_extern.lifecycle_state & (1ULL << MODE_MENU_PREINIT))
   {
      rgui->need_refresh = true;
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_PREINIT);
      rgui->old_input_state |= 1ULL << RARCH_MENU_TOGGLE;
   }

   rarch_check_block_hotkey();
   rarch_input_poll();

   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      goto deinit;
   }

   input_state = menu_input();

   if (rgui->do_held)
   {
      if (!first_held)
      {
         first_held = true;
         rgui->delay_timer = initial_held ? 12 : 6;
         rgui->delay_count = 0;
      }

      if (rgui->delay_count >= rgui->delay_timer)
      {
         first_held = false;
         rgui->trigger_state = input_state;
         rgui->scroll_accel = min(rgui->scroll_accel + 1, 64);
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
      rgui->scroll_accel = 0;
   }

   rgui->delay_count++;
   rgui->old_input_state = input_state;

   if (driver.block_input)
      rgui->trigger_state = 0;

   action = RGUI_ACTION_NOOP;

   // don't run anything first frame, only capture held inputs for old_input_state
   if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      action = RGUI_ACTION_UP;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
      action = RGUI_ACTION_SCROLL_UP;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
      action = RGUI_ACTION_SCROLL_DOWN;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      action = RGUI_ACTION_CANCEL;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      action = RGUI_ACTION_OK;
   else if (rgui->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      action = RGUI_ACTION_START;

   if (menu_ctx)
      input_entry_ret = menu_iterate_func(rgui, action);

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, rgui->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   rarch_render_cached_frame();

   // Throttle in case VSync is broken (avoid 1000+ FPS RGUI).
   time = rarch_get_time_usec();
   delta = (time - rgui->last_time) / 1000;
   target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   sleep_msec = target_msec - delta;
   if (sleep_msec > 0)
      rarch_sleep((unsigned int)sleep_msec);
   rgui->last_time = rarch_get_time_usec();

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);

   ret = rgui_input_postprocess(rgui, rgui->old_input_state);

   if (ret < 0)
   {
      unsigned type = 0;
      file_list_get_last(rgui->menu_stack, NULL, &type);
      while (type != RGUI_SETTINGS)
      {
         file_list_pop(rgui->menu_stack, &rgui->selection_ptr);
         file_list_get_last(rgui->menu_stack, NULL, &type);
      }
   }

   if (ret || input_entry_ret)
      goto deinit;

   return true;

deinit:
   return false;
}
#endif

// Quite intrusive and error prone.
// Likely to have lots of small bugs.
// Cleanly exit the main loop to ensure that all the tiny details get set properly.
// This should mitigate most of the smaller bugs.
bool menu_replace_config(const char *path)
{
   if (strcmp(path, g_extern.config_path) == 0)
      return false;

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;

   // Load dummy core.
   *g_extern.fullpath = '\0';
   *g_settings.libretro = '\0'; // Load core in new config.
   g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
   rgui->load_no_rom = false;

   return true;
}

// Save a new config to a file. Filename is based on heuristics to avoid typing.
bool menu_save_new_config(void)
{
   char config_dir[PATH_MAX];
   *config_dir = '\0';

   if (*g_settings.rgui_config_directory)
      strlcpy(config_dir, g_settings.rgui_config_directory, sizeof(config_dir));
   else if (*g_extern.config_path) // Fallback
      fill_pathname_basedir(config_dir, g_extern.config_path, sizeof(config_dir));
   else
   {
      const char *msg = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_ERR("%s\n", msg);
      return false;
   }

   bool found_path = false;
   char config_name[PATH_MAX];
   char config_path[PATH_MAX];
   if (*g_settings.libretro && !path_is_directory(g_settings.libretro) && path_file_exists(g_settings.libretro)) // Infer file name based on libretro core.
   {
      unsigned i;
      // In case of collision, find an alternative name.
      for (i = 0; i < 16; i++)
      {
         fill_pathname_base(config_name, g_settings.libretro, sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));

         char tmp[64];
         *tmp = '\0';
         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   // Fallback to system time ...
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));
   }

   char msg[512];
   bool ret;
   if (config_save_file(config_path))
   {
      strlcpy(g_extern.config_path, config_path, sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".", config_path);
      RARCH_LOG("%s\n", msg);
      ret = true;
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".", config_path);
      RARCH_ERR("%s\n", msg);
      ret = false;
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   return ret;
}

void menu_poll_bind_state(struct rgui_bind_state *state)
{
   unsigned i, b, a, h;
   memset(state->state, 0, sizeof(state->state));
   state->skip = input_input_state_func(NULL, 0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   input_joypad_poll(joypad);
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (b = 0; b < RGUI_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);
      for (h = 0; h < RGUI_MAX_HATS; h++)
      {
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h) ? HAT_UP_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h) ? HAT_DOWN_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h) ? HAT_LEFT_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h) ? HAT_RIGHT_MASK : 0;
      }
   }
}

void menu_poll_bind_get_rested_axes(struct rgui_bind_state *state)
{
   unsigned i, a;
   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] = input_joypad_axis_raw(joypad, i, a);
}

static bool menu_poll_find_trigger_pad(struct rgui_bind_state *state, struct rgui_bind_state *new_state, unsigned p)
{
   unsigned a, b, h;
   const struct rgui_bind_state_port *n = &new_state->state[p];
   const struct rgui_bind_state_port *o = &state->state[p];

   for (b = 0; b < RGUI_MAX_BUTTONS; b++)
   {
      if (n->buttons[b] && !o->buttons[b])
      {
         state->target->joykey = b;
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   // Axes are a bit tricky ...
   for (a = 0; a < RGUI_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] - new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] - new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000) // Take care of case where axis rests on +/- 0x7fff (e.g. 360 controller on Linux)
      {
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         // Lock the current axis.
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0 ? 0x7fff : -0x7fff; 
         return true;
      }

      if (locked_distance >= 20000) // Unlock the axis.
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < RGUI_MAX_HATS; h++)
   {
      uint16_t trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;
      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         state->target->joykey = HAT_MAP(h, sane_trigger);
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

bool menu_poll_find_trigger(struct rgui_bind_state *state, struct rgui_bind_state *new_state)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (menu_poll_find_trigger_pad(state, new_state, i))
      {
         g_settings.input.joypad_map[state->player] = i; // Update the joypad mapping automatically. More friendly that way.
         return true;
      }
   }
   return false;
}

static void menu_search_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      file_list_search(rgui->selection_buf, str, &rgui->selection_ptr);
   rgui->keyboard.display = false;
   rgui->keyboard.label = NULL;
   rgui->old_input_state = -1ULL; // Avoid triggering states on pressing return.
}

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
   {
      rgui->keyboard.display = true;
      rgui->keyboard.label = "Search:";
      rgui->keyboard.buffer = input_keyboard_start_line(rgui, menu_search_callback);
   }
}

static inline int menu_list_get_first_char(file_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   file_list_get_alt_at_offset(buf, offset, &path);
   int ret = tolower(*path);
  
   // "Normalize" non-alphabetical entries so they are lumped together for purposes of jumping.
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static inline bool menu_list_elem_is_dir(file_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   unsigned type = 0;
   file_list_get_at_offset(buf, offset, &path, &type);
   return type != RGUI_FILE_PLAIN;
}

void menu_populate_entries(void *data, unsigned menu_type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   unsigned i, last;

   switch (menu_type)
   {
      case RGUI_SETTINGS_DISK_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Disk Index", RGUI_SETTINGS_DISK_INDEX, 0);
         file_list_push(rgui->selection_buf, "Disk Image Append", RGUI_SETTINGS_DISK_APPEND, 0);
         break;
      case RGUI_SETTINGS_PATH_OPTIONS:
         file_list_clear(rgui->selection_buf);
         file_list_push(rgui->selection_buf, "Browser Directory", RGUI_BROWSER_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Config Directory", RGUI_CONFIG_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Core Directory", RGUI_LIBRETRO_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Core Info Directory", RGUI_LIBRETRO_INFO_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Savestate Directory", RGUI_SAVESTATE_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Savefile Directory", RGUI_SAVEFILE_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "System Directory", RGUI_SYSTEM_DIR_PATH, 0);
         file_list_push(rgui->selection_buf, "Screenshot Directory", RGUI_SCREENSHOT_DIR_PATH, 0);
         break;
      case RGUI_SETTINGS:
         file_list_clear(rgui->selection_buf);

         file_list_push(rgui->selection_buf, "Core", RGUI_SETTINGS_CORE, 0);
         if (rgui->core_info && core_info_list_num_info_files(rgui->core_info))
            file_list_push(rgui->selection_buf, "Load Content (Detect Core)", RGUI_SETTINGS_OPEN_FILEBROWSER_DEFERRED_CORE, 0);

         if (rgui->info.library_name || g_extern.system.info.library_name)
         {
            char load_game_core_msg[64];
            snprintf(load_game_core_msg, sizeof(load_game_core_msg), "Load Content (%s)",
                  rgui->info.library_name ? rgui->info.library_name : g_extern.system.info.library_name);
            file_list_push(rgui->selection_buf, load_game_core_msg, RGUI_SETTINGS_OPEN_FILEBROWSER, 0);
         }

         file_list_push(rgui->selection_buf, "Lakka", RGUI_LAKKA_LAUNCH, 0);

         if (g_extern.main_is_init && !g_extern.libretro_dummy)
         {
            file_list_push(rgui->selection_buf, "Save State", RGUI_SETTINGS_SAVESTATE_SAVE, 0);
            file_list_push(rgui->selection_buf, "Load State", RGUI_SETTINGS_SAVESTATE_LOAD, 0);
            file_list_push(rgui->selection_buf, "Take Screenshot", RGUI_SETTINGS_SCREENSHOT, 0);
            file_list_push(rgui->selection_buf, "Resume Content", RGUI_SETTINGS_RESUME_GAME, 0);
            file_list_push(rgui->selection_buf, "Restart Content", RGUI_SETTINGS_RESTART_GAME, 0);

         }
         file_list_push(rgui->selection_buf, "Restart RetroArch", RGUI_SETTINGS_RESTART_EMULATOR, 0);
         file_list_push(rgui->selection_buf, "Save New Config", RGUI_SETTINGS_SAVE_CONFIG, 0);
         file_list_push(rgui->selection_buf, "Quit RetroArch", RGUI_SETTINGS_QUIT_RARCH, 0);
         break;
   }
}

void menu_parse_and_resolve(void *data, unsigned menu_type)
{
   const core_info_t *info = NULL;
   const char *dir;
   size_t i, list_size;
   file_list_t *list;
   rgui_handle_t *rgui;

   rgui = (rgui_handle_t*)data;
   dir = NULL;

   file_list_clear(rgui->selection_buf);

   // parsing switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_DEFERRED_CORE:
         break;
      default:
         {
            /* Directory parse */
            file_list_get_last(rgui->menu_stack, &dir, &menu_type);

            if (!*dir)
            {
               file_list_push(rgui->selection_buf, "/", menu_type, 0);
               return;
            }

            const char *exts;
            char ext_buf[1024];
            if (menu_type == RGUI_SETTINGS_CORE)
               exts = EXT_EXECUTABLES;
            else if (menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               exts = ""; // we ignore files anyway
            else if (rgui->defer_core)
               exts = rgui->core_info ? core_info_list_get_all_extensions(rgui->core_info) : "";
            else if (rgui->info.valid_extensions)
            {
               exts = ext_buf;
               if (*rgui->info.valid_extensions)
                  snprintf(ext_buf, sizeof(ext_buf), "%s|zip", rgui->info.valid_extensions);
               else
                  *ext_buf = '\0';
            }
            else
               exts = g_extern.system.valid_extensions;

            struct string_list *list = dir_list_new(dir, exts, true);
            if (!list)
               return;

            dir_list_sort(list, true);

            if (menu_type_is(menu_type) == RGUI_FILE_DIRECTORY)
               file_list_push(rgui->selection_buf, "<Use this directory>", RGUI_FILE_USE_DIRECTORY, 0);

            for (i = 0; i < list->size; i++)
            {
               bool is_dir = list->elems[i].attr.b;

               if ((menu_type_is(menu_type) == RGUI_FILE_DIRECTORY) && !is_dir)
                  continue;

#ifdef HAVE_LIBRETRO_MANAGEMENT
               if (menu_type == RGUI_SETTINGS_CORE && (is_dir || strcasecmp(list->elems[i].data, SALAMANDER_FILE) == 0))
                  continue;
#endif

               // Need to preserve slash first time.
               const char *path = list->elems[i].data;
               if (*dir)
                  path = path_basename(path);

               // Push menu_type further down in the chain.
               // Needed for shader manager currently.
               file_list_push(rgui->selection_buf, path,
                     is_dir ? menu_type : RGUI_FILE_PLAIN, 0);
            }

            string_list_free(list);
         }
   }

   // resolving switch
   switch (menu_type)
   {
      case RGUI_SETTINGS_CORE:
         dir = NULL;
         list = (file_list_t*)rgui->selection_buf;
         file_list_get_last(rgui->menu_stack, &dir, &menu_type);
         list_size = list->size;
         for (i = 0; i < list_size; i++)
         {
            const char *path;
            unsigned type = 0;
            file_list_get_at_offset(list, i, &path, &type);
            if (type != RGUI_FILE_PLAIN)
               continue;

            char core_path[PATH_MAX];
            fill_pathname_join(core_path, dir, path, sizeof(core_path));

            char display_name[256];
            if (rgui->core_info &&
                  core_info_list_get_display_name(rgui->core_info,
                     core_path, display_name, sizeof(display_name)))
               file_list_set_alt_at_offset(list, i, display_name);
         }
         file_list_sort_on_alt(rgui->selection_buf);
         break;
      case RGUI_SETTINGS_DEFERRED_CORE:
         core_info_list_get_supported_cores(rgui->core_info, rgui->deferred_path, &info, &list_size);
         for (i = 0; i < list_size; i++)
         {
            file_list_push(rgui->selection_buf, info[i].path, RGUI_FILE_PLAIN, 0);
            file_list_set_alt_at_offset(rgui->selection_buf, i, info[i].display_name);
         }
         file_list_sort_on_alt(rgui->selection_buf);
         break;
      default:
         (void)0;
   }

   rgui->scroll_indices_size = 0;

   // Before a refresh, we could have deleted a file on disk, causing
   // selection_ptr to suddendly be out of range. Ensure it doesn't overflow.
   if (rgui->selection_ptr >= rgui->selection_buf->size && rgui->selection_buf->size)
      rgui->selection_ptr = rgui->selection_buf->size - 1;
   else if (!rgui->selection_buf->size)
      rgui->selection_ptr = 0;
}

void menu_init_core_info(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
}
