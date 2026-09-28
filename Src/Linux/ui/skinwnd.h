/*
** Winamp for Linux - a borderless GTK window showing a skin bitmap.
**
** It replaces the Win32 window/DC plumbing of the classic windows: the ported
** drawing code renders into Buffer() at 1x, the window shows it at 1x or 2x
** (double size) with nearest neighbour scaling, and mouse input is handed to
** the ported *_handlemouseevent() style handlers in skin coordinates.
*/
#pragma once

#include "gfx.h"
#include "skin.h"

#include <gtk/gtk.h>
#include <string>
#include <vector>

class SkinWindow
{
public:
	struct Handler
	{
		virtual ~Handler() {}
		virtual void OnPaint(SkinWindow *) {}  // called before the buffer is shown (for windows that redraw fully)
		// type: 1 = left down, 0 = move, -1 = left up, -2 = capture lost; x/y in skin pixels
		virtual void OnMouse(SkinWindow *, int x, int y, int type, int stats, int root_x, int root_y) = 0;
		virtual void OnDoubleClick(SkinWindow *, int x, int y, int stats, int root_x, int root_y) = 0;
		virtual void OnRightClick(SkinWindow *, int x, int y, int stats) = 0;
		virtual void OnScroll(SkinWindow *, int x, int y, int delta, int stats) { (void)x; (void)y; (void)delta; (void)stats; }
		virtual void OnActivate(SkinWindow *, bool active) = 0;
		virtual void OnDrop(SkinWindow *, int x, int y, const std::vector<std::string> &files) { (void)x; (void)y; (void)files; }
		virtual void OnClose(SkinWindow *) {}
	};

	SkinWindow(const char *title, const char *role, Handler *h);
	~SkinWindow();

	GtkWidget *Widget() { return window; }
	Bitmap &Buffer() { return buffer; }

	void SetSize(int w, int h);     // skin pixels
	void SetScale(int s);           // 1 or 2
	int Scale() const { return scale; }
	int Width() const { return buffer.Width(); }
	int Height() const { return buffer.Height(); }

	void Move(int x, int y);        // screen position of the top left corner
	int X() const { return pos_x; }
	int Y() const { return pos_y; }
	void Show();
	void Hide();
	bool Visible() const { return visible; }
	bool Active() const { return active; }
	void Raise();
	void SetKeepAbove(bool above);
	void SetShape(const SkinRegion *region); // nullptr = rectangular

	void Invalidate();
	void InvalidateRect(int x, int y, int w, int h);

	// an image painted unscaled on top of the (scaled) buffer at skin position x,y;
	// used by the visualizer, which draws at the doubled resolution in double size mode
	Bitmap &Overlay() { return overlay; }
	void ShowOverlay(int x, int y);
	void HideOverlay();

	// begin a window-manager driven move (fallback when we can't position windows ourselves)
	void BeginWMMove(int root_x, int root_y);

	static bool CanPositionWindows(); // true on X11

private:
	static gboolean on_draw(GtkWidget *, cairo_t *cr, gpointer self);
	static gboolean on_button_press(GtkWidget *, GdkEventButton *e, gpointer self);
	static gboolean on_button_release(GtkWidget *, GdkEventButton *e, gpointer self);
	static gboolean on_motion(GtkWidget *, GdkEventMotion *e, gpointer self);
	static gboolean on_scroll(GtkWidget *, GdkEventScroll *e, gpointer self);
	static gboolean on_focus(GtkWidget *, GdkEventFocus *e, gpointer self);
	static gboolean on_delete(GtkWidget *, GdkEvent *, gpointer self);
	static gboolean on_grab_broken(GtkWidget *, GdkEvent *, gpointer self);
	static void on_drag_data(GtkWidget *, GdkDragContext *ctx, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer self);
	static int stats_from(guint state);
	void ApplyShape();

	GtkWidget *window = nullptr, *area = nullptr;
	Handler *handler;
	Bitmap buffer;
	Bitmap overlay;
	bool overlay_visible = false;
	int overlay_x = 0, overlay_y = 0, overlay_w = 0, overlay_h = 0;
	int scale = 1;
	int pos_x = 0, pos_y = 0;
	bool visible = false, active = false, left_down = false;
	const SkinRegion *shape = nullptr;
};
