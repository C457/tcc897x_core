
#include "mktcimg.h"


#define GUID_VERSION			0x00010000
#define GUID_MAGIC			"EFI PART"
#define GUID_RESERVED			34

/* GPT Signature Should be 0x5452415020494645 */
#define GPT_SIGNATURE_1			0x54524150
#define GPT_SIGNATURE_2			0x20494645

/* GPT Offsets */
#define PPROTECTIVE_MBR_SIZE		0x200
#define HEADER_SIZE_OFFSET		12
#define HEADER_CRC_OFFSET		16
#define PRIMARY_HEADER_OFFSET		24
#define BACKUP_HEADER_OFFSET		32
#define FIRST_USABLE_LBA_OFFSET		40
#define LAST_USABLE_LBA_OFFSET		48
#define PARTITION_ENTRIES_OFFSET	72
#define PARTITION_COUNT_OFFSET		80
#define PENTRY_SIZE_OFFSEt		84
#define PARTITION_CRC_OFFSET		88

#define MIN_PARTITION_ARRAY_SIZE	0x4000

#define PARTITION_ENTRY_LAST_LBA	40

#define ENTRY_SIZE			0x80
#define UNIQUE_GUID_OFFSET		16
#define FIRST_LBA_OFFSET		32
#define LAST_LBA_OFFSET			40
#define ATTRIBUTE_FLAG_OFFSET		48
#define PARTITION_NAME_OFFSET		56

#define MAX_GPT_NAME_SIZE          	72
#define PARTITION_TYPE_GUID_SIZE   	16
#define UNIQUE_PARTITION_GUID_SIZE 	16
#define NUM_PARTITIONS             	32


/* Some useful define used to access the MBR/EBR table */
#define BLOCK_SIZE                0x200
#define TABLE_ENTRY_0             0x1BE
#define TABLE_ENTRY_1             0x1CE
#define TABLE_ENTRY_2             0x1DE
#define TABLE_ENTRY_3             0x1EE
#define TABLE_SIGNATURE           0x1FE
#define TABLE_ENTRY_SIZE          0x010

#define OFFSET_STATUS             0x00
#define OFFSET_TYPE               0x04
#define OFFSET_FIRST_SEC          0x08
#define OFFSET_SIZE               0x0C
#define COPYBUFF_SIZE             (1024 * 16)
#define BINARY_IN_TABLE_SIZE      (16 * 512)
#define MAX_FILE_ENTRIES          20

#define MBR_EBR_TYPE              0x05
#define MBR_MODEM_TYPE            0x06
#define MBR_MODEM_TYPE2           0x0C
#define MBR_SBL1_TYPE             0x4D
#define MBR_SBL2_TYPE             0x51
#define MBR_SBL3_TYPE             0x45
#define MBR_RPM_TYPE              0x47
#define MBR_TZ_TYPE               0x46
#define MBR_MODEM_ST1_TYPE        0x4A
#define MBR_MODEM_ST2_TYPE        0x4B
#define MBR_EFS2_TYPE             0x4E

#define MAX_PARTITIONS 128


/* UEFI Partition Info Structure -- START */
struct partition_entry {
	unsigned char type_guid[PARTITION_TYPE_GUID_SIZE];
	unsigned dtype;
	unsigned char unique_partition_guid[UNIQUE_PARTITION_GUID_SIZE];
	unsigned long long first_lba;
	unsigned long long last_lba;
	unsigned long long size;
	unsigned long long attribute_flag;
	unsigned char name[MAX_GPT_NAME_SIZE];
};

struct gpt_partition_entry {
	unsigned char type_guid[PARTITION_TYPE_GUID_SIZE];
	unsigned char unique_partition_guid[UNIQUE_PARTITION_GUID_SIZE];
	unsigned long long first_lba;
	unsigned long long last_lba;
	unsigned long long attribute_flag;
	unsigned char name[MAX_GPT_NAME_SIZE];
};

struct uefi_header{
	u8         magic[8];

	u32        version;
	u32        header_size;

	u32        crc32;
	u8         rev[4];

	u64        header_lba;
	u64        backup_lba;
	u64        first_lba;
	u64        last_lba;

	u8         guid[16];

	u64        efi_entries_lba;
	u32        efi_entries_count;
	u32        efi_entries_size;
	u32        efi_entries_crc32;
}__attribute__((packed));

struct guid_partition_tbl {
	u8 mbr[512];
	union{
		struct uefi_header guid_header;
		u8 alloc[512];
	};
	struct gpt_partition_entry guid_entry[ENTRY_SIZE];
};

struct guid_partition {
	u8     name[256];
	u64    size;
	u8     path[256];
};

static const u8 guid_partition_type[5][16] = {
	{ 0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
		0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7,}, // Basic Data Partition.
	{ 0xaf, 0x3d, 0xc6, 0x0f, 0x83, 0x84, 0x72, 0x47,
		0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4,}, // Linux Filesystem Data.
	{ 0x5d, 0x2a, 0x3a, 0xfe, 0x32, 0x4f, 0xa7, 0x41,
		0xb7, 0x25, 0xac, 0xcc, 0x32, 0x85, 0xa3, 0x09,}, // Chrome OS Kernel.
	{ 0x02, 0xe2, 0xb8, 0x3c, 0x7e, 0x3b, 0xdd, 0x47,
		0x8a, 0x3c, 0x7f, 0xf2, 0xa1, 0x3c, 0xfc, 0xec,}, // Chrome OS Boot.
	{ 0x3d, 0x75, 0x0a, 0x2e, 0x48, 0x9e, 0xb0, 0x43,
		0x83, 0x37, 0xb1, 0x51, 0x92, 0xcb, 0x1b, 0x5e,}, // Chrome OS Reserved
};

static const u8 volume_guid[16] = {
	0x54, 0x45, 0x4c, 0x45, 0x43, 0x48, 0x49, 0x50, // TELECHIPSANDROID
	0x53, 0x41, 0x4e, 0x44, 0x53, 0x4f, 0x49, 0x44,
};


void prepare_guid_header(struct guid_partition_tbl* , unsigned int);

void fill_crc32(struct guid_partition_tbl *ptbl);
unsigned int calculate_crc32(unsigned char *buffer, int len);

unsigned int tc_calcCRC32(unsigned char *, u64 , u32);

int guid_add_partition(struct guid_partition_tbl *ptbl, u64 first_lba,
		u64 last_lba, u8 *name);

void prepare_backup_guid_header(struct uefi_header *hdr);
int parse_file_line(FILE *fd);
struct guid_partition*  parse_ptn(FILE *fp, u32 nline);
