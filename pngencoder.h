#ifndef _PNGENCODER_H_
#define _PNGENCODER_H_

#pragma once

/*===========================================================================*/
/*			             Simple PNG Decoder/Encoder              */
/*---------------------------------------------------------------------------*/
/* Author: Javad Taheri
/* e-mail: mrjavadtaheri@gmail.com
/* License: Free to use.
/*
/* --------------------------- (Encoder Section) -----------------------------
/* Introduction:
/*         This encoder creates PNG files from DIB data. DIB data must be in 
/*         CF_DIB format. The following table shows input DIB formats and out-
/*         put corresponding PNG file.
/*
/*         +-----------------------------------+----------------------------+
/*         |             DIB (in)              |            PNG (out)       |
/*         +-----------+-----------+-----------+----------------+-----------+
/*         | bit-depth | grayscale | interlace |   color type   | bit-depth |
/*         +-----------+-----------+-----------+----------------+-----------+
/*         | 1         |    yes    |    no     | indexed/grays. | 1         |
/*         | 4         |    yes    |    no     | indexed/grays. | 4         |
/*         | 8         |    yes    |    yes    | indexed/grays. | 8         |
/*         | 16        |    yes    |    yes    | rgb/grayscale  | 8         |
/*         | 24        |    yes    |    yes    | rgb/grayscale  | 8         |
/*         | 32        |    yes    |    yes    | rgb+alpha/     | 8         |
/*         |           |           |           | grayscale+alpha|           |
/*         +-----------+-----------------------+----------------+-----------+
/*
/*         Notes:
/*         -----------------------
/*         This encoder only use main PNG chunks: IHDR, PLTE, IDAT & IEND
/*         the ancillary chunks is not used.
/*
/*         Input DIB can be bottom-top or top-bottom line order.
/*         1-bit & 4-bit DIBs can not be interlaced.
/*
/*         For compiling this code you need zlib.
/*
/*         Please send your suggestions for me.
/*
/* Usage:
/*         Main function is WritePngFile() and overloaded to write to
/*         file or memory.
/*
/*         Syntax:
/*         -----------------------
/*         LONG
/*         WritePngFile(
/*             _In_ LPCTSTR lpszFileName,
/*                    - or -
/*             _Out_ LPVOID* ppvPNGData,
/*             _Out_ LPDWORD pdwPNGSize,
/*
/*             _In_ LPVOID pvDIBData,
/*             _In_ BOOL bGrayscale  = FALSE,
/*             _In_ BOOL bInterlace  = FALSE,
/*             _In_ int nFilter      = WPF_FilterBest,
/*             _In_ int nCompression = WPF_DefaultCompression);
/*
/*         Parameters:
/*
/*         [in]lpszFileName
/*			       The full qualified path of the PNG file.
/*         
/*         [out]ppvPNGData
/*                 Pointer to the result PNG file. You must use
/*                 free() function to release this memory after
/*                 no longer needed it.
/*        
/*         [out]pdwPNGSize
/*                 Pointer to a DWORD that recieve the entire PNG file size.
/*
/*         [in]pvDIBData
/*                 Pointer to buffer that contains input DIB data in CF_DIB 
/*                 format.
/*
/*         [in]bGrayscale
/*                 If this flag is set TRUE, the result PNG will be in grayscale
/*                 color type. Also the function converts color pixels to grayscale.
/*
/*         [in]bInterlace
/*                 If this flag is set TRUE, the result PNG will be interlaced.
/*
/*         [in]nFilter
/*                 Specifies the filtering method. It can be one of the PngFilterTypes
/*                 enum element.
/*
/*         [in]nCompression
/*                 Specifies the compression level. It can be one of the PngCompressionLevels
/*                 enum element.
/*
/*         Return Value:
/*                 Returns PNG_SUCCESS if operation is successful or other
/*                 defined png status codes.
/*                 If the return value is PNG_WIN32_ERROR you can call GetLastError()
/*                 for more information.
/*
/*         Remarks:
/*         -----------------------
/*                 You must release the out PNG memory (ppvPNGData) after use or no 
/*                 longer needed it by free() function.
/*
/*                 If the specified filter type is WPF_FilterBest, the function 
/*                 selects best filter type for each scanline.
/*
/*                 If the bGrayscale is TRUE, the result PNG file will be in colortype 
/*                 grayscale(0) for DIBs less than 32-bit and color type grayscale+alpha(4)
/*                 for 32-bit DIBs.
/*                 Used formula for converting color pixels to grayscale is simply:
/*                 gray = (r + g + b) / 3;
/*
/*
/* Version:
/*		   2.0 - aug, 2016
/*               * encoder added
/*               * support multi-IDAT chunks
/*               ! bug fixed on interlaced images
/*               ! minor changes on internal functions
/*         1.0 - aug, 2016
/*               * first code (only png decoder)
/*===========================================================================*/

#include "pngcmn.h"

enum PngFilterTypes {
	WPF_FilterNone		= 0,
	WPF_FilterSub		= 1,
	WPF_FilterUp		= 2,
	WPF_FilterAverage	= 3,
	WPF_FilterPaeth		= 4,
	WPF_FilterBest		= 5,
};

enum PngCompressionLevels {
	WPF_DefaultCompression	= 0,
	WPF_NoCompression		= 1,
	WPF_BestSpeed			= 2,
	WPF_BestCompression		= 3,
};

// Write PNG file to disk
LONG
WritePngFile(
	_In_ LPCTSTR lpszFileName,
	_In_ LPVOID pvDIBData,
	_In_ BOOL bGrayscale    = FALSE,
	_In_ BOOL bInterlace	= FALSE,
	_In_ int nFilter		= WPF_FilterBest,			// one of PngFilterTypes enum
	_In_ int nCompression	= WPF_DefaultCompression	// one of PngCompressionTypes enum
	);

// Write PNG file to memory
LONG
WritePngFile(
	_Out_ LPVOID* ppvPNGData,
	_Out_ LPDWORD pdwPNGSize,
	_In_ LPVOID pvDIBData,
	_In_ BOOL bGrayscale    = FALSE,
	_In_ BOOL bInterlace	= FALSE,
	_In_ int nFilter		= WPF_FilterBest,
	_In_ int nCompression	= WPF_DefaultCompression
);

#endif