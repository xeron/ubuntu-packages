#ifndef GSD_RR_PRIVATE_H
#define GSD_RR_PRIVATE_H

#include <X11/Xlib.h>

#include <X11/extensions/Xrandr.h>

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    XRRScreenResources *resources;
    
    GsdRROutput **	outputs;
    GsdRRCrtc **	crtcs;
    GsdRRMode **	modes;
    
    GsdRRScreen *	screen;

    GsdRRMode **	clone_modes;

    RROutput            primary;
};

struct GsdRRScreenPrivate
{
    GdkScreen *			gdk_screen;
    GdkWindow *			gdk_root;
    Display *			xdisplay;
    Screen *			xscreen;
    Window			xroot;
    ScreenInfo *		info;
    
    int				randr_event_base;
    int				rr_major_version;
    int				rr_minor_version;
    
    Atom                        connector_type_atom;
    gboolean                    dpms_capable;
};

struct _GsdRROutputInfoPrivate
{
    char *		name;

    gboolean		on;
    int			width;
    int			height;
    int			rate;
    int			x;
    int			y;
    GsdRRRotation	rotation;

    gboolean		connected;
    gchar		vendor[4];
    guint		product;
    guint		serial;
    double		aspect;
    int			pref_width;
    int			pref_height;
    char *		display_name;
    gboolean            primary;
};

struct _GsdRRConfigPrivate
{
  gboolean clone;
  GsdRRScreen *screen;
  GsdRROutputInfo **outputs;
};

gboolean _gsd_rr_output_name_is_laptop (const char *name);

#endif
