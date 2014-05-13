/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014 Abhinav Jangda <abhijangda@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef ET_OPUS_TAG_H_
#define ET_OPUS_TAG_H_

#include <glib.h>
#include "et_core.h"

G_BEGIN_DECLS

gboolean et_opus_tag_read_file_tag (GFile *file, File_Tag *FileTag, GError **error);

G_END_DECLS

#endif /* ET_OPUS_TAG_H_ */
