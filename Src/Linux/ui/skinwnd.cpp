#include "skinwnd.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <string.h>

bool handle_key(SkinWindow *w, GdkEventKey *e); // keyboard.cpp

static gboolean on_key(GtkWidget *, GdkEventKey *e, gpointer self)
{
	return handle_key((SkinWindow *)self, e) ? TRUE : FALSE;
}

bool SkinWindow::CanPositionWindows()
{
#ifdef GDK_WINDOWING_X11
	return GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
	return false;
#endif
}

SkinWindow::SkinWindow(const char *title, const char *role, Handler *h) : handler(h)
{
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_role(GTK_WINDOW(window), role);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_widget_set_app_paintable(window, TRUE);

	area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), area);
	gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
	                            GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
	g_signal_connect(area, "draw", G_CALLBACK(on_draw), this);
	g_signal_connect(area, "button-press-event", G_CALLBACK(on_button_press), this);
	g_signal_connect(area, "button-release-event", G_CALLBACK(on_button_release), this);
	g_signal_connect(area, "motion-notify-event", G_CALLBACK(on_motion), this);
	g_signal_connect(area, "scroll-event", G_CALLBACK(on_scroll), this);
	g_signal_connect(area, "grab-broken-event", G_CALLBACK(on_grab_broken), this);
	g_signal_connect(window, "focus-in-event", G_CALLBACK(on_focus), this);
	g_signal_connect(window, "focus-out-event", G_CALLBACK(on_focus), this);
	g_signal_connect(window, "delete-event", G_CALLBACK(on_delete), this);
	g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), this);

	static GtkTargetEntry targets[] = {{(gchar *)"text/uri-list", 0, 0}};
	gtk_drag_dest_set(area, GTK_DEST_DEFAULT_ALL, targets, 1, (GdkDragAction)(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
	g_signal_connect(area, "drag-data-received", G_CALLBACK(on_drag_data), this);
	gtk_widget_show(area);
}

SkinWindow::~SkinWindow()
{
	if (window) gtk_widget_destroy(window);
}

void SkinWindow::SetSize(int w, int h)
{
	if (w == buffer.Width() && h == buffer.Height()) return;
	Bitmap nb(w, h);
	if (buffer.Valid()) BitBlt(nb, 0, 0, w, h, buffer, 0, 0);
	buffer = nb;
	gtk_widget_set_size_request(area, w * scale, h * scale);
	gtk_window_resize(GTK_WINDOW(window), w * scale, h * scale);
	ApplyShape();
	Invalidate();
}

void SkinWindow::SetScale(int s)
{
	if (s < 1) s = 1;
	if (s == scale) return;
	overlay_visible = false;
	scale = s;
	gtk_widget_set_size_request(area, buffer.Width() * scale, buffer.Height() * scale);
	gtk_window_resize(GTK_WINDOW(window), buffer.Width() * scale, buffer.Height() * scale);
	ApplyShape();
	Invalidate();
}

void SkinWindow::Move(int x, int y)
{
	pos_x = x;
	pos_y = y;
	gtk_window_move(GTK_WINDOW(window), x, y);
}

void SkinWindow::Show()
{
	if (visible) return;
	visible = true;
	gtk_window_move(GTK_WINDOW(window), pos_x, pos_y);
	gtk_widget_show(window);
	gtk_window_move(GTK_WINDOW(window), pos_x, pos_y);
	ApplyShape();
}

void SkinWindow::Hide()
{
	if (!visible) return;
	visible = false;
	gtk_widget_hide(window);
}

void SkinWindow::Raise()
{
	if (visible) gtk_window_present(GTK_WINDOW(window));
}

void SkinWindow::SetKeepAbove(bool above)
{
	gtk_window_set_keep_above(GTK_WINDOW(window), above);
}

void SkinWindow::SetShape(const SkinRegion *region)
{
	shape = (region && !region->Empty()) ? region : nullptr;
	ApplyShape();
}

// region.txt polygons -> window shape (even-odd fill, like CreatePolyPolygonRgn(ALTERNATE))
void SkinWindow::ApplyShape()
{
	if (!gtk_widget_get_realized(window)) gtk_widget_realize(window);
	if (!shape)
	{
		gtk_widget_shape_combine_region(window, nullptr);
		return;
	}
	int w = buffer.Width() * scale, h = buffer.Height() * scale;
	cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A1, w, h);
	cairo_t *cr = cairo_create(mask);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	size_t p = 0;
	for (int count : shape->counts)
	{
		for (int i = 0; i < count && p + 1 < shape->points.size(); i++, p += 2)
		{
			double x = shape->points[p] * scale, y = shape->points[p + 1] * scale;
			if (i == 0) cairo_move_to(cr, x, y);
			else cairo_line_to(cr, x, y);
		}
		cairo_close_path(cr);
	}
	cairo_fill(cr);
	cairo_destroy(cr);
	cairo_region_t *rgn = gdk_cairo_region_create_from_surface(mask);
	gtk_widget_shape_combine_region(window, rgn);
	cairo_region_destroy(rgn);
	cairo_surface_destroy(mask);
}

void SkinWindow::Invalidate()
{
	gtk_widget_queue_draw(area);
}

void SkinWindow::InvalidateRect(int x, int y, int w, int h)
{
	gtk_widget_queue_draw_area(area, x * scale, y * scale, w * scale, h * scale);
}

void SkinWindow::ShowOverlay(int x, int y)
{
	if (overlay_visible && (x != overlay_x || y != overlay_y || overlay.Width() != overlay_w || overlay.Height() != overlay_h))
		gtk_widget_queue_draw_area(area, overlay_x * scale, overlay_y * scale, overlay_w, overlay_h); // where it was
	overlay_visible = true;
	overlay_x = x;
	overlay_y = y;
	overlay_w = overlay.Width();
	overlay_h = overlay.Height();
	gtk_widget_queue_draw_area(area, x * scale, y * scale, overlay_w, overlay_h);
}

void SkinWindow::HideOverlay()
{
	if (!overlay_visible) return;
	overlay_visible = false;
	gtk_widget_queue_draw_area(area, overlay_x * scale, overlay_y * scale, overlay_w, overlay_h);
}

void SkinWindow::BeginWMMove(int root_x, int root_y)
{
	gtk_window_begin_move_drag(GTK_WINDOW(window), 1, root_x, root_y, GDK_CURRENT_TIME);
}

gboolean SkinWindow::on_draw(GtkWidget *, cairo_t *cr, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	w->handler->OnPaint(w);
	cairo_surface_t *s = w->buffer.Surface();
	if (!s) return TRUE;
	cairo_save(cr);
	cairo_scale(cr, w->scale, w->scale);
	cairo_set_source_surface(cr, s, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_paint(cr);
	cairo_restore(cr);
	if (w->overlay_visible && w->overlay.Valid() && w->overlay.Width() == w->overlay_w && w->overlay.Height() == w->overlay_h)
	{
		cairo_set_source_surface(cr, w->overlay.Surface(), w->overlay_x * w->scale, w->overlay_y * w->scale);
		cairo_rectangle(cr, w->overlay_x * w->scale, w->overlay_y * w->scale, w->overlay.Width(), w->overlay.Height());
		cairo_fill(cr);
	}
	return TRUE;
}

int SkinWindow::stats_from(guint state)
{
	int s = 0;
	if (state & GDK_BUTTON1_MASK) s |= MK_LBUTTON;
	if (state & GDK_BUTTON3_MASK) s |= MK_RBUTTON;
	if (state & GDK_BUTTON2_MASK) s |= MK_MBUTTON;
	if (state & GDK_SHIFT_MASK) s |= MK_SHIFT;
	if (state & GDK_CONTROL_MASK) s |= MK_CONTROL;
	return s;
}

gboolean SkinWindow::on_button_press(GtkWidget *, GdkEventButton *e, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	int x = (int)e->x / w->scale, y = (int)e->y / w->scale;
	int stats = stats_from(e->state);
	if (e->button == 1)
	{
		stats |= MK_LBUTTON;
		if (e->type == GDK_2BUTTON_PRESS)
			w->handler->OnDoubleClick(w, x, y, stats, (int)e->x_root, (int)e->y_root);
		else if (e->type == GDK_BUTTON_PRESS)
		{
			w->left_down = true;
			w->handler->OnMouse(w, x, y, 1, stats, (int)e->x_root, (int)e->y_root);
		}
	}
	return TRUE;
}

gboolean SkinWindow::on_button_release(GtkWidget *, GdkEventButton *e, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	int x = (int)e->x / w->scale, y = (int)e->y / w->scale;
	int stats = stats_from(e->state) & ~MK_LBUTTON;
	if (e->button == 1 && w->left_down)
	{
		w->left_down = false;
		w->handler->OnMouse(w, x, y, -1, stats, (int)e->x_root, (int)e->y_root);
	}
	else if (e->button == 3)
		w->handler->OnRightClick(w, x, y, stats & ~MK_RBUTTON);
	return TRUE;
}

gboolean SkinWindow::on_motion(GtkWidget *, GdkEventMotion *e, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	int x = (int)e->x, y = (int)e->y;
	// floor division so dragging left of the window still works in double size
	x = x < 0 ? (x - w->scale + 1) / w->scale : x / w->scale;
	y = y < 0 ? (y - w->scale + 1) / w->scale : y / w->scale;
	w->handler->OnMouse(w, x, y, 0, stats_from(e->state), (int)e->x_root, (int)e->y_root);
	gdk_event_request_motions(e);
	return TRUE;
}

gboolean SkinWindow::on_scroll(GtkWidget *, GdkEventScroll *e, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	int delta = 0;
	if (e->direction == GDK_SCROLL_UP) delta = 120;
	else if (e->direction == GDK_SCROLL_DOWN) delta = -120;
	else if (e->direction == GDK_SCROLL_SMOOTH)
	{
		static double acc = 0;
		acc -= e->delta_y;
		if (acc >= 1.0) { delta = 120; acc = 0; }
		else if (acc <= -1.0) { delta = -120; acc = 0; }
	}
	if (delta) w->handler->OnScroll(w, (int)e->x / w->scale, (int)e->y / w->scale, delta, stats_from(e->state));
	return TRUE;
}

gboolean SkinWindow::on_focus(GtkWidget *, GdkEventFocus *e, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	w->active = e->in != 0;
	w->handler->OnActivate(w, w->active);
	return FALSE;
}

gboolean SkinWindow::on_delete(GtkWidget *, GdkEvent *, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	w->handler->OnClose(w);
	return TRUE;
}

gboolean SkinWindow::on_grab_broken(GtkWidget *, GdkEvent *, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	if (w->left_down)
	{
		w->left_down = false;
		w->handler->OnMouse(w, 0, 0, -2, 0, 0, 0);
	}
	return FALSE;
}

void SkinWindow::on_drag_data(GtkWidget *, GdkDragContext *ctx, gint x, gint y, GtkSelectionData *data, guint, guint time, gpointer self)
{
	SkinWindow *w = (SkinWindow *)self;
	std::vector<std::string> files;
	gchar **uris = gtk_selection_data_get_uris(data);
	for (gchar **u = uris; u && *u; u++)
	{
		gchar *fn = g_filename_from_uri(*u, nullptr, nullptr);
		files.push_back(fn ? fn : *u);
		g_free(fn);
	}
	g_strfreev(uris);
	gtk_drag_finish(ctx, !files.empty(), FALSE, time);
	if (!files.empty()) w->handler->OnDrop(w, x / w->scale, y / w->scale, files);
}
