/* nls.h          Header file for NLS (internationalization) defines    */
/* Copyright (C)  1998-2002  Ben Webb                                   */
/*                Email: ben@bellatrix.pcl.ox.ac.uk                     */
/*                WWW: http://dopewars.sourceforge.net/                 */

/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU General Public License          */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */

/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */

/* You should have received a copy of the GNU General Public License    */
/* along with this program; if not, write to the Free Software          */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston,               */
/*                   MA  02111-1307, USA.                               */


#ifndef __NLS_H__
#define __NLS_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef ENABLE_NLS
# include <locale.h>
# ifdef HAVE_GETTEXT
#  include <libintl.h>
# else
#  include "../intl/libintl.h"
# endif
# define _(String) gettext (String)
# ifdef gettext_noop
#  define N_(String) gettext_noop (String)
# else
#  define N_(String) (String)
# endif
#else
# define gettext(String) (String)
# define dgettext(Domain,Message) (Message)
# define dcgettext(Domain,Message,Type) (Message)
# define _(String) (String)
# define N_(String) (String)
#endif

#endif /* __NLS_H__ */
