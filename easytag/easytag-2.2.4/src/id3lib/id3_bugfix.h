/* 
 * id3lib: a software library for creating and manipulating id3v1/v2 tags
 * Copyright 1999, 2000  Scott Thomas Haug
 * Copyright 2002 Thijmen Klok (thijmen@id3lib.org)
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.

 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

 * The id3lib authors encourage improvements and optimisations to be sent to
 * the id3lib coordinator.  Please see the README file for details on where to
 * send such submissions.  See the AUTHORS file for a list of people who have
 * contributed to id3lib.  See the ChangeLog file for a list of changes to
 * id3lib.  These files are distributed with id3lib at
 * http://download.sourceforge.net/id3lib/
 */

#ifndef _ID3LIB_BUGFIX_H_
#define _ID3LIB_BUGFIX_H_


#ifdef ENABLE_ID3LIB

#include "id3.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

  ID3_C_EXPORT bool                  CCONV ID3Field_SetEncoding    (ID3Field *field, ID3_TextEnc enc);
  ID3_C_EXPORT ID3_TextEnc           CCONV ID3Field_GetEncoding    (const ID3Field *field);
  ID3_C_EXPORT bool                  CCONV ID3Field_IsEncodable    (const ID3Field *field);
  ID3_C_EXPORT ID3_FieldType         CCONV ID3Field_GetType        (const ID3Field *field);
  //ID3_C_EXPORT ID3_FieldID           CCONV ID3Field_GetID          (const ID3Field *field);

  ID3_C_EXPORT const Mp3_Headerinfo* CCONV ID3Tag_GetMp3HeaderInfo (ID3Tag *tag);
  
#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif /* ENABLE_ID3LIB */

#endif /* _ID3LIB_BUGFIX_H_ */
