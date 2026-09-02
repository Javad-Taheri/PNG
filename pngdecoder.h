#ifndef _PNGDECODER_H_
#define _PNGDECODER_H_

#pragma once

/*===========================================================================*/
/*			             Simple PNG Decoder/Encoder                          */
/*---------------------------------------------------------------------------*/
/* Author: Javad Taheri
/* e-mail: mrjavadtaheri@gmail.com
/* License: Free to use.
/*
/* --------------------------- (Decoder Section) -----------------------------
/* Introduction: 
/*         This decoder code is really simple and is understandable for
/*         reading PNG file format. It creates DIB (CF_DIB) format from 
/*         supported PNG files according to the following table:
/*        
/*         +-----------------------------------------------+---------------+
/*         |               PNG FILE (in)                   |               |
/*         +------------------+----------------+-----------+   DIB (out)   +
/*         |    color type    | bit-depth      | interlace |   bit-depth   |
/*         +------------------+----------------+-----------+---------------+
/*         | grayscale(0)     | 1, 2, 4, 8, 16 |   8, 16   | 1, 4, 4, 8, 8 |
/*         | rgb triple(2)    | 8, 16          |   8, 16   | 24, 24        |
/*         | indexed(3)       | 1, 2, 4, 8     |   8       | 1, 4, 4, 8    |
/*         | grayscale+alpa(4)| 8, 16          |   8, 16   | 32, 32        |
/*         | rgb+alpha(6)     | 8, 16          |   8, 16   | 32, 32        |
/*         +------------------+----------------+-----------+---------------+
/*         
/*         Notes: 
/*         -----------------------
/*         For compiling this code you need zlib.
/* 
/*         This decoder only use main PNG chunks: IHDR, PLTE, IDAT & IEND
/*         the ancillary chunks is not used. Also multi-IDAT chunks are 
/*         supported.
/*         
/*         Please send your suggestions for me.
/*
/* Usage:
/*         Main function is ReadPngFile() and overloaded to read from 
/*         file or memory.
/*
/*         Syntax:
/*         -----------------------
/*         LONG	ReadPngFile(
/*			_In_ LPCTSTR lpszFileName,
/*			_Outptr_result_maybenull_ LPVOID* ppvDIBData,
/*			_Out_ LPDWORD pdwDIBSize,
/*			_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader = NULL);
/*
/*         Parameters:
/* 
/*         [in]lpszFileName
/*			       The full qualified path of the PNG file
/*
/*         [out]ppvDIBData   
/*                 Pointer to result DIB data. You must use
/*                 free() function to release this memory after
/*                 no longer needed it.
/*
/*         [out]pdwDIBSize
/*                 Pointer to a DWORD that recieve the entire DIB size.
/*
/*         [out, optional]pOutImageHeader
/*                 You can recieve the IHDR data as a PNG_IMAGE_HEADER
/*                 structure for details about the PNG file.
/*
/*         Return value: 
/*                 Returns PNG_SUCCESS if operation is successful or other
/*                 defined png status codes. 
/*                 If the return value is PNG_WIN32_ERROR you can call GetLastError()
/*                 for more information.
/*
/*         Remarks:
/*         -----------------------
/*                 You must release the out DIB memory after use or no longer 
/*                 needed it by free() function.
/*
/*         Sample:
/*         -----------------------
/*         DWORD dwDIBSize = 0;
/*         LPVOID pvDIBData = NULL;
/*         PNG_IMAGE_HEADER imageHeader;
/*         if (ReadPngFile(_T("c:\\sample.png"), &pvDIBData, &dwDIBSize, &imageHeader) == PNG_SUCCESS)
/*         {
/*                 // TODO: you can paint the DIB by SetDIBitsToDevice() API function
/*                 free(pvDIBData);
/*         }
/*
/* Version:
/*		   2.0 - aug, 2016
/*               * encoder added
/*               * support multi-IDAT chunks
/*               ! bug fixed on interlaced images
/*               ! minor changes on internal functions
/*         1.0 - aug, 2016
/*===========================================================================*/

#include "pngcmn.h"


// Load PNG from file
LONG
ReadPngFile(
	_In_ LPCTSTR lpszFileName,
	_Outptr_result_maybenull_ LPVOID* ppvDIBData,
	_Out_ LPDWORD pdwDIBSize,
	_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader = NULL);

// Load PNG from memory
LONG
ReadPngFile(
	_In_ LPBYTE lpMem,
	_In_ DWORD dwMemSize,
	_Outptr_result_maybenull_ LPVOID* ppvDIBData,
	_Out_ LPDWORD pdwDIBSize,
	_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader = NULL);


#endif /*_PNGDECODER_H_*/