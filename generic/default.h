/*
 * default.h --
 *
 *	This file defines the defaults for all options for all of
 *	the Tk widgets.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _DEFAULT
#define _DEFAULT

#ifdef PLATFORM_SDL
# include "tkSDLDefault.h"
#else
# if defined(__WIN32__) || defined(_WIN32) || defined(__MINGW32__)
#   include "tkWinDefault.h"
# else
#   if defined(MAC_OSX_TK)
#	include "tkMacOSXDefault.h"
#   else
#	include "tkUnixDefault.h"
#   endif
# endif
#endif

#endif /* _DEFAULT */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
