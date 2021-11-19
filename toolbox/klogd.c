#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/klog.h>

#include <linux/limits.h>

#define LOG_TAG "KLOG"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/threads.h>
#include <private/android_filesystem_config.h>

#define CONSOLE_PATH 			"/dev/console"
#define KERNEL_LOG_CONFIG_PATH	"/storage/log/log_config"

#define LOG_COUNT_MAX			99

#define unlikely(_cond) (__builtin_expect( (_cond) != 0, 0 ))
#define likely(_cond)	(__builtin_expect( (_cond) != 0, 1 ))

#define LOGW(format, ...)  fprintf (stdout, LOG_TAG format, ## __VA_ARGS__);
#define LOGE(format, ...)  fprintf (stderr, LOG_TAG format, ## __VA_ARGS__);
#define LOGI(format, ...)  fprintf (stdout, LOG_TAG format, ## __VA_ARGS__);

static struct log_context {
	char *buf;
	const int buflen;
	int curpos;
	int klog_level;
	int printk_level;
	int do_logcat;
} context = {
	.buflen = 4096,
	.do_logcat = 0,
};

int gLogsaverFd;
char gLogsaveFilepath[PATH_MAX] = {0, };
mutex_t gLogsaverMutex;

static int get_klogenv(struct log_context *ctx)
{
	char value[PROPERTY_VALUE_MAX];

	/* EMERG, ALERT and CRIT */
	property_get("ro.sys.klog.printk", value, "3");
	ctx->printk_level = atoi(value);

	/* EMERG, ALERT, CRIT, ERR, WARNING, NOTICE and INFO */
	property_get("rw.sys.klog.printf", value, "7");
	ctx->klog_level = atoi(value);

	property_get("rw.sys.klog.dologcat", value, "0");
	ctx->do_logcat = atoi(value);

	return 0;
}

static inline int read_klog(struct log_context *ctx)
{
	if (ctx->curpos >= ctx->buflen)
		ctx->curpos = 0;

	char *buf = ctx->buf + ctx->curpos;
	unsigned int buflen = ctx->buflen - ctx->curpos;
	int ret = klogctl(2, buf, buflen);
	if (unlikely(ret < 0)) {
		perror("read_klog");
		return ret;
	}

	ctx->curpos += ret;

	return ctx->curpos;
}

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

int check_run_process(char *process_name)
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
		if(!strcmp (name, process_name)) {
			closedir(dp);
			return pid; 
		}
	}

	closedir(dp);

	return -1;
}

static void deinit_klog(void)
{
	klogctl(8, NULL, 7);
	klogctl(0, NULL, 0);
	printf("klogd exited\r\n");
}

static int init_klog(struct log_context *ctx)
{
	klogctl(1, NULL, 0);
	klogctl(8, NULL, ctx->printk_level);
	atexit(deinit_klog);

	printf("klogd started(k%d:f%d:l%d)\r\n",
		context.printk_level, context.klog_level, context.do_logcat);
	return 0;
}

static int init_console(const char *path)
{
	int fd = open(path, O_WRONLY | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		perror(path);
		return fd;
	}
	dup2(fd, fileno(stdout));
	dup2(fd, fileno(stderr));
	close(fd);

	//setvbuf(stdout, (char *) NULL, _IONBF, 0);
	//setvbuf(stderr, (char *) NULL, _IONBF, 0);

	return 0;
}

int init_logsaver()
{
	int configFd;
	int lognum;
	
	int ret = access("/storage/log/log_config", F_OK);
	if(ret != 0) {
		configFd = open("/storage/log/log_config", O_RDWR | O_CREAT, 0644);
		lognum = 0;
	} else {
		configFd = open("/storage/log/log_config", O_RDWR);
		read(configFd, &lognum, sizeof(int));	
	}

	lognum++;
	if(lognum == LOG_COUNT_MAX) {
		lognum = 0;
	}

	lseek(configFd, 0, SEEK_SET);
	write(configFd, &lognum, sizeof(int));
	close(configFd);

	sprintf(gLogsaveFilepath, "/storage/log/log_%02d.txt", lognum);

	gLogsaverFd = open(gLogsaveFilepath, O_RDWR | O_CREAT, 0644);
	if(gLogsaverFd < 0) {
		printf("[%s] logsaver file open failed\n", __func__);
		gLogsaverFd = 0;
		return -1;
	}

	mutex_init(&gLogsaverMutex);

	return 0;
}

static void handle_signal(int sig)
{
	/* SIGTERM is a normal way which is being used by the 'init' process. */
	if (sig == SIGTERM) {
		exit(EXIT_SUCCESS);
	} else if (sig == SIGINT) {
		/* close file */
		mutex_lock(&gLogsaverMutex);
		close(gLogsaverFd);
		gLogsaverFd = 0;
		mutex_unlock(&gLogsaverMutex);

		mutex_destroy(&gLogsaverMutex);

		/* remove file */
		if(remove(gLogsaveFilepath)) {
			perror("file remove failed ");
		}

		exit(EXIT_SUCCESS);
	} else {
		printf("klogd terminated by signal#%d\r\n", sig);
		fflush(stdout);
		exit(EXIT_FAILURE);
	}
}

static inline void print_out(const char *s, int level, int logcat)
{
	printf("["LOG_TAG"]" "%s\r\n", s);

	if (logcat) {
		switch (level) {
		case 0 ... 3: LOGE("%s", s); break;
		case 4: LOGW("%s", s); break;
		default: LOGI("%s", s); break;
		}
	}
}

static void proc_klog(struct log_context *ctx)
{
	int len;
	char value[PROPERTY_VALUE_MAX] = {0, };

	while (likely((len = read_klog(ctx)) >= 0)) {
		const char *start = ctx->buf;
		char *next;
		while (likely(len > 0 && (next = memchr(start, '\n', len)))) {
			if(gLogsaverFd == 0) {
				property_get("tcc.QB.boot.with", value, "0");			

				if(!strcmp(value, "snapshot")) {

					// check process
					while(1) {
						if(0 < check_run_process("log_summary")) {
							printf("[%s] check run log_summary\n", __func__);
							break;
						}
					}

					init_logsaver();

					int ret = fchmod(gLogsaverFd, 0644);
					ret = fchown(gLogsaverFd, AID_SYSTEM, AID_SYSTEM);
				}
			}

			/* for current string */
			*next++ = '\0'; /* \n -> \0 */
			const int curlen
				= (unsigned long)next - (unsigned long)start;
			const char *cur = start;

			/* for next memchr() */
			start = next;
			len -= curlen;

			int level = -1;
			if (likely(curlen >= 3 &&
					*cur == '<' && *(cur + 2) == '>')) {
				 /* at least "<X>" */
				level = *(cur + 1) - '0';
				cur += 3;
			}

			/* behavior of console printk */
			if (level < ctx->klog_level) {
				print_out(cur, level, ctx->do_logcat);

				mutex_lock(&gLogsaverMutex);
				if(gLogsaverFd > 0) {
					write(gLogsaverFd, cur, strlen(cur));
					write(gLogsaverFd, "\n", 1);
				}
				mutex_unlock(&gLogsaverMutex);
			}
		}
		if (len < 0)
			len = 0;

		ctx->curpos = len;
		if (len && start != ctx->buf) {
			/* word(s) that not terminated by trailing '\n' */
			memmove(ctx->buf, start, ctx->curpos);
		}
	}
}

int klogd_main(int argc, char **agrv)
{
	//daemon(0, 0);

	get_klogenv(&context);

	context.buf = malloc(context.buflen);
	if (!context.buf) {
		perror("malloc");
		return EXIT_FAILURE;
	}

	if (init_console(CONSOLE_PATH)) {
		free(context.buf);
		return EXIT_FAILURE;
	}

	if (init_klog(&context)) {
		free(context.buf);
		return EXIT_FAILURE;
	}

	signal(SIGTERM, handle_signal);
	signal(SIGINT, handle_signal);
	signal(SIGQUIT, handle_signal);
	signal(SIGHUP, handle_signal);

	proc_klog(&context);

	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);

	free(context.buf);

	return EXIT_FAILURE;
}
