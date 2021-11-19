
#include "common.h"

#define TAG_AREA_IMAGE_HEADER			"[HEADER]"
#define TAG_AREA_IMAGE_TYPE_KERNEL_IMAGE	"KERNEL_IMAGE"
#define TAG_AREA_IMAGE_TYPE_RAW_IMAGE		"RAW_IMAGE"
#define TAG_AREA_IMAGE_TYPE_KEYBOX_IMAGE	"KEYBOX_IMAGE"
#define TAG_AREA_IMAGE_TYPE_UID_IMAGE		"UID_IMAGE"

#define TAG_AREA_IMAGE_TYPE_DISK_IMAGE		"FILESYSTEM_IMAGE"
#define TAG_DISK_IMAGE_VERSION			"TCC FAT IMG V0.1"
#define TAG_DISK_IMAGE_SIZE			"DISKSIZE"
#define TAG_DISK_IMAGE_PARTITION_CNT		"PART_CNT"

//#define DBG

#ifdef DBG
#define DEBUG printf
#else
#define DEBUG
#endif

typedef struct {
	u8  	tagHeader[8];
	u32  	ulHeaderSize;
	u32  	ulCRC32;

	u8 	tagImageType[16];

	u8 	tagVersion[16];

	u8 	areaName[16];

	u8 	tagDiskSize[8];
	u64	llDiskSize;

	u8 	tagPartitionCount[8];
	u32  	ulPartitionCount;
	u32 	ulDummy2;
}tagDiskImageHeader;

typedef struct {
	u64 ullTargetAddress;
	u64 ullLength;
}tagDiskImageBunchHeaderType;


typedef struct {
	u64 ullPartitionInfoStartOffset;
}tagGDiskInfo;

tagGDiskInfo DiskInfo;
int get_file_offset(FILE *fd);
int Write_BunchHeader(FILE *fd , tagDiskImageBunchHeaderType *BunchHeader);
