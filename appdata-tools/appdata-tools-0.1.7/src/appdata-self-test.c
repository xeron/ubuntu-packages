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

#include "config.h"

#include <stdlib.h>
#include <glib.h>

#include "appdata-common.h"
#include "appdata-problem.h"

/**
 * appdata_test_get_data_file:
 **/
static gchar *
appdata_test_get_data_file (const gchar *filename)
{
	gboolean ret;
	gchar *full;
	char full_tmp[PATH_MAX];

	/* check to see if we are being run in the build root */
	full = g_build_filename ("..", "data", filename, NULL);
	ret = g_file_test (full, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;
	g_free (full);

	/* check to see if we are being run in make check */
	full = g_build_filename ("..", "..", "data", filename, NULL);
	ret = g_file_test (full, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;
	g_free (full);
	full = NULL;
out:
	/* canonicalize path */
	if (full != NULL && !g_str_has_prefix (full, "/")) {
		realpath (full, full_tmp);
		g_free (full);
		full = g_strdup (full_tmp);
	}
	return full;
}

static void
print_failures (GList *failures)
{
	AppdataProblem *p;
	GList *l;

	for (l = failures; l != NULL; l = l->next) {
		p = l->data;
		g_print ("%s\n", p->description);
	}
}

static gboolean
ensure_failure (GList *failures, const gchar *description)
{
	AppdataProblem *p;
	GList *l;

	for (l = failures; l != NULL; l = l->next) {
		p = l->data;
		if (g_strcmp0 (description, p->description) == 0)
			return TRUE;
	}
	print_failures (failures);
	return FALSE;
}

static GKeyFile *
get_config (void)
{
	GKeyFile *config;
	const gchar * const licences[] = {
		"CC0", "CC-BY", "CC-BY-SA", "GFDL", NULL};
	config = g_key_file_new ();
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthUpdatecontactMin", 6);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthNameMin", 4);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthNameMax", 30);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthSummaryMin", 8);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthSummaryMax", 100);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthParaMin", 50);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthParaMax", 600);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthListItemMin", 20);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthListItemMax", 100);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberParaMin", 2);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberParaMax", 4);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberScreenshotsMin", 1);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberScreenshotsMax", 5);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireContactdetails", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireUrl", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"HasNetworkAccess", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireCopyright", TRUE);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeWidthMin", 10);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeHeightMin", 10);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeWidthMax", 10000);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeHeightMax", 10000);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthParaCharsBeforeList", 300);
	g_key_file_set_string_list (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				    "AcceptableLicences", licences,
				    g_strv_length ((gchar **) licences));
	return config;
}

static void
appdata_success_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	filename = appdata_test_get_data_file ("success.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	print_failures (list);
	g_assert_cmpint (g_list_length (list), ==, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_wrong_extension_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	filename = appdata_test_get_data_file ("wrong-extension.xml");
	list = appdata_check_file_for_problems (config, filename);
	g_assert (ensure_failure (list, "incorrect extension, expected '.appdata.xml'"));
	g_assert (ensure_failure (list, "<id> is not present"));
	g_assert (ensure_failure (list, "<url> is not present"));
	g_assert (ensure_failure (list, "<updatecontact> is not present"));
	g_assert (ensure_failure (list, "<licence> is not present"));
	g_assert_cmpint (g_list_length (list), >, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_broken_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	filename = appdata_test_get_data_file ("broken.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	g_assert (ensure_failure (list, "<updatecontact> is too short"));
	g_assert (ensure_failure (list, "<url> does not start with 'http://'"));
	g_assert (ensure_failure (list, "<licence> is not valid"));
	g_assert (ensure_failure (list, "<id> does not have correct extension for kind"));
	g_assert (ensure_failure (list, "<id> has invalid type attribute"));
	g_assert (ensure_failure (list, "Not enough <p> tags for a good description"));
	g_assert (ensure_failure (list, "<li> is too short"));
	g_assert (ensure_failure (list, "<ul> cannot start a description"));
	g_assert (ensure_failure (list, "<url> has invalid type attribute"));
	g_assert (ensure_failure (list, "Not enough <screenshot> tags"));
	g_assert (ensure_failure (list, "<licence> is duplicated"));
	g_assert (ensure_failure (list, "<p> should not start with 'This application'"));
	g_assert (ensure_failure (list, "xml:lang should never be 'C'"));
	g_assert (ensure_failure (list, "<p> does not end in '.|:|!'"));
	g_assert (ensure_failure (list, "<name> cannot end in '.'"));
	g_assert (ensure_failure (list, "<summary> cannot end in '.'"));
	g_assert (ensure_failure (list, "<li> cannot end in '.'"));
	g_assert (ensure_failure (list, "<application> used more than once"));
	g_assert (ensure_failure (list, "<?xml> header not found"));
	g_assert (ensure_failure (list, "<!-- Copyright [year] [name] --> is not present"));
	g_assert_cmpint (g_list_length (list), >, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_short_para_before_list_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	filename = appdata_test_get_data_file ("short-para-before-list.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	g_assert (ensure_failure (list, "Not enough <p> content before <ul>"));
	g_assert_cmpint (g_list_length (list), >, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_screenshots_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireCorrectAspectRatio", TRUE);
	g_key_file_set_double (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
			       "DesiredAspectRatio", 1.777777777);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeWidthMin", 624);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeHeightMin", 351);

	filename = appdata_test_get_data_file ("screenshots.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	g_assert (ensure_failure (list, "<screenshot> has no content"));
	g_assert (ensure_failure (list, "<screenshot> has unknown type"));
	g_assert (ensure_failure (list, "<screenshot> has duplicated data"));
	g_assert (ensure_failure (list, "<screenshot> has more than one default"));
	g_assert (ensure_failure (list, "<screenshot> url not found"));
	g_assert (ensure_failure (list, "<screenshot> url not valid"));
	g_assert (ensure_failure (list, "<screenshot> width did not match specified"));
	g_assert (ensure_failure (list, "<screenshot> height did not match specified"));
	g_assert (ensure_failure (list, "<screenshot> aspect ratio was not 16:9"));
	g_assert (ensure_failure (list, "<screenshot> height was too small"));
	g_assert (ensure_failure (list, "<screenshot> width was too small"));
	g_assert_cmpint (g_list_length (list), >, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_translated_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireTranslations", TRUE);
	filename = appdata_test_get_data_file ("translated.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	print_failures (list);
	g_assert (!ensure_failure (list, "<p> is too short [p]"));
	g_assert (!ensure_failure (list, "<name> is duplicated"));
	g_assert (!ensure_failure (list, "<name> has no translations"));
	g_assert (!ensure_failure (list, "<description> has no translations"));
	g_assert_cmpint (g_list_length (list), >, 0);

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

static void
appdata_metadata_func (void)
{
	gchar *filename;
	GList *list;
	GKeyFile *config;

	config = get_config ();
	filename = appdata_test_get_data_file ("metadata.appdata.xml");
	list = appdata_check_file_for_problems (config, filename);
	g_assert_cmpint (g_list_length (list), ==, 5);
	g_assert (ensure_failure (list, "Not enough <p> tags for a good description"));

	g_list_free_full (list, (GDestroyNotify) appdata_problem_free);
	g_free (filename);
	g_key_file_free (config);
}

int
main (int argc, char **argv)
{
	gint retval;
#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init ();
#endif
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	/* tests go here */
	g_test_add_func ("/appdata/success", appdata_success_func);
	g_test_add_func ("/appdata/wrong-extension", appdata_wrong_extension_func);
	g_test_add_func ("/appdata/broken", appdata_broken_func);
	g_test_add_func ("/appdata/screenshots", appdata_screenshots_func);
	g_test_add_func ("/appdata/translated", appdata_translated_func);
	g_test_add_func ("/appdata/metadata", appdata_metadata_func);
	g_test_add_func ("/appdata/short-para-before-list", appdata_short_para_before_list_func);

	/* go go go! */
	retval = g_test_run ();
	return retval;
}

