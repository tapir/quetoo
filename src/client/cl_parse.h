/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __CL_PARSE_H__
#define __CL_PARSE_H__

#include "cl_types.h"

#ifdef __CL_LOCAL_H__
extern char *svc_strings[256];

boolean_t Cl_CheckOrDownloadFile(const char *file_name);
void Cl_ParseConfigString(void);
void Cl_ParseMuzzleFlash(void);
void Cl_ParseServerMessage(void);
void Cl_Download_f(void);
#endif /* __CL_LOCAL_H__ */

#endif /* __CL_PARSE_H__ */
