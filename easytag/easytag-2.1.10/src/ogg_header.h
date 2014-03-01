/* ogg_header.h - 2003/12/29 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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


#ifndef __OGG_HEADER_H__
#define __OGG_HEADER_H__


#include <glib.h>
#include "et_core.h"

/****************
 * Declarations *
 ****************/

/*
 * Error domain and codes for errors while reading/writing OGG files
 */
GQuark et_ogg_error_quark (void);

#define ET_OGG_ERROR et_ogg_error_quark ()

/*
 * EtOGGError:
 * @ET_OGG_ERROR_EOS: reached end of logical bitstream
 * @ET_OGG_ERROR_EOF: reached end of file
 * @ET_OGG_ERROR_SN: page and state's serial number are unequal
 * @ET_OGG_ERROR_TRUNC: input truncated or empty
 * @ET_OGG_ERROR_NOTOGG: input is not ogg bitstream
 * @ET_OGG_ERROR_PAGE: cannot read first page of ogg bitstream
 * @ET_OGG_ERROR_HEADER: error reading initial header packet
 * @ET_OGG_ERROR_INVALID: ogg bitstream doesnot contains Speex or Vorbis data
 * @ET_OGG_ERROR_CORRUPT: corrupt secondary header
 * @ET_OGG_ERROR_EXTRA: need to save extra headers
 * @ET_OGG_ERROR_VORBIS: eof before end of Vorbis headers
 * @ET_OGG_ERROR_FAILED: corrupt or missing data
 * @ET_OGG_ERROR_OUTPUT: error writing stream to output
 *
 * Errors that can occur when reading OGG files.
 */
typedef enum
{
    ET_OGG_ERROR_EOS,
    ET_OGG_ERROR_EOF,
    ET_OGG_ERROR_SN,
    ET_OGG_ERROR_TRUNC,
    ET_OGG_ERROR_NOTOGG,
    ET_OGG_ERROR_PAGE,
    ET_OGG_ERROR_HEADER,
    ET_OGG_ERROR_INVALID,
    ET_OGG_ERROR_CORRUPT,
    ET_OGG_ERROR_EXTRA,
    ET_OGG_ERROR_VORBIS,
    ET_OGG_ERROR_FAILED,
    ET_OGG_ERROR_OUTPUT
} EtOGGError;

/**************
 * Prototypes *
 **************/

gboolean Ogg_Header_Read_File_Info (const gchar *filename,
                                    ET_File_Info *ETFileInfo);
gboolean Ogg_Header_Display_File_Info_To_UI (gchar *filename, ET_File_Info *ETFileInfo);

gboolean Speex_Header_Read_File_Info        (gchar *filename, ET_File_Info *ETFileInfo);


#endif /* __OGG_HEADER_H__ */
