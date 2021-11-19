#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/loop.h>
#include <errno.h>

#define KMSG_BUF_SHIFT  17
#define KMSG_BUF_LEN    (1 << KMSG_BUF_SHIFT)

int qb_kmsg_main(int argc, char *argv[])
{
	FILE *f_proc;
	FILE *f_log;
	int error;

	fprintf(stdout, "qb_kmsg start!!\n");

	if (access("/data/hibernate", F_OK) != 0) {
		fprintf(stdout, "qb_kmsg mount start!!\n");

		if (mount("/dev/block/platform/bdm/by-name/userdata", "/data", "ext4", MS_NOATIME | MS_NOSUID | MS_NODEV, "nomblk_io_submit,errors=panic") == -1) {
			fprintf(stdout, "qb_kmsg mount fail error: %s\n", strerror(errno));
			return -1;
		}

		fprintf(stdout, "qb_kmsg mount end!!\n");
	}

	fprintf(stdout, "qb_kmsg malloc start!!\n");

	char *qb_dmesg_buf = malloc(KMSG_BUF_LEN + 1);

	if (qb_dmesg_buf == NULL) {
		fprintf(stdout, "QB - qb_dmesg_buf kmalloc is failed!!\n");
		return -1;
	}

	memset(qb_dmesg_buf, 0, KMSG_BUF_LEN + 1);

	/* Open kmsg File */
	f_proc = fopen("/proc/kmsg", "r");
	if (!f_proc) {
		fprintf(stdout, "qb_kmsg cannot open /proc/kmsg error: %s\n", strerror(errno));
		return -1;
	}

	fprintf(stdout, "qb_kmsg fopen kmsg end!!\n");

	/* To Read */
	char kmsg_buffer[1];
	unsigned int idx_kmsg = 0;
	char fin_str[32];

	memset(fin_str, 0, 32);

	fprintf(stdout, "qb_kmsg strcpy start!!\n");

	strcpy(fin_str, "QB : qb_kmsg log finished");

	fprintf(stdout, "qb_kmsg read start!!\n");

	while (fread(kmsg_buffer, 1, 1, f_proc) > 0) {
		qb_dmesg_buf[idx_kmsg] = kmsg_buffer[0];

		if (idx_kmsg >= strlen(fin_str)) {
			if (!memcmp(&qb_dmesg_buf[idx_kmsg - strlen(fin_str)], fin_str, strlen(fin_str))) {
				idx_kmsg++;
				break;
			}
		}

		if (idx_kmsg >= KMSG_BUF_LEN) {
			idx_kmsg = KMSG_BUF_LEN + 1;
			break;
		}

		idx_kmsg++;
	}
	qb_dmesg_buf[idx_kmsg] = '\0';

	/* To Close */
	int ret = fclose(f_proc);

	if (ret != 0) {
		fprintf(stdout, "qb_kmsg fclose error!! %s\n", strerror(errno));
		return -1;
	}

	fprintf(stdout, "qb_kmsg fopen qb_kmsg start!!\n");

	/* Open Log File */
	f_log = fopen("/data/hibernate/qb_kmsg", "w+");
	if (!f_log) {
		fprintf(stdout, "qb_kmsg cannot open /data/hibernate/qb_kmsg error: %s\n", strerror(errno));
		fclose(f_proc);
		return -1;
	}

	fprintf(stdout, "qb_kmsg fwrite start!!\n");

	unsigned int i = 0;

	for (i = 0; i < idx_kmsg; i++)
		fwrite(&qb_dmesg_buf[i], 1, 1, f_log);

	ret = fclose(f_log);
	if (ret != 0) {
		fprintf(stdout, "qb_kmsg log_fd Close ERROR!! %d\n", ret);
		return -1;
	}

	sync();

	error = umount("/data");
	fprintf(stdout, "qb_kmsg umount error = %d\n", error);

	return 0;
}
