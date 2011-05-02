/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <getopt.h>
#include <signal.h>

#include "wayland-server.h"
#include "compositor.h"

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char *option_socket_name = NULL;
static const char *option_background = "background.jpg";
static const char *option_shell = NULL;
static int option_idle_time = 300;
static int option_connector = 0;

WL_EXPORT void
wlsc_matrix_init(struct wlsc_matrix *matrix)
{
	static const struct wlsc_matrix identity = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
	};

	memcpy(matrix, &identity, sizeof identity);
}

static void
wlsc_matrix_multiply(struct wlsc_matrix *m, const struct wlsc_matrix *n)
{
	struct wlsc_matrix tmp;
	const GLfloat *row, *column;
	div_t d;
	int i, j;

	for (i = 0; i < 16; i++) {
		tmp.d[i] = 0;
		d = div(i, 4);
		row = m->d + d.quot * 4;
		column = n->d + d.rem;
		for (j = 0; j < 4; j++)
			tmp.d[i] += row[j] * column[j * 4];
	}
	memcpy(m, &tmp, sizeof tmp);
}

static void
wlsc_matrix_translate(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix translate = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 }
	};

	wlsc_matrix_multiply(matrix, &translate);
}

static void
wlsc_matrix_scale(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix scale = {
		{ x, 0, 0, 0,  0, y, 0, 0,  0, 0, z, 0,  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &scale);
}

static void
wlsc_matrix_transform(struct wlsc_matrix *matrix, struct wlsc_vector *v)
{
	int i, j;
	struct wlsc_vector t;

	for (i = 0; i < 4; i++) {
		t.f[i] = 0;
		for (j = 0; j < 4; j++)
			t.f[i] += v->f[j] * matrix->d[i + j * 4];
	}

	*v = t;
}

WL_EXPORT void
wlsc_tweener_init(struct wlsc_tweener *tweener,
		  double k, double current, double target)
{
	tweener->k = k;
	tweener->friction = 100.0;
	tweener->current = current;
	tweener->previous = current;
	tweener->target = target;
}

WL_EXPORT void
wlsc_tweener_update(struct wlsc_tweener *tweener, uint32_t msec)
{
	double force, v, current, step;

	step = (msec - tweener->timestamp) / 500.0;
	tweener->timestamp = msec;

	current = tweener->current;
	v = current - tweener->previous;
	force = tweener->k * (tweener->target - current) / 10.0 +
		(tweener->previous - current) - v * tweener->friction;

	tweener->current =
		current + (current - tweener->previous) + force * step * step;
	tweener->previous = current;

	if (tweener->current >= 1.0) {
#ifdef TWEENER_BOUNCE
		tweener->current = 2.0 - tweener->current;
		tweener->previous = 2.0 - tweener->previous;
#else
		tweener->current = 1.0;
		tweener->previous = 1.0;
#endif
	}

	if (tweener->current <= 0.0) {
		tweener->current = 0.0;
		tweener->previous = 0.0;
	}
}

WL_EXPORT int
wlsc_tweener_done(struct wlsc_tweener *tweener)
{
	return fabs(tweener->previous - tweener->target) < 0.0002 &&
		fabs(tweener->current - tweener->target) < 0.0002;
}

WL_EXPORT struct wlsc_surface *
wlsc_surface_create(struct wlsc_compositor *compositor,
		    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	wl_list_init(&surface->surface.destroy_listener_list);
	wl_list_init(&surface->link);
	wl_list_init(&surface->buffer_link);
	surface->map_type = WLSC_SURFACE_MAP_UNMAPPED;

	glGenTextures(1, &surface->texture);
	glBindTexture(GL_TEXTURE_2D, surface->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	surface->compositor = compositor;
	surface->visual = NULL;
	surface->image = EGL_NO_IMAGE_KHR;
	surface->saved_texture = 0;
	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	wlsc_matrix_init(&surface->matrix);
	wlsc_matrix_scale(&surface->matrix, width, height, 1);
	wlsc_matrix_translate(&surface->matrix, x, y, 0);

	wlsc_matrix_init(&surface->matrix_inv);
	wlsc_matrix_translate(&surface->matrix_inv, -x, -y, 0);
	wlsc_matrix_scale(&surface->matrix_inv, 1.0 / width, 1.0 / height, 1);

	return surface;
}

WL_EXPORT void
wlsc_surface_damage_rectangle(struct wlsc_surface *surface,
			      int32_t x, int32_t y,
			      int32_t width, int32_t height)
{
	struct wlsc_compositor *compositor = surface->compositor;

	pixman_region32_union_rect(&compositor->damage_region,
				   &compositor->damage_region,
				   surface->x + x, surface->y + y,
				   width, height);
	wlsc_compositor_schedule_repaint(compositor);
}

WL_EXPORT void
wlsc_surface_damage(struct wlsc_surface *surface)
{
	wlsc_surface_damage_rectangle(surface, 0, 0,
				      surface->width, surface->height);
}

WL_EXPORT uint32_t
wlsc_compositor_get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
destroy_surface(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_surface *surface =
		container_of(resource, struct wlsc_surface, surface.resource);
	struct wl_listener *l, *next;
	struct wlsc_compositor *compositor = surface->compositor;
	uint32_t time;

	wlsc_surface_damage(surface);

	wl_list_remove(&surface->link);
	if (surface->saved_texture == 0)
		glDeleteTextures(1, &surface->texture);
	else
		glDeleteTextures(1, &surface->saved_texture);


	if (surface->image != EGL_NO_IMAGE_KHR)
		compositor->destroy_image(compositor->display,
					  surface->image);

	wl_list_remove(&surface->buffer_link);

	time = wlsc_compositor_get_time();
	wl_list_for_each_safe(l, next,
			      &surface->surface.destroy_listener_list, link)
		l->func(l, &surface->surface, time);

	free(surface);
}

static void
wlsc_buffer_attach(struct wl_buffer *buffer, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;
	struct wl_list *surfaces_attached_to;

	if (es->saved_texture != 0)
		es->texture = es->saved_texture;

	glBindTexture(GL_TEXTURE_2D, es->texture);

	if (wl_buffer_is_shm(buffer)) {
		/* Unbind any EGLImage texture that may be bound, so we don't
		 * overwrite it.*/
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     0, 0, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		es->pitch = wl_shm_buffer_get_stride(buffer) / 4;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     es->pitch, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));
		es->visual = buffer->visual;

		surfaces_attached_to = buffer->user_data;

		wl_list_remove(&es->buffer_link);
		wl_list_insert(surfaces_attached_to, &es->buffer_link);
	} else {
		es->image = ec->create_image(ec->display, NULL,
					     EGL_WAYLAND_BUFFER_WL,
					     buffer, NULL);
		
		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);
		es->visual = buffer->visual;
		es->pitch = es->width;
	}
}

static void
wlsc_sprite_attach(struct wlsc_sprite *sprite, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;

	es->pitch = es->width;
	es->image = sprite->image;
	if (sprite->image != EGL_NO_IMAGE_KHR) {
		glBindTexture(GL_TEXTURE_2D, es->texture);
		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);
	} else {
		if (es->saved_texture == 0)
			es->saved_texture = es->texture;
		es->texture = sprite->texture;
	}

	es->visual = sprite->visual;
}

enum sprite_usage {
	SPRITE_USE_CURSOR = (1 << 0),
};

static struct wlsc_sprite *
create_sprite_from_png(struct wlsc_compositor *ec,
		       const char *filename, uint32_t usage)
{
	uint32_t *pixels;
	struct wlsc_sprite *sprite;
	int32_t width, height;
	uint32_t stride;

	pixels = wlsc_load_image(filename, &width, &height, &stride);
	if (pixels == NULL)
		return NULL;

	sprite = malloc(sizeof *sprite);
	if (sprite == NULL) {
		free(pixels);
		return NULL;
	}

	sprite->visual = &ec->compositor.premultiplied_argb_visual;
	sprite->width = width;
	sprite->height = height;
	sprite->image = EGL_NO_IMAGE_KHR;

	if (usage & SPRITE_USE_CURSOR && ec->create_cursor_image != NULL)
		sprite->image = ec->create_cursor_image(ec, width, height);

	glGenTextures(1, &sprite->texture);
	glBindTexture(GL_TEXTURE_2D, sprite->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (sprite->image != EGL_NO_IMAGE_KHR) {
		ec->image_target_texture_2d(GL_TEXTURE_2D, sprite->image);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, width, height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
	}

	free(pixels);

	return sprite;
}

static const struct {
	const char *filename;
	int hotspot_x, hotspot_y;
} pointer_images[] = {
	{ DATADIR "/wayland/bottom_left_corner.png",	 6, 30 },
	{ DATADIR "/wayland/bottom_right_corner.png",	28, 28 },
	{ DATADIR "/wayland/bottom_side.png",		16, 20 },
	{ DATADIR "/wayland/grabbing.png",		20, 17 },
	{ DATADIR "/wayland/left_ptr.png",		10,  5 },
	{ DATADIR "/wayland/left_side.png",		10, 20 },
	{ DATADIR "/wayland/right_side.png",		30, 19 },
	{ DATADIR "/wayland/top_left_corner.png",	 8,  8 },
	{ DATADIR "/wayland/top_right_corner.png",	26,  8 },
	{ DATADIR "/wayland/top_side.png",		18,  8 },
	{ DATADIR "/wayland/xterm.png",			15, 15 }
};

static void
create_pointer_images(struct wlsc_compositor *ec)
{
	int i, count;

	count = ARRAY_LENGTH(pointer_images);
	ec->pointer_sprites = malloc(count * sizeof *ec->pointer_sprites);
	for (i = 0; i < count; i++) {
		ec->pointer_sprites[i] =
			create_sprite_from_png(ec,
					       pointer_images[i].filename,
					       SPRITE_USE_CURSOR);
	}
}

static struct wlsc_surface *
background_create(struct wlsc_output *output, const char *filename)
{
	struct wlsc_surface *background;
	struct wlsc_sprite *sprite;

	background = wlsc_surface_create(output->compositor,
					 output->x, output->y,
					 output->width, output->height);
	if (background == NULL)
		return NULL;

	sprite = create_sprite_from_png(output->compositor, filename, 0);
	if (sprite == NULL) {
		free(background);
		return NULL;
	}

	wlsc_sprite_attach(sprite, &background->surface);

	return background;
}

static void
wlsc_surface_draw(struct wlsc_surface *es,
		  struct wlsc_output *output, pixman_region32_t *clip)
{
	struct wlsc_compositor *ec = es->compositor;
	GLfloat *v, inv_width, inv_height;
	unsigned int *p;
	pixman_region32_t repaint;
	pixman_box32_t *rectangles;
	int i, n;

	pixman_region32_init_rect(&repaint,
				  es->x, es->y, es->width, es->height);
	pixman_region32_intersect(&repaint, &repaint, clip);
	if (!pixman_region32_not_empty(&repaint))
		return;

	if (es->visual == &ec->compositor.argb_visual) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else if (es->visual == &ec->compositor.premultiplied_argb_visual) {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	rectangles = pixman_region32_rectangles(&repaint, &n);
	v = wl_array_add(&ec->vertices, n * 16 * sizeof *v);
	p = wl_array_add(&ec->indices, n * 6 * sizeof *p);
	inv_width = 1.0 / es->pitch;
	inv_height = 1.0 / es->height;
	for (i = 0; i < n; i++, v += 16, p += 6) {
		v[ 0] = rectangles[i].x1;
		v[ 1] = rectangles[i].y1;
		v[ 2] = (GLfloat) (rectangles[i].x1 - es->x) * inv_width;
		v[ 3] = (GLfloat) (rectangles[i].y1 - es->y) * inv_height;

		v[ 4] = rectangles[i].x1;
		v[ 5] = rectangles[i].y2;
		v[ 6] = v[ 2];
		v[ 7] = (GLfloat) (rectangles[i].y2 - es->y) * inv_height;

		v[ 8] = rectangles[i].x2;
		v[ 9] = rectangles[i].y1;
		v[10] = (GLfloat) (rectangles[i].x2 - es->x) * inv_width;
		v[11] = v[ 3];

		v[12] = rectangles[i].x2;
		v[13] = rectangles[i].y2;
		v[14] = v[10];
		v[15] = v[ 7];

		p[0] = i * 4 + 0;
		p[1] = i * 4 + 1;
		p[2] = i * 4 + 2;
		p[3] = i * 4 + 2;
		p[4] = i * 4 + 1;
		p[5] = i * 4 + 3;
	}

	glBindTexture(GL_TEXTURE_2D, es->texture);
	v = ec->vertices.data;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[0]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[2]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawElements(GL_TRIANGLES, n * 6, GL_UNSIGNED_INT, ec->indices.data);

	ec->vertices.size = 0;
	ec->indices.size = 0;
	pixman_region32_fini(&repaint);
}

static void
wlsc_surface_raise(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(&compositor->surface_list, &surface->link);
}

WL_EXPORT void
wlsc_surface_update_matrix(struct wlsc_surface *es)
{
	wlsc_matrix_init(&es->matrix);
	wlsc_matrix_scale(&es->matrix, es->width, es->height, 1);
	wlsc_matrix_translate(&es->matrix, es->x, es->y, 0);

	wlsc_matrix_init(&es->matrix_inv);
	wlsc_matrix_translate(&es->matrix_inv, -es->x, -es->y, 0);
	wlsc_matrix_scale(&es->matrix_inv,
			  1.0 / es->width, 1.0 / es->height, 1);
}

WL_EXPORT void
wlsc_compositor_damage_all(struct wlsc_compositor *compositor)
{
	struct wlsc_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		wlsc_output_damage(output);
}

WL_EXPORT void
wlsc_output_finish_frame(struct wlsc_output *output, int msecs)
{
	struct wlsc_compositor *compositor = output->compositor;
	struct wlsc_surface *es;
	struct wlsc_animation *animation, *next;

	wl_list_for_each(es, &compositor->surface_list, link) {
		if (es->output == output) {
			wl_display_post_frame(compositor->wl_display,
					      &es->surface, msecs);
		}
	}

	output->finished = 1;

	wl_event_source_timer_update(compositor->timer_source, 5);
	compositor->repaint_on_timeout = 1;

	wl_list_for_each_safe(animation, next,
			      &compositor->animation_list, link)
		animation->frame(animation, output, msecs);
}

WL_EXPORT void
wlsc_output_damage(struct wlsc_output *output)
{
	struct wlsc_compositor *compositor = output->compositor;

	pixman_region32_union_rect(&compositor->damage_region,
				   &compositor->damage_region,
				   output->x, output->y,
				   output->width, output->height);
	wlsc_compositor_schedule_repaint(compositor);
}

static void
fade_frame(struct wlsc_animation *animation,
	   struct wlsc_output *output, uint32_t msecs)
{
	struct wlsc_compositor *compositor =
		container_of(animation,
			     struct wlsc_compositor, fade.animation);

	wlsc_tweener_update(&compositor->fade.tweener, msecs);
	if (wlsc_tweener_done(&compositor->fade.tweener)) {
		if (compositor->fade.tweener.current > 0.999) {
			compositor->state = WLSC_COMPOSITOR_SLEEPING;
			compositor->shell->lock(compositor->shell);
		}
		compositor->fade.tweener.current =
			compositor->fade.tweener.target;
		wl_list_remove(&animation->link);
		wl_list_init(&animation->link);
	}

	wlsc_output_damage(output);
}

static void
fade_output(struct wlsc_output *output,
	    GLfloat tint, pixman_region32_t *region)
{
	struct wlsc_compositor *compositor = output->compositor;
	struct wlsc_surface surface;
	GLfloat color[4] = { 0.0, 0.0, 0.0, tint };

	surface.compositor = compositor;
	surface.x = output->x;
	surface.y = output->y;
	surface.width = output->width;
	surface.height = output->height;
	surface.texture = GL_NONE;

	if (tint <= 1.0)
		surface.visual =
			&compositor->compositor.premultiplied_argb_visual;
	else
		surface.visual = &compositor->compositor.rgb_visual;

	glUseProgram(compositor->solid_shader.program);
	glUniformMatrix4fv(compositor->solid_shader.proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform4fv(compositor->solid_shader.color_uniform, 1, color);
	wlsc_surface_draw(&surface, output, region);
}
 
static void
wlsc_output_repaint(struct wlsc_output *output)
{
	struct wlsc_compositor *ec = output->compositor;
	struct wlsc_surface *es;
	struct wlsc_input_device *eid;
	pixman_region32_t new_damage, total_damage, repaint;
	int using_hardware_cursor = 1;

	output->prepare_render(output);

	glViewport(0, 0, output->width, output->height);

	glUseProgram(ec->texture_shader.program);
	glUniformMatrix4fv(ec->texture_shader.proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform1i(ec->texture_shader.tex_uniform, 0);

	pixman_region32_init(&new_damage);
	pixman_region32_init(&total_damage);
	pixman_region32_intersect_rect(&new_damage,
				       &ec->damage_region,
				       output->x, output->y,
				       output->width, output->height);
	pixman_region32_subtract(&ec->damage_region,
				 &ec->damage_region, &new_damage);
	pixman_region32_union(&total_damage, &new_damage,
			      &output->previous_damage_region);
	pixman_region32_copy(&output->previous_damage_region, &new_damage);

	if (ec->focus)
		if (output->set_hardware_cursor(output, ec->input_device) < 0)
			using_hardware_cursor = 0;
	if (ec->fade.tweener.current > 0.001)
		using_hardware_cursor = 0;

	es = container_of(ec->surface_list.next, struct wlsc_surface, link);
	if (es->fullscreen_output == output) {
		if (es->visual == &ec->compositor.rgb_visual &&
		    using_hardware_cursor) {
			if (output->prepare_scanout_surface(output, es) == 0) {
				/* We're drawing nothing now,
				 * draw the damaged regions later. */
				pixman_region32_union(&ec->damage_region,
						      &ec->damage_region,
						      &total_damage);
				return;
			}
		}

		if (es->width < output->width ||
		    es->height < output->height)
			glClear(GL_COLOR_BUFFER_BIT);
		wlsc_surface_draw(es, output, &total_damage);
	} else {
		wl_list_for_each(es, &ec->surface_list, link) {
			if (es->visual != &ec->compositor.rgb_visual)
				continue;

			pixman_region32_init_rect(&repaint,
						  es->x, es->y,
						  es->width, es->height);
			wlsc_surface_draw(es, output, &total_damage);
			pixman_region32_subtract(&total_damage,
						 &total_damage, &repaint);
		}

		if (output->background)
			wlsc_surface_draw(output->background,
					  output, &total_damage);
		else
			fade_output(output, 1.0, &total_damage);

		glUseProgram(ec->texture_shader.program);
		wl_list_for_each_reverse(es, &ec->surface_list, link) {
			if (ec->overlay == es)
				continue;

			if (es->visual == &ec->compositor.rgb_visual) {
				pixman_region32_union_rect(&total_damage,
							   &total_damage,
							   es->x, es->y,
							   es->width, es->height);
				continue;
			}

			wlsc_surface_draw(es, output, &total_damage);
		}
	}

	if (ec->overlay)
		wlsc_surface_draw(ec->overlay, output, &total_damage);

	if (ec->focus)
		wl_list_for_each(eid, &ec->input_device_list, link) {
			if (&eid->input_device != ec->input_device ||
			    !using_hardware_cursor)
				wlsc_surface_draw(eid->sprite, output,
						  &total_damage);
		}

	if (ec->fade.tweener.current > 0.001)
		fade_output(output, ec->fade.tweener.current, &total_damage);
}

static int
repaint(void *data)
{
	struct wlsc_compositor *ec = data;
	struct wlsc_output *output;
	int repainted_all_outputs = 1;

	wl_list_for_each(output, &ec->output_list, link) {
		if (!output->repaint_needed)
			continue;

		if (!output->finished) {
			repainted_all_outputs = 0;
			continue;
		}

		wlsc_output_repaint(output);
		output->finished = 0;
		output->repaint_needed = 0;
		output->present(output);
	}

	if (repainted_all_outputs)
		ec->repaint_on_timeout = 0;
	else
		wl_event_source_timer_update(ec->timer_source, 1);

	return 1;
}

WL_EXPORT void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor)
{
	struct wlsc_output *output;

	if (compositor->state == WLSC_COMPOSITOR_SLEEPING)
		return;

	wl_list_for_each(output, &compositor->output_list, link)
		output->repaint_needed = 1;

	if (compositor->repaint_on_timeout)
		return;

	wl_event_source_timer_update(compositor->timer_source, 1);
	compositor->repaint_on_timeout = 1;
}

WL_EXPORT void
wlsc_compositor_fade(struct wlsc_compositor *compositor, float tint)
{
	int done;

	done = wlsc_tweener_done(&compositor->fade.tweener);
	compositor->fade.tweener.target = tint;
	if (wlsc_tweener_done(&compositor->fade.tweener))
		return;

	if (done)
		compositor->fade.tweener.timestamp =
			wlsc_compositor_get_time();

	wlsc_compositor_damage_all(compositor);
	if (wl_list_empty(&compositor->fade.animation.link))
		wl_list_insert(compositor->animation_list.prev,
			       &compositor->fade.animation.link);
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->resource, client);
}

WL_EXPORT void
wlsc_surface_assign_output(struct wlsc_surface *es)
{
	struct wlsc_compositor *ec = es->compositor;
	struct wlsc_output *output;

	struct wlsc_output *tmp = es->output;
	es->output = NULL;

	wl_list_for_each(output, &ec->output_list, link) {
		if (output->x < es->x && es->x < output->x + output->width &&
		    output->y < es->y && es->y < output->y + output->height) {
			if (output != tmp)
				printf("assiging surface %p to output %p\n",
				       es, output);
			es->output = output;
		}
	}
	
	if (es->output == NULL) {
		printf("no output found\n");
		es->output = container_of(ec->output_list.next,
					  struct wlsc_output, link);
	}
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer,
	       int32_t x, int32_t y)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	/* FIXME: This damages the entire old surface, but we should
	 * really just damage the part that's no longer covered by the
	 * surface.  Anything covered by the new surface will be
	 * damaged by the client. */
	wlsc_surface_damage(es);

	wlsc_buffer_attach(buffer, surface);

	switch (es->map_type) {
	case WLSC_SURFACE_MAP_FULLSCREEN:
		es->x = (es->fullscreen_output->width - es->width) / 2;
		es->y = (es->fullscreen_output->height - es->height) / 2;
		break;
	default:
		es->x += x;
		es->y += y;
		break;
	}
	es->width = buffer->width;
	es->height = buffer->height;
	if (x != 0 || y != 0)
		wlsc_surface_assign_output(es);
	wlsc_surface_update_matrix(es);

	es->compositor->shell->attach(es->compositor->shell, es);
}

static void
surface_map_toplevel(struct wl_client *client,
		     struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;

	switch (es->map_type) {
	case WLSC_SURFACE_MAP_UNMAPPED:
		es->x = 10 + random() % 400;
		es->y = 10 + random() % 400;
		wlsc_surface_update_matrix(es);
		/* assign to first output */
		es->output = container_of(ec->output_list.next,
					  struct wlsc_output, link);
		wl_list_insert(&es->compositor->surface_list, &es->link);
		break;
	case WLSC_SURFACE_MAP_TOPLEVEL:
		return;
	case WLSC_SURFACE_MAP_FULLSCREEN:
		es->fullscreen_output = NULL;
		es->x = es->saved_x;
		es->y = es->saved_y;
		wlsc_surface_update_matrix(es);
		break;
	default:
		break;
	}

	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_TOPLEVEL;
}

static void
surface_map_transient(struct wl_client *client,
		      struct wl_surface *surface, struct wl_surface *parent,
		      int x, int y, uint32_t flags)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_surface *pes = (struct wlsc_surface *) parent;

	switch (es->map_type) {
	case WLSC_SURFACE_MAP_UNMAPPED:
		wl_list_insert(&es->compositor->surface_list, &es->link);
		/* assign to parents output  */
		es->output = pes->output;
		break;
	case WLSC_SURFACE_MAP_FULLSCREEN:
		es->fullscreen_output = NULL;
		break;
	default:
		break;
	}

	es->x = pes->x + x;
	es->y = pes->y + y;

	wlsc_surface_update_matrix(es);
	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_TRANSIENT;
}

static void
surface_map_fullscreen(struct wl_client *client, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_output *output;

	/* FIXME: Fullscreen on first output */
	/* FIXME: Handle output going away */
	output = container_of(es->compositor->output_list.next,
			      struct wlsc_output, link);

	switch (es->map_type) {
	case WLSC_SURFACE_MAP_UNMAPPED:
		es->x = 10 + random() % 400;
		es->y = 10 + random() % 400;
		/* assign to first output */
		es->output = output;
		wl_list_insert(&es->compositor->surface_list, &es->link);
		break;
	case WLSC_SURFACE_MAP_FULLSCREEN:
		return;
	default:
		break;
	}

	es->saved_x = es->x;
	es->saved_y = es->y;
	es->x = (output->width - es->width) / 2;
	es->y = (output->height - es->height) / 2;
	es->fullscreen_output = output;
	wlsc_surface_update_matrix(es);
	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_FULLSCREEN;
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	wlsc_surface_damage_rectangle(es, x, y, width, height);
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map_toplevel,
	surface_map_transient,
	surface_map_fullscreen,
	surface_damage
};

static void
wlsc_input_device_attach(struct wlsc_input_device *device,
			 int x, int y, int width, int height)
{
	wlsc_surface_damage(device->sprite);

	device->hotspot_x = x;
	device->hotspot_y = y;

	device->sprite->x = device->input_device.x - device->hotspot_x;
	device->sprite->y = device->input_device.y - device->hotspot_y;
	device->sprite->width = width;
	device->sprite->height = height;
	wlsc_surface_update_matrix(device->sprite);

	wlsc_surface_damage(device->sprite);
}

static void
wlsc_input_device_attach_buffer(struct wlsc_input_device *device,
				struct wl_buffer *buffer, int x, int y)
{
	wlsc_buffer_attach(buffer, &device->sprite->surface);
	wlsc_input_device_attach(device, x, y, buffer->width, buffer->height);
}

static void
wlsc_input_device_attach_sprite(struct wlsc_input_device *device,
				struct wlsc_sprite *sprite, int x, int y)
{
	wlsc_sprite_attach(sprite, &device->sprite->surface);
	wlsc_input_device_attach(device, x, y, sprite->width, sprite->height);
}

void
wlsc_input_device_set_pointer_image(struct wlsc_input_device *device,
				    enum wlsc_pointer_type type)
{
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->input_device.compositor;

	wlsc_input_device_attach_sprite(device,
					compositor->pointer_sprites[type],
					pointer_images[type].hotspot_x,
					pointer_images[type].hotspot_y);
}

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;
	struct wlsc_surface *surface;

	surface = wlsc_surface_create(ec, 0, 0, 0, 0);
	if (surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &surface_interface;
	surface->surface.client = client;

	wl_client_add_resource(client, &surface->surface.resource);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

static void
wlsc_surface_transform(struct wlsc_surface *surface,
		       int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	struct wlsc_vector v = { { x, y, 0, 1 } };

	wlsc_matrix_transform(&surface->matrix_inv, &v);
	*sx = v.f[0] * surface->width;
	*sy = v.f[1] * surface->height;
}

struct wlsc_surface *
pick_surface(struct wl_input_device *device, int32_t *sx, int32_t *sy)
{
	struct wlsc_compositor *ec =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;

	wl_list_for_each(es, &ec->surface_list, link) {
		wlsc_surface_transform(es, device->x, device->y, sx, sy);
		if (0 <= *sx && *sx < es->width &&
		    0 <= *sy && *sy < es->height)
			return es;
	}

	return NULL;
}


static void
motion_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) grab->input_device;
	struct wlsc_surface *es =
		(struct wlsc_surface *) device->input_device.pointer_focus;
	int32_t sx, sy;

	wlsc_surface_transform(es, x, y, &sx, &sy);
	wl_client_post_event(es->surface.client,
			     &device->input_device.object,
			     WL_INPUT_DEVICE_MOTION,
			     time, x, y, sx, sy);
}

static void
motion_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
	wl_client_post_event(grab->input_device->pointer_focus->client,
			     &grab->input_device->object,
			     WL_INPUT_DEVICE_BUTTON,
			     time, button, state);
}

static void
motion_grab_end(struct wl_grab *grab, uint32_t time)
{
}

static const struct wl_grab_interface motion_grab_interface = {
	motion_grab_motion,
	motion_grab_button,
	motion_grab_end
};

WL_EXPORT void
wlsc_compositor_wake(struct wlsc_compositor *compositor)
{
	if (compositor->idle_inhibit)
		return;

	wlsc_compositor_fade(compositor, 0.0);
	compositor->state = WLSC_COMPOSITOR_ACTIVE;

	wl_event_source_timer_update(compositor->idle_source,
				     option_idle_time * 1000);
}

static void
wlsc_compositor_idle_inhibit(struct wlsc_compositor *compositor)
{
	wlsc_compositor_wake(compositor);
	compositor->idle_inhibit++;
}

static void
wlsc_compositor_idle_release(struct wlsc_compositor *compositor)
{
	compositor->idle_inhibit--;
	wlsc_compositor_wake(compositor);
}

static int
idle_handler(void *data)
{
	struct wlsc_compositor *compositor = data;

	if (compositor->idle_inhibit)
		return 1;

	wlsc_compositor_fade(compositor, 1.0);

	return 1;
}

void
notify_motion(struct wl_input_device *device, uint32_t time, int x, int y)
{
	struct wlsc_surface *es;
	struct wlsc_compositor *ec =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_output *output;
	const struct wl_grab_interface *interface;
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	int32_t sx, sy;
	int x_valid = 0, y_valid = 0;
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

	wlsc_compositor_wake(ec);

	wl_list_for_each(output, &ec->output_list, link) {
		if (output->x <= x && x <= output->x + output->width)
			x_valid = 1;

		if (output->y <= y && y <= output->y + output->height)
			y_valid = 1;

		/* FIXME: calculate this only on output addition/deletion */
		if (output->x < min_x)
			min_x = output->x;
		if (output->y < min_y)
			min_y = output->y;

		if (output->x + output->width > max_x)
			max_x = output->x + output->width;
		if (output->y + output->height > max_y)
			max_y = output->y + output->height;
	}
	
	if (!x_valid) {
		if (x < min_x)
			x = min_x;
		else if (x >= max_x)
			x = max_x;
	}
	if (!y_valid) {
		if (y < min_y)
			y = min_y;
		else  if (y >= max_y)
			y = max_y;
	}

	device->x = x;
	device->y = y;

	if (device->grab) {
		interface = device->grab->interface;
		interface->motion(device->grab, time, x, y);
	} else {
		es = pick_surface(device, &sx, &sy);
		wl_input_device_set_pointer_focus(device,
						  &es->surface,
						  time, x, y, sx, sy);
		if (es)
			wl_client_post_event(es->surface.client,
					     &device->object,
					     WL_INPUT_DEVICE_MOTION,
					     time, x, y, sx, sy);
	}

	wlsc_surface_damage(wd->sprite);

	wd->sprite->x = device->x - wd->hotspot_x;
	wd->sprite->y = device->y - wd->hotspot_y;
	wlsc_surface_update_matrix(wd->sprite);

	wlsc_surface_damage(wd->sprite);
}

WL_EXPORT void
wlsc_surface_activate(struct wlsc_surface *surface,
		      struct wlsc_input_device *device, uint32_t time)
{
	wlsc_surface_raise(surface);
	if (device->selection)
		wlsc_selection_set_focus(device->selection,
					 &surface->surface, time);

	wl_input_device_set_keyboard_focus(&device->input_device,
					   &surface->surface,
					   time);
}

struct wlsc_binding {
	uint32_t key;
	uint32_t button;
	uint32_t modifier;
	wlsc_binding_handler_t handler;
	void *data;
	struct wl_list link;
};

void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_binding *b;
	struct wlsc_surface *surface =
		(struct wlsc_surface *) device->pointer_focus;

	if (state)
		wlsc_compositor_idle_inhibit(compositor);
	else
		wlsc_compositor_idle_release(compositor);

	if (state && surface && device->grab == NULL) {
		wlsc_surface_activate(surface, wd, time);
		wl_input_device_start_grab(device,
					   &device->motion_grab,
					   button, time);
	}

	wl_list_for_each(b, &compositor->binding_list, link) {
		if (b->button == button &&
		    b->modifier == wd->modifier_state && state) {
			b->handler(&wd->input_device,
				   time, 0, button, state, b->data);
			break;
		}
	}

	if (device->grab)
		device->grab->interface->button(device->grab, time,
						button, state);

	if (!state && device->grab && device->grab_button == button)
		wl_input_device_end_grab(device, time);
}

static void
terminate_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct wlsc_compositor *compositor = data;

	if (state)
		wl_display_terminate(compositor->wl_display);
}

WL_EXPORT struct wlsc_binding *
wlsc_compositor_add_binding(struct wlsc_compositor *compositor,
			    uint32_t key, uint32_t button, uint32_t modifier,
			    wlsc_binding_handler_t handler, void *data)
{
	struct wlsc_binding *binding;

	binding = malloc(sizeof *binding);
	if (binding == NULL)
		return NULL;

	binding->key = key;
	binding->button = button;
	binding->modifier = modifier;
	binding->handler = handler;
	binding->data = data;
	wl_list_insert(compositor->binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT void
wlsc_binding_destroy(struct wlsc_binding *binding)
{
	wl_list_remove(&binding->link);
	free(binding);
}

static void
update_modifier_state(struct wlsc_input_device *device,
		      uint32_t key, uint32_t state)
{
	uint32_t modifier;

	switch (key) {
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		modifier = MODIFIER_CTRL;
		break;

	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		modifier = MODIFIER_ALT;
		break;

	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		modifier = MODIFIER_SUPER;
		break;

	default:
		modifier = 0;
		break;
	}

	if (state)
		device->modifier_state |= modifier;
	else
		device->modifier_state &= ~modifier;
}

void
notify_key(struct wl_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	uint32_t *k, *end;
	struct wlsc_binding *b;

	if (state)
		wlsc_compositor_idle_inhibit(compositor);
	else
		wlsc_compositor_idle_release(compositor);

	wl_list_for_each(b, &compositor->binding_list, link) {
		if (b->key == key &&
		    b->modifier == wd->modifier_state) {
			b->handler(&wd->input_device,
				   time, key, 0, state, b->data);
			break;
		}
	}

	update_modifier_state(wd, key, state);
	end = device->keys.data + device->keys.size;
	for (k = device->keys.data; k < end; k++) {
		if (*k == key)
			*k = *--end;
	}
	device->keys.size = (void *) end - device->keys.data;
	if (state) {
		k = wl_array_add(&device->keys, sizeof *k);
		*k = key;
	}

	if (device->keyboard_focus != NULL)
		wl_client_post_event(device->keyboard_focus->client,
				     &device->object,
				     WL_INPUT_DEVICE_KEY, time, key, state);
}

void
notify_pointer_focus(struct wl_input_device *device,
		     uint32_t time, struct wlsc_output *output,
		     int32_t x, int32_t y)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;
	int32_t sx, sy;

	if (output) {
		device->x = x;
		device->y = y;
		es = pick_surface(device, &sx, &sy);
		wl_input_device_set_pointer_focus(device,
						  &es->surface,
						  time, x, y, sx, sy);

		compositor->focus = 1;

		wd->sprite->x = device->x - wd->hotspot_x;
		wd->sprite->y = device->y - wd->hotspot_y;
		wlsc_surface_update_matrix(wd->sprite);
	} else {
		wl_input_device_set_pointer_focus(device, NULL,
						  time, 0, 0, 0, 0);
		compositor->focus = 0;
	}

	wlsc_surface_damage(wd->sprite);
}

void
notify_keyboard_focus(struct wl_input_device *device,
		      uint32_t time, struct wlsc_output *output,
		      struct wl_array *keys)
{
	struct wlsc_input_device *wd =
		(struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;
	uint32_t *k, *end;

	if (!wl_list_empty(&compositor->surface_list))
		es = container_of(compositor->surface_list.next,
				  struct wlsc_surface, link);
	else
		es = NULL;

	if (output) {
		wl_array_copy(&wd->input_device.keys, keys);
		wd->modifier_state = 0;
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++) {
			wlsc_compositor_idle_inhibit(compositor);
			update_modifier_state(wd, *k, 1);
		}

		wl_input_device_set_keyboard_focus(&wd->input_device,
						   &es->surface, time);
	} else {
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++)
			wlsc_compositor_idle_release(compositor);

		wd->modifier_state = 0;
		wl_input_device_set_keyboard_focus(&wd->input_device,
						   NULL, time);
	}
}


static void
input_device_attach(struct wl_client *client,
		    struct wl_input_device *device_base,
		    uint32_t time,
		    struct wl_buffer *buffer, int32_t x, int32_t y)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) device_base;

	if (time < device->input_device.pointer_focus_time)
		return;
	if (device->input_device.pointer_focus == NULL)
		return;
	if (device->input_device.pointer_focus->client != client)
		return;

	if (buffer == NULL) {
		wlsc_input_device_set_pointer_image(device,
						    WLSC_POINTER_LEFT_PTR);
		return;
	}

	wlsc_input_device_attach_buffer(device, buffer, x, y);
}

const static struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec)
{
	wl_input_device_init(&device->input_device, &ec->compositor);

	device->input_device.object.interface = &wl_input_device_interface;
	device->input_device.object.implementation =
		(void (**)(void)) &input_device_interface;
	wl_display_add_object(ec->wl_display, &device->input_device.object);
	wl_display_add_global(ec->wl_display, &device->input_device.object, NULL);

	device->sprite = wlsc_surface_create(ec,
					     device->input_device.x,
					     device->input_device.y, 32, 32);
	device->hotspot_x = 16;
	device->hotspot_y = 16;
	device->modifier_state = 0;

	device->input_device.motion_grab.interface = &motion_grab_interface;

	wl_list_insert(ec->input_device_list.prev, &device->link);

	wlsc_input_device_set_pointer_image(device, WLSC_POINTER_LEFT_PTR);
}

static void
wlsc_output_post_geometry(struct wl_client *client,
			  struct wl_object *global, uint32_t version)
{
	struct wlsc_output *output =
		container_of(global, struct wlsc_output, object);

	wl_client_post_event(client, global,
			     WL_OUTPUT_GEOMETRY,
			     output->x, output->y,
			     output->width, output->height);
}

static const char vertex_shader[] =
	"uniform mat4 proj;\n"
	"attribute vec2 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = proj * vec4(position, 0.0, 1.0);\n"
	"   v_texcoord = texcoord;\n"
	"}\n";

static const char texture_fragment_shader[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char solid_fragment_shader[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = color\n;"
	"}\n";

static int
compile_shader(GLenum type, const char *source)
{
	GLuint s;
	char msg[512];
	GLint status;

	s = glCreateShader(type);
	glShaderSource(s, 1, &source, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(s, sizeof msg, NULL, msg);
		fprintf(stderr, "shader info: %s\n", msg);
		return GL_NONE;
	}

	return s;
}

static int
wlsc_shader_init(struct wlsc_shader *shader,
		 const char *vertex_source, const char *fragment_source)
{
	char msg[512];
	GLint status;

	shader->vertex_shader =
		compile_shader(GL_VERTEX_SHADER, vertex_source);
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
		fprintf(stderr, "link info: %s\n", msg);
		return -1;
	}

	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->tex_uniform = glGetUniformLocation(shader->program, "tex");

	return 0;
}

static int
init_solid_shader(struct wlsc_shader *shader,
		  GLuint vertex_shader, const char *fragment_source)
{
	GLint status;
	char msg[512];

	shader->vertex_shader = vertex_shader;
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
 		fprintf(stderr, "link info: %s\n", msg);
		return -1;
 	}
 
	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->color_uniform = glGetUniformLocation(shader->program, "color");

	return 0;
}

void
wlsc_output_destroy(struct wlsc_output *output)
{
	destroy_surface(&output->background->surface.resource, NULL);
}

void
wlsc_output_move(struct wlsc_output *output, int x, int y)
{
	struct wlsc_compositor *c = output->compositor;
	int flip;

	output->x = x;
	output->y = y;

	if (output->background) {
		output->background->x = x;
		output->background->y = y;
		wlsc_surface_update_matrix(output->background);
	}

	pixman_region32_init(&output->previous_damage_region);

	wlsc_matrix_init(&output->matrix);
	wlsc_matrix_translate(&output->matrix,
			      -output->x - output->width / 2.0,
			      -output->y - output->height / 2.0, 0);

	flip = (output->flags & WL_OUTPUT_FLIPPED) ? -1 : 1;
	wlsc_matrix_scale(&output->matrix,
			  2.0 / output->width,
			  flip * 2.0 / output->height, 1);

	pixman_region32_union_rect(&c->damage_region,
				   &c->damage_region,
				   x, y, output->width, output->height);
}

void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height, uint32_t flags)
{
	output->compositor = c;
	output->x = x;
	output->y = y;
	output->width = width;
	output->height = height;

	output->background =
		background_create(output, option_background);

	output->flags = flags;
	output->finished = 1;
	wlsc_output_move(output, x, y);

	output->object.interface = &wl_output_interface;
	wl_display_add_object(c->wl_display, &output->object);
	wl_display_add_global(c->wl_display, &output->object,
			      wlsc_output_post_geometry);
}

static void
shm_buffer_created(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to;

	surfaces_attached_to = malloc(sizeof *surfaces_attached_to);
	if (!surfaces_attached_to) {
		buffer->user_data = NULL;
		return;
	}

	wl_list_init(surfaces_attached_to);

	buffer->user_data = surfaces_attached_to;
}

static void
shm_buffer_damaged(struct wl_buffer *buffer,
		   int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct wlsc_surface *es;
	GLsizei tex_width = wl_shm_buffer_get_stride(buffer) / 4;

	wl_list_for_each(es, surfaces_attached_to, buffer_link) {
		glBindTexture(GL_TEXTURE_2D, es->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     tex_width, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));
		/* Hmm, should use glTexSubImage2D() here but GLES2 doesn't
		 * support any unpack attributes except GL_UNPACK_ALIGNMENT. */
	}
}

static void
shm_buffer_destroyed(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct wlsc_surface *es, *next;

	wl_list_for_each_safe(es, next, surfaces_attached_to, buffer_link) {
		wl_list_remove(&es->buffer_link);
		wl_list_init(&es->buffer_link);
	}

	free(surfaces_attached_to);
}

const static struct wl_shm_callbacks shm_callbacks = {
	shm_buffer_created,
	shm_buffer_damaged,
	shm_buffer_destroyed
};

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display)
{
	struct wl_event_loop *loop;
	const char *extensions;

	ec->wl_display = display;

	wl_compositor_init(&ec->compositor, &compositor_interface, display);

	ec->shm = wl_shm_init(display, &shm_callbacks);

	ec->image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	ec->image_target_renderbuffer_storage = (void *)
		eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	ec->create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
	ec->destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");
	ec->bind_display =
		(void *) eglGetProcAddress("eglBindWaylandDisplayWL");
	ec->unbind_display =
		(void *) eglGetProcAddress("eglUnbindWaylandDisplayWL");

	extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		fprintf(stderr,
			"GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	extensions =
		(const char *) eglQueryString(ec->display, EGL_EXTENSIONS);
	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->display, ec->wl_display);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->binding_list);
	wl_list_init(&ec->animation_list);
	wlsc_tweener_init(&ec->fade.tweener, 0.8, 0.0, 0.0);
	ec->fade.animation.frame = fade_frame;
	wl_list_init(&ec->fade.animation.link);

	wlsc_compositor_add_binding(ec, KEY_BACKSPACE, 0,
				    MODIFIER_CTRL | MODIFIER_ALT,
				    terminate_binding, ec);

	create_pointer_images(ec);

	screenshooter_create(ec);

	glActiveTexture(GL_TEXTURE0);

	if (wlsc_shader_init(&ec->texture_shader,
			     vertex_shader, texture_fragment_shader) < 0)
		return -1;
	if (init_solid_shader(&ec->solid_shader,
			      ec->texture_shader.vertex_shader,
			      solid_fragment_shader) < 0)
		return -1;

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, option_idle_time * 1000);

	ec->timer_source = wl_event_loop_add_timer(loop, repaint, ec);
	pixman_region32_init(&ec->damage_region);
	wlsc_compositor_schedule_repaint(ec);

	return 0;
}

static int on_term_signal(int signal_number, void *data)
{
	struct wlsc_compositor *ec = data;

	wl_display_terminate(ec->wl_display);

	return 1;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wlsc_compositor *ec;
	struct wl_event_loop *loop;
	int width, height, o;
	void *shell_module;
	int (*shell_init)(struct wlsc_compositor *ec);
	char *p;

	static const char opts[] = "b:c:g:S:i:s:";
	static const struct option longopts[ ] = {
		{ "background", 1, NULL, 'b' },
		{ "connector", 1, NULL, 'c' },
		{ "geometry", 1, NULL, 'g' },
		{ "socket", 1, NULL, 'S' },
		{ "idle-time", 1, NULL, 'i' },
		{ "shell", 1, NULL, 's' },
		{ NULL, }
	};

	width = 1024;
	height = 640;

	while (o = getopt_long(argc, argv, opts, longopts, &o), o > 0) {
		switch (o) {
		case 'b':
			option_background = optarg;
			break;
		case 'c':
			option_connector = strtol(optarg, &p, 0);
			if (*p != '\0') {
				fprintf(stderr,
					"invalid connection option: %s\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'g':
			if (sscanf(optarg, "%dx%d", &width, &height) != 2) {
				fprintf(stderr,
					"invalid geometry option: %s\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'S':
			option_socket_name = optarg;
			break;
		case 'i':
			option_idle_time = strtol(optarg, &p, 0);
			if (*p != '\0') {
				fprintf(stderr,
					"invalid idle time option: %s\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			option_shell = optarg;
			break;
		}
	}

	display = wl_display_create();

	ec = NULL;

	shell_init = desktop_shell_init;
	if (option_shell) {
		shell_module = dlopen(option_shell, RTLD_LAZY);
		if (!shell_module) {
			fprintf(stderr, "failed to load shell module: %m\n");
			exit(EXIT_FAILURE);
		}
		shell_init = dlsym(shell_module, "shell_init");
		if (!shell_init) {
			fprintf(stderr,
				"failed to lookup shell init function: %m\n");
			exit(EXIT_FAILURE);
		}
	}

#if BUILD_WAYLAND_COMPOSITOR
	if (getenv("WAYLAND_DISPLAY"))
		ec = wayland_compositor_create(display, width, height);
#endif

#if BUILD_X11_COMPOSITOR
	if (ec == NULL && getenv("DISPLAY"))
		ec = x11_compositor_create(display, width, height);
#endif

#if BUILD_OPENWFD_COMPOSITOR
	if (ec == NULL && getenv("OPENWFD"))
		ec = wfd_compositor_create(display, option_connector);
#endif

#if BUILD_DRM_COMPOSITOR
	if (ec == NULL)
		ec = drm_compositor_create(display, option_connector);
#endif

	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	if (shell_init(ec) < 0)
		exit(EXIT_FAILURE);

	if (wl_display_add_socket(display, option_socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_signal(loop, SIGTERM, on_term_signal, ec);
	wl_event_loop_add_signal(loop, SIGINT, on_term_signal, ec);

	wl_display_run(display);

	if (ec->has_bind_display)
		ec->unbind_display(ec->display, display);
	wl_display_destroy(display);

	ec->destroy(ec);

	return 0;
}
