/* mp4_tag.c - 2005/08/06 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2001-2005  Jerome Couderc <easytag@gmail.com>
 *  Copyright (C) 2005  Michael Ihde <mike.ihde@randomwalking.com>
 *  Copyright (C) 2005  Stewart Whitman <swhitman@cox.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h" // For definition of ENABLE_MP4

#ifdef ENABLE_MP4

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "mp4_tag.h"
#include "picture.h"
#include "easytag.h"
#include "setting.h"
#include "log.h"
#include "misc.h"
#include "et_core.h"
#include "charset.h"

#include <tag_c.h>


/*
 * Mp4_Tag_Read_File_Tag:
 *
 * Read tag data into an Mp4 file.
 *
 * Note:
 *  - for string fields, //if field is found but contains no info (strlen(str)==0), we don't read it
 *  - for track numbers, if they are zero, then we don't read it
 */
gboolean Mp4tag_Read_File_Tag (gchar *filename, File_Tag *FileTag)
{
    TagLib_File *mp4file;
    TagLib_Tag *tag;
    guint track;

    g_return_val_if_fail (filename != NULL && FileTag != NULL, FALSE);

    /* Get data from tag */
    mp4file = taglib_file_new_type(filename,TagLib_File_MP4);
    if (mp4file == NULL)
    {
        gchar *filename_utf8 = filename_to_display(filename);
        Log_Print(LOG_ERROR,_("Error while opening file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        g_free(filename_utf8);
        return FALSE;
    }

    /* Check for audio track */
    if (!taglib_file_is_valid (mp4file))
    {
        gchar *filename_utf8 = filename_to_display (filename);
        Log_Print (LOG_ERROR, _("File contains no audio track: '%s'"),
                   filename_utf8);
        g_free (filename_utf8);
        taglib_file_free (mp4file);
        return FALSE;
    }

    tag = taglib_file_tag (mp4file);
    if (tag == NULL)
    {
        gchar *filename_utf8 = filename_to_display (filename);
        Log_Print (LOG_ERROR, _("Error reading tags from file: '%s'"),
                   filename_utf8);
        g_free (filename_utf8);
        taglib_file_free (mp4file);
        return FALSE;
    }

    /*********
     * Title *
     *********/
    FileTag->title = g_strdup(taglib_tag_title(tag));

    /**********
     * Artist *
     **********/
    FileTag->artist = g_strdup(taglib_tag_artist(tag));

    /*********
     * Album *
     *********/
    FileTag->album = g_strdup(taglib_tag_album(tag));

    /****************
     * Album Artist *
     ****************/
    /* TODO: No album artist or disc number support in the TagLib C API! */

    /********
     * Year *
     ********/
    FileTag->year = g_strdup_printf("%u", taglib_tag_year(tag));

    /*************************
     * Track and Total Track *
     *************************/
    track = taglib_tag_track(tag);

    if (track != 0)
        FileTag->track = et_track_number_to_string (track);
    /* TODO: No total track support in the TagLib C API! */

    /*********
     * Genre *
     *********/
    FileTag->genre = g_strdup(taglib_tag_genre(tag));

    /***********
     * Comment *
     ***********/
    FileTag->comment = g_strdup(taglib_tag_comment(tag));

    /**********************
     * Composer or Writer *
     **********************/
    /* TODO: No composer support in the TagLib C API! */

    /*****************
     * Encoding Tool *
     *****************/
    /* TODO: No encode_by support in the TagLib C API! */

    /***********
     * Picture *
     ***********/
    /* TODO: No encode_by support in the TagLib C API! */

    /* Free allocated data */
    taglib_tag_free_strings();
    taglib_file_free(mp4file);

    return TRUE;
}


/*
 * Mp4_Tag_Write_File_Tag:
 *
 * Write tag data into an Mp4 file.
 *
 * Note:
 *  - for track numbers, we write 0's if one or the other is blank
 */
gboolean Mp4tag_Write_File_Tag (ET_File *ETFile)
{
    File_Tag *FileTag;
    gchar    *filename;
    gchar    *filename_utf8;
    TagLib_File *mp4file = NULL;
    TagLib_Tag *tag;
    gboolean success;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);

    FileTag = (File_Tag *)ETFile->FileTag->data;
    filename      = ((File_Name *)ETFile->FileNameCur->data)->value;
    filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;

    /* Open file for writing */
    mp4file = taglib_file_new_type(filename, TagLib_File_MP4);
    if (mp4file == NULL)
    {
        Log_Print(LOG_ERROR,_("Error while opening file: '%s' (%s)."),filename_utf8,_("MP4 format invalid"));
        return FALSE;
    }

    tag = taglib_file_tag (mp4file);
    if (tag == NULL)
    {
        gchar *filename_utf8 = filename_to_display (filename);
        Log_Print (LOG_ERROR, _("Error reading tags from file: '%s'"),
                   filename_utf8);
        g_free (filename_utf8);
        taglib_file_free (mp4file);
        return FALSE;
    }

    /*********
     * Title *
     *********/
    if (FileTag->title && g_utf8_strlen(FileTag->title, -1) > 0)
    {
        taglib_tag_set_title(tag, FileTag->title);
    }else
    {
        taglib_tag_set_title(tag,"");
    }

    /**********
     * Artist *
     **********/
    if (FileTag->artist && g_utf8_strlen(FileTag->artist, -1) > 0)
    {
        taglib_tag_set_artist(tag,FileTag->artist);
    }else
    {
        taglib_tag_set_artist(tag,"");
    }

    /*********
     * Album *
     *********/
    if (FileTag->album && g_utf8_strlen(FileTag->album, -1) > 0)
    {
        taglib_tag_set_album(tag,FileTag->album);
    }else
    {
        taglib_tag_set_album(tag,"");
    }


    /********
     * Year *
     ********/
    if (FileTag->year && g_utf8_strlen(FileTag->year, -1) > 0)
    {
        taglib_tag_set_year(tag,atoi(FileTag->year));
    }else
    {
        taglib_tag_set_year(tag,0);
    }

    /*************************
     * Track and Total Track *
     *************************/
    if ( FileTag->track && g_utf8_strlen(FileTag->track, -1) > 0 )
    {
        taglib_tag_set_track(tag,atoi(FileTag->track));
    }else
    {
        taglib_tag_set_track(tag,0);
    }

    /*********
     * Genre *
     *********/
    if (FileTag->genre && g_utf8_strlen(FileTag->genre, -1) > 0 )
    {
        taglib_tag_set_genre(tag,FileTag->genre);
    }else
    {
        taglib_tag_set_genre(tag,"");
    }

    /***********
     * Comment *
     ***********/
    if (FileTag->comment && g_utf8_strlen(FileTag->comment, -1) > 0)
    {
        taglib_tag_set_comment(tag,FileTag->comment);
    }else
    {
        taglib_tag_set_comment(tag,"");
    }

    /**********************
     * Composer or Writer *
     **********************/

    /*****************
     * Encoding Tool *
     *****************/

    /***********
     * Picture *
     ***********/

    success = taglib_file_save (mp4file) ? TRUE : FALSE;
    taglib_file_free(mp4file);

    return success;
}


#endif /* ENABLE_MP4 */
