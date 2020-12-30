#include<malloc.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include"ext2.h"

ext2_inode* iget(unsigned int inode)
{/*获得内存空闲inode节点*/
	unsigned char buf[sizeof(ext2_inode)] = { 0X0 };
	ext2_inode* inode_info;
	inode_info = (ext2_inode*)malloc(sizeof(ext2_inode));
	if (inode_info == NULL)return NULL;
	memcpy(inode_info, buf, sizeof(ext2_inode));
	inode_info->inode = inode;
	return inode_info;
}
STATE iput(ext2_inode* inode)
{
	/*释放内存i节点*/
	free(inode);
	return OK;
}

unsigned int find_free_inode(ext2_dir_entry* dentry, ext2_dir_entry* Pdentry)
{/*找到一个有空闲inode节点的组，返回组号*/
	int i = 0;
	unsigned int min;
	if (dentry->file_type == F)
	{
		i = Pdentry->inode / sb.s_inodes_per_group;
		for (; i < groups_counts; i++)
		{
			if (gdesc[i].bg_free_inodes_count > 0)return i;
		}
	}
	min = 0;
	for (i = 1; i < groups_counts; i++)
	{
		if (gdesc[i].bg_free_inodes_count > gdesc[min].bg_free_inodes_count)
		{
			min = i;
		}
	}
	return min;
}
unsigned int get_free(unsigned char* buf)
{/*在buf中找到从start处往后的第一个空闲位,返回其偏移量*/
	int i = 0, j = 0;
	unsigned char z = 0x01, f = 0xff, t;
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		if (buf[i] != f)break;
	}
	if (i >= BLOCK_SIZE)return BLOCK_SIZE;
	for (j = 1; j <= 8; j++)
	{
		t = buf[i] & z;
		if (t == 0)
		{
			buf[i] = buf[i] | z;
			return i * 8 + j;
		}
		z = z << 1;
	}
}

STATE set_free(unsigned int offset, unsigned char* buf)
{/*此函数将buf中位于offset处的位置为空*/
	int i, j;
	unsigned char z = 0x01;
	if (offset >= BLOCK_SIZE)return FALSE;
	i = offset / 8;
	for (j = 1; j < offset % 8; j++)
		z = z << 1;
	buf[i] = buf[i] ^ z;
	return OK;
}

STATE dir_file_read(unsigned char* block[], ext2_inode* inode)
{	/*此函数将inode所指向的目录文件读入buf*/
	int counts, i;
	counts = inode->i_blocks;
	if (inode->i_size == 0)
	{
		return OK;
	}
	fseek(ext2_fp, inode->i_block[0] * BLOCK_SIZE, 0);
	block[0] = (unsigned char*)malloc(BLOCK_SIZE);
	if (block[0] == NULL)return SYSERROR;
	fread(block[0], BLOCK_SIZE, 1, ext2_fp);
	for (i = 1; i < counts; i++)
	{
		block[i] = (unsigned char*)malloc(BLOCK_SIZE);
		if (block[i] == NULL)return SYSERROR;
		fseek(ext2_fp, (inode->i_block[i] - inode->i_block[i - 1] - 1) * BLOCK_SIZE, 1);
		fread(block[i], BLOCK_SIZE, 1, ext2_fp);
	}
	return OK;
}

unsigned int find_free_block(ext2_inode* inode_info)
{/*找到一个有空闲块的组，返回组号*/
	unsigned int group;
	unsigned int offset_in_inode = inode_info->i_size / BLOCK_SIZE + 1;
	if (inode_info->i_size != 0)
	{
		group = inode_info->i_block[offset_in_inode - 1];
	}
	else
	{
		group = inode_info->inode / sb.s_inodes_per_group;
	}
	while (group < groups_counts)
	{
		if (gdesc[group].bg_free_blocks_count > 0)return group;
		group++;
	}
	return groups_counts + 1;
}

STATE find_empty(unsigned char* block[], ext2_dir_entry* dentry,
	ext2_dir_entry* Pdentry, ext2_inode* inode_info,
	unsigned int* offset)
{/*此函数在block数组中找到一个可以放下size大小目录项的地方，
 返回其在父目录文件的偏移量，
 若未能找到一块合适的空闲位置，则申请block*/
	unsigned int i = 0, j;
	unsigned short rec_len, real_len;
	unsigned char name_len;
	unsigned short size = dentry->rec_len;
	int flag;
	while (block[i] != NULL)
	{
		j = 0;
		while (j < BLOCK_SIZE)
		{
			memcpy(&rec_len, block[i] + j + 4, sizeof(short));
			if (rec_len > size)
			{
				memcpy(&name_len, block[i] + j + 6, sizeof(char));
				real_len = (((name_len + 8) - 1) / 4 + 1) * 4;
				if (rec_len - real_len >= size)
				{
					dentry->rec_len = rec_len - real_len;
					memcpy(block[i] + j + 4, &real_len, sizeof(short));
					*offset = i * BLOCK_SIZE + j + real_len;
					return OK;
				}
				j += rec_len;
			}
			else j += rec_len;
		}
	}
	flag = block_alloc(inode_info);
	if (flag != OK)return flag;
	block[i] = (unsigned char*)malloc(BLOCK_SIZE);
	if (block[i] == NULL)return SYSERROR;
	for (j = 0; j < BLOCK_SIZE; j++)
		block[i][j] = 0x0;
	dentry->rec_len = 1024;
	*offset = i * BLOCK_SIZE;
	return OK;
}
/******文件系统格式化相关函数*******/
STATE sb_write()
{
	unsigned int i;
	unsigned char buf[BLOCK_SIZE] = { 0x0 };
	fseek(ext2_fp, 1024, 0);
	memcpy(buf, &sb, sizeof(sb));
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	for (i = 1; i < groups_counts; i++)
	{
		fseek(ext2_fp, (sb.s_blocks_per_group - 1) * BLOCK_SIZE, 1);
		fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	}
	return OK;
}
STATE sb_read()
{
	unsigned char buf[BLOCK_SIZE];
	fseek(ext2_fp, 1024, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	memcpy(&sb, buf, BLOCK_SIZE);
	return OK;
}

STATE gdesc_write()
{
	unsigned char* gdesc_buf;
	unsigned int i, j;
	gdesc_buf = (unsigned char*)malloc(gdesc_blocks * BLOCK_SIZE);
	if (!gdesc_buf)return SYSERROR;
	for (i = 0; i < gdesc_blocks * BLOCK_SIZE; i++)
	{
		gdesc_buf[i] = 0x0;
	}
	j = 0;

	for (i = 0; i < groups_counts; i++)
	{
		memcpy(gdesc_buf + j, gdesc + i, sizeof(ext2_group_desc));
		j += sizeof(ext2_group_desc);
	}
	fseek(ext2_fp, (sb.s_first_data_block + 1) * BLOCK_SIZE, 0);
	fwrite(gdesc_buf, gdesc_blocks * BLOCK_SIZE, 1, ext2_fp);
	for (i = 1; i < groups_counts; i++)
	{
		fseek(ext2_fp, (sb.s_blocks_per_group - gdesc_blocks) * BLOCK_SIZE, 1);
		fwrite(gdesc_buf, gdesc_blocks * BLOCK_SIZE, 1, ext2_fp);
	}
	return OK;
}
STATE gdesc_read()
{
	unsigned char* gdesc_buf;
	unsigned int i, j;
	gdesc_buf = (unsigned char*)malloc(gdesc_blocks * BLOCK_SIZE);
	if (!gdesc_buf)return SYSERROR;
	fseek(ext2_fp, 1024 * 2, 0);
	fread(gdesc_buf, gdesc_blocks * BLOCK_SIZE, 1, ext2_fp);
	gdesc = (ext2_group_desc*)calloc(groups_counts, GDESC_SIZE);
	memcpy(gdesc, gdesc_buf, groups_counts * GDESC_SIZE);
	return OK;
}

STATE bitmap_write()
{
	unsigned char bbm_buf[BLOCK_SIZE], ibm_buf[BLOCK_SIZE], z_buf[BLOCK_SIZE] = { 0x0 };
	int used = 0, i, count, j;

	memcpy(&bbm_buf, &z_buf, BLOCK_SIZE);
	memcpy(&ibm_buf, &z_buf, BLOCK_SIZE);

	used = 1 + gdesc_blocks + 2 + inode_blocks_per_group;
	count = used / 8;
	for (i = 0; i < count; i++)
		bbm_buf[i] = 0xff;
	count = used % 8;
	if (count > 0)
	{
		bbm_buf[i] = ~(~0 << count);
	}

	ibm_buf[0] = 0x01;/*the first group*/
	count = sb.s_inodes_per_group / 8;
	i = BLOCK_SIZE - 1;
	for (; count > 0; count--)/*unused inode*/
	{
		ibm_buf[i] = 0xff;
		i -= 1;
	}
	count = sb.s_inodes_per_group % 8;
	if (count > 0)
	{
		ibm_buf[i] = ~0 << (8 - count);
	}

	fseek(ext2_fp, gdesc[0].bg_block_bitmap * BLOCK_SIZE, 0);
	fwrite(bbm_buf, BLOCK_SIZE, 1, ext2_fp);
	fwrite(ibm_buf, BLOCK_SIZE, 1, ext2_fp);
	ibm_buf[0] = 0x0;
	for (i = 1; i < groups_counts - 1; i++)
	{
		fseek(ext2_fp, (sb.s_blocks_per_group - 2) * BLOCK_SIZE, 1);
		fwrite(bbm_buf, BLOCK_SIZE, 1, ext2_fp);
		fwrite(ibm_buf, BLOCK_SIZE, 1, ext2_fp);
	}
	j = gdesc[groups_counts - 2].bg_free_blocks_count -
		gdesc[groups_counts - 1].bg_free_blocks_count;
	if (j > 0)
	{
		count = j / 8;
		i = BLOCK_SIZE - 1;
		for (; count > 0; count--)/*unused inode*/
		{
			bbm_buf[i] = 0xff;
			i -= 1;
		}
		count = j % 8;
		if (count > 0)
		{
			bbm_buf[i] = ~0 << (8 - count);
		}
	}
	fseek(ext2_fp, (sb.s_blocks_per_group - 2) * BLOCK_SIZE, 1);
	fwrite(bbm_buf, BLOCK_SIZE, 1, ext2_fp);
	fwrite(ibm_buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}

STATE root_write()
{
	unsigned int inode = root_inode->inode;
	unsigned char* buf;
	unsigned int offset, block;
	unsigned int i;
	buf = (unsigned char*)malloc(BLOCK_SIZE);
	if (!buf)return SYSERROR;
	/*read buf*/
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		buf[i] = 0x0;
	}
	/*write the inode_info to buf*/
	i = 0;
	block = gdesc[inode / sb.s_inodes_per_group].bg_inode_table +
		inode % sb.s_inodes_per_group / INODES_PER_BLOCK;
	offset = inode % sb.s_inodes_per_group % INODES_PER_BLOCK;
	memcpy(buf, &(root_inode->i_mode), INODE_SIZE);
	fseek(ext2_fp, block * BLOCK_SIZE, 0);
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}

STATE root_read()
{
	//extern block_read(long block,unsigned char* );
	unsigned char buf[BLOCK_SIZE] = { 0x0 };
	unsigned int offset, block;
	unsigned int inode;
	root_inode = iget(sb.s_first_ino);
	inode = root_inode->inode;
	block = gdesc[inode / sb.s_inodes_per_group].bg_inode_table +
		inode % sb.s_inodes_per_group / INODES_PER_BLOCK;
	offset = inode % sb.s_inodes_per_group % INODES_PER_BLOCK;
	//block_read(block,buf);
	fseek(ext2_fp, block * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	memcpy(&(root_inode->i_mode), buf + offset, INODE_SIZE);
}
/******文件系统格式化相关函数完*******/