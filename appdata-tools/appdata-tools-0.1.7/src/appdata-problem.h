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

#include <glib.h>

typedef enum {
	APPDATA_PROBLEM_KIND_TAG_DUPLICATED,
	APPDATA_PROBLEM_KIND_TAG_MISSING,
	APPDATA_PROBLEM_KIND_TAG_INVALID,
	APPDATA_PROBLEM_KIND_ATTRIBUTE_MISSING,
	APPDATA_PROBLEM_KIND_ATTRIBUTE_INVALID,
	APPDATA_PROBLEM_KIND_MARKUP_INVALID,
	APPDATA_PROBLEM_KIND_STYLE_INCORRECT,
	APPDATA_PROBLEM_KIND_FILENAME_INVALID,
	APPDATA_PROBLEM_KIND_FAILED_TO_OPEN,
	APPDATA_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
	APPDATA_PROBLEM_KIND_DUPLICATE_DATA,
	APPDATA_PROBLEM_KIND_VALUE_MISSING,
	APPDATA_PROBLEM_KIND_URL_NOT_FOUND,
	APPDATA_PROBLEM_KIND_FILE_INVALID,
	APPDATA_PROBLEM_KIND_ASPECT_RATIO_INCORRECT,
	APPDATA_PROBLEM_KIND_RESOLUTION_INCORRECT,
	APPDATA_PROBLEM_KIND_LAST
} AppdataProblemKind;

typedef struct {
	AppdataProblemKind	 kind;
	gchar			*description;
	gint			 line_number;
	gint			 char_number;
} AppdataProblem;

AppdataProblem		*appdata_problem_new		(AppdataProblemKind	 kind);
void			 appdata_problem_free		(AppdataProblem		*problem);
const gchar		*appdata_problem_kind_to_string	(AppdataProblemKind	 kind);
