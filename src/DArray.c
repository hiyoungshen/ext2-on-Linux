#include<stdlib.h>
#include<stdio.h>
#include"ext2.h"
DArray* InitDArray(UINT initialCapacity,UINT capacityIncrement)
{
	DArray *darray=NULL;
	darray=(DArray*)malloc(sizeof(DArray));
	if(darray==NULL)
	{
		return NULL;
	}
	darray->base=(DArrayElem*)malloc(initialCapacity*sizeof(DArrayElem));
	darray->used=0;
	darray->offset=0;
	darray->capacity=initialCapacity;
	darray->increment=capacityIncrement;
	return darray;
}

void AddElement(DArray *darray,DArrayElem element)
{
	DArrayElem *newbase=NULL;

	if(darray->used>=darray->capacity)
	{
		newbase=(DArrayElem*)realloc(darray->base,(darray->capacity+darray->increment)*sizeof(DArrayElem));
		if(newbase==NULL)
		{
			exit(0);
		}
		darray->base=newbase;
		darray->capacity+=darray->increment;
	}
	darray->base[(darray->used)++]=element;
}

DArrayElem* NextElement(DArray *darray)
{
	if(darray->offset>=darray->used)
	{
		return NULL;
	}
	return &(darray->base[darray->offset++]);
}

void DestroyDArray(DArray *darray)
{
	free(darray->base);
	free(darray);
}
