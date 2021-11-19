/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include <cutils/android_reboot.h>
#include <unistd.h>

#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

static int which_number (char *s)
{
	int len, i;

	len = strlen (s);

	for(i = 0; i < len; i++) {
		if((s[i] < '0' || s[i] > '9'))
			return -1;
		}

	return atoi (s);
}

int get_pid_from_proc_by_name (char *str)
{
	DIR *dp;
	struct dirent *dir;
	char buf[100], line[1024], tag[100], name[100];
	int pid;
	FILE *fp;

	dp = opendir("/proc");
	if(!dp) {
		return -1;
	}

	while ((dir = readdir(dp))) {
		pid = which_number(dir->d_name);

		if(pid == -1) {
			continue;
		}

		snprintf(buf, 100, "/proc/%d/status", pid);
		fp = fopen(buf, "r");
		if(fp == NULL) {
			continue;
		}

		fgets(line, 1024, fp);

		fclose(fp);

		sscanf(line, "%s %s", tag, name);
		if(!strcmp (name, str)) {
			closedir(dp);
			return pid;
		}
	}

	closedir(dp);
	return -1;
}

int main(int argc, char *argv[])
{
    int ret;
    size_t prop_len;
    char property_val[PROPERTY_VALUE_MAX];
    const char *cmd = "reboot";
    char *optarg = "";

    opterr = 0;
    do {
        int c;

        c = getopt(argc, argv, "p");

        if (c == EOF) {
            break;
        }

        switch (c) {
        case 'p':
            cmd = "shutdown";
            break;
        case '?':
            fprintf(stderr, "usage: %s [-p] [rebootcommand]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } while (1);

    if(argc > optind + 1) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc > optind)
        optarg = argv[optind];

	/* get klogd pid */
	pid_t pid;
	pid = get_pid_from_proc_by_name("klogd");

	/* send signal */
	kill(pid, SIGINT);

    prop_len = snprintf(property_val, sizeof(property_val), "%s,%s", cmd, optarg);
    if (prop_len >= sizeof(property_val)) {
        fprintf(stderr, "reboot command too long: %s\n", optarg);
        exit(EXIT_FAILURE);
    }

    ret = property_set(ANDROID_RB_PROPERTY, property_val);
    if(ret < 0) {
        perror("reboot");
        exit(EXIT_FAILURE);
    }

    // Don't return early. Give the reboot command time to take effect
    // to avoid messing up scripts which do "adb shell reboot && adb wait-for-device"
    while(1) { pause(); }

    fprintf(stderr, "Done\n");
    return 0;
}
