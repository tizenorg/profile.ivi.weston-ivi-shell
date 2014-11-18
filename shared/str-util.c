/*
 * Copyright © 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@linux.intel.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#ifdef IN_WESTON
#include <wayland-util.h>
#else
#define WL_EXPORT
#endif

#include "str-util.h"

static bool
convert_strtol(const char *str, char **endptr, int base, long *val)
{
       char *end = NULL;
       long v;
       int prev_errno = errno;

       if (!str || !val)
               return false;
       if (!endptr)
               endptr = &end;

       errno = 0;
       v = strtol(str, endptr, base);
       if (errno != 0 || *endptr == str || **endptr != '\0')
               return false;

       errno = prev_errno;
       *val = v;
       return true;
}

static bool
convert_strtoul (const char *str, char **endptr, int base, unsigned long *val)
{
       char *end = NULL;
       unsigned long v;
       int i = 0;
       int prev_errno = errno;

       if (!str || !val)
	       return false;

       /* check for negative numbers */
       while (str[i]) {
	       if (!isspace(str[i])) {
		       if (str[i] == '-')
			       return false;
		       else
			       break;
	       }
	       i++;
       }

       if (!endptr)
	       endptr = &end;

       errno = 0;
       v = strtoul(str, endptr, base);
       if (errno != 0 || *endptr == str || **endptr != '\0')
	       return false;

       errno = prev_errno;
       *val = v;
       return true;
}

WL_EXPORT bool
weston_strtoi(const char *str, char **endptr, int base, int *val)
{
	long v;

	if (!convert_strtol(str, endptr, base, &v) || v > INT_MAX
			|| v < INT_MIN)
		return false;

	*val = (int)v;
	return true;
}

WL_EXPORT bool
weston_strtol(const char *str, char **endptr, int base, long *val)
{
	return convert_strtol(str, endptr, base, val);
}

WL_EXPORT bool
weston_strtoui(const char *str, char **endptr, int base, unsigned int *val)
{
	unsigned long v;

	if (!convert_strtoul(str, endptr, base, &v) || v > UINT_MAX)
		return false;

	*val = (unsigned int)v;
	return true;
}

WL_EXPORT bool
weston_strtoul(const char *str, char **endptr, int base, unsigned long *val)
{
	return convert_strtoul(str, endptr, base, val);
}
