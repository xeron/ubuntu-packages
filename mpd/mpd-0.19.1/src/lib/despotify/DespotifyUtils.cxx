/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "DespotifyUtils.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

extern "C" {
#include <despotify.h>
}

#include <stdio.h>

const Domain despotify_domain("despotify");

static struct despotify_session *g_session;
static void (*registered_callbacks[8])(struct despotify_session *,
		int, void *, void *);
static void *registered_callback_data[8];

static void
callback(struct despotify_session* ds, int sig,
	 void *data, gcc_unused void *callback_data)
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {
		void (*cb)(struct despotify_session *, int, void *, void *) = registered_callbacks[i];
		void *cb_data = registered_callback_data[i];

		if (cb)
			cb(ds, sig, data, cb_data);
	}
}

bool mpd_despotify_register_callback(void (*cb)(struct despotify_session *, int, void *, void *),
		void *cb_data)
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {

		if (!registered_callbacks[i]) {
			registered_callbacks[i] = cb;
			registered_callback_data[i] = cb_data;

			return true;
		}
	}

	return false;
}

void mpd_despotify_unregister_callback(void (*cb)(struct despotify_session *, int, void *, void *))
{
	size_t i;

	for (i = 0; i < sizeof(registered_callbacks) / sizeof(registered_callbacks[0]); i++) {

		if (registered_callbacks[i] == cb) {
			registered_callbacks[i] = nullptr;
		}
	}
}

Tag
mpd_despotify_tag_from_track(const ds_track &track)
{
	char tracknum[20];
	char comment[80];
	char date[20];

	if (!track.has_meta_data)
		return Tag();

	TagBuilder tag;
	snprintf(tracknum, sizeof(tracknum), "%d", track.tracknumber);
	snprintf(date, sizeof(date), "%d", track.year);
	snprintf(comment, sizeof(comment), "Bitrate %d Kbps, %sgeo restricted",
		 track.file_bitrate / 1000,
		 track.geo_restricted ? "" : "not ");
	tag.AddItem(TAG_TITLE, track.title);
	tag.AddItem(TAG_ARTIST, track.artist->name);
	tag.AddItem(TAG_TRACK, tracknum);
	tag.AddItem(TAG_ALBUM, track.album);
	tag.AddItem(TAG_DATE, date);
	tag.AddItem(TAG_COMMENT, comment);
	tag.SetDuration(SignedSongTime::FromMS(track.length));

	return tag.Commit();
}

struct despotify_session *mpd_despotify_get_session(void)
{
	const char *user;
	const char *passwd;
	bool high_bitrate;

	if (g_session)
		return g_session;

	user = config_get_string(CONF_DESPOTIFY_USER, nullptr);
	passwd = config_get_string(CONF_DESPOTIFY_PASSWORD, nullptr);
	high_bitrate = config_get_bool(CONF_DESPOTIFY_HIGH_BITRATE, true);

	if (user == nullptr || passwd == nullptr) {
		LogDebug(despotify_domain,
			 "disabling despotify because account is not configured");
		return nullptr;
	}

	if (!despotify_init()) {
		LogWarning(despotify_domain, "Can't initialize despotify");
		return nullptr;
	}

	g_session = despotify_init_client(callback, nullptr,
					  high_bitrate, true);
	if (!g_session) {
		LogWarning(despotify_domain,
			   "Can't initialize despotify client");
		return nullptr;
	}

	if (!despotify_authenticate(g_session, user, passwd)) {
		LogWarning(despotify_domain,
			   "Can't authenticate despotify session");
		despotify_exit(g_session);
		return nullptr;
	}

	return g_session;
}
