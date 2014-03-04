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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

#include "appdata-common.h"
#include "appdata-problem.h"

#define EXIT_CODE_SUCCESS	0
#define EXIT_CODE_USAGE		1
#define EXIT_CODE_WARNINGS	2

/**
 * gs_string_replace:
 **/
static guint
gs_string_replace (GString *string, const gchar *search, const gchar *replace)
{
	gchar *tmp;
	guint count = 0;
	guint replace_len;
	guint search_len;

	search_len = strlen (search);
	replace_len = strlen (replace);

	do {
		tmp = g_strstr_len (string->str, -1, search);
		if (tmp == NULL)
			goto out;

		/* reallocate the string if required */
		if (search_len > replace_len) {
			g_string_erase (string,
					tmp - string->str,
					search_len - replace_len);
		}
		if (search_len < replace_len) {
			g_string_insert_len (string,
					    tmp - string->str,
					    search,
					    replace_len - search_len);
		}

		/* just memcmp in the new string */
		memcpy (tmp, replace, replace_len);
		count++;
	} while (TRUE);
out:
	return count;
}

/**
 * appdata_validate_format_html:
 **/
static void
appdata_validate_format_html (const gchar *filename, GList *problems)
{
	AppdataProblem *problem;
	GList *l;
	GString *tmp;

	g_print ("<html>\n");
	g_print ("<head>\n");
	g_print ("<style type=\"text/css\">\n");
	g_print ("body {width: 70%%; font: 12px/20px Arial, Helvetica;}\n");
	g_print ("p {color: #333;}\n");
	g_print ("</style>\n");
	g_print ("<title>AppData Validation Results for %s</title>\n", filename);
	g_print ("</head>\n");
	g_print ("<body>\n");
	if (problems == NULL) {
		g_print ("<h1>Success!</h1>\n");
		g_print ("<p>%s validated successfully.</p>\n", filename);
	} else {
		g_print ("<h1>Validation failed!</h1>\n");
		g_print ("<p>%s did not validate:</p>\n", filename);
		g_print ("<ul>\n");
		for (l = problems; l != NULL; l = l->next) {
			problem = l->data;
			tmp = g_string_new (problem->description);
			gs_string_replace (tmp, "&", "&amp;");
			gs_string_replace (tmp, "<", "[");
			gs_string_replace (tmp, ">", "]");
			g_print ("<li>");
			g_print ("%s\n", tmp->str);
			if (problem->line_number > 0) {
				g_print (" (line %i, char %i)",
					 problem->line_number,
					 problem->char_number);
			}
			g_print ("</li>\n");
			g_string_free (tmp, TRUE);
		}
		g_print ("</ul>\n");
	}
	g_print ("</body>\n");
	g_print ("</html>\n");
}

/**
 * appdata_validate_format_xml:
 **/
static void
appdata_validate_format_xml (const gchar *filename, GList *problems)
{
	AppdataProblem *problem;
	GList *l;
	GString *tmp;

	g_print ("<results version=\"1\">\n");
	g_print ("  <filename>%s</filename>\n", filename);
	if (problems != NULL) {
		g_print ("  <problems>\n");
		for (l = problems; l != NULL; l = l->next) {
			problem = l->data;
			tmp = g_string_new (problem->description);
			gs_string_replace (tmp, "&", "&amp;");
			gs_string_replace (tmp, "<", "");
			gs_string_replace (tmp, ">", "");
			if (problem->line_number > 0) {
				g_print ("    <problem type=\"%s\" line=\"%i\">%s</problem>\n",
					 appdata_problem_kind_to_string (problem->kind),
					 problem->line_number,
					 tmp->str);
			} else {
				g_print ("    <problem type=\"%s\">%s</problem>\n",
					 appdata_problem_kind_to_string (problem->kind),
					 tmp->str);
			}
			g_string_free (tmp, TRUE);
		}
		g_print ("  </problems>\n");
	}
	g_print ("</results>\n");
}

/**
 * appdata_validate_format_text:
 **/
static void
appdata_validate_format_text (const gchar *filename, GList *problems)
{
	AppdataProblem *problem;
	GList *l;
	const gchar *tmp;
	guint i;

	if (problems == NULL) {
		/* TRANSLATORS: the file is valid */
		g_print (_("%s validated OK."), filename);
		g_print ("\n");
		return;
	}
	g_print ("%s %i %s\n",
		 filename,
		 g_list_length (problems),
		 _("problems detected:"));
	for (l = problems; l != NULL; l = l->next) {
		problem = l->data;
		tmp = appdata_problem_kind_to_string (problem->kind);
		g_print ("• %s ", tmp);
		for (i = strlen (tmp); i < 20; i++)
			g_print (" ");
		if (problem->line_number > 0) {
			g_print (" : %s [ln:%i ch:%i]\n",
				 problem->description,
				 problem->line_number,
				 problem->char_number);
		} else {
			g_print (" : %s\n", problem->description);
		}
	}
}

/**
 * appdata_validate_and_show_results:
 **/
static gint
appdata_validate_and_show_results (GKeyFile *config,
				   const gchar *filename,
				   const gchar *output_format)
{
	const gchar *tmp;
	gchar *original_filename = NULL;
	gint retval = EXIT_CODE_SUCCESS;
	GList *problems = NULL;

	/* scan file for problems */
	problems = appdata_check_file_for_problems (config, filename);
	if (problems != NULL)
		retval = EXIT_CODE_WARNINGS;

	/* print problems */
	original_filename = g_key_file_get_string (config,
						   APPDATA_TOOLS_VALIDATE_GROUP_NAME,
						   "OriginalFilename", NULL);
	tmp = original_filename != NULL ? original_filename : filename;
	if (g_strcmp0 (output_format, "html") == 0) {
		appdata_validate_format_html (tmp, problems);
	} else if (g_strcmp0 (output_format, "xml") == 0) {
		appdata_validate_format_xml (tmp, problems);
	} else {
		appdata_validate_format_text (tmp, problems);
	}

	g_free (original_filename);
	g_list_free_full (problems, (GDestroyNotify) appdata_problem_free);
	return retval;
}

/**
 * appdata_validate_log_ignore_cb:
 **/
static void
appdata_validate_log_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
				const gchar *message, gpointer user_data)
{
}

/**
 * appdata_validate_log_handler_cb:
 **/
static void
appdata_validate_log_handler_cb (const gchar *log_domain, GLogLevelFlags log_level,
				 const gchar *message, gpointer user_data)
{
	/* not a console */
	if (isatty (fileno (stdout)) == 0) {
		g_print ("%s\n", message);
		return;
	}

	/* critical is also in red */
	if (log_level == G_LOG_LEVEL_CRITICAL ||
	    log_level == G_LOG_LEVEL_WARNING ||
	    log_level == G_LOG_LEVEL_ERROR) {
		g_print ("%c[%dm%s\n%c[%dm", 0x1B, 31, message, 0x1B, 0);
	} else {
		/* debug in blue */
		g_print ("%c[%dm%s\n%c[%dm", 0x1B, 34, message, 0x1B, 0);
	}
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean nonet = FALSE;
	gboolean relax = FALSE;
	gboolean ret;
	gboolean strict = FALSE;
	gboolean verbose = FALSE;
	gboolean version = FALSE;
	gchar *config_dump = NULL;
	gchar *filename = NULL;
	gchar *output_format = NULL;
	GError *error = NULL;
	gint retval = EXIT_CODE_SUCCESS;
	gint retval_tmp;
	GKeyFile *config = NULL;
	GOptionContext *context;
	gint i;
	const gchar * const licences[] = {
		"CC0", "CC-BY", "CC-BY-SA", "GFDL", NULL};
	const GOptionEntry options[] = {
		{ "relax", 'r', 0, G_OPTION_ARG_NONE, &relax,
			/* TRANSLATORS: this is the --relax argument */
			_("Be less strict when checking files"), NULL },
		{ "strict", 's', 0, G_OPTION_ARG_NONE, &strict,
			/* TRANSLATORS: this is the --relax argument */
			_("Be more strict when checking files"), NULL },
		{ "nonet", '\0', 0, G_OPTION_ARG_NONE, &nonet,
			/* TRANSLATORS: this is the --nonet argument */
			_("Do not use network access"), NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: this is the --verbose argument */
			_("Show extra debugging information"), NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
			/* TRANSLATORS: this is the --version argument */
			_("Show the version number and then quit"), NULL },
		{ "filename", '\0', 0, G_OPTION_ARG_STRING, &filename,
			/* TRANSLATORS: this is the --filename argument */
			_("The source filename when using a temporary file"), NULL },
		{ "output-format", '\0', 0, G_OPTION_ARG_STRING, &output_format,
			/* TRANSLATORS: this is the --output-format argument */
			_("The output format [text|html|xml]"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init ();
#endif
	context = g_option_context_new ("AppData Validation Program");
	g_option_context_add_main_entries (context, options, NULL);
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: this is where the user used unknown command
		 * line switches -- the exact error follows */
		g_print ("%s %s\n", _("Failed to parse command line:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* just show the version */
	if (version) {
		g_print ("%s\n", PACKAGE_VERSION);
		goto out;
	}

	/* verbose? */
	if (verbose) {
		g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
		g_log_set_handler ("AppDataTools",
				   G_LOG_LEVEL_ERROR |
				   G_LOG_LEVEL_CRITICAL |
				   G_LOG_LEVEL_DEBUG |
				   G_LOG_LEVEL_WARNING,
				   appdata_validate_log_handler_cb, NULL);
	} else {
		/* hide all debugging */
		g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
		g_log_set_handler ("AppDataTools",
				   G_LOG_LEVEL_DEBUG,
				   appdata_validate_log_ignore_cb, NULL);
	}

	if (argc < 2) {
		retval = EXIT_CODE_USAGE;
		/* TRANSLATORS: this is explaining how to use the tool */
		g_print ("%s %s %s\n", _("Usage:"), argv[0], _("<file>"));
		goto out;
	}

	/* set some config values */
	config = g_key_file_new ();
	g_key_file_set_string_list (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				    "AcceptableLicences", licences,
				    g_strv_length ((gchar **) licences));
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthUpdatecontactMin", 6);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"LengthNameMin", 3);
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
				"LengthParaCharsBeforeList", 300);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberParaMin", 2);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberParaMax", 4);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberScreenshotsMin", 1);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeWidthMin", 624);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeHeightMin", 351);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeWidthMax", 1600);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"ScreenshotSizeHeightMax", 900);
	g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"NumberScreenshotsMax", 5);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireContactdetails", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireUrl", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"HasNetworkAccess", TRUE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireCopyright", FALSE);
	g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				"RequireCorrectAspectRatio", FALSE);
	g_key_file_set_double (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
			       "DesiredAspectRatio", 1.777777777);

	/* relax the requirements a bit */
	if (relax) {
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthNameMax", 100);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthSummaryMax", 200);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthParaMin", 10);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthParaMax", 1000);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthListItemMin", 4);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthListItemMax", 1000);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"LengthParaCharsBeforeList", 100);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"NumberParaMin", 1);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"NumberParaMax", 10);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"NumberScreenshotsMin", 0);
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"RequireContactdetails", FALSE);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"NumberScreenshotsMax", 10);
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"RequireUrl", FALSE);
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"HasNetworkAccess", FALSE);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"ScreenshotSizeWidthMin", 300);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"ScreenshotSizeHeightMin", 150);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"ScreenshotSizeWidthMax", 3200);
		g_key_file_set_integer (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"ScreenshotSizeHeightMax", 1800);
	}

	/* make the requirements more strict */
	if (strict) {
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"RequireTranslations", TRUE);
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"RequireCopyright", TRUE);
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"RequireCorrectAspectRatio", TRUE);
	}

	/* we're using a temporary file */
	if (filename != NULL) {
		g_key_file_set_string (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
				       "OriginalFilename", filename);
	}

	/* the user has forced no network mode */
	if (nonet) {
		g_key_file_set_boolean (config, APPDATA_TOOLS_VALIDATE_GROUP_NAME,
					"HasNetworkAccess", FALSE);
	}
	config_dump = g_key_file_to_data (config, NULL, &error);
	g_debug ("%s", config_dump);

	/* validate each file */
	for (i = 1; i < argc; i++) {
		retval_tmp = appdata_validate_and_show_results (config,
								argv[i],
								output_format);
		if (retval_tmp != EXIT_CODE_SUCCESS)
			retval = retval_tmp;
	}
out:
	if (config != NULL)
		g_key_file_free (config);
	g_free (config_dump);
	g_free (filename);
	g_free (output_format);
	g_option_context_free (context);
	return retval;
}
