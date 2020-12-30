#include<malloc.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include"ext2.h"
STATE block_write(unsigned char* buf, unsigned int block)
{
	if (fseek(ext2_fp, block * BLOCK_SIZE, 0));
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}
STATE block_read(unsigned char* buf, unsigned int block)
{
	fseek(ext2_fp, block * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}

ext2_inode* inode_read(unsigned int inode)
{
	int flag = OK;
	unsigned block, offset;
	unsigned char buf[BLOCK_SIZE];
	ext2_inode* inode_info = iget(inode);
	if (inode_info == NULL)return SYSERROR;
	/*读inode信息到内存结构*/
	block = gdesc[inode / sb.s_inodes_per_group].bg_inode_table +
		inode % sb.s_inodes_per_group / INODES_PER_BLOCK;
	offset = (inode % sb.s_inodes_per_group) % INODES_PER_BLOCK - 1;
	fseek(ext2_fp, block * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	memcpy(&(inode_info->i_mode), buf + offset * INODE_SIZE, INODE_SIZE);
	return inode_info;
}
STATE inode_write(unsigned int inode, ext2_inode* inode_info)
{
	unsigned char buf[BLOCK_SIZE];
	unsigned int block, offset;

	block = gdesc[inode / sb.s_inodes_per_group].bg_inode_table +
		inode % sb.s_inodes_per_group / INODES_PER_BLOCK;
	offset = inode % sb.s_inodes_per_group % INODES_PER_BLOCK - 1;
	fseek(ext2_fp, block * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);

	memcpy(buf + offset * INODE_SIZE, &(inode_info->i_mode), INODE_SIZE);

	fseek(ext2_fp, -BLOCK_SIZE, 1);
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}
STATE ibitmap_read(unsigned int group, unsigned char* buf)
{
	fseek(ext2_fp, gdesc[group].bg_inode_bitmap * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}
STATE ibitmap_write(unsigned int group, unsigned char* buf)
{
	fseek(ext2_fp, gdesc[group].bg_inode_bitmap * BLOCK_SIZE, 0);
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}
STATE bbitmap_read(unsigned int group, unsigned char* buf)
{
	fseek(ext2_fp, gdesc[group].bg_block_bitmap * BLOCK_SIZE, 0);
	fread(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}
STATE bbitmap_write(unsigned int group, unsigned char* buf)
{
	fseek(ext2_fp, gdesc[group].bg_block_bitmap * BLOCK_SIZE, 0);
	fwrite(buf, BLOCK_SIZE, 1, ext2_fp);
	return OK;
}

unsigned int inode_alloc(ext2_dir_entry* dentry, ext2_dir_entry* Pdentry)
{/*分配inode，返回其索引号*/
	unsigned int group, offset;
	unsigned char buf[BLOCK_SIZE], z_buf[BLOCK_SIZE] = { 0x0 };
	ext2_inode inode;
	group = find_free_inode(dentry, Pdentry);
	if (group >= groups_counts)return 0;
	if (ibitmap_read(group, buf) != OK)return 0;
	offset = get_free(buf);/*此函数用于得到buf中第一个空闲位，并将其置位*/
	if (offset >= BLOCK_SIZE)return 0;
	/*写超级块、组描述符*/
	sb.s_free_inodes_count--;
	gdesc[group].bg_free_inodes_count--;
	ibitmap_write(group, buf);
	/*初始化inode*/
	memcpy(&inode, z_buf, sizeof(ext2_inode));
	inode_write(group * sb.s_inodes_per_group + offset, &inode);
	return group * sb.s_inodes_per_group + offset;
}
STATE inode_delete(ext2_dir_entry* dentry)
{/*删除索引节点号为inode的索引节点*/
	unsigned char buf[BLOCK_SIZE];
	unsigned int group, offset, inode = dentry->inode;
	group = inode / sb.s_inodes_per_group;
	offset = inode % sb.s_inodes_per_group;
	ibitmap_read(group, buf);
	set_free(offset, buf);
	dentry->inode = 0;
	ibitmap_write(group, buf);
	sb.s_free_inodes_count++;
	gdesc[group].bg_free_inodes_count++;
	return OK;
}



STATE block_alloc(ext2_inode* inode_info)
{
	unsigned int group, offset;
	unsigned char bbitmap_buf[BLOCK_SIZE], b_buf[BLOCK_SIZE];
	unsigned int block, t_no, t_off;
	group = find_free_block(inode_info);
	if (group >= groups_counts)
	{
		return INSUFFICIENTSPACE;
	}
	bbitmap_read(group, bbitmap_buf);
	offset = get_free(bbitmap_buf);
	sb.s_free_blocks_count--;
	gdesc[group].bg_free_blocks_count--;
	if (bbitmap_write(group, bbitmap_buf) != OK)return SYSERROR;
	block = sb.s_first_data_block + group * sb.s_blocks_per_group + offset - 1;
	t_no = inode_info->i_blocks;
	if (t_no >= 0 && t_no <= 11)
	{
		inode_info->i_block[t_no] = block;
	}
	else if (t_no > 11 && t_no <= 11 + 256)
	{/*一次间接 下标12*/
		if (inode_info->i_blocks > 12)
		{/*i_block[12]存在*/
			block_read(b_buf, inode_info->i_block[12]);
			t_off = t_no - 12;
			memcpy(b_buf + t_off * 4, &block, sizeof(int));
			block_write(b_buf, inode_info->i_block[12]);
		}
		else if (inode_info->i_blocks == 12)
		{/*没有一次间接*/
			/*找一个一次间接的块*/
			group = find_free_block(inode_info);
			if (group >= groups_counts)
			{
				return INSUFFICIENTSPACE;
			}
			bbitmap_read(group, bbitmap_buf);
			offset = get_free(bbitmap_buf);
			sb.s_free_blocks_count--;
			gdesc[group].bg_free_blocks_count--;
			if (bbitmap_write(group, bbitmap_buf) != OK)return SYSERROR;
			memset(b_buf, 0, BLOCK_SIZE);
			inode_info->i_block[12] =
				sb.s_first_data_block + group * sb.s_blocks_per_group + offset - 1;
			memcpy(b_buf, &block, sizeof(int));
			block_write(b_buf, inode_info->i_block[12]);
		}
		else return SYSERROR;
	}
	inode_info->i_blocks++;
	if (inode_write(inode_info->inode, inode_info) != OK)return SYSERROR;
	return OK;
}

STATE block_delete(ext2_inode* inode_info, unsigned int no)
{
	unsigned int block, group, i = no, off;/*no为i_block的下标*/
	unsigned char bbitmap_buf[BLOCK_SIZE], b_buf[BLOCK_SIZE];
	if (no >= 0 && no < 12)block = inode_info->i_block[no];
	else if (no >= 12 && no < 12 + 256)
	{
		if (block_read(b_buf, inode_info->i_block[12]) != OK)return SYSERROR;
		off = (no - 12);
		memcpy(&block, b_buf + off * 4, sizeof(int));
	}
	else return SYSERROR;
	group = block / sb.s_blocks_per_group;
	bbitmap_read(group, bbitmap_buf);
	set_free(block % sb.s_blocks_per_group, bbitmap_buf);
	bbitmap_write(group, bbitmap_buf);
	sb.s_free_blocks_count++;
	gdesc[group].bg_free_blocks_count++;

	i = block + 1;
	while (i < inode_info->i_blocks && i <= 12)
	{
		inode_info->i_block[i - 1] = inode_info->i_block[i];
		i++;
	}
	i -= 12;
	while (i < inode_info->i_blocks - 12)
	{
		memcpy(&off, b_buf + i * 4, sizeof(int));
		memcpy(b_buf + (i - 1) * 4, &off, sizeof(int));
		i++;
	}
	if (inode_info->i_blocks > 12)
		block_write(b_buf, inode_info->i_block[12]);
	inode_info->i_blocks--;
	if (block == inode_info->i_blocks)
		inode_info->i_size = BLOCK_SIZE * (inode_info->i_blocks);
	else inode_info->i_size -= BLOCK_SIZE;
	inode_write(inode_info->inode, inode_info);
	return OK;
}
STATE dentry_add(ext2_dir_entry* dentry, ext2_dir_entry* Pdentry)
{/*在父目录文件中插入一个目录项*/
	unsigned char* block[N_BLOCKS] = { NULL };
	unsigned int offset, i;
	int flag;
	unsigned int size = (((8 + dentry->name_len) - 1) / 4 + 1) * 4;
	ext2_inode* inode_info = inode_read(Pdentry->inode);

	dentry->rec_len = ((8 + dentry->name_len - 1) / 4 + 1) * 4;
	flag = dir_file_read(block, inode_info);
	if (flag != OK)return flag;
	flag = find_empty(block, dentry, Pdentry, inode_info, &offset);
	if (flag != OK)return flag;
	memcpy(block[offset / BLOCK_SIZE] + offset % BLOCK_SIZE, dentry, size);
	if (offset >= inode_info->i_size)
		inode_info->i_size = inode_info->i_size + size;
	block_write(block[offset / BLOCK_SIZE], inode_info->i_block[offset / BLOCK_SIZE]);
	inode_write(inode_info->inode, inode_info);
	return OK;
}

unsigned int find_dentry(ext2_dir_entry* dentry, unsigned char* block[],
	unsigned int offset)
{
	char name[NAME_LEN], temp[NAME_LEN], namez[NAME_LEN] = { 0x0 };
	unsigned int i, j;
	unsigned short tr_len;
	unsigned char name_len = dentry->name_len, tn_len, type;
	memcpy(name, namez, NAME_LEN);
	memcpy(name, dentry->name, dentry->name_len);
	i = offset / BLOCK_SIZE;
	j = offset % BLOCK_SIZE;
	while (block[i] != NULL)
	{

		while (j < BLOCK_SIZE)
		{
			memcpy(temp, namez, NAME_LEN);
			memcpy(&tn_len, block[i] + j + 6, sizeof(char));
			memcpy(&tr_len, block[i] + j + 4, sizeof(short));
			if (tn_len != name_len)
			{
				j += tr_len;
				continue;
			}
			memcpy(temp, block[i] + j + 8, name_len);
			if (strcmp(name, temp) == 0)
			{
				memcpy(&type, block[i] + j + 7, sizeof(char));
				if (type == dentry->file_type)return i * BLOCK_SIZE + j;
			}
			j += tr_len;
		}
		i++;
		j = 0;
	}
	return BLOCK_SIZE * N_BLOCKS + 1;

}
STATE dentry_delete(ext2_dir_entry* dentry, ext2_dir_entry* Pdentry)
{/*此函数删除父目录Pdentry文件中的dentry项*/
	unsigned char* block[N_BLOCKS] = { NULL };
	unsigned int offset, i, num, off;
	unsigned short rec_len, z = 0, tr_len, for_rec_len = 0;
	unsigned char name_len, tn_len;
	char name1[NAME_LEN] = { 0x0 }, name2[NAME_LEN] = { 0x0 },
		namez[NAME_LEN] = { 0x0 };
	ext2_inode* Pinode_info = inode_read(Pdentry->inode);
	ext2_inode* Cinode_info;
	dir_file_read(block, Pinode_info);
	offset = find_dentry(dentry, block, 0);
	if (offset >= BLOCK_SIZE * N_BLOCKS + 1)
	{
		return WRONGPATH;
	}
	num = offset / BLOCK_SIZE;
	off = offset % BLOCK_SIZE;
	memcpy(&rec_len, block[num] + off + 4, sizeof(short));
	memcpy(&name_len, block[num] + off + 6, sizeof(char));
	memcpy(name2, dentry->name, name_len);
	memcpy(dentry, block[num] + off, (((8 + name_len) - 1) / 4 + 1) * 4);

	if (off == 0)
	{
		block_delete(Pinode_info, num);
		i = num + 1;
		while (i < Pinode_info->i_block)
		{
			Pinode_info->i_block[i - 1] = Pinode_info->i_block[i];
			block[i - 1] = block[i];
			i++;
			Pinode_info->i_size -= ((8 + dentry->name_len - 1) / 4 + 1) * 4;
		}
		inode_write(Pinode_info->inode, Pinode_info);
	}
	else
	{
		i = 0;
		while (i < off)
		{
			memcpy(&tr_len, block[num] + i + 4, sizeof(short));
			i += tr_len;
			for_rec_len = tr_len;
		}
		i -= for_rec_len;
		for_rec_len += rec_len;
		memcpy(block[num] + i + 4, &for_rec_len, sizeof(short));
		memcpy(block[num] + off + 4, &z, sizeof(short));

		block_write(block[num], Pinode_info->i_block[num]);
		if (offset + rec_len >= Pinode_info->i_size)Pinode_info->i_size -= (
			(8 + dentry->name_len - 1) / 4 + 1) * 4;
		inode_write(Pinode_info->inode, Pinode_info);
	}
	return OK;
}
STATE dentry_read(ext2_dir_entry* dentry, ext2_dir_entry* Pdentry)
{/*将父目录文件Pdentry中的dentry项读出来，写入dentry中*/
	ext2_inode* inode;
	unsigned short rec_len = ((dentry->name_len + 8 - 1) / 4 + 1) * 4;
	unsigned char* block[N_BLOCKS] = { 0 };
	unsigned int offset;
	inode = inode_read(Pdentry->inode);
	dir_file_read(block, inode);
	offset = find_dentry(dentry, block, 0);
	if (offset >= BLOCK_SIZE * N_BLOCKS + 1)
	{
		return WRONGPATH;
	}

	memcpy(dentry, block[offset / BLOCK_SIZE] + offset % BLOCK_SIZE, rec_len);
	return OK;
}
ext2_dir_entry* director_resolve(char path[], int length)
{/*此函数按照路径path进行查找，返回path最终文件的目录项；
 其输入的path可以是绝对路径或者是相对于当前路径的相对路径*/
	ext2_dir_entry* Pdentry, * dentry;
	int i, j;
	unsigned short name_len;
	char name[NAME_LEN];
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	i = 0;
	if (length == 0)return current_path.current;
	if (path[0] == '/')/*绝对路径开始找起*/
	{
		Pdentry = current_path.root;
		i++;
	}
	else if (path[0] == '.')/*相对路径开始找起*/
	{
		if (path[0] == '.' && (path[1] == '/' || strlen(path) == 1))
		{
			Pdentry = current_path.current;
			i = 2;
		}
		else if (path[0] == '.' && path[1] == '.')
		{
			if (current_path.num == 1)
				Pdentry = current_path.current;
			else
			{
				Pdentry = current_path.path[current_path.num - 2];
			}
			i = 2;
		}
		else return WRONGPATH;
	}
	else Pdentry = current_path.current;
	//memcpy(&dentry,Pdentry,sizeof(dentry));

	while (i < length)
	{
		name_len = 0;
		memcpy(name, z_buf, NAME_LEN);
		while (path[i] != '/' && i < length)
		{
			name[name_len] = path[i];
			name_len++;
			i++;
		}
		//i++;
		if (name_len > 0)
		{
			dentry = (ext2_dir_entry*)malloc(sizeof(ext2_dir_entry));
			memset(dentry, 0, sizeof(dentry));
			memcpy(dentry->name, name, name_len);
			dentry->name_len = name_len;
			dentry->rec_len = ((8 + name_len - 1) / 4 + 1) * 4;
			dentry->file_type = D;
			if (dentry_read(dentry, Pdentry) != OK)
			{
				dentry->file_type = F;
				if (dentry_read(dentry, Pdentry) != OK)return WRONGPATH;
			}
			if (Pdentry != current_path.current &&
				Pdentry != current_path.root)
			{
				if (current_path.num >= 2 &&
					Pdentry != current_path.path[current_path.num - 2])free(Pdentry);
				else if (current_path.num < 2)free(Pdentry);
			}
			Pdentry = dentry;
			if (i >= length)return Pdentry;
		}
		else i++;
	}
	return Pdentry;
}

