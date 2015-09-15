#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>

#include "dialog-window.h"

typedef struct dialog_window {
    GtkWidget *dialog;
    GtkWidget *ca_box;
    GtkWidget *v_box;
    GtkWidget *icon_box;
    GtkWidget *btn_box;
    GtkWidget *label;
    GtkWidget *cancel_btn;
    GtkWidget *settings_btn;
    GtkWidget *hp_btn;
    GtkWidget *hs_btn;
    GtkWidget *mic_btn;

    int button_response;
    wdypi_dialog_cb cb;
    void *cb_userdata;
} dialog_window;

/* It's okay to have a global here - we should never show more than one dialog */
dialog_window dlg;

void wdypi_dialog_kill()
{
    dialog_window *d = &dlg;
    if (d->dialog) {
        gtk_widget_destroy(d->dialog);
        d->dialog = NULL;
    }
}

static void on_iconbtn_clicked (GtkWidget *widget, gpointer data)
{
    dialog_window *d = &dlg;
    d->button_response = (ssize_t) data;
    gtk_dialog_response(GTK_DIALOG(d->dialog), GTK_RESPONSE_OK);
}

static void on_response (GtkWidget *widget, gint response_id, gpointer data)
{
    int resp;
    dialog_window *d = data;
    if (!d->cb)
        return;

    switch (response_id) {
        case GTK_RESPONSE_YES:
            resp = WDYPI_DIALOG_SOUND_SETTINGS;
            break;
        case GTK_RESPONSE_OK:
            resp = d->button_response;
            break;
        default:
            resp = WDYPI_DIALOG_CANCELLED;
    }

    d->cb(resp, d->cb_userdata);

    wdypi_dialog_kill();
}

static GtkWidget * create_icon_button(int response, const char *name, const char *icon)
{
    GtkWidget *btn = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lbl = gtk_label_new(name);
    GtkWidget *img = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_DIALOG);

    gtk_box_pack_end(GTK_BOX(box), lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), img, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);
    gtk_container_add(GTK_CONTAINER(btn), box);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_iconbtn_clicked), (void*) (ssize_t) response);
    return btn;
}

static void dialog_create(dialog_window *d, bool show_headset, bool show_mic)
{
    guint32 timestamp;
    d->dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(d->dialog), _("Unknown Audio Device"));
    gtk_container_set_border_width(GTK_CONTAINER(d->dialog), 6);
    gtk_window_set_icon_name(GTK_WINDOW(d->dialog), "audio-headphones");
    gtk_window_set_resizable(GTK_WINDOW(d->dialog), FALSE);

    d->ca_box = gtk_dialog_get_content_area(GTK_DIALOG(d->dialog));
    d->v_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(d->v_box), 5);
    d->icon_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(d->icon_box), TRUE);

    d->label = gtk_label_new(_("What kind of device did you plug in?"));
    gtk_misc_set_alignment(GTK_MISC(d->label), 0.5, 0.5);
    gtk_box_pack_start(GTK_CONTAINER(d->v_box), d->label, FALSE, FALSE, 6);

    d->hp_btn = create_icon_button(WDYPI_DIALOG_HEADPHONES, _("Headphones"), "audio-headphones");
    gtk_box_pack_start(GTK_BOX(d->icon_box), d->hp_btn, FALSE, TRUE, 0);
    if (show_headset) {
        d->hs_btn = create_icon_button(WDYPI_DIALOG_HEADSET, _("Headset"), "audio-headset");
        gtk_box_pack_start(GTK_BOX(d->icon_box), d->hs_btn, FALSE, TRUE, 0);
    }
    if (show_mic) {
        d->mic_btn = create_icon_button(WDYPI_DIALOG_MICROPHONE, _("Microphone"), "audio-input-microphone");
        gtk_box_pack_start(GTK_BOX(d->icon_box), d->mic_btn, FALSE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_CONTAINER(d->v_box), d->icon_box, FALSE, FALSE, 6);

    d->cancel_btn = gtk_dialog_add_button(GTK_DIALOG(d->dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
    d->settings_btn = gtk_dialog_add_button(GTK_DIALOG(d->dialog), _("Sound Settings…"), GTK_RESPONSE_YES);

    gtk_container_add(GTK_CONTAINER(d->ca_box), d->v_box);
    g_signal_connect(d->dialog, "response", G_CALLBACK(on_response), d);

    gtk_widget_show_all(d->dialog);
    /* This event did not originate from a pointer or key movement, so we can't
       use GDK_CURRENT_TIME (or just gtk_window_present) here. There is also no
       other source timestamp to rely on. Without a timestamp, the dialog would
       often show up behind other windows. */
    timestamp = gdk_x11_get_server_time(gtk_widget_get_window(GTK_WIDGET(d->dialog)));
    gtk_window_present_with_time(GTK_WINDOW(d->dialog), timestamp);
}

void wdypi_dialog_run(bool show_headset, bool show_mic, wdypi_dialog_cb cb, void *cb_userdata)
{
    dialog_window *d = &dlg;

    wdypi_dialog_kill();
    d->cb = cb;
    d->cb_userdata = cb_userdata;
    dialog_create(d, show_headset, show_mic);
}
