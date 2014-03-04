/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib/gi18n.h>

#include "appdata-problem.h"

/**
 * appdata_problem_new:
 */
AppdataProblem *
appdata_problem_new (AppdataProblemKind kind)
{
	AppdataProblem *problem;
	problem = g_slice_new0 (AppdataProblem);
	problem->kind = kind;
	return problem;
}

/**
 * appdata_problem_free:
 */
void
appdata_problem_free (AppdataProblem *problem)
{
	g_slice_free (AppdataProblem, problem);
}

/**
 * appdata_problem_kind_to_string:
 */
const gchar *
appdata_problem_kind_to_string (AppdataProblemKind kind)
{
	if (kind == APPDATA_PROBLEM_KIND_TAG_DUPLICATED) {
		/* TRANSLATORS: file contains two tags the same */
		return _("tag duplicated");
	}
	if (kind == APPDATA_PROBLEM_KIND_TAG_MISSING) {
		/* TRANSLATORS: file does not contain a required tag */
		return _("tag missing");
	}
	if (kind == APPDATA_PROBLEM_KIND_TAG_INVALID) {
		/* TRANSLATORS: file contains a tag that is invalid */
		return _("tag invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_ATTRIBUTE_MISSING) {
		/* TRANSLATORS: tag exists with missing attribute */
		return _("attribute missing");
	}
	if (kind == APPDATA_PROBLEM_KIND_ATTRIBUTE_INVALID) {
		/* TRANSLATORS: attribute value isn't allowed */
		return _("attribute invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_MARKUP_INVALID) {
		/* TRANSLATORS: the file isn't well formed */
		return _("markup invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_STYLE_INCORRECT) {
		/* TRANSLATORS: the description didn't meet the style guide */
		return _("style invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_FILENAME_INVALID) {
		/* TRANSLATORS: the filename did not have the correct suffix */
		return _("filename invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_FAILED_TO_OPEN) {
		/* TRANSLATORS: the file was not readable */
		return _("failed to open");
	}
	if (kind == APPDATA_PROBLEM_KIND_TRANSLATIONS_REQUIRED) {
		/* TRANSLATORS: there were no translations in the file */
		return _("translations required");
	}
	if (kind == APPDATA_PROBLEM_KIND_DUPLICATE_DATA) {
		/* TRANSLATORS: there was duplicate data in the file */
		return _("duplicate data");
	}
	if (kind == APPDATA_PROBLEM_KIND_VALUE_MISSING) {
		/* TRANSLATORS: there was no data specified in the tag */
		return _("value missing");
	}
	if (kind == APPDATA_PROBLEM_KIND_FILE_INVALID) {
		/* TRANSLATORS: the file specified was invalid */
		return _("file invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_ASPECT_RATIO_INCORRECT) {
		/* TRANSLATORS: the file specified was invalid */
		return _("aspect ratio invalid");
	}
	if (kind == APPDATA_PROBLEM_KIND_RESOLUTION_INCORRECT) {
		/* TRANSLATORS: the file specified was invalid */
		return _("resolution invalid");
	}

	/* TRANSLATORS: if we get here, we have problems */
	return _("unknown error");
}
