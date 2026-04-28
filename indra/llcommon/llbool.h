/*
 * @file llbool.h
 * @brief Separate the LL definition of BOOL out of stdtypes.h in order to be able
 * to use the Objective-C native BOOL that is defined as a signed Char in the language.
 * The LL definition of BOOL prevents using Apple's system libraries and frameworks
 * Dayturn
 *
 * $LicenseInfo:firstyear=2018&license=viewerlgpl$
 * Dayturn Viewer Source Code
 * Created by Dayturn on 26.07.2018.
 * Copyright (C) 2018, Geir Nøklebye <geir.noklebye@dayturn.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details  
 * $/LicenseInfo$
*/
#ifndef LL_BOOL_H
#define LL_BOOL_H

typedef signed int		BOOL;
// definition moved from stdtypes.h

#ifndef TRUE
#define TRUE			(1)  // definition moved from stdtypes.h
#endif

#ifndef FALSE
#define FALSE			(0) // definition moved from stdtypes.h
#endif

#endif
