#ifndef __PA_BACKEND_H__
#define __PA_BACKEND_H__

#include <stdbool.h>
#include <pulse/pulseaudio.h>

typedef struct pa_backend pa_backend;

typedef void (*pa_backend_cb)(bool headsetmic, bool headphonemic, void *userdata);

pa_backend *pa_backend_new(pa_backend_cb cb, void *cb_userdata);

void pa_backend_free(pa_backend *p);

void pa_backend_card_changed(pa_backend *p, const pa_card_info *i);

void pa_backend_set_context(pa_backend *p, const pa_context *c);

void pa_backend_set_port(pa_backend *p, const char *portname, bool is_output);


#endif
