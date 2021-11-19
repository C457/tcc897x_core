//+[TCCQB]
/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CUTILS_QBMANAGER_H
#define __CUTILS_QBMANAGER_H

#include <linux/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <cutils/log.h>

#include <linux/qb_manager.h>	// QuickBoot Definitions. ( Properties... )



#ifdef __cplusplus
extern "C" {
#endif

#define QB_DEV				"/dev/qb_com"
#define QBUF_SIZE 			1024

//#define uartshow(x...)
//#define uartshow(x...) \
	{ char ulog[QBUF_SIZE]; snprintf(ulog, QBUF_SIZE, x); uart_log(ulog); }
#define uartshow(x...) \
	{ char *str; int len = snprintf(NULL, 0, x); if (str = malloc((len+1)*sizeof(char))) { \
	snprintf(str, len+1, x); uart_log(str); free(str); } else { char ulog[QBUF_SIZE]; \
	snprintf(ulog, QBUF_SIZE, x); uart_log(ulog); } }


static void uart_log(char *u_log)
{
	int fd = -1;
	fd = open(QB_DEV, O_RDWR);
	if (fd < 0) {
		ALOGE("Failed to open %s.\n", QB_DEV);
		return;
	}
	ioctl(fd, 0x901, u_log);
	close(fd);
	ALOGI("- %s", u_log);
	return;
}

#ifdef __cplusplus
}
#endif

#endif /* __CUTILS_QBMANAGER_H */
//-[TCCQB]
//
