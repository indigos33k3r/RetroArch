/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2012-2013 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../general.h"
#include "../conf/config_file.h"
#include "../file.h"

#include "menu_common.h"

#include "../file_ext.h"

#define attempt_load_game_fails (1ULL << MODE_EXIT)

int main(int argc, char *argv[])
{
   void* args = (void*) NULL;
   unsigned i;

   rarch_main_clear_state();
   rarch_init_msg_queue();

   int init_ret;
   if ((init_ret = rarch_main_init(argc, argv))) return init_ret;

   menu_init();

   g_extern.lifecycle_state |= (1ULL << MODE_GAME);

   do
   {
      if (g_extern.system.shutdown)
      {
         break;
      }
      else if (g_extern.lifecycle_state & (1ULL << MODE_LOAD_GAME))
      {
         // If ROM load fails, we exit RetroArch. On console it might make more sense to go back to menu though ...
         if (load_menu_game())
            g_extern.lifecycle_state |= (1ULL << MODE_GAME);
         else
         {
            g_extern.lifecycle_state = attempt_load_game_fails;

            if (g_extern.lifecycle_state & (1ULL << MODE_EXIT))
            {
               return 1;
            }
         }

         g_extern.lifecycle_state &= ~(1ULL << MODE_LOAD_GAME);
      }
      else if (g_extern.lifecycle_state & (1ULL << MODE_GAME))
      {
         if (driver.video_poke && driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

         while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate())
         {
            if (!(g_extern.lifecycle_state & (1ULL << MODE_GAME)))
            {
               break;
            }
         }
         g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
      }
      else if (g_extern.lifecycle_state & (1ULL << MODE_MENU))
      {
         g_extern.lifecycle_state |= 1ULL << MODE_MENU_PREINIT;
         // Menu should always run with vsync on.
         video_set_nonblock_state_func(false);
         // Stop all rumbling when entering RGUI.
         for (i = 0; i < MAX_PLAYERS; i++)
         {
            driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }

         if (driver.audio_data)
            audio_stop_func();

         while (!g_extern.system.shutdown && menu_iterate())
         {
            if (!(g_extern.lifecycle_state & (1ULL << MODE_MENU)))
            {
               break;
            }
         }

         driver_set_nonblock_state(driver.nonblock_state);

         if (driver.audio_data && !g_extern.audio_data.mute && !audio_start_func())
         {
            RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }

         g_extern.lifecycle_state &= ~(1ULL << MODE_MENU);
      }
      else
      {
         break;
      }
   } while(true);

   if (g_extern.lifecycle_state & (1ULL << MODE_GAME_ONESHOT))
      return 0;

   g_extern.system.shutdown = false;

   menu_free();

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   if (g_extern.lifecycle_state & (1ULL << MODE_GAME_ONESHOT))
      return 0;

   rarch_main_deinit();
   rarch_deinit_msg_queue();
   global_uninit_drivers();

   rarch_main_clear_state();

   return 0;
}
