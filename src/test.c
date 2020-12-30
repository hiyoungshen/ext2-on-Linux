#define fp ext2_fp
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
//
//#include"type.h"
//#include"DArray.h"
#include"ext2.h"

#define M 3
#define N 256

extern DWORD TotalSectors;
extern WORD Bytes_Per_Sector;
extern BYTE Sectors_Per_Cluster;
extern WORD Reserved_Sector;
extern DWORD Sectors_Per_FAT;
extern UINT Position_Of_RootDir;
extern UINT Position_Of_FAT1;
extern UINT Position_Of_FAT2;

extern CHAR VDiskPath[256];
extern CHAR cur_path[256];
//FILE *fp;

CHAR argv[M][N];

void DisplayInfo()//打印虚拟磁盘的详细信息
{
	printf("总扇区数:%d\n", TotalSectors);
	printf("每个扇区的字节数:%d\n", Bytes_Per_Sector);
	printf("每个簇的扇区数:%d\n", Sectors_Per_Cluster);
	printf("保留扇区数:%d\n", Reserved_Sector);
	printf("每个FAT所占的扇区数:%d\n", Sectors_Per_FAT);
	printf("FAT1的起始位置:%d\n", Position_Of_FAT1);
	printf("FAT2的起始位置:%d\n", Position_Of_FAT2);
	printf("根目录的起始位置:%d\n", Position_Of_RootDir);
}

DWORD DiskSize(PCHAR option)//确定虚拟磁盘的空间大小
{
	if (option[strlen(option) - 1] == 'G')
	{
		return (DWORD)(atoi(option) * 1024 * 1024 * 1024);
	}
	else if (option[strlen(option) - 1] == 'M')
	{
		return (DWORD)(atoi(option) * 1024 * 1024);
	}
	else if (option[strlen(option) - 1] == 'k')
	{
		return (DWORD)(atoi(option) * 1024);
	}
	else
	{
		return (DWORD)(atoi(option));
	}
}

void InputCmd()//输入基本的命令
{
	UINT i = 0;
	if (strlen(cur_path) == 0)
	{
		printf("磁盘还未创建，可以使用mkdisk命令创建并格式化磁盘\n");
	}
	else {
		printf("%s>", cur_path);
	}
	memset(argv, 0, sizeof(argv));
	for (i = 0; i < M; i++)
	{
		scanf("%s", argv[i]);
		if (getchar() == 10)
		{
			break;
		}
	}
	printf("\n");
}

void MkdiskCheckup(PCHAR option)//处理虚拟磁盘的初始化工作
{
	DWORD disksize = 0;
	if (strlen(option) < 2)
	{
		printf("参数有误\n");
		return;
	}
	disksize = DiskSize(option);
	if (OK == CreateVDisk(disksize, VDiskPath))
	{
		printf("创建虚拟磁盘完成!\n");
		if (OK == FormatVDisk(VDiskPath, "VHD"))
		{
			printf("格式化虚拟磁盘完成!\n");
		}
	}
}

void DirCheckup(DArray* array)//列出某一路径下所有文件或目录的信息
{
	DArrayElem* e;
	Properties pro;
	DWORD left = 0;

	while ((e = NextElement(array)) != NULL)
	{
		GetProperty(e->fullpath, &pro);
		printf("%s    ", pro.lastModifiedTime);
		if (pro.type == 0x10)
		{
			printf("<DIR> ");
			printf("包含:%3d个文件 %3d个目录", pro.share.contain[0], pro.share.contain[1]);
			printf("%10d ", pro.size);

		}
		else {
			printf("%40d ", pro.size);
		}
		printf("%s\n", pro.name);
	}
	GetVDiskFreeSpace(&left);
	printf("虚拟磁盘当前可用空间为:%d字节\n", left);
}

void DisErrorInfo(STATE state)
{
	if (state == SYSERROR)
	{
		printf("系统错误\n");
	}
	else if (state == VDISKERROR)
	{
		printf("虚拟磁盘错误\n");
	}
	else if (state == INSUFFICIENTSPACE)
	{
		printf("虚拟磁盘空间不足\n");
	}
	else if (state == WRONGPATH)
	{
		printf("路径错误\n");
	}
	else if (state == NAMEEXIST)
	{
		printf("有重名文件或目录\n");
	}
	else if (state == ACCESSDENIED)
	{
		printf("拒绝访问\n");
	}
	else
	{
		printf("未知错误\n");
	}
}

void ListHelpInfo()
{
	printf("mkdisk    创建并格式化一个虚拟磁盘\n");
	printf("dir       列出目录中的内容\n");
	printf("cd        进入目录\n");
	printf("mkdir     创建一个目录\n");
	printf("cpdir     复制目录以及该目录中的所有子目录和文件\n");
	printf("deldir    删除目录以及该目录中的所有子目录和文件\n");
	printf("mkfile    创建一个空文件\n");
	printf("cpfile    复制文件\n");
	printf("delfile   删除文件\n");
	printf("mv        移动一个目录或文件\n");
	printf("ren       为目录或文件重命名\n");
	printf("cpfilein  将此虚拟文件系统中的文件复制到外部文件系统中\n");
	printf("cpfileout 将外部文件系统中的文件复制到此虚拟文件系统中\n");
	printf("q         退出虚拟文件系统\n");
	printf("注：以上命令均区分大小写\n");
}

int main()
{
	DArray* array = NULL;
	PCHAR cmd = NULL;
	PCHAR option = NULL;
	CHAR temp[256] = { 0 };
	UINT tag = 0;
	STATE state;

	//fp=ext2_fp;
	printf("虚拟磁盘地址\n");
	gets(VDiskPath);
	while (!LoadVDisk(VDiskPath))
	{
		InputCmd();
		cmd = argv[0];
		if (!strcmp(cmd, "mkdisk"))
		{
			option = argv[1];
			strcpy(VDiskPath, argv[2]);
			if (strlen(VDiskPath) == 0)
				printf("缺少参数\n\n");
			else MkdiskCheckup(option);
		}
	}
	//DisplayInfo();
	while (TRUE)
	{
		InputCmd();
		cmd = argv[0];
		if (!strcmp(cmd, "mkdisk"))
		{
			option = argv[1];
			MkdiskCheckup(option);
			LoadVDisk(VDiskPath);
			continue;
		}
		else if ((!strcmp(cmd, "mkdir")))//创建目录
		{
			if (strlen(argv[1]) != 0)
			{
				state = CreateDir(argv[1]);
				if (state == OK)
				{
					printf("创建目录成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "deldir")))//删除目录
		{
			if (strlen(argv[1]) != 0)
			{
				state = DeleteDir(argv[1]);
				if (state == OK)
				{
					printf("删除目录成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "dir")))//列出目录下的信息
		{
			array = InitDArray(10, 10);
			memset(temp, 0, sizeof(temp));
			if (strlen(argv[1]) != 0)
			{
				strcpy(temp, argv[1]);
				if (IsFile(temp, &tag))
				{
					if (tag == 1)
					{
						printf("不是目录的路径\n\n");
						continue;
					}
				}
			}
			else {
				GetCurrentPath(temp);
			}
			state = ListAll(temp, array);
			if (state == OK)
			{
				DirCheckup(array);
				printf("操作成功\n\n");
			}
			else {
				DisErrorInfo(state);
				printf("\n");
			}
			DestroyDArray(array);
			continue;
		}
		else if ((!strcmp(cmd, "cpdir")))//复制目录
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = CopyDir(argv[1], argv[2]);
				if (state == OK)
				{
					printf("复制目录成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if (!strcmp(cmd, "cd"))//进入目录
		{
			if (strlen(argv[1]) != 0)
			{
				state = OpenDir(argv[1]);
				if (state == OK)
				{
					printf("操作成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "mkfile")))//创建文件
		{
			if (strlen(argv[1]) != 0)
			{
				state = VCreateFile(argv[1]);
				if (state == OK)
				{
					printf("创建文件成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "delfile")))//删除文件
		{
			if (strlen(argv[1]) != 0)
			{
				state = VDeleteFile(argv[1]);
				if (state == OK)
				{
					printf("删除文件成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if (!strcmp(cmd, "cpfile"))//复制文件
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = VCopyFile(argv[1], argv[2]);
				if (state == OK)
				{
					printf("复制文件成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if (!strcmp(cmd, "cpfilein"))//将外部文件复制到虚拟文件系统中
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = CopyFileIn(argv[1], argv[2]);
				if (state == OK)
				{
					printf("复制文件成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if (!strcmp(cmd, "cpfileout"))//将虚拟文件系统中文件复制到外部
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = CopyFileOut(argv[1], argv[2]);
				if (state == OK)
				{
					printf("复制文件成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "mv")))//移动目录或文件
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = Move(argv[1], argv[2]);
				if (state == OK)
				{
					printf("移动成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if ((!strcmp(cmd, "ren")))//重命名文件或目录，路径
		{
			if (strlen(argv[1]) != 0 && strlen(argv[2]) != 0)
			{
				state = Rename(argv[1], argv[2]);
				if (state == OK)
				{
					printf("重命名成功\n\n");
				}
				else {
					DisErrorInfo(state);
					printf("\n");
				}
			}
			else {
				printf("缺少参数\n");
			}
			continue;
		}
		else if (!strcmp(cmd, "q"))//退出
		{
			CloseVDisk();
			//fclose(fp);
			break;
		}
		else if (!strcmp(cmd, "help"))
		{
			ListHelpInfo();
			printf("\n");
		}
		else
		{
			printf("没有此命令\n\n");
			continue;
		}
	}
	return 0;
}