/* wavpack_tag.c - 2007/02/15 */
/*
 *  EasyTAG - Tag editor for many file types
 *  Copyright (C) 2007 Maarten Maathuis (madman2003@gmail.com)
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


#ifndef __WAVPACK_HEADER_H__
#define __WAVPACK_HEADER_H__


#include "et_core.h"

/****************
 * Declarations *
 ****************/


/**************
 * Prototypes *
 **************/

gboolean Wavpack_Header_Read_File_Info          (gchar *filename, ET_File_Info *ETFileInfo);
gboolean Wavpack_Header_Display_File_Info_To_UI (gchar *filename_utf8, ET_File_Info *ETFileInfo);


#endif /* __WAVPACK_HEADER_H__ */
