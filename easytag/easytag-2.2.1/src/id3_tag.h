/* id3tag.h - 2001/02/16 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
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


#ifndef __ID3TAG_H__
#define __ID3TAG_H__


#include <glib.h>
#include "et_core.h"


/****************
 * Declarations *
 ****************/
#define ID3_INVALID_GENRE 255


/**************
 * Prototypes *
 **************/
gboolean Id3tag_Read_File_Tag (const gchar *filename, File_Tag *FileTag);
gboolean Id3tag_Write_File_v24Tag (ET_File *ETFile);
gboolean Id3tag_Write_File_Tag    (ET_File *ETFile);

gchar   *Id3tag_Genre_To_String (unsigned char genre_code);
guchar   Id3tag_String_To_Genre (gchar *genre);

gchar *et_id3tag_get_tpos_from_file_tag (File_Tag *file_tag);

#endif /* __ID3TAG_H__ */
