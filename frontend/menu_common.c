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

#include "../gfx/shader_common.h"
#include "../gfx/gl_common.h"
#include "../gfx/gfx_common.h"
#include "../performance.h"
#include "../driver.h"
#include "../input/input_common.h"
#include "../input/keyboard_line.h"

#include "../compat/posix_string.h"

#define FBWIDTH 1440
#define FBHEIGHT 900
#define HSPACING 380
#define VSPACING 192
#define C_ACTIVE_ZOOM 1
#define C_PASSIVE_ZOOM 0.75
#define I_ACTIVE_ZOOM 0.666666666
#define I_PASSIVE_ZOOM 0.5

const GLfloat background_color[] = {
   0, 200, 200, 0.75,
   0, 200, 200, 0.75,
   0, 200, 200, 0.75,
   0, 200, 200, 0.75,
};

menu_category categories[8];

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
      cat0.icon = png_texture_load("/usr/share/retroarch/settings.png", &dim, &dim);
      cat0.alpha = 1.0;
      cat0.zoom = C_ACTIVE_ZOOM;
      cat0.active_item = 0;
      cat0.num_items = 2;
      cat0.items = calloc(cat0.num_items, sizeof(menu_item));
      cat0.items[0].name = "Theme";
      cat0.items[0].icon = png_texture_load("/usr/share/retroarch/setting.png", &dim, &dim);
      cat0.items[0].alpha = 1.0;
      cat0.items[0].zoom = I_ACTIVE_ZOOM;
      cat0.items[0].y = VSPACING*1;
      cat0.items[1].name = "Network";
      cat0.items[1].icon = png_texture_load("/usr/share/retroarch/setting.png", &dim, &dim);
      cat0.items[1].alpha = 0.5;
      cat0.items[1].zoom = I_PASSIVE_ZOOM;
      cat0.items[1].y = VSPACING*2;
   categories[0] = cat0;

   menu_category cat1;
      cat1.name = "MasterSystem";
      cat1.icon = png_texture_load("/usr/share/retroarch/mastersystem.png", &dim, &dim);
      cat1.alpha = 0.5;
      cat1.zoom = C_PASSIVE_ZOOM;
      cat1.active_item = 0;
      cat1.num_items = 3;
      cat1.items = calloc(cat0.num_items, sizeof(menu_item));
      cat1.items[0].name = "Zool";
      cat1.items[0].icon = png_texture_load("/usr/share/retroarch/mastersystem-cartidge.png", &dim, &dim);
      cat1.items[0].alpha = 0;
      cat1.items[0].zoom = I_ACTIVE_ZOOM;
      cat1.items[0].y = VSPACING*1;
      cat1.items[1].name = "Sonic Chaos";
      cat1.items[1].icon = png_texture_load("/usr/share/retroarch/mastersystem-cartidge.png", &dim, &dim);
      cat1.items[1].alpha = 0;
      cat1.items[1].zoom = I_PASSIVE_ZOOM;
      cat1.items[1].y = VSPACING*2;
      cat1.items[2].name = "Wonderboy 3";
      cat1.items[2].icon = png_texture_load("/usr/share/retroarch/mastersystem-cartidge.png", &dim, &dim);
      cat1.items[2].alpha = 0;
      cat1.items[2].zoom = I_PASSIVE_ZOOM;
      cat1.items[2].y = VSPACING*3;
   categories[1] = cat1;

   menu_category cat2;
      cat2.name = "Nintendo Entertainment System";
      cat2.icon = png_texture_load("/usr/share/retroarch/nes.png", &dim, &dim);
      cat2.alpha = 0.5;
      cat2.zoom = C_PASSIVE_ZOOM;
      cat2.active_item = 0;
      cat2.num_items = 3;
      cat2.items = calloc(cat0.num_items, sizeof(menu_item));
      cat2.items[0].name = "Mario Bros.";
      cat2.items[0].icon = png_texture_load("/usr/share/retroarch/nes-cartidge.png", &dim, &dim);
      cat2.items[0].alpha = 0;
      cat2.items[0].zoom = I_ACTIVE_ZOOM;
      cat2.items[0].y = VSPACING*1;
      cat2.items[1].name = "Mario Bros.";
      cat2.items[1].icon = png_texture_load("/usr/share/retroarch/nes-cartidge.png", &dim, &dim);
      cat2.items[1].alpha = 0;
      cat2.items[1].zoom = I_PASSIVE_ZOOM;
      cat2.items[1].y = VSPACING*2;
      cat2.items[2].name = "Mario Bros.";
      cat2.items[2].icon = png_texture_load("/usr/share/retroarch/nes-cartidge.png", &dim, &dim);
      cat2.items[2].alpha = 0;
      cat2.items[2].zoom = I_PASSIVE_ZOOM;
      cat2.items[2].y = VSPACING*3;
   categories[2] = cat2;

   menu_category cat3;
      cat3.name = "SEGA Megadrive";
      cat3.icon = png_texture_load("/usr/share/retroarch/megadrive.png", &dim, &dim);
      cat3.alpha = 0.5;
      cat3.zoom = C_PASSIVE_ZOOM;
      cat3.active_item = 0;
      cat3.num_items = 2;
      cat3.items = calloc(cat0.num_items, sizeof(menu_item));
      cat3.items[0].name = "Sonic 2";
      cat3.items[0].icon = png_texture_load("/usr/share/retroarch/megadrive-cartidge.png", &dim, &dim);
      cat3.items[0].alpha = 0;
      cat3.items[0].zoom = I_ACTIVE_ZOOM;
      cat3.items[0].y = VSPACING*1;
      cat3.items[1].name = "Sonic 3";
      cat3.items[1].icon = png_texture_load("/usr/share/retroarch/megadrive-cartidge.png", &dim, &dim);
      cat3.items[1].alpha = 0;
      cat3.items[1].zoom = I_PASSIVE_ZOOM;
      cat3.items[1].y = VSPACING*2;
   categories[3] = cat3;

   menu_category cat4;
      cat4.name = "Super Nintendo";
      cat4.icon = png_texture_load("/usr/share/retroarch/snes.png", &dim, &dim);
      cat4.alpha = 0.5;
      cat4.zoom = C_PASSIVE_ZOOM;
      cat4.active_item = 0;
   categories[4] = cat4;

   menu_category cat5;
      cat5.name = "PlayStation 1";
      cat5.icon = png_texture_load("/usr/share/retroarch/ps1.png", &dim, &dim);
      cat5.alpha = 0.5;
      cat5.zoom = C_PASSIVE_ZOOM;
      cat5.active_item = 0;
   categories[5] = cat5;

   menu_category cat6;
      cat6.name = "GameBoy Color";
      cat6.icon = png_texture_load("/usr/share/retroarch/gameboycolor.png", &dim, &dim);
      cat6.alpha = 0.5;
      cat6.zoom = C_PASSIVE_ZOOM;
      cat6.active_item = 0;
   categories[6] = cat6;

   menu_category cat7;
      cat7.name = "NeoGeo";
      cat7.icon = png_texture_load("/usr/share/retroarch/neogeo.png", &dim, &dim);
      cat7.alpha = 0.5;
      cat7.zoom = C_PASSIVE_ZOOM;
      cat7.active_item = 0;
   categories[7] = cat7;

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
   
   for (int i = 0; i < sizeof(categories) / sizeof(menu_category); i++)
   {
      float ca = (i == menu_active_category) ? 1.0 : 0.5;
      float cz = (i == menu_active_category) ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      add_tween(0.01, categories[i].alpha, ca, &categories[i].alpha, &inOutQuad);
      add_tween(0.01, categories[i].zoom,  cz, &categories[i].zoom,  &inOutQuad);

      for (int j = 0; j < categories[i].num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
                    (j == categories[i].active_item) ? 1.0 : 0.5;
         add_tween(0.01, categories[i].items[j].alpha, ia, &categories[i].items[j].alpha, &inOutQuad);
      }
   }
}

void draw_background(void *data)
{
   gl_t *gl = (gl_t*)data;

   glEnable(GL_BLEND);

   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = background_color;
   glBindTexture(GL_TEXTURE_2D, NULL);

   if (gl->shader)
      gl->shader->use(GL_SHADER_STOCK_BLEND);
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
   gl->coords.color = gl->white_color_ptr;
}

void draw_icon(void *data, GLuint texture, float x, float y, float alpha, float rotation, float scale)
{
   gl_t *gl = (gl_t*)data;

   glViewport(x, FBHEIGHT-y, 192, 192);

   glEnable(GL_BLEND);

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   static const GLfloat vtest[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };

   gl->coords.vertex = vtest;
   gl->coords.tex_coord = vtest;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   if (gl->shader)
      gl->shader->use(GL_SHADER_STOCK_BLEND);

   math_matrix mymat;

   math_matrix mrot;
   matrix_rotate_z(&mrot, rotation);
   matrix_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   math_matrix mscal;
   matrix_scale(&mscal, scale, scale, 1);
   matrix_multiply(&mymat, &mscal, &mymat);

   gl_shader_set_coords(gl, &gl->coords, &mymat);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   glViewport(0, 0, gl->win_width, gl->win_height);
   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}

void lakka_draw(void *data)
{
   timeSinceStart = (float)clock()/CLOCKS_PER_SEC;
   float dt = timeSinceStart - oldTimeSinceStart;
   oldTimeSinceStart = timeSinceStart;

   //printf("%f\n", 1.0/dt);

   update_tweens(dt);

   gl_t *gl = (gl_t*)data;
   glViewport(0, 0, gl->win_width, gl->win_height);

   draw_background(gl);


   for(int i = 0; i < sizeof(categories) / sizeof(menu_category); i++)
   {
      draw_icon(gl, 
         categories[i].icon, 
         all_categories_x + 35 + HSPACING*(i+1), 
         300+96, 
         categories[i].alpha, 
         0, 
         categories[i].zoom);

      for(int j = 0; j < categories[i].num_items; j++)
      {
         draw_icon(gl, 
            categories[i].items[j].icon, 
            all_categories_x + 35 + HSPACING*(i+1), 
            300+96 + categories[i].items[j].y, 
            categories[i].items[j].alpha, 
            0, 
            categories[i].items[j].zoom);
      }
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
         if (menu_active_category < sizeof(categories)  / sizeof(menu_category) - 1)
         {
            menu_active_category++;
            switch_categories();
         }
         break;

      case RGUI_ACTION_OK:
         if (menu_active_category == 0) {
            strlcpy(g_extern.fullpath, "/home/kivutar/Jeux/roms/sonic3.smd", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/libretro-genplus.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
         } 
         else if (menu_active_category == 1)
         {
            strlcpy(g_extern.fullpath, "/storage/roms/zelda.smc", sizeof(g_extern.fullpath));
            strlcpy(g_settings.libretro, "/usr/lib/libretro/pocketsnes-libretro.so", sizeof(g_settings.libretro));
            g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
            return -1;
         }
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
      driver.video_poke->set_texture_enable(driver.video_data, false, MENU_TEXTURE_FULLSCREEN);

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
