/*
 * Copyright (C) 2013 DENSO CORPORATION
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdbool.h>

#include "compositor.h"

struct ivi_layout_surface;

struct ivi_shell
{
    struct wl_resource *resource;
    struct wl_listener destroy_listener;

    struct weston_compositor *compositor;

    struct wl_list ivi_surface_list; /* struct ivi_shell_surface::link */
    struct wl_list client_list;

    struct wl_listener show_input_panel_listener;
    struct wl_listener hide_input_panel_listener;
    struct wl_listener update_input_panel_listener;

    struct weston_layer panel_layer;
    struct weston_layer input_panel_layer;

    bool locked;
    bool showing_input_panels;

    struct {
        struct weston_surface *surface;
        pixman_box32_t cursor_rectangle;
    } text_input;

    struct {
        struct wl_resource *binding;
        struct wl_list surfaces;
    } input_panel;
};

struct weston_view *
get_default_view(struct weston_surface *surface);

int
input_panel_setup(struct ivi_shell *shell);

void
input_panel_destroy(struct ivi_shell *shell);

WL_EXPORT void
send_wl_shell_info(int32_t pid, const char *window_title, struct weston_surface *surface);

WL_EXPORT void
get_wl_shell_info(struct ivi_layout_surface *layout_surface, uint32_t id_surface,
                  int32_t *pid_ret, const char **window_title_ret);
