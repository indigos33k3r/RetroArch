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

static uint16_t menu_framebuf[1440 * 900];

rgui_handle_t *rgui;

int num_items;
int active_item;

//forward decl
static int menu_iterate_func(void *data, unsigned action);

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
}

static uint16_t black_filler(unsigned x, unsigned y)
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

static void rgui_render(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (rgui->need_refresh && 
         (g_extern.lifecycle_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
      0, 0, rgui->width, rgui->height, black_filler);

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

   // A game is loaded, display its menu
   if (g_extern.main_is_init && !g_extern.libretro_dummy)
   {
      fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
            0, 0, 10, 10, hightlight_filler);
   }
}

void load_menu_game_prepare(void)
{
   // redraw RGUI frame
   rgui->old_input_state = rgui->trigger_state = 0;
   rgui->do_held = false;
   rgui->msg_force = true;

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
   file_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);

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

   core_info_list_free(rgui->core_info);

   free(rgui);
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

static int menu_iterate_func(void *data, unsigned action)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   rgui_set_texture(rgui, false);

   switch (action)
   {
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

      default:
         break;
   }

   rgui->need_refresh = false;

   rgui_render(rgui);

   return 0;
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

   if (ret || input_entry_ret)
      goto deinit;

   return true;

deinit:
   return false;
}

void menu_init_core_info(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
}
