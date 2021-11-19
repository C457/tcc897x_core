#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include "gpt.h"
#include "sparse.h"

void OnCreateFsImage(tagDiskImageHeader *DiskImageHeader, u32 pcount, u64 disksize);
void MakeMBR(int);
int Write_BunchHeader(FILE *, tagDiskImageBunchHeaderType *);


u64 DISK_MakeCRC(FILE *outfd)
{
	u8 * pBuffer; 
	u32 iReadSize;
	u32 nCRC32 = 0;
	u64 llCheckSize;
	u64 llTotalLength;

	pBuffer = malloc(sizeof(char)*64*1024);

	fseek(outfd, 0, SEEK_SET);
	llTotalLength = get_file_offset(outfd);
	fseek(outfd, 0, SEEK_SET);

	do{

		iReadSize = fread(pBuffer, 1, 64*1024, outfd);
		if(iReadSize <= 0)
			break;

		llCheckSize += iReadSize;
		nCRC32 = tc_calcCRC32(pBuffer, iReadSize, nCRC32);

	}while(iReadSize == 64*1024);

	free(pBuffer);
	
	return nCRC32;
}

int get_file_offset(FILE *fd)
{
	fseek(fd, 0, SEEK_END);
	return ftell(fd);
}

void OnCreateFsImage(tagDiskImageHeader *DiskImageHeader, 
		u32 pcount, u64 disksize)
{
	/* Initialize Disk Image Header */
	memset(DiskImageHeader, 0x0, sizeof(tagDiskImageHeader));
	memcpy(DiskImageHeader->tagHeader, TAG_AREA_IMAGE_HEADER, 8);
	DiskImageHeader->ulHeaderSize = sizeof(tagDiskImageHeader);
	strncpy((char *)DiskImageHeader->tagImageType, TAG_AREA_IMAGE_TYPE_DISK_IMAGE, 16);
	memcpy(DiskImageHeader->tagVersion, TAG_DISK_IMAGE_VERSION, 16);
	strncpy((char *)DiskImageHeader->areaName, "SD Data" ,16);
	memcpy(DiskImageHeader->tagDiskSize, TAG_DISK_IMAGE_SIZE, 8);
	memcpy(DiskImageHeader->tagPartitionCount, TAG_DISK_IMAGE_PARTITION_CNT, 8);

	DiskImageHeader->ulPartitionCount= pcount;
	DiskImageHeader->llDiskSize = disksize * BLOCK_SIZE;
}

static void prepare_mbr(u8  *mbr, u32 lba)
{
	mbr[0x1be] = 0x00; // bootalbe == false
	mbr[0x1bf] = 0xFF; // CHS
	mbr[0x1c0] = 0xFF; // CHS
	mbr[0x1c1] = 0xFF; // CHS

	mbr[0x1c2] = 0xEE; //MBR_PROTECTED_TYPE; // SET GPT partitoin
	mbr[0x1c3] = 0xFF; // CHS
	mbr[0x1c4] = 0xFF; // CHS
	mbr[0x1c5] = 0xFF; // CHS

	mbr[0x1c6] = 0x01; // Relative Start Sector
	mbr[0x1c7] = 0x00;
	mbr[0x1c8] = 0x00;
	mbr[0x1c9] = 0x00;

	memcpy(mbr + 0x1ca, &lba, sizeof(u32));

	mbr[0x1fe] = 0x55; // MBR Signature
	mbr[0x1ff] = 0xAA; // MBR Signature
}

int Write_BunchHeader(FILE *fd , tagDiskImageBunchHeaderType *BunchHeader)
{
	fseek(fd, 0, DiskInfo.ullPartitionInfoStartOffset);
	if(!fwrite(BunchHeader, sizeof(char), sizeof(tagDiskImageBunchHeaderType), fd)){
		return -1;
	}
	DiskInfo.ullPartitionInfoStartOffset += get_file_offset(fd);
	return 0;
}

void OnCreateGPTHeader(struct guid_partition_tbl *ptbl, 
		struct guid_partition *plist, u64 storage_size, u32 nplist)
{
	struct uefi_header *hdr = &ptbl->guid_header;
	u32 idx; 
	u64 npart, size;

	npart = GUID_RESERVED;

	prepare_mbr(ptbl->mbr, storage_size);
	prepare_guid_header(ptbl, storage_size);
	for(idx = 0; idx < nplist; idx++){
		size = plist[idx].size;
		if(size == 0) size = storage_size - npart - GUID_RESERVED + 1;
		guid_add_partition(ptbl, npart , npart+size-1, plist[idx].name);
		npart += size;
	}

	fill_crc32(ptbl);
}

int mktcimg(FILE *infd, FILE *outfd, FILE *gptfd, u64 storage_size)
{
	struct guid_partition_tbl ptbl;
	struct uefi_header *hdr = &ptbl.guid_header;
	struct guid_partition *plist;
	tagDiskImageHeader	DiskImageHeader;
	tagDiskImageBunchHeaderType BunchHeader;


	u32 pNum = 0;
	u32 idx, ridx;
	char *fbuf;
	u64 remain;

	FILE *fplist;

	pNum = parse_file_line(infd);
	plist = parse_ptn(infd, pNum);

	memset(&ptbl, 0 , sizeof(struct guid_partition_tbl));

	/* FWDN HEADER */
	DiskInfo.ullPartitionInfoStartOffset = 0; 
	OnCreateFsImage(&DiskImageHeader, pNum , storage_size);
	if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
		DEBUG("Disk Image Header Write Failed \n");
		return -1;
	}
	DiskInfo.ullPartitionInfoStartOffset = sizeof(tagDiskImageHeader); 

	BunchHeader.ullTargetAddress = 0;
	BunchHeader.ullLength = sizeof(struct guid_partition_tbl);
	if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

	/* GPT HEADER AND PARTITION ENTRY */
	OnCreateGPTHeader(&ptbl, plist, storage_size, pNum);
	if(!fseek(outfd , 0 , get_file_offset(outfd))){
		DEBUG("cant seek for GPT header \n");
		goto error;
	}

	if(!fwrite(&ptbl, sizeof(char), sizeof(struct guid_partition_tbl), outfd)){
		DEBUG("GPT Header Write Failed \n");
		goto error;
	}

	if(!fwrite(&ptbl, sizeof(char), sizeof(struct guid_partition_tbl), gptfd)){
		DEBUG("GPT Header Write Failed \n");
		goto error;
	}

	DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);

	for(idx = 0; idx < pNum ; idx++){

		if(plist[idx].size == 0 || plist[idx].path[0] == NULL){
			BunchHeader.ullTargetAddress= ptbl.guid_entry[idx].first_lba * BLOCK_SIZE;
			BunchHeader.ullLength = 0;
			if(Write_BunchHeader(outfd, &BunchHeader)) goto error;
		}else{
			
			DEBUG("idx : %d  %s \n", idx , plist[idx].name);

			fplist = fopen((char*)plist[idx].path, "r");

			if(fplist == NULL){
				DEBUG("file : %s can not open\n" , plist[idx].path);
				goto error;
			}

			if(!check_sparse_image(fplist)){
				DEBUG("sparse image is detected !!\n");
				sparse_image_write(fplist, outfd, ptbl.guid_entry[idx].first_lba);
			}else{

				BunchHeader.ullTargetAddress= ptbl.guid_entry[idx].first_lba * BLOCK_SIZE;
				BunchHeader.ullLength = BYTES_TO_SECTOR(get_file_offset(fplist))*512;
				if(Write_BunchHeader(outfd, &BunchHeader)) goto error1;
				fseek(fplist, 0 , SEEK_SET);
				fseek(outfd, 0, get_file_offset(outfd));

				fbuf = malloc(sizeof(char) * 0x100000); /* 1MB Buffer */
				if(fbuf == NULL){
					DEBUG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
					goto error1;
				}
				remain = BYTES_TO_SECTOR(BunchHeader.ullLength)*512;

				while(remain){

					if(remain >= 0x100000){
						if(!fread(fbuf, 1, 0x100000, fplist)){
							DEBUG(" tt read file : %s failed \n", plist[idx].path);
							goto error2;
						}
						if(!fwrite(fbuf, 1, 0x100000, outfd)){
							DEBUG("write outfile failed \n");
							goto error2;
						}
						remain -= 0x100000;
					}else{
						if(!fread(fbuf, 1, remain, fplist)){
							DEBUG("read file : %s failed \n", plist[idx].path);
							goto error2;
						}
						if(!fwrite(fbuf, 1, remain, outfd)){
							DEBUG("Write outfile failed \n");
							goto error2;
						}
						remain = 0;
					}
				}

				DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
				free(fbuf);
			}
			fclose(fplist);
		}
	}

	BunchHeader.ullTargetAddress = (hdr->last_lba + 1) * 512;
	BunchHeader.ullLength = sizeof(struct gpt_partition_entry) * ENTRY_SIZE;
	if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

	fseek(outfd, 0, get_file_offset(outfd));
	if(!fwrite(&ptbl.guid_entry,sizeof(char) ,sizeof(struct gpt_partition_entry) * ENTRY_SIZE, outfd)){
		DEBUG("write backup p entry failed \n");
		goto error;
	}

	if(!fwrite(&ptbl.guid_entry,sizeof(char) ,sizeof(struct gpt_partition_entry) * ENTRY_SIZE, gptfd)){
		DEBUG("write backup p entry failed \n");
		goto error;
	}
	DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);


	BunchHeader.ullTargetAddress = hdr->backup_lba * BLOCK_SIZE;
	BunchHeader.ullLength = 512;
	if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

	prepare_backup_guid_header(&ptbl.guid_header);
	fill_crc32(&ptbl);
	fseek(outfd, 0, get_file_offset(outfd));
	if(!fwrite(&ptbl.guid_header,1, 512, outfd)){
		DEBUG("write backup gpt header failed \n");
		goto error;
	}

	if(!fwrite(&ptbl.guid_header,1, 512, gptfd)){
		DEBUG("write backup gpt header failed \n");
		goto error;
	}

	DiskImageHeader.ulCRC32 = DISK_MakeCRC(outfd);
	fseek(outfd, 0, SEEK_SET);
	if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
		DEBUG("outfile write error\n");
		goto error;
	}

	free(plist);
	return 0;
error:
	free(plist);
	return -1;
error1:
	fclose(fplist);
	free(plist);
	return -2;
error2:
	fclose(fplist);
	free(plist);
	free(fbuf);
	return -2;


}

void ussage(void)
{
	printf(" mktcimg ");
	printf(" --storage_size [bytes] ");
	printf(" --fplist [partition list file] ");
	printf(" --outfile [.fai] ");
	printf(" --gptfile [.gpt]\n ");
}

int main(int argc , char **argv)
{
	uint64_t storage_size = 0;
	FILE *infd, *outfd, *gptfd;
	int res;

	DEBUG("%s === %d \n" , __func__, __LINE__);
	if(argc < 9){
		ussage();
		return 0;
	}
	
	if(!strcmp(argv[1], "--storage_size")){
		//storage_size = BYTES_TO_SECTOR(parse_size(argv[2]));
		storage_size = BYTES_TO_SECTOR(strtoull(argv[2], 0, 10));
		DEBUG("%s === %d  %llx \n" , __func__, __LINE__, storage_size);
	}else return -1;

	if(!strcmp(argv[3], "--fplist")){
		infd = fopen(argv[4], "r");
		if(infd == NULL) {
			DEBUG("file : %s is not exist\n", argv[4]);
			return -1;
		}
	}else return -1;


	if(!strcmp(argv[5], "--outfile")){
		outfd = fopen(argv[6], "w+");
		if(outfd == NULL){
			DEBUG("file : %s can not create\n", argv[4]);
			fclose(infd);
			return -1;
		}
	}else return -1;

	if(!strcmp(argv[7], "--gptfile")){
		gptfd = fopen(argv[8], "w+");
		if(gptfd == NULL){
			DEBUG("file : %s can not create\n", argv[4]);
			fclose(infd); fclose(outfd);
			return -1;
		}
	}else return -1;

	res = mktcimg (infd, outfd, gptfd, storage_size);

	if(res) DEBUG("error occures \n");

	fclose(infd);
	fclose(outfd);
	fclose(gptfd);
	
	return res;
}


























