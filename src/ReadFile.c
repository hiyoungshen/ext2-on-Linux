#include<stdio.h>
#include<stdlib.h>
#include<malloc.h>
#include<string.h>
#include"fs.h"
#include"ext2.h"
STATE ReadFile(BYTE buf[],DWORD length, PDWORD size,PFile pfile)
{
	ext2_inode *inode;
	char b_buf[BLOCK_SIZE]={0x0};
	unsigned int no,off,len;
	*size=0;
	inode=inode_read(pfile->inode);
	if(0<=inode->i_blocks&&inode->i_blocks<=12)
	{
		while(pfile->off<inode->i_size)
		{
			no=pfile->off/BLOCK_SIZE;
			off=pfile->off%BLOCK_SIZE;
			if(inode->i_size-pfile->off>BLOCK_SIZE)len=BLOCK_SIZE;
			else len=(inode->i_size-pfile->off);
			block_read(b_buf,inode->i_block[no]);
			if(length-*size<=len)
			{
				memcpy(buf+*size,b_buf+off,length-*size);
				size=length;
				pfile->off+=(length-*size);
			}
			else
			{
				memcpy(buf+*size,b_buf+off,len);
				*size+=len;
				pfile->off+=len;
			}
		}
		return OK;
	}
	else 
		return VDISKERROR;

}