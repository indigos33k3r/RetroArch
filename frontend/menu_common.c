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
#include <time.h>
#include "menu_common.h"
#include "tween.h"
#include "png_texture_load.h"

#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../performance.h"
#include "../driver.h"
#include "../input/input_common.h"
#include "../input/keyboard_line.h"

#include "../compat/posix_string.h"

#define FBWIDTH 480
#define FBHEIGHT 300
#define HSPACING 127

menu_category categories[4];

int menu_active_category = 0;

int dim = 256;

float all_categories_x = 0;

rgui_handle_t *rgui;

GLuint image;

float timeSinceStart, oldTimeSinceStart;

static double gx = 0.0;
static double gx2 = 0.0;

double v = 1.0;

//forward decl
static int menu_iterate_func(void *data, unsigned action);

static rgui_handle_t *rgui_init(void)
{
   clock_t t = clock();
   timeSinceStart = ((float)t)/CLOCKS_PER_SEC;
   oldTimeSinceStart = 0;

   menu_category cat0;
   cat0.name = "Settings";
   cat0.icon = png_texture_load("media/lakka/settings.png", &dim, &dim),
   cat0.alpha = 1.0;
   cat0.active_item = 0;
   categories[0] = cat0;

   menu_category cat1;
   cat1.name = "Nintendo Entertainment System";
   cat1.icon = png_texture_load("media/lakka/nes.png", &dim, &dim),
   cat1.alpha = 0.5;
   cat1.active_item = 0;
   categories[1] = cat1;

   menu_category cat2;
   cat2.name = "Super Nintendo";
   cat2.icon = png_texture_load("media/lakka/snes.png", &dim, &dim),
   cat2.alpha = 0.5;
   cat2.active_item = 0;
   categories[2] = cat2;

   menu_category cat3;
   cat3.name = "SEGA Megadrive";
   cat3.icon = png_texture_load("media/lakka/megadrive.png", &dim, &dim),
   cat3.alpha = 0.5;
   cat3.active_item = 0;
   categories[3] = cat3;

   rgui_handle_t *rgui = (rgui_handle_t*)calloc(1, sizeof(*rgui));

   return rgui;
}

// With this, F1 switch back to the game
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

void switch_categories()
{
   add_tween(0.01, all_categories_x, -menu_active_category * HSPACING, &all_categories_x, &inOutQuad);
   
   for(int i = 0; i < sizeof(categories) / sizeof(menu_category); i++)
   {
      if (i == menu_active_category) {
         add_tween(0.01, categories[i].alpha, 1.0, &categories[i].alpha, &inOutQuad);
      }
      else
      {
         add_tween(0.01, categories[i].alpha, 0.5, &categories[i].alpha, &inOutQuad);
      }
   }
}

void draw_category(GLuint texture, float x, float y, float alpha)
{
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glLoadIdentity();
   glColor4f(1, 1, 1, alpha);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTranslated(x/FBWIDTH, (FBHEIGHT-y)/FBHEIGHT, 0);
   glBegin(GL_TRIANGLES);
      glTexCoord2d(0,0);    glVertex2d(  0.0/FBWIDTH,   0.0/FBHEIGHT);
      glTexCoord2d(0,1);    glVertex2d(  0.0/FBWIDTH, 64.0/FBHEIGHT);
      glTexCoord2d(1,1);    glVertex2d(64.0/FBWIDTH, 64.0/FBHEIGHT);
      glTexCoord2d(0,0);    glVertex2d(  0.0/FBWIDTH,   0.0/FBHEIGHT);
      glTexCoord2d(1,1);    glVertex2d(64.0/FBWIDTH, 64.0/FBHEIGHT);
      glTexCoord2d(1,0);    glVertex2d(64.0/FBWIDTH,   0.0/FBHEIGHT);
   glEnd();
   glColor4f(1, 1, 1, 1);
}

void lakka_draw(void *data)
{
   timeSinceStart = (float)clock()/CLOCKS_PER_SEC;
   float dt = timeSinceStart - oldTimeSinceStart;
   oldTimeSinceStart = timeSinceStart;

   //printf("%f\n", 1.0/dt);

   update_tweens(dt);

   for(int i = 0; i < sizeof(categories) / sizeof(menu_category); i++)
   {
      draw_category(categories[i].icon, all_categories_x + 16 + HSPACING*(i+1), 100+32, categories[i].alpha);
   }
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
   rgui = rgui_init();

   strlcpy(rgui->base_path, g_settings.rgui_browser_directory, sizeof(rgui->base_path));
   rgui->menu_stack = (file_list_t*)calloc(1, sizeof(file_list_t));
   file_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);

   rgui->trigger_state = 0;
   rgui->old_input_state = 0;
   rgui->do_held = false;

   menu_update_libretro_info();

   rgui->last_time = rarch_get_time_usec();
}

void menu_free(void)
{
#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   file_list_free(rgui->menu_stack);

   core_info_list_free(rgui->core_info);

   free_tweens();
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

   switch (action)
   {
      case RGUI_ACTION_LEFT:
         if (menu_active_category > 0)
         {
            menu_active_category--;
            switch_categories();
         }
         break;

      case RGUI_ACTION_RIGHT:
         if (menu_active_category < sizeof(categories) / sizeof(menu_category) - 1)
         {
            menu_active_category++;
            switch_categories();
         }
         break;

      case RGUI_ACTION_OK:
         //if (active_item == 0) {
            strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/sonic3.smd", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-genplus.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
         /*} 
         else if (active_item == 1)
         {
            strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/zelda.smc", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-snes9x-next.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
         }*/
         break;

      default:
         break;
   }

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

   // enable rendering of the menu
   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, true, MENU_TEXTURE_FULLSCREEN);

   rarch_render_cached_frame();

   // Throttle in case VSync is broken (avoid 1000+ FPS RGUI).
   time = rarch_get_time_usec();
   delta = (time - rgui->last_time) / 1000;
   target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   sleep_msec = target_msec - delta;
   if (sleep_msec > 0)
      rarch_sleep((unsigned int)sleep_msec);
   rgui->last_time = rarch_get_time_usec();

   // disable rendering of the menu
   if (driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, true, MENU_TEXTURE_FULLSCREEN);

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
