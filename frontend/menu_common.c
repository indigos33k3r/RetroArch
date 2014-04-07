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

#define HSPACING 300
#define VSPACING 75
#define C_ACTIVE_ZOOM 1.0
#define C_PASSIVE_ZOOM 0.5
#define I_ACTIVE_ZOOM 0.75
#define I_PASSIVE_ZOOM 0.35
#define DELAY 0.02

const GLfloat background_color[] = {
   0.1, 0.74, 0.61, 0.75,
   0.1, 0.74, 0.61, 0.75,
   0.1, 0.74, 0.61, 0.75,
   0.1, 0.74, 0.61, 0.75,
};

menu_category* categories = NULL;

int depth = 0;

GLuint arrow_icon;
GLuint run_icon;
GLuint resume_icon;
GLuint savestate_icon;
GLuint loadstate_icon;
GLuint screenshot_icon;
GLuint reload_icon;

struct font_output_list run_label;
struct font_output_list resume_label;

int num_categories = 0;

int menu_active_category = 0;

int dim = 192;

float all_categories_x = 0;

rgui_handle_t *rgui;

//forward decl
static int menu_iterate_func(void *data, unsigned action);

void menu_init_core_info(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;
   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*rgui->libretro_dir)
      rgui->core_info = core_info_list_new(rgui->libretro_dir);
}

static rgui_handle_t *rgui_init(void)
{
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

// Move the categories left or right depending on the menu_active_category variable
void switch_categories()
{
   // translation
   add_tween(DELAY, -menu_active_category * HSPACING, &all_categories_x, &inOutQuad);
   
   // alpha tweening
   for (int i = 0; i < num_categories; i++)
   {
      float ca = (i == menu_active_category) ? 1.0 : 0.5;
      float cz = (i == menu_active_category) ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      add_tween(DELAY, ca, &categories[i].alpha, &inOutQuad);
      add_tween(DELAY, cz, &categories[i].zoom,  &inOutQuad);

      for (int j = 0; j < categories[i].num_items; j++)
      {
         float ia = (i != menu_active_category     ) ? 0   : 
                    (j == categories[i].active_item) ? 1.0 : 0.5;
         add_tween(DELAY, ia, &categories[i].items[j].alpha, &inOutQuad);
      }
   }
}

void switch_items()
{
   for (int j = 0; j < categories[menu_active_category].num_items; j++)
   {
      float ia = (j == categories[menu_active_category].active_item) ? 1.0 : 0.5;
      float iz = (j == categories[menu_active_category].active_item) ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
      float iy = (j == categories[menu_active_category].active_item) ? VSPACING*2.5 :
                 (j  < categories[menu_active_category].active_item) ? VSPACING*(j-categories[menu_active_category].active_item - 1) :
                                                                       VSPACING*(j-categories[menu_active_category].active_item + 3);

      add_tween(DELAY, ia, &categories[menu_active_category].items[j].alpha, &inOutQuad);
      add_tween(DELAY, iz, &categories[menu_active_category].items[j].zoom,  &inOutQuad);
      add_tween(DELAY, iy, &categories[menu_active_category].items[j].y,     &inOutQuad);
   }
}

void switch_subitems ()
{
   menu_item ai = categories[menu_active_category].items[categories[menu_active_category].active_item];

   for (int k = 0; k < ai.num_subitems; k++) {
      // Above items
      if (k < ai.active_subitem) {
         add_tween(DELAY, 0.5, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k-ai.active_subitem + 2), &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      // Active item
      } else if (k == ai.active_subitem) {
         add_tween(DELAY, 1.0, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*2.5, &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_ACTIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      // Under items
      } else if (k > ai.active_subitem) {
         add_tween(DELAY, 0.5, &ai.subitems[k].alpha, &inOutQuad);
         add_tween(DELAY, VSPACING*(k-ai.active_subitem + 3), &ai.subitems[k].y, &inOutQuad);
         add_tween(DELAY, I_PASSIVE_ZOOM, &ai.subitems[k].zoom, &inOutQuad);
      }
   }
}

void reset_submenu()
{
   if (! (g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)) { // Keeps active submenu state (do we really want that?)
      categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem = 0;
      for (int i = 0; i < num_categories; i++) {
         for (int j = 0; j < categories[i].num_items; j++) {
            for (int k = 0; k < categories[i].items[j].num_subitems; k++) {
               categories[i].items[j].subitems[k].alpha = 0;
               categories[i].items[j].subitems[k].zoom = k == categories[i].items[j].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
               categories[i].items[j].subitems[k].y = k == 0 ? VSPACING*2.5 : VSPACING*(3+k);
            }
         }
      }
   }
}

void open_submenu ()
{
   add_tween(DELAY, -HSPACING * (menu_active_category+1), &all_categories_x, &inOutQuad);

   // Reset contextual menu style
   reset_submenu();
   
   for (int i = 0; i < num_categories; i++) {
      if (i == menu_active_category) {
         add_tween(DELAY, 1.0, &categories[i].alpha, &inOutQuad);
         for (int j = 0; j < categories[i].num_items; j++) {
            if (j == categories[i].active_item) {
               for (int k = 0; k < categories[i].items[j].num_subitems; k++) {
                  if (k == categories[i].items[j].active_subitem) {
                     add_tween(DELAY, 1.0, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
                     add_tween(DELAY, I_ACTIVE_ZOOM, &categories[i].items[j].subitems[k].zoom, &inOutQuad);
                  } else {
                     add_tween(DELAY, 0.5, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
                     add_tween(DELAY, I_PASSIVE_ZOOM, &categories[i].items[j].subitems[k].zoom, &inOutQuad);
                  }
               }
            } else {
               add_tween(DELAY, 0, &categories[i].items[j].alpha, &inOutQuad);
            }
         }
      } else {
         add_tween(DELAY, 0, &categories[i].alpha, &inOutQuad);
      }
   }
}

void close_submenu ()
{
   add_tween(DELAY, -HSPACING * menu_active_category, &all_categories_x, &inOutQuad);
   
   for (int i = 0; i < num_categories; i++) {
      if (i == menu_active_category) {
         add_tween(DELAY, 1.0, &categories[i].alpha, &inOutQuad);
         add_tween(DELAY, C_ACTIVE_ZOOM, &categories[i].zoom, &inOutQuad);
         for (int j = 0; j < categories[i].num_items; j++) {
            if (j == categories[i].active_item) {
               add_tween(DELAY, 1.0, &categories[i].items[j].alpha, &inOutQuad);
               for (int k = 0; k < categories[i].items[j].num_subitems; k++) {
                  add_tween(DELAY, 0, &categories[i].items[j].subitems[k].alpha, &inOutQuad);
               }
            } else {
               add_tween(DELAY, 0.5, &categories[i].items[j].alpha, &inOutQuad);
            }
         }
      } else {
         add_tween(DELAY, 0.5, &categories[i].alpha, &inOutQuad);
         add_tween(DELAY, C_PASSIVE_ZOOM, &categories[i].zoom, &inOutQuad);
         for (int j = 0; j < categories[i].num_items; j++) {
            add_tween(DELAY, 0, &categories[i].items[j].alpha, &inOutQuad);
         }
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

   glViewport(x, gl->win_height - y, dim, dim);

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

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color = gl->white_color_ptr;
}

struct font_rect
{
   int x, y;
   int width, height;
   int pot_width, pot_height;
};

/* font rendering */

static void calculate_msg_geometry(const struct font_output *head, struct font_rect *rect)
{
   int x_min = head->off_x;
   int x_max = head->off_x + head->width+10;
   int y_min = head->off_y;
   int y_max = head->off_y + head->height;

   while ((head = head->next))
   {
      int left = head->off_x;
      int right = head->off_x + head->width;
      int bottom = head->off_y;
      int top = head->off_y + head->height;

      if (left < x_min)
         x_min = left;
      if (right > x_max)
         x_max = right;

      if (bottom < y_min)
         y_min = bottom;
      if (top > y_max)
         y_max = top;
   }

   rect->x = x_min;
   rect->y = y_min;
   rect->width = x_max - x_min;
   rect->height = y_max - y_min;
}

static void adjust_power_of_two(gl_t *gl, struct font_rect *geom)
{
   // Some systems really hate NPOT textures.
   geom->pot_width  = next_pow2(geom->width);
   geom->pot_height = next_pow2(geom->height);

   if (geom->pot_width > gl->max_font_size)
      geom->pot_width = gl->max_font_size;
   if (geom->pot_height > gl->max_font_size)
      geom->pot_height = gl->max_font_size;

   if ((geom->pot_width > gl->font_tex_w) || (geom->pot_height > gl->font_tex_h))
   {
      gl->font_tex_buf = (uint32_t*)realloc(gl->font_tex_buf,
            geom->pot_width * geom->pot_height * sizeof(uint32_t));

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geom->pot_width, geom->pot_height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

      gl->font_tex_w = geom->pot_width;
      gl->font_tex_h = geom->pot_height;
   }
}

static void copy_glyph(const struct font_output *head, const struct font_rect *geom, uint32_t *buffer, unsigned width, unsigned height)
{
   int h, w;
   // head has top-left oriented coords.
   int x = head->off_x - geom->x;
   int y = head->off_y - geom->y;
   y     = height - head->height - y - 1;

   const uint8_t *src = head->output;
   int font_width  = head->width  + ((x < 0) ? x : 0);
   int font_height = head->height + ((y < 0) ? y : 0);

   if (x < 0)
   {
      src += -x;
      x    = 0;
   }

   if (y < 0)
   {
      src += -y * head->pitch;
      y    = 0;
   }

   if (x + font_width > (int)width)
      font_width = width - x;

   if (y + font_height > (int)height)
      font_height = height - y;

   uint32_t *dst = buffer + y * width + x;
   for (h = 0; h < font_height; h++, dst += width, src += head->pitch)
   {
      uint8_t *d = (uint8_t*)dst;
      for (w = 0; w < font_width; w++)
      {
         *d++ = 0xff;
         *d++ = 0xff;
         *d++ = 0xff;
         *d++ = src[w];
      }
   }
}

static void blit_fonts(gl_t *gl, const struct font_output *head, const struct font_rect *geom)
{
   memset(gl->font_tex_buf, 0, gl->font_tex_w * gl->font_tex_h * sizeof(uint32_t));

   while (head)
   {
      copy_glyph(head, geom, gl->font_tex_buf, gl->font_tex_w, gl->font_tex_h);
      head = head->next;
   }

   glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
   glTexSubImage2D(GL_TEXTURE_2D,
      0, 0, 0, gl->font_tex_w, gl->font_tex_h,
      GL_RGBA, GL_UNSIGNED_BYTE, gl->font_tex_buf);
}

static void calculate_font_coords(gl_t *gl,
      GLfloat font_vertex[8], GLfloat font_vertex_dark[8], GLfloat font_tex_coords[8], GLfloat scale, GLfloat pos_x, GLfloat pos_y)
{
   unsigned i;
   GLfloat scale_factor = scale;

   GLfloat lx = pos_x;
   GLfloat hx = (GLfloat)gl->font_last_width * scale_factor / gl->vp.width + lx;
   GLfloat ly = pos_y;
   GLfloat hy = (GLfloat)gl->font_last_height * scale_factor / gl->vp.height + ly;

   font_vertex[0] = lx;
   font_vertex[2] = hx;
   font_vertex[4] = lx;
   font_vertex[6] = hx;
   font_vertex[1] = hy;
   font_vertex[3] = hy;
   font_vertex[5] = ly;
   font_vertex[7] = ly;

   GLfloat shift_x = 2.0f / gl->vp.width;
   GLfloat shift_y = 2.0f / gl->vp.height;
   for (i = 0; i < 4; i++)
   {
      font_vertex_dark[2 * i + 0] = font_vertex[2 * i + 0] - shift_x;
      font_vertex_dark[2 * i + 1] = font_vertex[2 * i + 1] - shift_y;
   }

   lx = 0.0f;
   hx = (GLfloat)gl->font_last_width / gl->font_tex_w;
   ly = 1.0f - (GLfloat)gl->font_last_height / gl->font_tex_h; 
   hy = 1.0f;

   font_tex_coords[0] = lx;
   font_tex_coords[2] = hx;
   font_tex_coords[4] = lx;
   font_tex_coords[6] = hx;
   font_tex_coords[1] = ly;
   font_tex_coords[3] = ly;
   font_tex_coords[5] = hy;
   font_tex_coords[7] = hy;
}

static void draw_text(void *data, struct font_output_list out, float x, float y, float scale, float alpha)
{
   gl_t *gl = (gl_t*)data;
   if (!gl->font)
      return;

   for (int i = 0; i < 4; i++)
   {
      gl->font_color[4 * i + 0] = 1.0;
      gl->font_color[4 * i + 1] = 1.0;
      gl->font_color[4 * i + 2] = 1.0;
      gl->font_color[4 * i + 3] = alpha;
   }

   if (gl->shader)
      gl->shader->use(GL_SHADER_STOCK_BLEND);

   gl_set_viewport(gl, gl->win_width, gl->win_height, true, false);

   glEnable(GL_BLEND);

   GLfloat font_vertex[8]; 
   GLfloat font_vertex_dark[8]; 
   GLfloat font_tex_coords[8];

   glBindTexture(GL_TEXTURE_2D, gl->font_tex);

   gl->coords.tex_coord = font_tex_coords;

   struct font_output *head = out.head;

   struct font_rect geom;
   calculate_msg_geometry(head, &geom);
   adjust_power_of_two(gl, &geom);
   blit_fonts(gl, head, &geom);

   //gl->font_driver->free_output(gl->font, &out);

   gl->font_last_width = geom.width;
   gl->font_last_height = geom.height;

   calculate_font_coords(gl, font_vertex, font_vertex_dark, font_tex_coords, 
         scale, x / gl->win_width, (gl->win_height - y) / gl->win_height);

   gl->coords.vertex = font_vertex;
   gl->coords.color  = gl->font_color;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   // Post - Go back to old rendering path.
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_coords;
   gl->coords.color     = gl->white_color_ptr;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   glDisable(GL_BLEND);

   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};
   gl_set_projection(gl, &ortho, true);
}

// main display loop
void lakka_draw(void *data)
{
   // Throttle in case VSync is broken (avoid 1000+ FPS RGUI).
   rarch_time_t time, delta, target_msec, sleep_msec;
   time = rarch_get_time_usec();
   delta = (time - rgui->last_time) / 1000;
   target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   sleep_msec = target_msec - delta;
   if (sleep_msec > 0)
      rarch_sleep((unsigned int)sleep_msec);
   rgui->last_time = rarch_get_time_usec();
   
   update_tweens((float)delta/10000);

   gl_t *gl = (gl_t*)data;

   glViewport(0, 0, gl->win_width, gl->win_height);

   draw_background(gl);

   for(int i = 0; i < num_categories; i++)
   {
      // draw items
      for(int j = 0; j < categories[i].num_items; j++)
      {
         draw_icon(gl, 
            categories[i].items[j].icon, 
            156 + HSPACING*(i+1) + all_categories_x - dim/2.0, 
            300 + categories[i].items[j].y + dim/2.0, 
            categories[i].items[j].alpha, 
            0, 
            categories[i].items[j].zoom);

         if (i == menu_active_category && j == categories[i].active_item && depth == 1) // performance improvement
         {
            for(int k = 0; k < categories[i].items[j].num_subitems; k++)
            {
               if (k == 0 && g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)
               {
                  draw_icon(gl, 
                     resume_icon, 
                     156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                     categories[i].items[j].subitems[k].alpha, 
                     0, 
                     categories[i].items[j].subitems[k].zoom);
                  draw_text(gl, 
                     resume_label, 
                     156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + 15, 
                     categories[i].items[j].subitems[k].zoom, 
                     categories[i].items[j].subitems[k].alpha);
               }
               else if (k == 0)
               {
                  draw_icon(gl, 
                     run_icon, 
                     156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                     categories[i].items[j].subitems[k].alpha, 
                     0, 
                     categories[i].items[j].subitems[k].zoom);
                  draw_text(gl, 
                     run_label, 
                     156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + 15, 
                     categories[i].items[j].subitems[k].zoom, 
                     categories[i].items[j].subitems[k].alpha);
               } else if (g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)
               {
               draw_icon(gl, 
                  categories[i].items[j].subitems[k].icon, 
                  156 + HSPACING*(i+2) + all_categories_x - dim/2.0, 
                  300 + categories[i].items[j].subitems[k].y + dim/2.0, 
                  categories[i].items[j].subitems[k].alpha, 
                  0, 
                  categories[i].items[j].subitems[k].zoom);
               /*if category.prefix ~= "settings" and  (k == 2 or k == 3) and item.slot == -1 then
                  love.graphics.print(subitem.name .. " <" .. item.slot .. " (auto)>", 256 + (HSPACING*(i+1)) + all_categories.x, 300-15 + subitem.y)
               elseif category.prefix ~= "settings" and  (k == 2 or k == 3) then
                  love.graphics.print(subitem.name .. " <" .. item.slot .. ">", 256 + (HSPACING*(i+1)) + all_categories.x, 300-15 + subitem.y)
               else*/
                  draw_text(gl, 
                     categories[i].items[j].subitems[k].out, 
                     156 + HSPACING*(i+2) + all_categories_x + dim/2.0, 
                     300 + categories[i].items[j].subitems[k].y + 15, 
                     categories[i].items[j].subitems[k].zoom, 
                     categories[i].items[j].subitems[k].alpha);
               /*end*/
               }
            }
         }

         if (depth == 0) {
            if (i == menu_active_category && j > categories[menu_active_category].active_item - 4 && j < categories[menu_active_category].active_item + 10) // performance improvement
               draw_text(gl, 
                  categories[i].items[j].out, 
                  156 + HSPACING*(i+1) + all_categories_x + dim/2.0, 
                  300 + categories[i].items[j].y + 15, 
                  categories[i].items[j].zoom, 
                  categories[i].items[j].alpha);
         } else {
            draw_icon(gl,
               arrow_icon, 
               156 + (HSPACING*(i+1)) + all_categories_x + 150 +-dim/2.0, 
               300 + categories[i].items[j].y + dim/2.0, 
               categories[i].items[j].alpha,
               0,
               categories[i].items[j].zoom);
         }
      }

      // draw category
      draw_icon(gl, 
         categories[i].icon, 
         156 + (HSPACING*(i+1)) + all_categories_x - dim/2.0, 
         300 + dim/2.0, 
         categories[i].alpha, 
         0, 
         categories[i].zoom);
   }

   struct font_output_list msg = (depth == 0) ? categories[menu_active_category].out : categories[menu_active_category].items[categories[menu_active_category].active_item].out;
   draw_text(gl, msg, 15.0, 35.0, 0.5, 1.0);

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
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

char * str_replace ( const char *string, const char *substr, const char *replacement ){
   char *tok = NULL;
   char *newstr = NULL;
   char *oldstr = NULL;
   char *head = NULL;
 
   /* if either substr or replacement is NULL, duplicate string a let caller handle it */
   if ( substr == NULL || replacement == NULL ) return strdup (string);
   newstr = strdup (string);
   head = newstr;
   while ( (tok = strstr ( head, substr ))){
      oldstr = newstr;
      newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
      /*failed to alloc mem, free old string and return NULL */
      if ( newstr == NULL ){
         free (oldstr);
         return NULL;
      }
      memcpy ( newstr, oldstr, tok - oldstr );
      memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
      memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
      memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
      /* move back head right after the last replacement */
      head = newstr + (tok - oldstr) + strlen( replacement );
      free (oldstr);
   }
   return newstr;
}

void textures_init(void *data)
{
   gl_t *gl = (gl_t*)data;

   arrow_icon = png_texture_load("/usr/share/retroarch/arrow.png", &dim, &dim);
   run_icon = png_texture_load("/usr/share/retroarch/run.png", &dim, &dim);
   resume_icon = png_texture_load("/usr/share/retroarch/resume.png", &dim, &dim);
   savestate_icon = png_texture_load("/usr/share/retroarch/savestate.png", &dim, &dim);
   loadstate_icon = png_texture_load("/usr/share/retroarch/loadstate.png", &dim, &dim);
   screenshot_icon = png_texture_load("/usr/share/retroarch/screenshot.png", &dim, &dim);
   reload_icon = png_texture_load("/usr/share/retroarch/reload.png", &dim, &dim);

   gl->font_driver->render_msg(gl->font, "Run", &run_label);
   gl->font_driver->render_msg(gl->font, "Resume", &resume_label);
}

void menu_init(void)
{
   rgui = rgui_init();

   gl_t *gl = (gl_t*)driver.video_data;

   strlcpy(rgui->base_path, g_settings.rgui_browser_directory, sizeof(rgui->base_path));
   rgui->menu_stack = (file_list_t*)calloc(1, sizeof(file_list_t));
   file_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);

   rgui->trigger_state = 0;
   rgui->old_input_state = 0;
   rgui->do_held = false;

   menu_update_libretro_info();

   menu_init_core_info(rgui);

   num_categories = rgui->core_info->count;

   textures_init(gl);

   categories = realloc(categories, num_categories * sizeof(menu_category));

   for (int i = 0; i < rgui->core_info->count; i++) {
      core_info_t corenfo = rgui->core_info->list[i];

      char core_id[256];
      strcpy(core_id, basename(corenfo.path));
      strcpy(core_id, str_replace(core_id, ".so", ""));
      strcpy(core_id, str_replace(core_id, ".dll", ""));
      strcpy(core_id, str_replace(core_id, ".dylib", ""));
      strcpy(core_id, str_replace(core_id, "-libretro", ""));
      strcpy(core_id, str_replace(core_id, "_libretro", ""));
      strcpy(core_id, str_replace(core_id, "libretro-", ""));
      strcpy(core_id, str_replace(core_id, "libretro_", ""));

      char texturepath[256];
      strcpy(texturepath, "/usr/share/retroarch/");
      strcat(texturepath, core_id);
      strcat(texturepath, ".png");

      char gametexturepath[256];
      strcpy(gametexturepath, "/usr/share/retroarch/");
      strcat(gametexturepath, core_id);
      strcat(gametexturepath, "-game.png");

      menu_category mcat;
      mcat.name        = corenfo.display_name;
      mcat.libretro    = corenfo.path;
      mcat.icon        = png_texture_load(texturepath, &dim, &dim);
      mcat.alpha       = i == 0 ? 1.0 : 0.5;
      mcat.zoom        = i == 0 ? C_ACTIVE_ZOOM : C_PASSIVE_ZOOM;
      mcat.active_item = 0;
      mcat.num_items   = 0;
      mcat.items       = calloc(mcat.num_items, sizeof(menu_item));
      struct font_output_list out;
      gl->font_driver->render_msg(gl->font, mcat.name, &out);
      mcat.out = out;
      
      struct string_list *list = dir_list_new(g_settings.rgui_browser_directory, corenfo.supported_extensions, true);
      dir_list_sort(list, true);

      for (int j = 0; j < list->size; j++) {
         if (! list->elems[j].attr.b) // exclude directories
         {
            int n = mcat.num_items;
            mcat.num_items++;
            mcat.items = realloc(mcat.items, mcat.num_items * sizeof(menu_item));

            mcat.items[n].name  = path_basename(list->elems[j].data);
            mcat.items[n].rom   = list->elems[j].data;
            mcat.items[n].icon  = png_texture_load(gametexturepath, &dim, &dim);
            mcat.items[n].alpha = i != menu_active_category ? 0 : n ? 0.5 : 1;
            mcat.items[n].zoom  = n ? I_PASSIVE_ZOOM : I_ACTIVE_ZOOM;
            mcat.items[n].y     = n ? VSPACING*(3+n) : VSPACING*2.5;
            mcat.items[n].active_subitem = 0;
            mcat.items[n].num_subitems   = 5;
            mcat.items[n].subitems       = calloc(mcat.items[n].num_subitems, sizeof(menu_subitem));

            for (int k = 0; k < mcat.items[n].num_subitems; k++) {
               switch (k) {
                  case 0:
                     mcat.items[n].subitems[k].name = "Run";
                     mcat.items[n].subitems[k].icon = run_icon;
                     break;
                  case 1:
                     mcat.items[n].subitems[k].name = "Save State";
                     mcat.items[n].subitems[k].icon = savestate_icon;
                     break;
                  case 2:
                     mcat.items[n].subitems[k].name = "Load State";
                     mcat.items[n].subitems[k].icon = loadstate_icon;
                     break;
                  case 3:
                     mcat.items[n].subitems[k].name = "Take Screenshot";
                     mcat.items[n].subitems[k].icon = screenshot_icon;
                     break;
                  case 4:
                     mcat.items[n].subitems[k].name = "Reload";
                     mcat.items[n].subitems[k].icon = reload_icon;
                     break;
               }
               mcat.items[n].subitems[k].alpha = 0;
               mcat.items[n].subitems[k].zoom = k == mcat.items[n].active_subitem ? I_ACTIVE_ZOOM : I_PASSIVE_ZOOM;
               mcat.items[n].subitems[k].y = k == 0 ? VSPACING*2.5 : VSPACING*(3+k);
               struct font_output_list out;
               gl->font_driver->render_msg(gl->font, mcat.items[n].subitems[k].name, &out);
               mcat.items[n].subitems[k].out = out;
            }

            struct font_output_list out;
            gl->font_driver->render_msg(gl->font, mcat.items[n].name, &out);
            mcat.items[n].out = out;
         }
      }
      categories[i] = mcat;
   }

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
         if (depth == 0 && menu_active_category > 0)
         {
            menu_active_category--;
            switch_categories();
         }
         break;

      case RGUI_ACTION_RIGHT:
         if (depth == 0 && menu_active_category < num_categories-1)
         {
            menu_active_category++;
            switch_categories();
         }
         break;

      case RGUI_ACTION_DOWN:
         if (depth == 0 && categories[menu_active_category].active_item < categories[menu_active_category].num_items - 1)
         {
            categories[menu_active_category].active_item++;
            switch_items();
         }
         if (depth == 1 && categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem < categories[menu_active_category].items[categories[menu_active_category].active_item].num_subitems -1 && g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0)
         {
            categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem++;
            switch_subitems();
         }
         break;

      case RGUI_ACTION_UP:
         if (depth == 0 && categories[menu_active_category].active_item > 0)
         {
            categories[menu_active_category].active_item--;
            switch_items();
         }
         if (depth == 1 && categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem > 0)
         {
            categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem--;
            switch_subitems();
         }
         break;

      case RGUI_ACTION_OK:
         if (depth == 1) {
            switch (categories[menu_active_category].items[categories[menu_active_category].active_item].active_subitem) {
               case 0:
                  if (g_extern.main_is_init && !g_extern.libretro_dummy && strcmp(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom) == 0) {
                     g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  } else {
                     strlcpy(g_extern.fullpath, categories[menu_active_category].items[categories[menu_active_category].active_item].rom, sizeof(g_extern.fullpath));
                     strlcpy(g_settings.libretro, categories[menu_active_category].libretro, sizeof(g_settings.libretro));
                     g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
                  }
                  return -1;
                  break;
               case 1:
                  rarch_save_state();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
               case 2:
                  rarch_load_state();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
               case 3:
                  rarch_take_screenshot();
                  break;
               case 4:
                  rarch_game_reset();
                  g_extern.lifecycle_state |= (1ULL << MODE_GAME);
                  return -1;
                  break;
            }
         } else if (depth == 0) {
            open_submenu();
            depth = 1;
         }
         break;

      case RGUI_ACTION_CANCEL:
         if (depth == 1) {
            close_submenu();
            depth = 0;
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
   /*time = rarch_get_time_usec();
   delta = (time - rgui->last_time) / 1000;
   target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   sleep_msec = target_msec - delta;
   if (sleep_msec > 0)
      rarch_sleep((unsigned int)sleep_msec);
   rgui->last_time = rarch_get_time_usec();*/

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

