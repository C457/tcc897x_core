#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gpt.h"


int parse_file_line(FILE *fd)
{
	char ch;
	u32 line = 0;
	while(!feof(fd)){
		ch = fgetc(fd);
		if(ch == '\n')
			line++;
	}
	return line;

}

u64 parse_size(char *sz)
{
	int l = strlen(sz);
	u64 n = strtoull(sz, 0, 10);
	if(l){
		switch(sz[l-1]){
			case 'k':
			case 'K':
				n *= 1024;
				break;
			case 'm':
			case 'M':
				n *= (1024*1024);
				break;
			case 'g':
			case 'G':
				n *= (1024*1024*1024);
		}
	}

	return n;
}

struct guid_partition* parse_ptn(FILE *fp, u32 nline)
{
	int idx;
	struct guid_partition *plist;

  	plist = malloc(sizeof(struct guid_partition) * nline);
	idx = 0;

	char text[256], *p;
	fseek(fp, 0, SEEK_SET);

	while(fgets(text, sizeof(text), fp))
	{
		p = strtok(text, ":");
		memcpy(plist[idx].name, p, strlen(p));
		DEBUG("location : %s \n", plist[idx].name);
		p = strtok(NULL , "@");	
		plist[idx].size = BYTES_TO_SECTOR(parse_size(p));
		DEBUG("location : %llu \n", plist[idx].size);
		p = strtok(NULL, "\r\n");
		if(p != NULL){
			memcpy(plist[idx].path, p, strlen(p));
			DEBUG("location : %s \n", plist[idx].path);
		}
		idx++;
	}
	return plist;
}
