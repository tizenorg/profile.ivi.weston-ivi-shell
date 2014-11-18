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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "weston-test-runner.h"

#include "../shared/str-util.h"

TEST(test_weston_strtol)
{
	bool ret;
	long val = -1;
	char *end = NULL, *str = NULL;

	ret = weston_strtol(NULL, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	ret = weston_strtol(NULL, NULL, 10, NULL);
	assert(ret == false);

	str = "12";
	ret = weston_strtol(str, NULL, 10, &val);
	assert(ret == true);
	assert(val == 12);

	ret = weston_strtol(str, &end, 10, &val);
	assert(end != NULL);
	assert(*end == '\0');

	str = "-12"; val = -1;
	ret = weston_strtol(str, &end, 10, &val);
	assert(ret == true);
	assert(val == -12);

	str = "0x12"; val = -1;
	ret = weston_strtol(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 0x12);

	str = "A"; val = -1;
	ret = weston_strtol(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 10);

	str = "-0x20"; val = -1;
	ret = weston_strtol(str, &end, 16, &val);
	assert(ret == true);
	assert(val == -0x20);

	str = "0012"; val = -1;
	ret = weston_strtol(str, &end, 8, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0101"; val = -1;
	ret = weston_strtol(str, &end, 2, &val);
	assert(ret == true);
	assert(val == 5);

	str = "s12"; val = -1;
	ret = weston_strtol(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	ret = weston_strtol(str, &end, 10, &val);
	assert(end == str);

	str = "214748364789L"; val = -1;
	ret = weston_strtol(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	str = ""; val = -1;
	ret = weston_strtol(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);
}

TEST(test_weston_strtoul)
{
	bool ret;
	unsigned long val = 0;
	char *end = NULL, *str = NULL;

	ret = weston_strtoul(NULL, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	ret = weston_strtoul(NULL, NULL, 10, NULL);
	assert(ret == false);

	str = "15";
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == true);
	assert(val == 15);

	ret = weston_strtoul(str, &end, 10, &val);
	assert(end != NULL);
	assert(*end == '\0');

	str = "0x12"; val = 0;
	ret = weston_strtoul(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 18);

	str = "A"; val = 0;
	ret = weston_strtoul(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0012"; val = 0;
	ret = weston_strtoul(str, &end, 8, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0101"; val = 0;
	ret = weston_strtoul(str, &end, 2, &val);
	assert(ret == true);
	assert(val == 5);

	str = "s15"; val = 0;
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	ret = weston_strtoul(str, &end, 10, &val);
	assert(end == str);

	str = "429496729533UL"; val = 0;
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = "-1"; val = 0;
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = "    -1234"; val = 0;
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = ""; val = 0;
	ret = weston_strtoul(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);
}

TEST(test_weston_strtoi)
{
	bool ret;
	int val = -1;
	char *end = NULL, *str = NULL;

	ret = weston_strtoi(NULL, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	ret = weston_strtoi(NULL, NULL, 10, NULL);
	assert(ret == false);

	str = "12";
	ret = weston_strtoi(str, NULL, 10, &val);
	assert(ret == true);
	assert(val == 12);

	ret = weston_strtoi(str, &end, 10, &val);
	assert(end != NULL);
	assert(*end == '\0');

	str = "-12"; val = -1;
	ret = weston_strtoi(str, &end, 10, &val);
	assert(ret == true);
	assert(val == -12);

	str = "0x12"; val = -1;
	ret = weston_strtoi(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 0x12);

	str = "A"; val = -1;
	ret = weston_strtoi(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 10);

	str = "-0x20"; val = -1;
	ret = weston_strtoi(str, &end, 16, &val);
	assert(ret == true);
	assert(val == -0x20);

	str = "0012"; val = -1;
	ret = weston_strtoi(str, &end, 8, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0101"; val = -1;
	ret = weston_strtoi(str, &end, 2, &val);
	assert(ret == true);
	assert(val == 5);

	str = "-5"; val = -1;
	ret = weston_strtoi(str, &end, 2, &val);
	assert(ret == true);
	assert(val == -5);

	str = "s12"; val = -1;
	ret = weston_strtoi(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	ret = weston_strtoi(str, &end, 10, &val);
	assert(end == str);

	str = "214748364789L"; val = -1;
	ret = weston_strtoi(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);

	str = ""; val = -1;
	ret = weston_strtoi(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == -1);
}

TEST(test_weston_strtoui)
{
	bool ret;
	unsigned int val = 0;
	char *end = NULL, *str = NULL;

	ret = weston_strtoui(NULL, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	ret = weston_strtoui(NULL, NULL, 10, NULL);
	assert(ret == false);

	str = "15";
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == true);
	assert(val == 15);

	ret = weston_strtoui(str, &end, 10, &val);
	assert(end != NULL);
	assert(*end == '\0');

	str = "0x12"; val = 0;
	ret = weston_strtoui(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 18);

	str = "A"; val = 0;
	ret = weston_strtoui(str, &end, 16, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0012"; val = 0;
	ret = weston_strtoui(str, &end, 8, &val);
	assert(ret == true);
	assert(val == 10);

	str = "0101"; val = 0;
	ret = weston_strtoui(str, &end, 2, &val);
	assert(ret == true);
	assert(val == 5);

	str = "s15"; val = 0;
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	ret = weston_strtoui(str, &end, 10, &val);
	assert(end == str);

	str = "429496729533UL"; val = 0;
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = "-1"; val = 0;
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = "    -1234"; val = 0;
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);

	str = ""; val = 0;
	ret = weston_strtoui(str, NULL, 10, &val);
	assert(ret == false);
	assert(val == 0);
}
