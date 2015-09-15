#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "pa-backend.h"

struct pa_backend {
    const pa_context *context;
    pa_backend_cb dialog_cb;
    void *cb_userdata;
    int headset_card;
    bool headset_plugged_in;
    bool has_headsetmic;
    bool has_headphonemic;

    const char *sink_port_name_to_set;
    const char *source_port_name_to_set;
};

void pa_backend_set_context(pa_backend *p, const pa_context *c)
{
    p->context = c;
}

pa_backend *pa_backend_new(pa_backend_cb cb, void *cb_userdata)
{
    pa_backend *p = calloc(1, sizeof(*p));

    if (!p)
        return NULL;

    p->headset_card = -1;
    p->dialog_cb = cb;
    p->cb_userdata = cb_userdata;
    return p;
}

typedef struct headset_ports {
    const pa_card_port_info *headphones, *headsetmic, *headphonemic;
} headset_ports;

/* In PulseAudio ports will show up with the following names:
   Headphones - analog-output-headphones
   Headset mic - analog-input-microphone-headset
   Jack in mic-in mode - analog-input-microphone

   However, since regular mics also show up as analog-input-microphone,
   we need to check for certain controls on alsa mixer level too, to know
   if we deal with a separate mic jack, or a multi-function jack with a 
   mic-in mode (also called "headphone mic"). 
   We check for the following names:

   Headphone Mic Jack - indicates headphone and mic-in mode share the same jack,
     i e, not two separate jacks. Hardware cannot distinguish between a
     headphone and a mic.
   Headset Mic Phantom Jack - indicates headset jack where hardware can not
     distinguish between headphones and headsets
   Headset Mic Jack - indicates headset jack where hardware can distinguish
     between headphones and headsets. There is no use popping up a dialog in
     this case, unless we already need to do this for the mic-in mode.
*/

static headset_ports get_headset_ports(const pa_card_info *c)
{
    headset_ports h = {NULL, NULL, NULL};
    int i;
    for (i = 0; i < c->n_ports; i++) {
        pa_card_port_info *p = c->ports[i];
        if (!strcmp(p->name, "analog-output-headphones"))
            h.headphones = p;
        else if (!strcmp(p->name, "analog-input-microphone-headset"))
            h.headsetmic = p;
        else if (!strcmp(p->name, "analog-input-microphone"))
            h.headphonemic = p;
    }
    return h;
}

static bool verify_alsa_card(int cardindex, bool *headsetmic, bool *headphonemic)
{
    char ctlstr[20];
    snd_hctl_t *hctl;
    snd_ctl_elem_id_t *id;
    int err;

    *headsetmic = false;
    *headphonemic = false;

    snprintf(ctlstr, sizeof(ctlstr), "hw:%i", cardindex);
    if ((err = snd_hctl_open(&hctl, ctlstr, 0)) < 0) {
        g_warning("snd_hctl_open failed: %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_hctl_load(hctl)) < 0) {
        g_warning("snd_hctl_load failed: %s", snd_strerror(err));
        snd_hctl_close(hctl);
        return false;
    }

    snd_ctl_elem_id_alloca(&id);

    snd_ctl_elem_id_clear(id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
    snd_ctl_elem_id_set_name(id, "Headphone Mic Jack");
    if (snd_hctl_find_elem(hctl, id))
        *headphonemic = true;

    snd_ctl_elem_id_clear(id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
    snd_ctl_elem_id_set_name(id, "Headset Mic Phantom Jack");
    if (snd_hctl_find_elem(hctl, id))
        *headsetmic = true;

    if (*headphonemic) {
        snd_ctl_elem_id_clear(id);
        snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
        snd_ctl_elem_id_set_name(id, "Headset Mic Jack");
        if (snd_hctl_find_elem(hctl, id))
            *headsetmic = true;
    }

    snd_hctl_close(hctl);
    return *headsetmic || *headphonemic;
}

void pa_backend_card_changed(pa_backend *p, const pa_card_info *i)
{
    headset_ports h;
    bool start_dialog = false, stop_dialog = false;

    h = get_headset_ports(i);

    if (!h.headphones || (!h.headsetmic && !h.headphonemic))
        return; /* Not a headset jack */

    if (p->headset_card != (int) i->index) {
        int cardindex = 0;
        bool hsmic, hpmic;
        const char *s = pa_proplist_gets(i->proplist, "alsa.card");
        if (!s)
            return;
        cardindex = strtol(s, NULL, 10);
        if (cardindex == 0 && strcmp(s, "0"))
            return;

        if (!verify_alsa_card(cardindex, &hsmic, &hpmic))
            return;

        p->headset_card = (int) i->index;
        p->has_headsetmic = hsmic && h.headsetmic;
        p->has_headphonemic = hpmic && h.headphonemic;
    }
    else {
        start_dialog = p->dialog_cb && (h.headphones->available != PA_PORT_AVAILABLE_NO) &&
            !p->headset_plugged_in;
        stop_dialog = p->dialog_cb && (h.headphones->available == PA_PORT_AVAILABLE_NO) &&
            p->headset_plugged_in;
    }


    p->headset_plugged_in = h.headphones->available != PA_PORT_AVAILABLE_NO;

    if (start_dialog)
        p->dialog_cb(p->has_headsetmic, p->has_headphonemic, p->cb_userdata);
    else if (stop_dialog)
        p->dialog_cb(false, false, p->cb_userdata);
}

/* 
 We need to re-enumerate sources and sinks every time the user makes a choice,
 because they can change due to use interaction in other software (or policy
 changes inside PulseAudio). Enumeration means PulseAudio will do a series of
 callbacks, one for every source/sink.
 Set the port when we find the correct source/sink.
 */

static void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    pa_backend *p = userdata;
    pa_operation *o;
    int j;
    const char *s = p->sink_port_name_to_set;

    if (eol)
        return;

    if (i->card != p->headset_card)
        return;

    if (i->active_port && !strcmp(i->active_port->name, s))
        return;

    for (j = 0; j < i->n_ports; j++)
        if (!strcmp(i->ports[j]->name, s))
            break;

    if (j >= i->n_ports)
        return;

    o = pa_context_set_sink_port_by_index(c, i->index, s, NULL, NULL);
    if (o)
        pa_operation_unref(o);
}

static void source_info_cb(pa_context *c, const pa_source_info *i, int eol, void *userdata)
{
    pa_backend *p = userdata;
    pa_operation *o;
    int j;
    const char *s = p->source_port_name_to_set;

    if (eol)
        return;

    if (i->card != p->headset_card)
        return;

    if (i->active_port && !strcmp(i->active_port->name, s))
        return;

    for (j = 0; j < i->n_ports; j++)
        if (!strcmp(i->ports[j]->name, s))
            break;

    if (j >= i->n_ports)
        return;

    o = pa_context_set_source_port_by_index(c, i->index, s, NULL, NULL);
    if (o)
        pa_operation_unref(o);
}


void pa_backend_set_port(pa_backend *p, const char *portname, bool is_output)
{
    pa_operation *o;

    if (is_output) {
        p->sink_port_name_to_set = portname;
        o = pa_context_get_sink_info_list(p->context, sink_info_cb, p);
    }
    else {
        p->source_port_name_to_set = portname;
        o = pa_context_get_source_info_list(p->context, source_info_cb, p);
    }

    if (o) {
        pa_operation_unref(o);
    }
}


void pa_backend_free(pa_backend *p)
{
    if (!p)
        return;

    free(p);
}

