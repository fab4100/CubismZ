/**
 *  @file szd_int16.c
 *  @author Sheng Di
 *  @date Aug, 2017
 *  @brief 
 *  (C) 2017 by Mathematics and Computer Science (MCS), Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "TightDataPointStorageI.h"
#include "sz.h"
#include "szd_int16.h"
#include "Huffman.h"

/**
 * 
 * 
 * @return status SUCCESSFUL (SZ_SCES) or not (other error codes) f
 * */
int SZ_decompress_args_int16(int16_t** newData, size_t r5, size_t r4, size_t r3, size_t r2, size_t r1, unsigned char* cmpBytes, size_t cmpSize)
{
	int status = SZ_SCES;
	size_t dataLength = computeDataLength(r5,r4,r3,r2,r1);
	
	//unsigned char* tmpBytes;
	size_t targetUncompressSize = dataLength <<2; //i.e., *4
	//tmpSize must be "much" smaller than dataLength
	size_t i, tmpSize = 3+1+sizeof(int16_t)+SZ_SIZE_TYPE;
	unsigned char* szTmpBytes;	
		
	if(cmpSize!=tmpSize)
	{
		int isZlib = isZlibFormat(cmpBytes[0], cmpBytes[1]);
		if(isZlib)
			szMode = SZ_BEST_COMPRESSION;
		else
			szMode = SZ_BEST_SPEED;		
		if(szMode==SZ_BEST_SPEED)
		{
			tmpSize = cmpSize;
			szTmpBytes = cmpBytes;	
		}
		else if(szMode==SZ_BEST_COMPRESSION || szMode==SZ_DEFAULT_COMPRESSION)
		{
			if(targetUncompressSize<MIN_ZLIB_DEC_ALLOMEM_BYTES) //Considering the minimum size
				targetUncompressSize = MIN_ZLIB_DEC_ALLOMEM_BYTES; 
			tmpSize = zlib_uncompress5(cmpBytes, (unsigned long)cmpSize, &szTmpBytes, (unsigned long)targetUncompressSize+4+SZ_SIZE_TYPE);//		(unsigned long)targetUncompressSize+8: consider the total length under lossless compression mode is actually 3+4+1+targetUncompressSize
			//szTmpBytes = (unsigned char*)malloc(sizeof(unsigned char)*tmpSize);
			//memcpy(szTmpBytes, tmpBytes, tmpSize);
			//free(tmpBytes); //release useless memory		
		}
		else
		{
			printf("Wrong value of szMode in the double compressed bytes.\n");
			status = SZ_MERR;
			return status;
		}	
	}
	else
		szTmpBytes = cmpBytes;
	//TODO: convert szTmpBytes to data array.
	TightDataPointStorageI* tdps;
	int errBoundMode = new_TightDataPointStorageI_fromFlatBytes(&tdps, szTmpBytes, tmpSize);
	//writeByteData(tdps->typeArray, tdps->typeArray_size, "decompress-typebytes.tbt");
	int dim = computeDimension(r5,r4,r3,r2,r1);	
	int intSize = sizeof(int16_t);
	if(tdps->isLossless)
	{
		*newData = (int16_t*)malloc(intSize*dataLength);
		if(sysEndianType==BIG_ENDIAN_SYSTEM)
		{
			memcpy(*newData, szTmpBytes+4+SZ_SIZE_TYPE, dataLength*intSize);
		}
		else
		{
			unsigned char* p = szTmpBytes+4+SZ_SIZE_TYPE;
			for(i=0;i<dataLength;i++,p+=intSize)
				(*newData)[i] = bytesToInt16_bigEndian(p);
		}		
	}
	else if (dim == 1)
		getSnapshotData_int16_1D(newData,r1,tdps, errBoundMode);
	else
	if (dim == 2)
		getSnapshotData_int16_2D(newData,r2,r1,tdps, errBoundMode);
	else
	if (dim == 3)
		getSnapshotData_int16_3D(newData,r3,r2,r1,tdps, errBoundMode);
	else
	if (dim == 4)
		getSnapshotData_int16_4D(newData,r4,r3,r2,r1,tdps, errBoundMode);
	else
	{
		printf("Error: currently support only at most 4 dimensions!\n");
		status = SZ_DERR;
	}
	free_TightDataPointStorageI(tdps);
	if(szMode!=SZ_BEST_SPEED && cmpSize!=1+4+sizeof(int16_t)+SZ_SIZE_TYPE)
		free(szTmpBytes);
	SZ_ReleaseHuffman();	
	return status;
}


void decompressDataSeries_int16_1D(int16_t** data, size_t dataSeriesLength, TightDataPointStorageI* tdps) 
{
	updateQuantizationInfo(tdps->intervals);
	size_t i, j;
	double interval = tdps->realPrecision*2;
	
	*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);

	int* type = (int*)malloc(dataSeriesLength*sizeof(int));

	decode_withTree(tdps->typeArray, dataSeriesLength, type);

	//sdi:Debug
	//writeUShortData(type, dataSeriesLength, "decompressStateBytes.sb");
	
	long predValue, tmp;
	int16_t minValue, exactData;
	
	minValue = tdps->minValue;
	
	int exactByteSize = tdps->exactByteSize;
	unsigned char* exactDataBytePointer = tdps->exactDataBytes;
	
	unsigned char curBytes[8] = {0,0,0,0,0,0,0,0};
	
	int rightShiftBits = computeRightShiftBits(exactByteSize, SZ_INT16);
	if(rightShiftBits<0)
	{
		printf("Error: rightShift < 0!\n");
		exit(0);
	}
	int type_;
	for (i = 0; i < dataSeriesLength; i++) {
		type_ = type[i];
		switch (type_) {
		case 0:
			// recover the exact data	
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[i] = exactData + minValue;
			break;
		default:
			//predValue = 2 * (*data)[i-1] - (*data)[i-2];
			predValue = (*data)[i-1];
			tmp = predValue + (type_-intvRadius)*interval;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[i] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[i] = SZ_INT16_MIN;
			else
				(*data)[i] = SZ_INT16_MAX;
			break;
		}
		//printf("%.30G\n",(*data)[i]);
	}
	free(type);
	return;
}

void decompressDataSeries_int16_2D(int16_t** data, size_t r1, size_t r2, TightDataPointStorageI* tdps) 
{
	updateQuantizationInfo(tdps->intervals);
	//printf("tdps->intervals=%d, intvRadius=%d\n", tdps->intervals, intvRadius);
	
	size_t i, j;
	size_t dataSeriesLength = r1*r2;
	//	printf ("%d %d\n", r1, r2);

	double realPrecision = tdps->realPrecision;

	*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);

	int* type = (int*)malloc(dataSeriesLength*sizeof(int));

	decode_withTree(tdps->typeArray, dataSeriesLength, type);

	int16_t minValue, exactData;

	minValue = tdps->minValue;
	
	int exactByteSize = tdps->exactByteSize;
	unsigned char* exactDataBytePointer = tdps->exactDataBytes;
	
	unsigned char curBytes[8] = {0,0,0,0,0,0,0,0};
	
	int rightShiftBits = computeRightShiftBits(exactByteSize, SZ_INT16);	
	
	long pred1D, pred2D, tmp;
	size_t ii, jj;

	/* Process Row-0, data 0 */

	// recover the exact data
	memcpy(curBytes, exactDataBytePointer, exactByteSize);
	exactData = bytesToInt16_bigEndian(curBytes);
	exactData = (uint16_t)exactData >> rightShiftBits;
	exactDataBytePointer += exactByteSize;
	(*data)[0] = exactData + minValue;

	/* Process Row-0, data 1 */
	int type_ = type[1]; 
	if (type_ != 0)
	{
		pred1D = (*data)[0];
		tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
		if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
			(*data)[1] = tmp;
		else if(tmp < SZ_INT16_MIN)
			(*data)[1] = SZ_INT16_MIN;
		else
			(*data)[1] = SZ_INT16_MAX;
			
	}
	else
	{
		// recover the exact data
		memcpy(curBytes, exactDataBytePointer, exactByteSize);
		exactData = bytesToInt16_bigEndian(curBytes);
		exactData = (uint16_t)exactData >> rightShiftBits;
		exactDataBytePointer += exactByteSize;
		(*data)[1] = exactData + minValue;
	}

	/* Process Row-0, data 2 --> data r2-1 */
	for (jj = 2; jj < r2; jj++)
	{
		type_ = type[jj];
		if (type_ != 0)
		{
			pred1D = 2*(*data)[jj-1] - (*data)[jj-2];				
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[jj] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[jj] = SZ_INT16_MIN;
			else
				(*data)[jj] = SZ_INT16_MAX;
		}
		else
		{
			// recover the exact data
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[jj] = exactData + minValue;
		}
	}

	size_t index;
	/* Process Row-1 --> Row-r1-1 */
	for (ii = 1; ii < r1; ii++)
	{
		/* Process row-ii data 0 */
		index = ii*r2;

		type_ = type[index];
		if (type_ != 0)
		{
			pred1D = (*data)[index-r2];		
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[index] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[index] = SZ_INT16_MIN;
			else
				(*data)[index] = SZ_INT16_MAX;
		}
		else
		{
			// recover the exact data
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[index] = exactData + minValue;
		}

		/* Process row-ii data 1 --> r2-1*/
		for (jj = 1; jj < r2; jj++)
		{
			index = ii*r2+jj;
			pred2D = (*data)[index-1] + (*data)[index-r2] - (*data)[index-r2-1];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				// recover the exact data
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}
		}
	}

	free(type);
	return;
}

void decompressDataSeries_int16_3D(int16_t** data, size_t r1, size_t r2, size_t r3, TightDataPointStorageI* tdps) 
{
	updateQuantizationInfo(tdps->intervals);
	size_t i, j;
	size_t dataSeriesLength = r1*r2*r3;
	size_t r23 = r2*r3;
//	printf ("%d %d %d\n", r1, r2, r3);
	unsigned char* leadNum;
	double realPrecision = tdps->realPrecision;

	*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
	int* type = (int*)malloc(dataSeriesLength*sizeof(int));

	decode_withTree(tdps->typeArray, dataSeriesLength, type);

	int16_t minValue, exactData;

	minValue = tdps->minValue;
	
	int exactByteSize = tdps->exactByteSize;
	unsigned char* exactDataBytePointer = tdps->exactDataBytes;
	
	unsigned char curBytes[8] = {0,0,0,0,0,0,0,0};
	
	int rightShiftBits = computeRightShiftBits(exactByteSize, SZ_INT16);	
	
	long pred1D, pred2D, pred3D, tmp;
	size_t ii, jj, kk;

	///////////////////////////	Process layer-0 ///////////////////////////
	/* Process Row-0 data 0*/

	// recover the exact data
	memcpy(curBytes, exactDataBytePointer, exactByteSize);
	exactData = bytesToInt16_bigEndian(curBytes);
	exactData = (uint16_t)exactData >> rightShiftBits;
	exactDataBytePointer += exactByteSize;
	(*data)[0] = exactData + minValue;

	/* Process Row-0, data 1 */
	pred1D = (*data)[0];

	int type_ = type[1];
	if (type_ != 0)
	{
		tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
		if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
			(*data)[1] = tmp;
		else if(tmp < SZ_INT16_MIN)
			(*data)[1] = SZ_INT16_MIN;
		else
			(*data)[1] = SZ_INT16_MAX;
	}
	else
	{
		memcpy(curBytes, exactDataBytePointer, exactByteSize);
		exactData = bytesToInt16_bigEndian(curBytes);
		exactData = (uint16_t)exactData >> rightShiftBits;
		exactDataBytePointer += exactByteSize;
		(*data)[1] = exactData + minValue;
	}
	/* Process Row-0, data 2 --> data r3-1 */
	for (jj = 2; jj < r3; jj++)
	{
		pred1D = 2*(*data)[jj-1] - (*data)[jj-2];

		type_ = type[jj];
		if (type_ != 0)
		{
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[jj] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[jj] = SZ_INT16_MIN;
			else
				(*data)[jj] = SZ_INT16_MAX;		}
		else
		{
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[jj] = exactData + minValue;
		}
	}

	size_t index;
	/* Process Row-1 --> Row-r2-1 */
	for (ii = 1; ii < r2; ii++)
	{
		/* Process row-ii data 0 */
		index = ii*r3;
		pred1D = (*data)[index-r3];

		type_ = type[index];
		if (type_ != 0)
		{
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[index] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[index] = SZ_INT16_MIN;
			else
				(*data)[index] = SZ_INT16_MAX;
		}
		else
		{
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[index] = exactData + minValue;
		}

		/* Process row-ii data 1 --> r3-1*/
		for (jj = 1; jj < r3; jj++)
		{
			index = ii*r3+jj;
			pred2D = (*data)[index-1] + (*data)[index-r3] - (*data)[index-r3-1];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}
		}
	}

	///////////////////////////	Process layer-1 --> layer-r1-1 ///////////////////////////

	for (kk = 1; kk < r1; kk++)
	{
		/* Process Row-0 data 0*/
		index = kk*r23;
		pred1D = (*data)[index-r23];

		type_ = type[index];
		if (type_ != 0)
		{
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[index] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[index] = SZ_INT16_MIN;
			else
				(*data)[index] = SZ_INT16_MAX;
		}
		else
		{
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[index] = exactData + minValue;
		}

		/* Process Row-0 data 1 --> data r3-1 */
		for (jj = 1; jj < r3; jj++)
		{
			index = kk*r23+jj;
			pred2D = (*data)[index-1] + (*data)[index-r23] - (*data)[index-r23-1];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}
		}

		/* Process Row-1 --> Row-r2-1 */
		for (ii = 1; ii < r2; ii++)
		{
			/* Process Row-i data 0 */
			index = kk*r23 + ii*r3;
			pred2D = (*data)[index-r3] + (*data)[index-r23] - (*data)[index-r23-r3];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}

			/* Process Row-i data 1 --> data r3-1 */
			for (jj = 1; jj < r3; jj++)
			{
				index = kk*r23 + ii*r3 + jj;
				pred3D = (*data)[index-1] + (*data)[index-r3] + (*data)[index-r23]
					- (*data)[index-r3-1] - (*data)[index-r23-r3] - (*data)[index-r23-1] + (*data)[index-r23-r3-1];

				type_ = type[index];
				if (type_ != 0)
				{
					tmp = pred3D + 2 * (type_ - intvRadius) * realPrecision;
					if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
						(*data)[index] = tmp;
					else if(tmp < SZ_INT16_MIN)
						(*data)[index] = SZ_INT16_MIN;
					else
						(*data)[index] = SZ_INT16_MAX;
				}
				else
				{
					memcpy(curBytes, exactDataBytePointer, exactByteSize);
					exactData = bytesToInt16_bigEndian(curBytes);
					exactData = (uint16_t)exactData >> rightShiftBits;
					exactDataBytePointer += exactByteSize;
					(*data)[index] = exactData + minValue;
				}
			}
		}
	}

	free(type);
	return;
}


void decompressDataSeries_int16_4D(int16_t** data, size_t r1, size_t r2, size_t r3, size_t r4, TightDataPointStorageI* tdps)
{
	updateQuantizationInfo(tdps->intervals);
	size_t i, j;
	size_t dataSeriesLength = r1*r2*r3*r4;
	size_t r234 = r2*r3*r4;
	size_t r34 = r3*r4;

	double realPrecision = tdps->realPrecision;

	*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
	int* type = (int*)malloc(dataSeriesLength*sizeof(int));
	decode_withTree(tdps->typeArray, dataSeriesLength, type);

	int16_t minValue, exactData;

	minValue = tdps->minValue;
	
	int exactByteSize = tdps->exactByteSize;
	unsigned char* exactDataBytePointer = tdps->exactDataBytes;
	
	unsigned char curBytes[8] = {0,0,0,0,0,0,0,0};
	
	int rightShiftBits = computeRightShiftBits(exactByteSize, SZ_INT16);	
	
	int type_;

	long pred1D, pred2D, pred3D, tmp;
	size_t ii, jj, kk, ll;
	size_t index;

	for (ll = 0; ll < r1; ll++)
	{
		///////////////////////////	Process layer-0 ///////////////////////////
		/* Process Row-0 data 0*/
		index = ll*r234;

		// recover the exact data
		memcpy(curBytes, exactDataBytePointer, exactByteSize);
		exactData = bytesToInt16_bigEndian(curBytes);
		exactData = (uint16_t)exactData >> rightShiftBits;
		exactDataBytePointer += exactByteSize;
		(*data)[index] = exactData + minValue;

		/* Process Row-0, data 1 */
		index = ll*r234+1;

		pred1D = (*data)[index-1];

		type_ = type[index];
		if (type_ != 0)
		{
			tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
			if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
				(*data)[index] = tmp;
			else if(tmp < SZ_INT16_MIN)
				(*data)[index] = SZ_INT16_MIN;
			else
				(*data)[index] = SZ_INT16_MAX;
		}
		else
		{
			memcpy(curBytes, exactDataBytePointer, exactByteSize);
			exactData = bytesToInt16_bigEndian(curBytes);
			exactData = (uint16_t)exactData >> rightShiftBits;
			exactDataBytePointer += exactByteSize;
			(*data)[index] = exactData + minValue;
		}

		/* Process Row-0, data 2 --> data r4-1 */
		for (jj = 2; jj < r4; jj++)
		{
			index = ll*r234+jj;

			pred1D = 2*(*data)[index-1] - (*data)[index-2];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}
		}

		/* Process Row-1 --> Row-r3-1 */
		for (ii = 1; ii < r3; ii++)
		{
			/* Process row-ii data 0 */
			index = ll*r234+ii*r4;

			pred1D = (*data)[index-r4];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}

			/* Process row-ii data 1 --> r4-1*/
			for (jj = 1; jj < r4; jj++)
			{
				index = ll*r234+ii*r4+jj;

				pred2D = (*data)[index-1] + (*data)[index-r4] - (*data)[index-r4-1];

				type_ = type[index];
				if (type_ != 0)
				{
					tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
					if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
						(*data)[index] = tmp;
					else if(tmp < SZ_INT16_MIN)
						(*data)[index] = SZ_INT16_MIN;
					else
						(*data)[index] = SZ_INT16_MAX;
				}
				else
				{
					memcpy(curBytes, exactDataBytePointer, exactByteSize);
					exactData = bytesToInt16_bigEndian(curBytes);
					exactData = (uint16_t)exactData >> rightShiftBits;
					exactDataBytePointer += exactByteSize;
					(*data)[index] = exactData + minValue;
				}
			}
		}

		///////////////////////////	Process layer-1 --> layer-r2-1 ///////////////////////////

		for (kk = 1; kk < r2; kk++)
		{
			/* Process Row-0 data 0*/
			index = ll*r234+kk*r34;

			pred1D = (*data)[index-r34];

			type_ = type[index];
			if (type_ != 0)
			{
				tmp = pred1D + 2 * (type_ - intvRadius) * realPrecision;
				if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
					(*data)[index] = tmp;
				else if(tmp < SZ_INT16_MIN)
					(*data)[index] = SZ_INT16_MIN;
				else
					(*data)[index] = SZ_INT16_MAX;
			}
			else
			{
				memcpy(curBytes, exactDataBytePointer, exactByteSize);
				exactData = bytesToInt16_bigEndian(curBytes);
				exactData = (uint16_t)exactData >> rightShiftBits;
				exactDataBytePointer += exactByteSize;
				(*data)[index] = exactData + minValue;
			}

			/* Process Row-0 data 1 --> data r4-1 */
			for (jj = 1; jj < r4; jj++)
			{
				index = ll*r234+kk*r34+jj;

				pred2D = (*data)[index-1] + (*data)[index-r34] - (*data)[index-r34-1];

				type_ = type[index];
				if (type_ != 0)
				{
					tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
					if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
						(*data)[index] = tmp;
					else if(tmp < SZ_INT16_MIN)
						(*data)[index] = SZ_INT16_MIN;
					else
						(*data)[index] = SZ_INT16_MAX;
				}
				else
				{
					memcpy(curBytes, exactDataBytePointer, exactByteSize);
					exactData = bytesToInt16_bigEndian(curBytes);
					exactData = (uint16_t)exactData >> rightShiftBits;
					exactDataBytePointer += exactByteSize;
					(*data)[index] = exactData + minValue;				
				}
			}

			/* Process Row-1 --> Row-r3-1 */
			for (ii = 1; ii < r3; ii++)
			{
				/* Process Row-i data 0 */
				index = ll*r234+kk*r34+ii*r4;

				pred2D = (*data)[index-r4] + (*data)[index-r34] - (*data)[index-r34-r4];

				type_ = type[index];
				if (type_ != 0)
				{
					tmp = pred2D + 2 * (type_ - intvRadius) * realPrecision;
					if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
						(*data)[index] = tmp;
					else if(tmp < SZ_INT16_MIN)
						(*data)[index] = SZ_INT16_MIN;
					else
						(*data)[index] = SZ_INT16_MAX;
				}
				else
				{
					memcpy(curBytes, exactDataBytePointer, exactByteSize);
					exactData = bytesToInt16_bigEndian(curBytes);
					exactData = (uint16_t)exactData >> rightShiftBits;
					exactDataBytePointer += exactByteSize;
					(*data)[index] = exactData + minValue;
				}

				/* Process Row-i data 1 --> data r4-1 */
				for (jj = 1; jj < r4; jj++)
				{
					index = ll*r234+kk*r34+ii*r4+jj;

					pred3D = (*data)[index-1] + (*data)[index-r4] + (*data)[index-r34]
							- (*data)[index-r4-1] - (*data)[index-r34-r4] - (*data)[index-r34-1] + (*data)[index-r34-r4-1];


					type_ = type[index];
					if (type_ != 0)
					{
						tmp = pred3D + 2 * (type_ - intvRadius) * realPrecision;
						if(tmp >= SZ_INT16_MIN&&tmp<SZ_INT16_MAX)
							(*data)[index] = tmp;
						else if(tmp < SZ_INT16_MIN)
							(*data)[index] = SZ_INT16_MIN;
						else
							(*data)[index] = SZ_INT16_MAX;
					}
					else
					{
						memcpy(curBytes, exactDataBytePointer, exactByteSize);
						exactData = bytesToInt16_bigEndian(curBytes);
						exactData = (uint16_t)exactData >> rightShiftBits;
						exactDataBytePointer += exactByteSize;
						(*data)[index] = exactData + minValue;
					}
				}
			}
		}
	}

	free(type);
	return;
}

void getSnapshotData_int16_1D(int16_t** data, size_t dataSeriesLength, TightDataPointStorageI* tdps, int errBoundMode)
{	
	SZ_Reset();
	size_t i;

	if (tdps->allSameData) {
		int16_t value = bytesToInt16_bigEndian(tdps->exactDataBytes);
		*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
		for (i = 0; i < dataSeriesLength; i++)
			(*data)[i] = value;
	} else {
		decompressDataSeries_int16_1D(data, dataSeriesLength, tdps);
	}
}

void getSnapshotData_int16_2D(int16_t** data, size_t r1, size_t r2, TightDataPointStorageI* tdps, int errBoundMode) 
{
	SZ_Reset();
	size_t i;
	size_t dataSeriesLength = r1*r2;
	if (tdps->allSameData) {
		int16_t value = bytesToInt16_bigEndian(tdps->exactDataBytes);
		*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
		for (i = 0; i < dataSeriesLength; i++)
			(*data)[i] = value;
	} else {
		decompressDataSeries_int16_2D(data, r1, r2, tdps);
	}
}

void getSnapshotData_int16_3D(int16_t** data, size_t r1, size_t r2, size_t r3, TightDataPointStorageI* tdps, int errBoundMode)
{
	SZ_Reset();
	size_t i;
	size_t dataSeriesLength = r1*r2*r3;
	if (tdps->allSameData) {
		int16_t value = bytesToInt16_bigEndian(tdps->exactDataBytes);
		*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
		for (i = 0; i < dataSeriesLength; i++)
			(*data)[i] = value;
	} else {
		decompressDataSeries_int16_3D(data, r1, r2, r3, tdps);
	}
}

void getSnapshotData_int16_4D(int16_t** data, size_t r1, size_t r2, size_t r3, size_t r4, TightDataPointStorageI* tdps, int errBoundMode)
{
	SZ_Reset();
	size_t i;
	size_t dataSeriesLength = r1*r2*r3*r4;
	if (tdps->allSameData) {
		int16_t value = bytesToInt16_bigEndian(tdps->exactDataBytes);
		*data = (int16_t*)malloc(sizeof(int16_t)*dataSeriesLength);
		for (i = 0; i < dataSeriesLength; i++)
			(*data)[i] = value;
	} else {
		decompressDataSeries_int16_4D(data, r1, r2, r3, r4, tdps);
	}
}
