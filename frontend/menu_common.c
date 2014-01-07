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

#include "../gfx/gfx_common.h"
#include "../performance.h"
#include "../driver.h"
#include "../input/input_common.h"
#include "../input/keyboard_line.h"

#include "../compat/posix_string.h"

#include "../gfx/fonts/bitmap.h"

static uint16_t menu_framebuf[1440 * 900];

#define TERM_START_X 15
#define TERM_START_Y 15
#define TERM_WIDTH (((rgui->width - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((rgui->height - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

rgui_handle_t *rgui;

int num_items;
int active_item;

//forward decl
static int menu_iterate_func(void *data, unsigned action);

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int y, x;
   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static void init_font(rgui_handle_t *rgui, const uint8_t *font_bmp_buf)
{
   unsigned i;
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));
   rgui->alloc_font = true;
   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   rgui->font = font;
}

static bool rguidisp_init_font(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;
   bool ret = true;

   if (font_bmp_buf)
      init_font(rgui, font_bmp_buf);
   else if (font_bin_buf)
      rgui->font = font_bin_buf;
   else
      ret = false;

   return ret;
}

static rgui_handle_t *rgui_init(void)
{
   uint16_t *framebuf = menu_framebuf;
   size_t framebuf_pitch;

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   rgui->frame_buf = framebuf;
   rgui->width = 1440;
   rgui->height = 900;
   framebuf_pitch = rgui->width * sizeof(uint16_t);

   rgui->frame_buf_pitch = framebuf_pitch;

   bool ret = rguidisp_init_font(rgui);

   if (!ret)
   {
      RARCH_ERR("No font bitmap or binary, abort");
      /* TODO - should be refactored - perhaps don't do rarch_fail but instead
       * exit program */
      g_extern.lifecycle_state &= ~((1ULL << MODE_MENU) | (1ULL << MODE_GAME));

      RARCH_ERR("Could not initialize menu.\n");
      rarch_fail(1, "menu_init()");
   }

   return rgui;
}

int rgui_input_postprocess(void *data, uint64_t old_state)
{
   (void)data;

   int ret = 0;

   if ((rgui->trigger_state & (1ULL << RARCH_MENU_TOGGLE)) &&
         g_extern.main_is_init &&
         !g_extern.libretro_dummy)
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      ret = -1;
   }

   return ret;
}

void rgui_set_texture(void *data, bool enable)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_frame(driver.video_data, menu_framebuf,
            enable, rgui->width, rgui->height, 1.0f);
}

static void rgui_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   if (rgui->alloc_font)
      free((uint8_t*)rgui->font);
}

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

static uint16_t hightlight_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = 1;
   return (col << 10) | (col << 9) | (col << 5) | (12 << 0);
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

   for (int i = 0; i < 3; i++)
   {
      if (i == active_item)
      {
         fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
            100 + (200*i), 100, 100, 100, hightlight_filler);
      }
      else
      {
         fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
            100 + (200*i), 100, 100, 100, green_filler);
      }
   }

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

   if (type == RGUI_SETTINGS)
   {
      return RGUI_SETTINGS;
   }
   else if (type == RGUI_SYSTEM_DIR_PATH)
   {
      return RGUI_FILE_DIRECTORY;
   }

   return 0;
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
   }

   // redraw RGUI frame
   rgui->old_input_state = rgui->trigger_state = 0;
   rgui->do_held = false;
   rgui->msg_force = true;

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
   active_item = 0;
   num_items = 3;

   rgui = rgui_init();

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
   rgui_free(rgui);

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

      case RGUI_ACTION_LEFT:
         if (active_item > 0)
         {
            active_item--;
         }
         break;

      case RGUI_ACTION_RIGHT:
         if (active_item < num_items - 1)
         {
            active_item++;
         }
         break;

      case RGUI_ACTION_OK:
         if (active_item == 0) {
            strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/sonic3.smd", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-genplus.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
         } 
         else if (active_item == 1)
         {
            strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/zelda.smc", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-snes9x-next.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
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
            menu_type_is(menu_type) == RGUI_FILE_DIRECTORY))
   {
      rgui->need_refresh = false;
      menu_populate_entries(rgui, RGUI_SETTINGS);
   }

   rgui_render(rgui);

   return 0;
}

static int menu_iterate_func(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   rgui_set_texture(rgui, false);

   return menu_settings_iterate(rgui, action);
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

   input_entry_ret = menu_iterate_func(rgui, action); // ret = -1 to launch

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

void menu_populate_entries(void *data, unsigned menu_type)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   file_list_clear(rgui->selection_buf);
   file_list_push(rgui->selection_buf, "Sonic 3", RGUI_LAKKA_LAUNCH, 0);
   file_list_push(rgui->selection_buf, "Lakka",   RGUI_LAKKA_LAUNCH, 0);

   if (g_extern.main_is_init && !g_extern.libretro_dummy)
   {
      file_list_push(rgui->selection_buf, "Save State", RGUI_SETTINGS_SAVESTATE_SAVE, 0);
      file_list_push(rgui->selection_buf, "Load State", RGUI_SETTINGS_SAVESTATE_LOAD, 0);
      file_list_push(rgui->selection_buf, "Take Screenshot", RGUI_SETTINGS_SCREENSHOT, 0);
      file_list_push(rgui->selection_buf, "Resume Content", RGUI_SETTINGS_RESUME_GAME, 0);
      file_list_push(rgui->selection_buf, "Restart Content", RGUI_SETTINGS_RESTART_GAME, 0);
   }
}

void menu_parse_and_resolve(void *data, unsigned menu_type)
{
   const char *dir;
   size_t i;
   rgui_handle_t *rgui;

   rgui = (rgui_handle_t*)data;
   dir = NULL;

   file_list_clear(rgui->selection_buf);

   /* Directory parse */
   file_list_get_last(rgui->menu_stack, &dir, &menu_type);

   struct string_list *list = dir_list_new(dir, ".smd|.sms", true);
   if (!list)
      return;

   dir_list_sort(list, true);

   for (i = 0; i < list->size; i++)
   {
      bool is_dir = list->elems[i].attr.b;

      if ((menu_type_is(menu_type) == RGUI_FILE_DIRECTORY) && !is_dir)
         continue;

      // Need to preserve slash first time.
      const char *path = list->elems[i].data;
      if (*dir)
         path = path_basename(path);

      // Push menu_type further down in the chain.
      file_list_push(rgui->selection_buf, path,
            is_dir ? menu_type : RGUI_FILE_PLAIN, 0);
   }

   string_list_free(list);


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
