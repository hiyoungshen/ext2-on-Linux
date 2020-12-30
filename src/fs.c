#include<stdio.h>
#include<stdlib.h>
#include<malloc.h>
#include<string.h>
#include"fs.h"
#include"ext2.h"

/*全局变量*/
DWORD TotalSectors = 0;
WORD Bytes_Per_Sector = 0;
BYTE Sectors_Per_Cluster = 0;
WORD Reserved_Sector = 0;
DWORD Sectors_Per_FAT = 0;
UINT Position_Of_RootDir = 1;
UINT Position_Of_FAT1 = 0;
UINT Position_Of_FAT2 = 0;

CHAR VDiskPath[256];
CHAR cur_path[256];
//FILE *fp;

/*此函数用于创建磁盘文件（虚拟文件系统）*/
STATE CreateVDisk(DWORD size, char* path)
{
	ext2_size = size;
	ext2_fp = fopen(path, "wb+");
	if (ext2_fp == NULL)return SYSERROR;
	if (ext2_fp)
	{
		fseek(ext2_fp, ext2_size - 1, 0);
		fwrite("#", 1, 1, ext2_fp);
		fclose(ext2_fp);
		return OK;
	}
	return SYSERROR;
}
/*格式化虚拟磁盘*/
STATE FormatVDisk(PCHAR path, PCHAR volumeLabel)
{
	STATE sb_write();
	STATE gdesc_write();
	STATE bitmap_write();
	STATE root_write();
	ext2_dir_entry dentry, Rdentry;/*根目录项*/
	int flag = OK;
	unsigned int i, j;
	unsigned char buf[BLOCK_SIZE] = { 0x0 }, z_buf[BLOCK_SIZE] = { 0X0 };
	ext2_fp = fopen(path, "rb+");
	if (NULL == ext2_fp)return VDISKERROR;
	//super block
	memcpy(&sb, buf, BLOCK_SIZE);
	sb.s_blocks_count = ext2_size / BLOCK_SIZE;
	sb.s_blocks_per_group = BLOCKS_PER_GROUP;
	sb.s_inodes_count = ext2_size / CAPACITY_PER_INODE;/*此处平均1K的存储区分配一个inode*/
	sb.s_inode_size = INODE_SIZE;
	sb.s_first_data_block = FIRST_DATA_BLOCK;
	sb.s_r_blocks_count = R_BLOCKS_COUNT;
	sb.s_free_inodes_count = sb.s_inodes_count;
	groups_counts = (sb.s_blocks_count - sb.s_first_data_block - 1) / sb.s_blocks_per_group + 1;/*组数fs.h*/
	gdesc_blocks = (groups_counts - 1) / (BLOCK_SIZE / GDESC_SIZE) + 1;/*组描述符块数fs.h*/
	sb.s_inodes_per_group = sb.s_inodes_count / groups_counts;
	inode_blocks_per_group = (sb.s_inodes_per_group - 1) / (BLOCK_SIZE / INODE_SIZE) + 1;/*每组中的inode块数fs.h*/
	sb.s_free_blocks_count = sb.s_blocks_count - FIRST_DATA_BLOCK -
		(1 + gdesc_blocks + 2 + inode_blocks_per_group) * groups_counts;
	sb.s_magic = 0xef53;
	sb.s_block_group_nr = FIRST_DATA_BLOCK + sb.s_blocks_per_group;
	//sb.s_volume_name="vfs_ext2";
	sb.s_prealloc_blocks = PREALLOC_BLOCKS;
	sb.s_prealloc_dir_blocks = PREALLOC_DIR_BLOCKS;
	/***gdesc_info****/
	gdesc = (ext2_group_desc*)calloc(groups_counts, GDESC_SIZE);
	if (!gdesc)return SYSERROR;
	for (i = 0; i < groups_counts; i++)
	{
		memcpy(gdesc + i, buf, sizeof(ext2_group_desc));
	}
	j = sb.s_free_blocks_count;
	for (i = 0; i < groups_counts; i++)
	{
		gdesc[i].bg_block_bitmap = i * sb.s_blocks_per_group + 1 + gdesc_blocks + 1;
		gdesc[i].bg_inode_bitmap = gdesc[i].bg_block_bitmap + 1;
		gdesc[i].bg_inode_table = gdesc[i].bg_inode_bitmap + 1;
		gdesc[i].bg_free_blocks_count =
			sb.s_blocks_per_group - 1 - gdesc_blocks - inode_blocks_per_group - 2;
		if (j < gdesc[i].bg_free_blocks_count)
			gdesc[i].bg_free_blocks_count = j;
		gdesc[i].bg_free_inodes_count = sb.s_inodes_per_group;
		gdesc[i].bg_used_dirs_count = 0x0;
		gdesc[i].bg_pad = 0;
		j -= gdesc[i].bg_free_blocks_count;

	}
	/*********create root***********/
	root_inode = iget(1);
	if (!root_inode)return SYSERROR;
	sb.s_first_ino = 1;
	sb.s_free_inodes_count--;
	gdesc[0].bg_free_inodes_count--;
	gdesc[0].bg_used_dirs_count++;
	root_inode->inode = sb.s_first_ino;
	root_inode->i_mode = 0x41B0;
	root_inode->i_uid = 0x0;
	root_inode->i_size = 0x0;
	root_inode->i_blocks = 0x0;

	/*****write to the ext2_fp******/
	flag = sb_write();
	if (flag != OK)
	{
		fclose(ext2_fp);
		return flag;
	}
	flag = gdesc_write();
	if (flag != OK)
	{
		fclose(ext2_fp);
		return flag;
	}
	flag = bitmap_write();
	if (flag != OK)
	{
		fclose(ext2_fp);
		return flag;
	}
	flag = root_write();

	/******初始化根目录******/
	memcpy(&Rdentry, z_buf, sizeof(ext2_dir_entry));
	Rdentry.file_type = D;
	Rdentry.inode = sb.s_first_ino;
	memcpy(&dentry, z_buf, sizeof(ext2_dir_entry));
	dentry.file_type = D;
	dentry.inode = sb.s_first_ino;
	memcpy(dentry.name, ".", 1);
	dentry.name_len = 1;
	dentry.rec_len = BLOCK_SIZE;
	dentry_add(&dentry, &Rdentry);
	memcpy(&dentry, z_buf, sizeof(ext2_dir_entry));
	dentry.file_type = D;
	dentry.inode = sb.s_first_ino;
	memcpy(dentry.name, "..", 2);
	dentry.name_len = 2;
	dentry.rec_len = 12;
	dentry_add(&dentry, &Rdentry);
	CloseVDisk();
	return flag;
}
/*加载虚拟磁盘*/
STATE LoadVDisk(PCHAR path)
{
	STATE sb_read();
	STATE gdesc_read();
	STATE root_read();
	ext2_dir_entry root_dentry;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	int flag = OK;
	ext2_fp = fopen(path, "rb+");
	if (ext2_fp == 0)
		return 0;
	flag = sb_read();
	if (flag != OK)
	{
		fclose(ext2_fp);
		return flag;
	}
	groups_counts = (sb.s_blocks_count - sb.s_first_data_block - 1) / sb.s_blocks_per_group + 1;/*组数fs.h*/
	gdesc_blocks = (groups_counts - 1) / (BLOCK_SIZE / GDESC_SIZE) + 1;/*组描述符块数fs.h*/
	inode_blocks_per_group = (sb.s_inodes_per_group - 1) / (BLOCK_SIZE / INODE_SIZE) + 1;/*每组中的inode块数fs.h*/
	ext2_size = sb.s_blocks_count * BLOCK_SIZE + 1024;
	flag = gdesc_read();
	if (flag != OK)
	{
		fclose(ext2_fp);
		return flag;
	}
	flag = root_read();
	memcpy(&root_dentry, z_buf, sizeof(ext2_dir_entry));
	root_dentry.inode = sb.s_first_ino;
	root_dentry.file_type = D;
	current_path.root = (ext2_dir_entry*)malloc(sizeof(ext2_dir_entry));
	memcpy(current_path.root, &root_dentry, sizeof(ext2_dir_entry));
	current_path.current = current_path.root;
	current_path.num++;
	current_path.path[current_path.num - 1] = current_path.current;
	strcpy(cur_path, "/");
	return flag;
}
/*关闭虚拟磁盘*/
STATE CloseVDisk()
{
	STATE sb_write();
	STATE gdesc_write();
	STATE root_write();
	int flag = OK;
	flag = sb_write();
	if (flag != OK)return flag;
	flag = gdesc_write();
	if (flag != OK)return flag;
	fclose(ext2_fp);
	return flag;
}
/*获得虚拟磁盘总大小*/
STATE GetVDiskSize(PDWORD totalsize)
{
	*totalsize = sb.s_blocks_count * BLOCK_SIZE + 1024;
}
/*获得虚拟磁盘剩余大小*/
STATE GetVDiskFreeSpace(PDWORD left)
{
	*left = sb.s_free_blocks_count * BLOCK_SIZE;
}

STATE GetCurrentPath(PCHAR path)
{
	strcpy(path, cur_path);
	return OK;
}
STATE ListAll(PCHAR dirname, DArray* darray)
{/*获得目录下所有子目录、文件列表*/
	ext2_dir_entry dentry, * Pdentry;
	ext2_inode* inode;
	DArrayElem element;
	unsigned char* block[N_BLOCKS] = { 0x0 };
	int i = 0, j;
	unsigned short rec_len;
	if (strlen(dirname) == 0)
	{
		Pdentry = current_path.current;
		strcpy(dirname, cur_path);
	}
	else
	{
		Pdentry = director_resolve(dirname, strlen(dirname));
		if (Pdentry == WRONGPATH)return WRONGPATH;
	}
	inode = inode_read(Pdentry->inode);
	if (inode == SYSERROR)return SYSERROR;
	if (dir_file_read(block, inode) != OK)return SYSERROR;
	while (block[i] != 0)
	{
		if (i == 0)
		{
			j = 12;
			memcpy(&rec_len, block[i] + j + 4, sizeof(short));
			j += rec_len;
		}
		else j = 0;
		while (j < BLOCK_SIZE)
		{
			memset(&dentry, 0, sizeof(dentry));
			memset(&element, 0, sizeof(element));
			memcpy(&dentry.name_len, block[i] + j + 6, sizeof(short));
			dentry.rec_len = ((8 + dentry.name_len - 1) / 4 + 1) * 4;
			memcpy(&dentry, block[i] + j, dentry.rec_len);
			strcpy(element.fullpath, dirname);
			strcat(element.fullpath, "/");
			strcat(&element.fullpath, dentry.name);
			strcat(element.name, dentry.name);
			element.tag = dentry.file_type;
			AddElement(darray, element);
			j += dentry.rec_len;
		}
		i++;
	}
	return OK;
}

STATE CreateDir(PCHAR dirname)
{/*在当前目录下创建名为dirname的子目录*/
	ext2_inode* inode;
	ext2_dir_entry dentry, Cdentry, pdentry, * temp;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	int i, flag;
	unsigned char name_len;
	char name[256] = { 0x0 }, path[256];

	strcpy(path, dirname);
	i = strlen(dirname) - 1;
	while (dirname[i] != '/' && i >= 0)i--;
	i += 1;
	name_len = strlen(dirname) - i;
	path[i] = 0;
	strcpy(name, dirname + i);
	if (strlen(path) == 0)memcpy(&pdentry, current_path.current, sizeof(pdentry));
	else
	{
		temp = director_resolve(path, strlen(path));
		if (temp == WRONGPATH)return WRONGPATH;
		else memcpy(&pdentry, temp, sizeof(pdentry));
	}
	if (pdentry.file_type != D)return WRONGPATH;
	memcpy(&dentry, z_buf, sizeof(ext2_dir_entry));
	dentry.name_len = strlen(name);
	memcpy(dentry.name, name, dentry.name_len);
	dentry.file_type = 2;
	if (dentry_read(&dentry, &pdentry) == OK)return NAMEEXIST;
	dentry.file_type = 2;
	dentry.rec_len = ((8 + dentry.name_len - 1) / 4 + 1) * 4;
	dentry.inode = inode_alloc(&dentry, &pdentry);
	if (dentry.inode == 0)return SYSERROR;
	if (dentry_add(&dentry, &pdentry) != OK)
	{
		inode_delete(&dentry);
		return SYSERROR;
	}
	gdesc[dentry.inode / sb.s_inodes_per_group].bg_used_dirs_count++;
	/*添加两个子目录*/
	memcpy(&Cdentry, z_buf, sizeof(ext2_dir_entry));
	Cdentry.file_type = D;
	Cdentry.inode = dentry.inode;
	Cdentry.name_len = 1;
	Cdentry.rec_len = 1024;
	memcpy(Cdentry.name, ".", Cdentry.name_len);
	flag = dentry_add(&Cdentry, &dentry);
	if (flag != OK)
	{
		inode_delete(&Cdentry);
		dentry_delete(&dentry, &pdentry);
		gdesc[dentry.inode / sb.s_inodes_per_group].bg_used_dirs_count--;
		return flag;
	}
	memcpy(&Cdentry, z_buf, sizeof(ext2_dir_entry));
	Cdentry.file_type = D;
	Cdentry.inode = current_path.current->inode;
	Cdentry.name_len = 2;
	Cdentry.rec_len = ((8 + Cdentry.name_len - 1) / 4 + 1) * 4;;
	memcpy(Cdentry.name, "..", Cdentry.name_len);
	flag = dentry_add(&Cdentry, &dentry);
	if (flag != OK)
	{
		inode_delete(&Cdentry);
		dentry_delete(&dentry, &pdentry);
		gdesc[dentry.inode / sb.s_inodes_per_group].bg_used_dirs_count--;
		return flag;
	}
	return OK;
}

STATE DeleteDir(PCHAR dirname)
{/*在当前目录下删除名为dirname的子目录*/
	ext2_dir_entry dentry, pdentry, * temp;
	ext2_inode* inode;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	unsigned int group;
	int i, flag;
	char path[PATH_LEN];
	temp = director_resolve(dirname, strlen(dirname));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));
	if (dentry.file_type != D)return WRONGPATH;
	strcpy(path, dirname);
	i = strlen(path) - 1;
	if (path[i] == '/')while (path[i] == '/' && i >= 0)i--;
	if (i < 0)memcpy(&pdentry, current_path.current, sizeof(pdentry));
	else
	{
		while (path[i] != '/' && i >= 0)i--;
		if (i < 0)memcpy(&pdentry, current_path.current, sizeof(pdentry));
		else
		{
			path[i] = 0;
			temp = director_resolve(path, strlen(path));
			if (temp == WRONGPATH)return WRONGPATH;
			memcpy(&pdentry, temp, sizeof(pdentry));
		}
	}
	flag = dentry_read(&dentry, &pdentry);
	if (flag != OK)return flag;
	inode = inode_read(dentry.inode);
	if (inode == SYSERROR)return SYSERROR;
	group = inode->inode / sb.s_inodes_per_group;
	while (inode->i_size != 0)
	{
		block_delete(inode, 0);
	}
	if (inode_delete(&dentry) != OK)return SYSERROR;
	if (dentry_delete(&dentry, &pdentry) != OK)return SYSERROR;
	gdesc[group].bg_used_dirs_count--;
	return OK;
}
STATE OpenDir(PCHAR dirname)
{
	ext2_dir_entry* dentry, * Pdentry;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	unsigned int i, length = strlen(dirname);
	char name[NAME_LEN] = { 0x0 };
	unsigned short rec_len;
	unsigned char name_len;
	i = 0;
	if (strlen(dirname) == 0)return OK;
	if (dirname[0] == '/')/*绝对路径开始找起*/
	{
		Pdentry = current_path.root;
		current_path.num = 1;
		cur_path[1] = 0;
		i++;
	}
	else if (dirname[0] == '.')/*相对路径开始找起*/
	{
		if (dirname[0] == '.' && (dirname[1] == '/' || strlen(dirname) == 1))
		{
			Pdentry = current_path.current;
			i = 2;
		}
		else if (dirname[0] == '.' && dirname[1] == '.')
		{
			if (current_path.num == 1)
			{
				Pdentry = current_path.current;
				i = 2;
			}
			else if (current_path.num > 1)
			{
				Pdentry = current_path.path[current_path.num - 1 - 1];
				i = strlen(cur_path) - 1;
				if (cur_path[i] == '/')i--;
				if (i > 0)while (cur_path[i] != '/')i--;
				cur_path[i + 1] = 0;
				current_path.num--;
				current_path.current = current_path.path[current_path.num - 1];
				i = 2;
			}
		}
		else return WRONGPATH;
	}
	else Pdentry = current_path.current;
	//i=0;
	while (i < length)
	{
		name_len = 0;
		memset(name, 0, sizeof(name));
		while (dirname[i] != '/' && i < length)
		{
			name[name_len] = dirname[i];
			name_len++;
			i++;
		}
		//i++;
		if (name_len > 0)
		{
			dentry = (ext2_dir_entry*)malloc(sizeof(ext2_dir_entry));
			memset(dentry, 0, sizeof(dentry));
			strcpy(dentry->name, name);
			dentry->name_len = name_len;
			dentry->rec_len = ((8 + name_len - 1) / 4 + 1) * 4;
			dentry->file_type = D;
			if (dentry_read(dentry, Pdentry) != OK)
			{
				dentry->file_type = F;
				if (dentry_read(dentry, Pdentry) != OK)return WRONGPATH;
			}
			Pdentry = dentry;
			current_path.path[current_path.num] = dentry;
			current_path.num++;
			current_path.current = current_path.path[current_path.num - 1];
			strcat(cur_path, current_path.current->name);
			strcat(cur_path, "/");
			if (i >= length)return OK;
		}
		else i++;
	}
	return OK;
}
STATE CopyDir(PCHAR sdirname, PCHAR dpath)
{
	ext2_dir_entry dentry, d_dentry, * temp;
	ext2_inode inode;
	int i, j, flag;
	unsigned short rec_len, real_len;
	unsigned char name_len;
	char s_path[256], d_path[256];
	unsigned char* block[N_BLOCKS] = { 0x0 };
	temp = director_resolve(sdirname, strlen(sdirname));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));
	temp = director_resolve(dpath, strlen(dpath));
	if (temp->file_type != D)return WRONGPATH;
	memcpy(&inode, inode_read(dentry.inode), sizeof(inode));
	dir_file_read(block, &inode);
	strcpy(d_path, dpath);
	if (strlen(d_path) == 0)
	{
		strcpy(d_path, dentry.name);
		strcat(d_path, "-副本");
	}
	else if ((strlen(d_path) == 1 && d_path[1] == '.') || (d_path[0] == '.' && d_path[1] == '/'))
	{
		strcat(d_path, "/");
		strcat(d_path, dentry.name);
		strcat(d_path, "-副本");
	}
	else
	{
		strcat(d_path, "/");
		strcat(d_path, dentry.name);

	}
	CreateDir(d_path);
	strcat(d_path, "/");
	temp = director_resolve(d_path, strlen(d_path));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&d_dentry, temp, sizeof(d_dentry));
	i = 0;
	strcpy(s_path, sdirname);
	strcat(s_path, "/");
	while (block[i] != 0)
	{
		if (i == 0)
		{
			j = 12;
			memcpy(&rec_len, block[i] + j + 4, sizeof(short));
			j += rec_len;
		}
		else j = 0;
		while (j < BLOCK_SIZE)
		{
			memcpy(&rec_len, block[i] + j + 4, sizeof(short));
			memcpy(&name_len, block[i] + j + 6, sizeof(char));
			real_len = ((8 + name_len - 1) / 4 + 1) * 4;
			memset(&dentry, 0, sizeof(dentry));
			memcpy(&dentry, block[i] + j, real_len);
			s_path[strlen(sdirname) + 1] = 0;
			strcat(s_path, dentry.name);
			if (dentry.file_type == D)
			{
				flag = CopyDir(s_path, d_path);
				if (flag != OK)return flag;
			}
			else if (dentry.file_type == F)
			{
				flag = VCopyFile(s_path, d_path);
				if (flag != OK)return flag;
			}
			j += rec_len;
		}
		i++;
	}
	return OK;
}

STATE Move(PCHAR sfilename, PCHAR dpath)
{
	ext2_dir_entry dentry, pdentry, ddentry, * temp;
	char ss[256] = { 0x0 };
	char* t1, * t2;
	temp = director_resolve(sfilename, strlen(sfilename));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));

	temp = director_resolve(dpath, strlen(dpath));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&ddentry, temp, sizeof(ddentry));
	if (ddentry.file_type == F)return WRONGPATH;

	strcpy(ss, sfilename);
	t1 = strstr(ss, dentry.name);
	if (t1 != NULL)t2 = strstr(t1, dentry.name);
	if (t2 == NULL)t2 = t1;
	memset(t2, 0, 1);
	temp = director_resolve(ss, strlen(ss));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&pdentry, temp, sizeof(pdentry));
	if (dentry_read(&dentry, &ddentry) == OK)return NAMEEXIST;
	if (dentry_delete(&dentry, &pdentry) != OK)return SYSERROR;
	dentry_add(&dentry, &ddentry);
	return OK;
}
STATE Rename(PCHAR path, PCHAR newname)
{/*path路径下的文件或子目录重命名为newname*/
	ext2_dir_entry dentry, Pdentry, * pdentry, old, * temp;
	char t_path[256];
	int i;
	temp = director_resolve(path, strlen(path));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));

	strcpy(t_path, path);
	i = strlen(t_path) - 1;
	if (t_path[i] == '/')while (t_path[i] == '/' && i >= 0)i--;
	if (i < 0)pdentry = current_path.current;
	else
	{
		while (t_path[i] != '/' && i >= 0)i--;
		if (i < 0)pdentry = current_path.current;
		else
		{
			t_path[i] = 0;
			pdentry = director_resolve(t_path, strlen(t_path));
		}
	}
	if (pdentry == WRONGPATH)return WRONGPATH;
	memcpy(&Pdentry, pdentry, sizeof(Pdentry));
	memcpy(&old, &dentry, sizeof(old));
	strcpy(dentry.name, newname);
	dentry.name_len = strlen(dentry.name);
	dentry.rec_len = ((8 + dentry.name_len - 1) / 4 + 1) * 4;
	if (dentry_read(&dentry, &Pdentry) == OK)return NAMEEXIST;
	dentry_delete(&old, &Pdentry);
	dentry_add(&dentry, &Pdentry);
	return OK;
}
STATE IsFile(PCHAR path, PUINT tag)
{
	ext2_dir_entry dentry, * temp;
	temp = director_resolve(path, strlen(path));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));
	if (dentry.file_type == D)*tag = 0;
	else *tag = dentry.file_type;
	return OK;
}
STATE GetParenetDir(PCHAR name, PCHAR parentDir)
{
	ext2_dir_entry* dentry;
	char path[256];
	char* t1, * t2;
	int i, j, s;
	i = 0;
	if (name[0] == '/')i++;
	while (i < strlen(name))
	{
		if (name[i] == '/')
		{
			j = i;
			while (name[i] == '/' && i < strlen(name))i++;
			if (i > strlen(name))
			{
				name[j] = 0;
				break;
			}
			strcpy(name + j, name + i);
			i = j;
		}
		while (i < strlen(name) && name[i] != '/')i++;
		i++;
	}
	if (strlen(name) == 0)s = 0;
	else if (strlen(name) == 1 && name[0] == '.')s = 0;
	else if (strlen(name) == 2 && name[0] == '.' && name[1] == '/')s = 0;
	else if (strlen(name) > 2 && name[0] == '.' && name[1] == '/')s = 1;
	else if (strlen(name) == 2 && name[0] == '.' && name[1] == '.')s = 2;
	else if (strlen(name) == 3 && name[0] == '.' && name[1] == '.' && name[2] == '/')s = 2;
	else if (strlen(name) > 3 && name[0] == '.' && name[1] == '.' && name[2] == '/')s = 3;
	else if (strlen(name) == 1 && name[0] == '/')s = 4;
	else if (strlen(name) > 1 && name[0] == '/')s = 5;
	else s = 9;
	switch (s)
	{
	case 0:
		strcpy(path, cur_path);//name是当前路径
		break;
	case 1:
		strcpy(path, cur_path);//当前路径的子目录或文件
		strcat(path, "/");
		strcat(path, name + 2);
		break;
	case 2:
		strcpy(path, cur_path);//name为当前目录的父目录
		break;
	case 3:
		strcpy(path, cur_path);//name为当前父路径的子目录或文件
		strcat(path, "/");
		strcat(path, name + 3);
		break;
	case 4:
		strcpy(path, "/");//name为根目录
		strcpy(parentDir, path);
		break;
	case 5:
		strcpy(path, name);
		break;
	default:
		return WRONGPATH;
	}
	strcpy(parentDir, path);
	i = strlen(path);
	i--;
	if (path[i] == '/')while (path[i] == '/')i--;
	if (i < 0)return OK;
	while (path[i] != '/')i--;
	parentDir[i] = 0;
	return OK;
}

/*文件相关操作*/
STATE VCreateFile(PCHAR filename)
{//在当前路径下创建一个文件
	ext2_dir_entry dentry, pdentry, * temp;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };	 //申请目录项
	int i;
	unsigned char name_len;
	char name[256] = { 0x0 }, path[256];

	strcpy(path, filename);
	i = strlen(filename) - 1;
	while (filename[i] == '/' && i >= 0)i--;
	if (i >= 0)
	{
		while (filename[i] != '/' && i >= 0)i--;
	}
	if (i >= -1)
	{
		i += 1;
		path[i] = 0;
		strcpy(name, filename + i);
		name_len = strlen(name);
	}
	temp = director_resolve(path, strlen(path));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&pdentry, temp, sizeof(pdentry));
	if (pdentry.file_type == F)return WRONGPATH;

	memcpy(&dentry, z_buf, sizeof(ext2_dir_entry));
	dentry.name_len = strlen(name);
	memcpy(dentry.name, name, dentry.name_len);
	dentry.file_type = F;
	if (dentry_read(&dentry, &pdentry) == OK)return NAMEEXIST;
	dentry.rec_len = ((8 + dentry.name_len - 1) / 4 + 1) * 4; //目录项长度为的倍数
	dentry.inode = inode_alloc(&dentry, &pdentry);  //申请inode节点
	if (dentry.inode == 0)return SYSERROR;
	if (dentry_add(&dentry, &pdentry) != OK)
	{
		inode_delete(&dentry);
		return SYSERROR;
	}
	return OK;
}
STATE VDeleteFile(PCHAR filename)
{//在当前目录下删除指定的文件
	ext2_dir_entry dentry, pdentry, * temp;
	ext2_inode inode, * ino;
	int flag, i;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	char path[PATH_LEN];
	strcpy(path, filename);
	temp = director_resolve(filename, strlen(filename));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&dentry, temp, sizeof(dentry));
	if (dentry.file_type != F)return WRONGPATH;
	i = strlen(path) - 1;
	if (path[i] == '/')while (path[i] == '/' && i >= 0)i--;
	if (i < 0)memcpy(&pdentry, current_path.current, sizeof(pdentry));
	else
	{
		while (path[i] != '/' && i >= 0)i--;
		if (i < 0)memcpy(&pdentry, current_path.current, sizeof(pdentry));
		else
		{
			path[i] = 0;
			temp = director_resolve(path, strlen(path));
			if (temp == WRONGPATH)return WRONGPATH;
			memcpy(&pdentry, temp, sizeof(pdentry));
		}
	}
	flag = dentry_read(&dentry, &pdentry);
	if (flag != OK)return flag;
	ino = inode_read(dentry.inode);
	if (&inode == 0)return SYSERROR;
	memcpy(&inode, ino, sizeof(inode));
	while (inode.i_size != 0)
	{
		flag = block_delete(&inode, 0);
		if (flag != OK)return flag;
	}
	inode_delete(&dentry);
	dentry_delete(&dentry, &pdentry);
	return OK;
}
STATE OpenFile(PCHAR filename, UINT mode, PFile pfile)
{
	ext2_dir_entry dentry;
	ext2_inode* inode;
	unsigned char z_buf[BLOCK_SIZE] = { 0x0 };
	memcpy(&dentry, z_buf, sizeof(ext2_dir_entry));
	dentry.name_len = strlen(filename);
	dentry.file_type = F;
	memcpy(dentry.name, filename, dentry.name_len);
	memcpy(&dentry, director_resolve(filename, strlen(filename)), sizeof(dentry));
	inode = inode_read(dentry.inode);
	strcpy(pfile->parent, cur_path);
	strcpy(pfile->name, filename);
	pfile->start = inode->i_block[0] * 1024;
	pfile->off = 0;
	pfile->size = inode->i_size;
	pfile->inode = inode->inode;
	return OK;
}
STATE CloseFile(PFile pfile)
{
	//free(pfile);
	return OK;
}


STATE WriteFile(BYTE buf[], DWORD length, PFile pfile)
{
	ext2_inode inode;
	unsigned long i = 0;
	unsigned int t_no, t_off;
	unsigned char b_buf[BLOCK_SIZE];
	inode.inode = pfile->inode;
	memcpy(&inode, inode_read(pfile->inode), sizeof(inode));
	while (i < length)
	{
		t_no = pfile->off / BLOCK_SIZE;//相对块号
		t_off = pfile->off % BLOCK_SIZE;//块内偏移量
		if (t_no < inode.i_blocks)
		{
			//需要完善，计算绝对块号，t_no=绝对块号
			block_read(b_buf, inode.i_block[t_no]);
			if ((length - i) >= (BLOCK_SIZE - t_off))
			{
				memcpy(b_buf + t_off, buf + i, BLOCK_SIZE - t_off);
				pfile->off += (BLOCK_SIZE - t_off);
				i += (BLOCK_SIZE - t_off);
				inode.i_size = pfile->off;
			}
			else
			{
				memcpy(b_buf + t_off, buf + i, length - i);
				pfile->off += (length - i);
				inode.i_size = pfile->off;
				i = length;
			}
			block_write(b_buf, inode.i_block[t_no]);
		}
		else
		{
			block_alloc(&inode);
			t_no = inode.i_blocks - 1;
			memset(b_buf, 0, BLOCK_SIZE);
			if ((length - i) >= BLOCK_SIZE)
			{
				memcpy(b_buf, buf + i, BLOCK_SIZE);
				pfile->off += BLOCK_SIZE;
				inode.i_size = pfile->off;
				i += BLOCK_SIZE;
			}
			else
			{
				memcpy(b_buf, buf + i, length - i);
				pfile->off += (length - i);
				inode.i_size = pfile->off;
				i = length;
			}
			//需要完善
			block_write(b_buf, inode.i_block[t_no]);
		}
	}
	inode_write(inode.inode, &inode);
	return OK;
}

STATE VCopyFile(PCHAR sfilename, PCHAR dpath)
{
	unsigned char b_buf[BLOCK_SIZE];
	ext2_dir_entry s_dentry, * temp;
	File s_file, d_file;
	ext2_inode s_inode;
	char dfilename[256] = { 0x0 };
	int i, flag;
	unsigned long size = 0;
	temp = director_resolve(sfilename, strlen(sfilename));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&s_dentry, temp, sizeof(s_dentry));
	if (s_dentry.file_type != F)return WRONGPATH;
	memcpy(&s_inode, inode_read(s_dentry.inode), sizeof(s_inode));

	strcpy(dfilename, dpath);
	strcat(dfilename, "/");
	strcat(dfilename, s_dentry.name);
	flag = VCreateFile(dfilename);
	if (flag != OK)return flag;

	flag = OpenFile(sfilename, 0, &s_file);
	if (flag != OK)return flag;
	flag = OpenFile(dfilename, 0, &d_file);
	if (flag != OK)return flag;

	for (i = 0; i < s_inode.i_blocks; i++)
	{
		size = 0;
		memset(b_buf, 0, BLOCK_SIZE);
		if (ReadFile(b_buf, BLOCK_SIZE, &size, &s_file) != OK)return SYSERROR;
		if (WriteFile(b_buf, size, &d_file) != OK)return SYSERROR;
	}
	CloseFile(&s_file);
	CloseFile(&d_file);
	return OK;
}
STATE GetProperty(PCHAR filename, Properties* properties)
{
	ext2_dir_entry dentry, pdentry, * temp;
	ext2_inode* inode;
	int i, j;
	unsigned char temp_type;
	unsigned short tr_len;
	unsigned char* block[N_BLOCKS] = { 0x0 };
	char path[256], name[NAME_LEN];
	strcpy(path, filename);

	temp = director_resolve(path, strlen(path));
	if (temp == WRONGPATH)return WRONGPATH;
	memcpy(&pdentry, temp, sizeof(dentry));

	inode = inode_read(pdentry.inode);
	strcpy(properties->createTime, &inode->i_ctime);
	strcpy(properties->lastModifiedTime, &inode->i_mtime);
	strcpy(properties->location, filename);
	strcpy(properties->name, pdentry.name);
	memset(&properties->share, 0, sizeof(properties->share));
	properties->size = inode->i_size;
	if (pdentry.file_type == D)properties->type = 0x10;
	else if (pdentry.file_type == F)
	{
		properties->type = 1;
		return OK;
	}
	/*得到子目录以及文件个数*/
	dir_file_read(block, inode);
	i = 0;
	while (block[i] != 0)
	{
		if (i == 0)
		{
			j = 12;
			memcpy(&tr_len, block[i] + j + 4, sizeof(short));
			j += tr_len;
		}
		else j = 0;
		while (j < BLOCK_SIZE)
		{
			memcpy(&temp_type, block[i] + j + 7, sizeof(char));
			memcpy(&tr_len, block[i] + j + 4, sizeof(short));
			if (temp_type == F)properties->share.contain[0]++;
			else if (temp_type == D)properties->share.contain[1]++;
			j += tr_len;
		}
		i++;
	}
	return OK;
}
STATE GetFileLength(PCHAR filename, PDWORD length)
{
	ext2_dir_entry* dentry;
	ext2_inode* inode;
	dentry = director_resolve(filename, strlen(filename));
	if (dentry == WRONGPATH)return WRONGPATH;
	inode = inode_read(dentry->inode);
	*length = inode->i_size;
	return OK;
}
STATE CopyFileIn(PCHAR sfilename, PCHAR dfilename)
{
	FILE* fp;
	File d_file;
	char c;
	char buf[BLOCK_SIZE], dpath[PATH_LEN], name[NAME_LEN];
	unsigned int size, count, i, file_size;
	int tag, flag, j;
	flag = IsFile(dfilename, &tag);
	if (flag == OK)
	{
		if (tag == F)flag = OpenFile(dfilename, 0, &d_file);
		else
		{
			strcpy(dpath, dfilename);
			strcat(dpath, "/");
			j = strlen(sfilename) - 1;
			while (sfilename[j] != '/' && sfilename[j] != '\\' && j >= 0)j--;
			strcpy(name, sfilename + j + 1);
			strcat(dpath, name);
			flag = VCreateFile(dpath);
			if (flag != OK)return flag;
			flag = OpenFile(dpath, 0, &d_file);
		}
	}
	else if (flag == WRONGPATH)
	{
		flag = VCreateFile(dfilename);
		if (flag != OK)return flag;
		flag = OpenFile(dfilename, 0, &d_file);
	}
	if (flag != OK)return flag;
	fp = fopen(sfilename, "rb");
	if (fp == NULL)return WRONGPATH;
	fseek(fp, 0, 0);
	i = ftell(fp);
	fseek(fp, 0, 2);
	size = ftell(fp);
	file_size = size - i;
	count = 0;
	fseek(fp, 0, 0);
	while (count < file_size / BLOCK_SIZE)
	{
		fread(buf, BLOCK_SIZE, 1, fp);
		WriteFile(buf, BLOCK_SIZE, &d_file);
	}
	size = 0;
	while (ftell(fp) != file_size)
	{
		fread(&buf[size], 1, 1, fp);
		size++;
	}
	WriteFile(buf, size, &d_file);
	fclose(fp);
	return OK;
}
STATE CopyFileOut(PCHAR sfilename, PCHAR dfilename)
{
	char buf[BLOCK_SIZE];
	unsigned int size = 0;
	int i;
	unsigned int file_len;
	File s_file;
	FILE* fp;
	fp = fopen(dfilename, "wb");
	if (fp == NULL)
	{
		i = strlen(sfilename) - 1;
		while (sfilename[i] != '/' && i >= 0)i--;
		i++;
		strcat(dfilename, sfilename + i);
		fp = fopen(dfilename, "wb+");
		if (fp == NULL)return WRONGPATH;
	}
	OpenFile(sfilename, 0, &s_file);
	GetFileLength(sfilename, &file_len);
	while (file_len > 0)
	{
		ReadFile(buf, BLOCK_SIZE, &size, &s_file);
		fwrite(buf, size, 1, fp);
		file_len -= size;
	}
	fclose(fp);
	return OK;
}


//void main()
//{
//	/*if(CreateVDisk(16*1024*1024))printf("Create success\n");;
//	FormatVDisk(0,0);
//	CloseVDisk();*/
//
//	
//
//	/*ext2_dir_entry dentry,Pdentry;
//	unsigned char buf[sizeof(ext2_dir_entry)]={0x0};
//	if(LoadVDisk(0))printf("load success\n");
//	memcpy(&Pdentry,buf,sizeof(Pdentry));
//	Pdentry.inode=1;
//	memcpy(&dentry,buf,sizeof(dentry));
//	memcpy(dentry.name,"a",1);
//	dentry.name_len=1;
//	dentry_read(&dentry,&Pdentry);
//	if(CloseVDisk())printf("close sucess\n");*/
//
//	/*if(LoadVDisk(0))printf("load success\n");
//	VCreateFile("b");
//	if(CloseVDisk())printf("close sucess\n");*/
//
//	File file;
//	char buf;
//	unsigned int size;
//	char *path="./a/b";
//	if(LoadVDisk(0))printf("load success\n");
//	ListAll(path,0);
//	if(director_resolve(path,strlen(path))==WRONGPATH)
//		printf("不存在路径：%s\n",path);
//	if(CloseVDisk())printf("close sucess\n");
//}

	//while(length>0)
	//{
	//	no=pfile->off/1024;
	//	if(no<0||no>inode->i_blocks)return VDISKERROR;
	//	else if(no>=0 && no<=12)
	//		block=inode->i_block[no];
	//	else if(no<=13+256-1)
	//	{/*一次间接*/
	//		block_read(t_buf,inode->i_block[13]);
	//		no=no-12;
	//		memcpy(&block,t_buf+no*4,sizeof(int));
	//	}
	//	else if(no<=13+256+256*256-1)
	//	{/*二次间接*/
	//		block_read(t_buf,inode->i_block[14]);
	//		no=no-13-256;
	//		memcpy(&i,t_buf+no/256*4,sizeof(int));
	//		block_read(t_buf,i);
	//		memcpy(&i,t_buf+no%256*4,sizeof(int));
	//		block=i;
	//	}
	//	else if(no<=13+256+256*256+256*256*256-1)
	//	{/*三次间接*/
	//		block_read(t_buf,inode->i_block[15]);
	//		no=no-13-256;
	//		memcpy(&i,t_buf+no/256/256*4,sizeof(int));
	//		block_read(t_buf,i);
	//		memcpy(&i,t_buf+no%(256*256)/256*4,sizeof(int));
	//		block_read(t_buf,i);
	//		memcpy(&i,t_buf+no%(256*256)%256*4,sizeof(int));
	//		block=i;
	//	}
	//	else return VDISKERROR;
	//	block_read(t_buf,block);
	//	strcpy(buf+*size,t_buf+pfile->off%BLOCK_SIZE);
	//	*size=*size+(BLOCK_SIZE-pfile->off%BLOCK_SIZE);
	//	length=length-(BLOCK_SIZE-pfile->off%BLOCK_SIZE);
	//	pfile->off=pfile->off+(BLOCK_SIZE-pfile->off%BLOCK_SIZE);
	//}