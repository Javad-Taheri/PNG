#include "stdafx.h"
#include "pngdecoder.h"
#include "crc.h"

#if defined(_WIN32)
#define ZLIB_WINAPI
#endif

#include <zlib.h>

//=========================================================
// PNG eye front specifications
/*

Color Combinations:

	Color    Allowed		Interpretation
	Type    Bit Depths

	0       1, 2, 4, 8, 16  Each pixel is a grayscale sample.
	2       8, 16			Each pixel is an R, G, B triple.
	3       1, 2, 4, 8		Each pixel is a palette index; a PLTE chunk must appear.
	4       8, 16			Each pixel is a grayscale sample, followed by an alpha sample.
	6       8, 16			Each pixel is an R, G, B triple, followed by an alpha sample.


ADAM7 Passes:

        0 1 2 3 4 5 6 7
	-------------------
	0 : 1 6 4 6 2 6 4 6
    1 : 7 7 7 7 7 7 7 7
    2 : 5 6 5 6 5 6 5 6
    3 : 7 7 7 7 7 7 7 7
    4 : 3 6 4 6 3 6 4 6
    5 : 7 7 7 7 7 7 7 7
    6 : 5 6 5 6 5 6 5 6
    7 : 7 7 7 7 7 7 7 7

*/
//=========================================================

#define ZLIB_BUF_SIZE 131072 // 128K

//=========================================================
//			   Local Functions Prototype
//
LONG CheckAndGetImageHeader(_In_ PNG_CHUNK_STRUCT* pChunk, _Out_ PNG_IMAGE_HEADER* pImageHeader);
LONG Uncompress(_In_ void* pvIn, _In_ DWORD dwSizeIn, _Out_ void** ppvOut, _Out_ LPDWORD pdwSizeOut);
LONG CreateEmptyDIB(_In_ PNG_IMAGE_HEADER* pImageHeader, _In_opt_ PNG_CHUNK_STRUCT* pPaletteChunk, _Outptr_result_maybenull_ void** ppvDIBData, _Out_ LPDWORD pdwDIBSize);
LONG UnfilterByte(_In_ UINT lineNum, _In_ UINT bytePos, _In_ int type, _Inout_ BYTE* pbyte, _In_ UINT stride, _In_ UINT bpp);
LONG UnfilterImageData(_In_ PNG_IMAGE_HEADER* pImageHeader, _Inout_ void* pvImageData);
void CopyPngPixelToDIB(_In_ PNG_IMAGE_HEADER* pihdr, _In_ BYTE* pPng, _Inout_ BYTE* pDib);
LONG CopyImageDataToDIB(_In_ PNG_IMAGE_HEADER* pImageHeader, _In_ void* pvImageData, _In_ DWORD dwImageSize, _In_ LPBITMAPINFO pbmi, _Out_ void* pvBits);
//
//=========================================================

/*
 Reads image header data from IHDR chunk and check for sanity
*/
LONG CheckAndGetImageHeader(
	_In_ PNG_CHUNK_STRUCT* pChunk, 
	_Out_ PNG_IMAGE_HEADER* pImageHeader)
{
	ASSERT(pChunk != NULL && pImageHeader != NULL);

	// Check Image Header chunk
	if (pChunk->dwLength != PNG_IHDR_SIZE)
		return PNG_BAD_FORMAT;

	memcpy(pImageHeader, pChunk->pData, PNG_IHDR_SIZE);

	pImageHeader->ihWidth = toLE32(pImageHeader->ihWidth);
	pImageHeader->ihHeight = toLE32(pImageHeader->ihHeight);

	if (PNGVerifyImageHeader(pImageHeader) == FALSE)
	{
		TRACE0("CheckAndGetImageHeader()- Not valid IHDR\n");
		return PNG_BAD_FORMAT;
	}

	return PNG_SUCCESS;
}

/* 
 Uncompresses image data from IDAT
*/
LONG Uncompress(
	_In_ void* pvIn,
	_In_ DWORD dwSizeIn,
	_Out_ void** ppvOut,
	_Out_ LPDWORD pdwSizeOut
	)
{
	int ret;
	unsigned long have;
	z_stream strm;
	unsigned char* buf_in = NULL;
	unsigned char* buf_out = NULL;
	unsigned char* psrc = NULL;
	unsigned char* pdest = NULL;
	unsigned long srcSize = 0;
	unsigned long destSize = 0;
	unsigned long bytesRead = 0;
	unsigned long bytesWrite = 0;

	*ppvOut = NULL;
	*pdwSizeOut = 0;

	buf_in = (unsigned char*)malloc(ZLIB_BUF_SIZE);
	if (buf_in == NULL)
	{
		return PNG_NOT_ENOUGH_MEMORY;
	}

	buf_out = (unsigned char*)malloc(ZLIB_BUF_SIZE);
	if (buf_out == NULL)
	{
		free(buf_in);
		return PNG_NOT_ENOUGH_MEMORY;
	}

	// allocate memory for extraction destinate (final output)
	destSize = dwSizeIn;

	ASSERT(destSize != 0);
	pdest = (unsigned char*)malloc(destSize);
	if (pdest == NULL)
	{
		free(buf_in);
		free(buf_out);
		return PNG_NOT_ENOUGH_MEMORY;
	}

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
	{
		free(buf_in);
		free(buf_out);
		free(pdest);
		return PNG_FAIL;
	}

	psrc = (unsigned char*)pvIn;
	srcSize = dwSizeIn;


	/* decompress until inflate stream ends */
	do {
		if (bytesRead < srcSize)
		{
			if (srcSize <= ZLIB_BUF_SIZE)
			{
				memcpy(buf_in, psrc, srcSize);
				strm.avail_in = srcSize;
				bytesRead = srcSize;
			}
			else
			{
				unsigned long n = min(ZLIB_BUF_SIZE, srcSize - bytesRead);
				memcpy(buf_in, psrc + bytesRead, n);
				bytesRead += n;
				strm.avail_in = n;
			}
		}


		if (strm.avail_in == 0)
			break;
		strm.next_in = buf_in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = ZLIB_BUF_SIZE;
			strm.next_out = buf_out;
			ret = inflate(&strm, Z_NO_FLUSH);

			ASSERT(ret != Z_STREAM_ERROR);  /* state not clobbered */

			switch (ret) {
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR: {
					inflateEnd(&strm);
					free(buf_in);
					free(buf_out);
					free(pdest);
					return PNG_FAIL;
				}
			}

			have = ZLIB_BUF_SIZE - strm.avail_out;

			if (destSize >= bytesWrite + have)
			{
				memcpy(pdest + bytesWrite, buf_out, have);
				bytesWrite += have;
			}
			else // we need more memory
			{
				destSize += have;
				pdest = (unsigned char*)realloc(pdest, destSize);

				memcpy(pdest + bytesWrite, buf_out, have);
				bytesWrite += have;
			}
			
			/* done when inflate() says it's done */
		} while (strm.avail_out == 0);
		
	} while (ret != Z_STREAM_END);

	/* clean up */
	inflateEnd(&strm);
	free(buf_in);
	free(buf_out);

	if (ret == Z_STREAM_END)
	{
		// free extra memory
		if (bytesWrite < destSize)
		{
			destSize = bytesWrite;
			pdest = (unsigned char*)realloc(pdest, destSize);
		}

		*ppvOut = pdest;
		*pdwSizeOut = destSize;

		return PNG_SUCCESS;
	}

	free(pdest);

	return  PNG_FAIL;
}

/*
 Creates empty DIB base on png image header and palette data
*/
LONG CreateEmptyDIB(
	_In_ PNG_IMAGE_HEADER* pImageHeader, 
	_In_opt_ PNG_CHUNK_STRUCT* pPaletteChunk, 
	_Outptr_result_maybenull_ void** ppvDIBData, 
	_Out_ LPDWORD pdwDIBSize)
{
	ASSERT(pImageHeader != NULL && ppvDIBData != NULL && pdwDIBSize != NULL);	

	BITMAPINFO bmi;
	ZeroMemory(&bmi, sizeof(BITMAPINFOHEADER));

	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biWidth = pImageHeader->ihWidth;
	bmi.bmiHeader.biHeight = pImageHeader->ihHeight;

	switch (pImageHeader->ihColorType)
	{	
	case PNG_COLOR_TYPE_GRAYSCALE:
		if (pImageHeader->ihBitDepth != 16)
			bmi.bmiHeader.biBitCount = (pImageHeader->ihBitDepth != 2) ? pImageHeader->ihBitDepth : 4;
		else
			bmi.bmiHeader.biBitCount = 8;
		break;

	case PNG_COLOR_TYPE_RGB_TRIPLE:
		bmi.bmiHeader.biBitCount = 24;
		break;

	case PNG_COLOR_TYPE_INDEXED:
		if (pPaletteChunk == NULL)
		{
			TRACE0("CreateEmptyDIB()- Palette chunk required\n");
			return PNG_BAD_FORMAT;
		}

		bmi.bmiHeader.biBitCount = (pImageHeader->ihBitDepth != 2) ? pImageHeader->ihBitDepth : 4;
		break;

	case PNG_COLOR_TYPE_GRAYSCALE_ALPHA:
	case PNG_COLOR_TYPE_RGBA_QUAD:
		bmi.bmiHeader.biBitCount = 32;
		break;

	default:
		TRACE0("CreateEmptyDIB()- Invalid color type\n");
		return PNG_BAD_FORMAT;
		break;
	}

	DWORD dwPaletteSize = DIBPalleteSize(&bmi);
	DWORD dwDIBSize = bmi.bmiHeader.biSize + dwPaletteSize + DIBCalcBitsSize(&bmi);

	void* pvDIBData = malloc(dwDIBSize);
	if (pvDIBData == NULL)
		return PNG_NOT_ENOUGH_MEMORY;

	memcpy(pvDIBData, &bmi, bmi.bmiHeader.biSize);

	LPBITMAPINFO pbmi = (LPBITMAPINFO)pvDIBData;
	
	// copy palette
	if (bmi.bmiHeader.biBitCount <= 8)
	{				
		if (pImageHeader->ihColorType == PNG_COLOR_TYPE_INDEXED)
		{
			ASSERT(pPaletteChunk->dwType == PNG_CHUNK_TYPE_PLTE);

			UINT numColors = DIBNumColors(&bmi);
			UINT numEntries = pPaletteChunk->dwLength / sizeof(PNG_PALETTE_ENTRY);
			
			if (pImageHeader->ihBitDepth != 2 && numEntries < numColors)
			{
				TRACE0("CreateEmptyDIB()- Not enough palette entries\n");
				return PNG_BAD_FORMAT;
			}
			else if (pImageHeader->ihBitDepth == 2 && numEntries < 4)
			{
				TRACE0("CreateEmptyDIB()- Not enough palette entries\n");
				return PNG_BAD_FORMAT;
			}

			PNG_PALETTE_ENTRY* pEntry = (PNG_PALETTE_ENTRY*)pPaletteChunk->pData;
			for (UINT n = 0; n < numColors; n++)
			{
				pbmi->bmiColors[n].rgbRed = pEntry->peRed;
				pbmi->bmiColors[n].rgbGreen = pEntry->peGreen;
				pbmi->bmiColors[n].rgbBlue = pEntry->peBlue;
				pbmi->bmiColors[n].rgbReserved = 0;
				pEntry++;
			}
		}
		else if (pImageHeader->ihColorType == PNG_COLOR_TYPE_GRAYSCALE)
		{
			// create grayscale palette
			if (pImageHeader->ihBitDepth == 8 || pImageHeader->ihBitDepth == 16)
			{				
				for (UINT n = 0; n < 256; n++)
				{
					pbmi->bmiColors[n].rgbRed = n;
					pbmi->bmiColors[n].rgbGreen = n;
					pbmi->bmiColors[n].rgbBlue = n;
					pbmi->bmiColors[n].rgbReserved = 0;
				}
			}
			else if (pImageHeader->ihBitDepth == 4)
			{
				pbmi->bmiColors[0].rgbRed = 0;
				pbmi->bmiColors[0].rgbGreen = 0;
				pbmi->bmiColors[0].rgbBlue = 0;
				pbmi->bmiColors[0].rgbReserved = 0;

				for (UINT n = 1; n < 16; n++) 
				{
					BYTE gray = (n - 1) * 17 + 17;
					pbmi->bmiColors[n].rgbRed = gray;
					pbmi->bmiColors[n].rgbGreen = gray;
					pbmi->bmiColors[n].rgbBlue = gray;
					pbmi->bmiColors[n].rgbReserved = 0;
				}
			}
			else if (pImageHeader->ihBitDepth == 2)
			{
				pbmi->bmiColors[0].rgbRed = 0;
				pbmi->bmiColors[1].rgbRed = 64;
				pbmi->bmiColors[2].rgbRed = 128;
				pbmi->bmiColors[3].rgbRed = 255;
				for (UINT n = 0; n < 4; n++) {
					pbmi->bmiColors[n].rgbGreen = pbmi->bmiColors[n].rgbRed;
					pbmi->bmiColors[n].rgbBlue = pbmi->bmiColors[n].rgbRed;
					pbmi->bmiColors[n].rgbReserved = 0;
				}
			}
			else if (pImageHeader->ihBitDepth == 1)
			{
				pbmi->bmiColors[0].rgbRed = 0;
				pbmi->bmiColors[0].rgbGreen = 0;
				pbmi->bmiColors[0].rgbBlue = 0;
				pbmi->bmiColors[0].rgbReserved = 0;
				pbmi->bmiColors[1].rgbRed = 255;
				pbmi->bmiColors[1].rgbGreen = 255;
				pbmi->bmiColors[1].rgbBlue = 255;
				pbmi->bmiColors[1].rgbReserved = 0;
			}
		}
	}
	

	*ppvDIBData = pvDIBData;
	*pdwDIBSize = dwDIBSize;
	return PNG_SUCCESS;
}


// ========================================================
//                   Decode Functions
// ========================================================

///////////////////////////////////////////////////////////
// Common stuffs
///////////////////////////////////////////////////////////

/*
 Unfilter one byte
*/
LONG UnfilterByte(
	_In_ UINT lineNum, /*scanline number, 0 based*/ 
	_In_ UINT bytePos, /*byte position in scanline*/
	_In_ int type, /*filter type*/
	_Inout_ BYTE* pbyte, /*filtered byte*/
	_In_ UINT stride, /*stride of scanline*/
	_In_ UINT bpp /*byte per pixel*/
	)
{
	BYTE a = 0;
	BYTE b = 0;
	BYTE c = 0;

	switch (type)
	{
	case PNG_FILTER_TYPE_NONE:
		break;

	case PNG_FILTER_TYPE_SUB:
		if (bytePos >= bpp) {
			*pbyte += *(pbyte - bpp);
		}
		break;

	case PNG_FILTER_TYPE_UP:
		if (lineNum > 0) {
			*pbyte += *(pbyte - stride - 1);
		}
		break;

	case PNG_FILTER_TYPE_AVERAGE:
		if (bytePos >= bpp) {
			a = *(pbyte - bpp);
		}

		if (lineNum > 0) {
			b = *(pbyte - stride - 1);
		}

		*pbyte += (a + b) / 2;
		break;

	case PNG_FILTER_TYPE_PAETH:
		if (bytePos >= bpp) {
			a = *(pbyte - bpp);
		}

		if (lineNum > 0) {
			b = *(pbyte - stride - 1);
		}

		if (bytePos >= bpp && lineNum > 0) {
			c = *(pbyte - stride - 1 - bpp);
		}

		*pbyte += PaethPredictor(a, b, c);
		break;

	default:
		TRACE1("UnfilterByte()- Wrong filter type, %d\n", type);
//		return PNG_BAD_FORMAT; // continue any way!
		break;
	}

	return PNG_SUCCESS;
}

/*
 Unfilters png image data
*/
LONG UnfilterImageData(
	_In_ PNG_IMAGE_HEADER* pImageHeader,
	_Inout_ void* pvImageData
	)
{
	UINT nBpp = PNGBytesPerPixel(pImageHeader);
	if (nBpp == 0)
	{
		TRACE0("UnfilterImageData()- Zero PNG Bpp\n");
		return PNG_BAD_FORMAT;
	}

	BYTE* ptr = (BYTE*)pvImageData;

	if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
	{
		UINT nBytesPerLine = PNGBytesPerLine(pImageHeader);
		if (nBytesPerLine == 0)
		{
			TRACE0("UnfilterImageData()- Zero bytes per line\n");
			return PNG_BAD_FORMAT;
		}

		for (UINT nLine = 0; nLine < pImageHeader->ihHeight; nLine++)
		{
			BYTE* pFilterType = ptr++;
			for (UINT nByte = 0; nByte < nBytesPerLine; nByte++)
			{
				if (UnfilterByte(nLine, nByte, *pFilterType, ptr++, nBytesPerLine, nBpp) != PNG_SUCCESS)
					return PNG_BAD_FORMAT;
			}

			*pFilterType = PNG_FILTER_TYPE_NONE; // Now scanline is unfiltered!
		}
	}
	/*end of if (PNG_INTERLACE_SEQUENTIAL)*/
	else if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_ADAM7)
	{
		for (int pass = 0; pass < 7; pass++)
		{
			UINT nPassLines = PNGPassLines(pImageHeader, pass);
			UINT nPassPixels = PNGPassLinePixels(pImageHeader, pass);				
			UINT nBytesInLine = nPassPixels * nBpp;

			// don't process empty passes
			if (nPassLines > 0 && nPassPixels > 0)
			{
				for (UINT nLine = 0; nLine < nPassLines; nLine++)
				{
					BYTE* pFilterType = ptr++;
					for (UINT nByte = 0; nByte < nBytesInLine; nByte++)
					{
						if (UnfilterByte(nLine, nByte, *pFilterType, ptr++, nBytesInLine, nBpp) != PNG_SUCCESS)
							return PNG_BAD_FORMAT;
					}

					*pFilterType = PNG_FILTER_TYPE_NONE;
				}
			}
		}/*end of for(pass)*/
	}
	/*end of if (PNG_INTERLACE_ADAM7)*/
	else
	{
		TRACE0("UnfilterImageData()- Invalid interlace method\n");
		return PNG_BAD_FORMAT;
	}
	/*end of if (interlace_method)*/

	return PNG_SUCCESS;
}


/*
 Copy and convert one PNG pixel into DIB format
*/
void CopyPngPixelToDIB(
	_In_ PNG_IMAGE_HEADER* pihdr,
	_In_ BYTE* pPng,
	_Inout_ BYTE* pDib
	)
{
	if (pihdr->ihBitDepth != 16)
	{
		if (pihdr->ihColorType == PNG_COLOR_TYPE_RGB_TRIPLE)
		{
			PNG_RGB_TRIPLE* prgbPng = (PNG_RGB_TRIPLE*)pPng;
			DIB_RGB_TRIPLE* prgbDib = (DIB_RGB_TRIPLE*)pDib;

			prgbDib->rgbRed = prgbPng->rgbRed;
			prgbDib->rgbGreen = prgbPng->rgbGreen;
			prgbDib->rgbBlue = prgbPng->rgbBlue;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGBA_QUAD)
		{
			PNG_RGBA_QUAD* prgbaPng = (PNG_RGBA_QUAD*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			prgbaDib->rgbaRed = prgbaPng->rgbaRed;
			prgbaDib->rgbaGreen = prgbaPng->rgbaGreen;
			prgbaDib->rgbaBlue = prgbaPng->rgbaBlue;
			prgbaDib->rgbaAlpha = prgbaPng->rgbaAlpha;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE_ALPHA)
		{
			PNG_GRAYSCALE_ALPHA* pgaPng = (PNG_GRAYSCALE_ALPHA*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			prgbaDib->rgbaRed = pgaPng->gaGray;
			prgbaDib->rgbaGreen = pgaPng->gaGray;
			prgbaDib->rgbaBlue = pgaPng->gaGray;
			prgbaDib->rgbaAlpha = pgaPng->gaAlpha;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_INDEXED || pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE)
		{
			if (pihdr->ihBitDepth != 2)
			{
				*pDib = *pPng;
			}
			else
			{
				// each 2-bit png byte contains 4 pixel data
				BYTE* pDibByte1 = pDib;
				BYTE* pDibByte2 = pDib + 1;

				*pDibByte1 = (*pPng & 0xc0) >> 2;
				*pDibByte1 |= (*pPng & 0x30) >> 4;

				*pDibByte2 = (*pPng & 0x0c) << 2;
				*pDibByte2 |= (*pPng & 0x03);
			}
		}
	}
	/*end of if (bit-depth != 16)*/
	else
	{
		ASSERT(pihdr->ihColorType != PNG_COLOR_TYPE_INDEXED);

		if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE)
		{
			*pDib = *((WORD*)pPng) &0x00ff;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGB_TRIPLE)
		{
			PNG_RGB_TRIPLE_16BIT* prgbPng = (PNG_RGB_TRIPLE_16BIT*)pPng;
			DIB_RGB_TRIPLE* prgbDib = (DIB_RGB_TRIPLE*)pDib;

			prgbDib->rgbRed = prgbPng->rgbRed & 0x00ff;
			prgbDib->rgbGreen = prgbPng->rgbGreen & 0x00ff;
			prgbDib->rgbBlue = prgbPng->rgbBlue & 0x00ff;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE_ALPHA)
		{
			PNG_GRAYSCALE_ALPHA_16BIT* pgaPng = (PNG_GRAYSCALE_ALPHA_16BIT*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			BYTE gray = pgaPng->gaGray & 0x00ff;
			prgbaDib->rgbaRed = gray;
			prgbaDib->rgbaGreen = gray;
			prgbaDib->rgbaBlue = gray;
			prgbaDib->rgbaAlpha = pgaPng->gaAlpha & 0x00ff;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGBA_QUAD)
		{
			PNG_RGBA_QUAD_16BIT* prgbaPng = (PNG_RGBA_QUAD_16BIT*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			prgbaDib->rgbaRed = prgbaPng->rgbaRed & 0x00ff;
			prgbaDib->rgbaGreen = prgbaPng->rgbaGreen & 0x00ff;
			prgbaDib->rgbaBlue = prgbaPng->rgbaBlue & 0x00ff;
			prgbaDib->rgbaAlpha = prgbaPng->rgbaAlpha & 0x00ff;
		}
	}
}

/*
 Copy and convert PNG image data into DIB
*/
LONG CopyImageDataToDIB(
	_In_ PNG_IMAGE_HEADER* pImageHeader,
	_In_ void* pvImageData,
	_In_ DWORD dwImageSize,
	_In_ LPBITMAPINFO pbmi,
	_Out_ void* pvBits
)
{
	ASSERT(pImageHeader != NULL && pvImageData != NULL && dwImageSize != 0 && pbmi != NULL && pvBits != NULL);

	static const UINT pass_first_pixel_x[7] = { 0, 4, 0, 2, 0, 1, 0 };
	static const UINT pass_next_pixel_x[7]  = { 8, 8, 4, 4, 2, 2, 1 };
	static const UINT pass_first_pixel_y[7] = { 0, 0, 4, 0, 2, 0, 1 };
	static const UINT pass_next_pixel_y[7]  = { 8, 8, 8, 4, 4, 2, 2 };

	UINT pngBpp = PNGBytesPerPixel(pImageHeader);
	UINT dibBpp = DIBBytesPerPixel(pbmi);
	UINT dibStride = DIBBytesPerLine(pbmi);

	BYTE* pDib = NULL;
	BYTE* pPng = (BYTE*)pvImageData;

	if (dibBpp == 0 || pngBpp == 0)
	{
		TRACE0("CopyImageDataToDIB()- Zero Bpp\n");
		return PNG_BAD_FORMAT;
	}

	if (pImageHeader->ihBitDepth >= 8)
	{
		if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
		{
			for (UINT y = 0; y < pImageHeader->ihHeight; y++)
			{
				pPng++; // skip filter byte
				pDib = (BYTE*)pvBits + (pImageHeader->ihHeight - y - 1) * dibStride;

				for (UINT x = 0; x < pImageHeader->ihWidth; x++)
				{
					CopyPngPixelToDIB(pImageHeader, pPng, pDib);
					pPng += pngBpp;
					pDib += dibBpp;
				}
			}
		}
		/*end of if (PNG_INTERLACE_SEQUENTIAL)*/
		else if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_ADAM7)
		{
			for (int pass = 0; pass < 7; pass++)
			{
				UINT nPassLines = PNGPassLines(pImageHeader, pass);
				UINT nPassPixels = PNGPassLinePixels(pImageHeader, pass);

				// don't process empty passes
				if (nPassLines > 0 && nPassPixels > 0)
				{
					UINT y = pass_first_pixel_y[pass];
					for (UINT nLine = 0; nLine < nPassLines; nLine++)
					{
						pPng++;
						pDib = (BYTE*)pvBits + (pImageHeader->ihHeight - y - 1) * dibStride;
						pDib += pass_first_pixel_x[pass] * dibBpp;

						for (UINT nPixel = 0; nPixel < nPassPixels; nPixel++)
						{
							CopyPngPixelToDIB(pImageHeader, pPng, pDib);
							pDib += pass_next_pixel_x[pass] * dibBpp;
							pPng += pngBpp;
						}

						y += pass_next_pixel_y[pass];
					}
				}
			}/*end of for(pass)*/
		}
		/*end of if (PNG_INTERLACE_ADAM7)*/
		else
		{
			TRACE0("CopyImageDataToDIB()- Invalid interlace method\n");
			return PNG_BAD_FORMAT;
		}
		/*end of if (interlace_method)*/
	}
	/*end of if (bit-depth >= 8)*/
	else if (pImageHeader->ihBitDepth < 8)
	{
		if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
		{
			UINT dibIncrement = pImageHeader->ihBitDepth != 2 ? 1 : 2;
			UINT nBytesPerLine = PNGBytesPerLine(pImageHeader);
			if (nBytesPerLine == 0)
				return PNG_BAD_FORMAT;

			for (UINT y = 0; y < pImageHeader->ihHeight; y++)
			{
				pPng++; // skip filter byte
				pDib = (BYTE*)pvBits + (pImageHeader->ihHeight - y - 1) * dibStride;

				for (UINT nByte = 0; nByte < nBytesPerLine; nByte++)
				{
					CopyPngPixelToDIB(pImageHeader, pPng, pDib);
					pPng++;
					pDib += dibIncrement;
				}
			}
		}
		else
		{
			TRACE0("CopyImageDataToDIB()- Less than 8-bit image can not be interlaced\n");
			return PNG_BAD_FORMAT;
		}
	}
	/*end of if (bit-depth < 8)*/

	return PNG_SUCCESS;
}

// ========================================================
//                End of Decode Functions
// ========================================================


/*
 Read png file and create coresponding DIB
*/
LONG ReadPngFile(
	_In_ LPCTSTR lpszFileName, 
	_Outptr_result_maybenull_ void** ppvDIBData, 
	_Out_ LPDWORD pdwDIBSize, 
	_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader)
{
	ASSERT(lpszFileName != NULL && ppvDIBData != NULL);

	if (lpszFileName == NULL || ppvDIBData == NULL)
		return PNG_INVALID_ARG;

	TRACE1("ReadPngFile()- FileName: %s\n", lpszFileName);

	*ppvDIBData = NULL;

	HANDLE hFile = INVALID_HANDLE_VALUE;
	LPBYTE pFileData = NULL;
	DWORD dwLastError = GetLastError();

	__try
	{
		hFile = CreateFile(lpszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			dwLastError = GetLastError();
			TRACE1("ReadPngFile()- Failed to open the file, error: 0x%08x\n", dwLastError);
			return PNG_WIN32_ERROR;
		}

		DWORD dwFileSize = GetFileSize(hFile, NULL);
		if (dwFileSize == INVALID_FILE_SIZE)
		{
			dwLastError = GetLastError();
			TRACE1("ReadPngFile()- Failed to get the file size, error: 0x%08x\n", dwLastError);
			return PNG_WIN32_ERROR;
		}

		// Check for empty and atleast file size
		if (dwFileSize == 0)
		{
			TRACE0("ReadPngFile()- Zero length file\n");
			return PNG_NOT_PNG;
		}

		// Allocate memort for reading entire file
		pFileData = (LPBYTE)malloc(dwFileSize);
		if (pFileData == NULL)
		{
			return PNG_NOT_ENOUGH_MEMORY;
		}

		// Read entire file to memory
		DWORD dwBytesRead;
		if (!ReadFile(hFile, pFileData, dwFileSize, &dwBytesRead, NULL) || (dwBytesRead != dwFileSize))
		{
			dwLastError = GetLastError();
			TRACE1("ReadPngFile()- Failed to ReadFile, error: 0x%08x\n", dwLastError);
			return PNG_WIN32_ERROR;
		}

		LONG lRet = ReadPngFile(pFileData, dwFileSize, ppvDIBData, pdwDIBSize, pOutImageHeader);
		if (lRet == PNG_WIN32_ERROR)
		{
			dwLastError = GetLastError();
			return PNG_WIN32_ERROR;
		}
		else
		{
			return lRet;
		}
	}
	__finally
	{
		if (hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);

		if (pFileData != NULL)
			free(pFileData);

		// Restore last error
		SetLastError(dwLastError);
	}

	return PNG_FAIL;
}

/*
 Read png file from memory
*/
LONG ReadPngFile(
	_In_ LPBYTE lpMem, 
	_In_ DWORD dwMemSize, 
	_Outptr_result_maybenull_ void** ppvDIBData,
	_Out_ LPDWORD pdwDIBSize, 
	_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader)
{
	ASSERT(lpMem != NULL && dwMemSize != 0 && ppvDIBData != NULL );

	if (lpMem == NULL || ppvDIBData == NULL || dwMemSize == 0)
		return PNG_INVALID_ARG;


	*ppvDIBData = NULL;

	DWORD dwLeadingBytes = dwMemSize;

	// Check PNG signature =========================
	if (!PNGVerifySignature(lpMem, dwLeadingBytes))
	{
		TRACE0("ReadPngFile()- Not valid IHDR\n");
		return PNG_NOT_PNG;
	}

	dwLeadingBytes -= PNG_SIGNATURE_SIZE;
	//==============================================
	
	//
	// Read PNG chunks
	//
	
	PNG_IMAGE_HEADER imageHeader;

	BOOL bIHDRPresents = FALSE;
	BOOL bPLTEPResents = FALSE;
	BOOL bIDATPresents = FALSE;

	PNG_CHUNK_STRUCT chunk;
	DWORD dwTotalChunkSize = 0;

	UINT nChunkCounter = 0;
	// Pointer to the first chunk
	PNG_CHUNK_HEADER* pChunkHeader = (PNG_CHUNK_HEADER*)(lpMem + PNG_SIGNATURE_SIZE);
	PNG_CHUNK_STRUCT chunkIHDR;
	PNG_CHUNK_STRUCT chunkPLTE;

	// pointer to memory than contains merged IDAT chunks
	void* pvCompressedImageData = NULL; 
	DWORD dwCompressedImageSize = 0;


	// Enum chunks =================================
	while (true)
	{		
		if (dwLeadingBytes < PNG_EMPTY_CHUNK_SIZE)
			return PNG_BAD_FORMAT;
				
		chunk.dwLength = toLE32(pChunkHeader->chLength);
		chunk.dwType = pChunkHeader->chType.dw;
		chunk.pData = (LPBYTE)pChunkHeader + PNG_CHUNK_HEADER_SIZE;
		dwTotalChunkSize = PNG_EMPTY_CHUNK_SIZE + chunk.dwLength;
		
		if (dwLeadingBytes < dwTotalChunkSize)
			return PNG_BAD_FORMAT;
		
		// Check chunk sanity
		chunk.ulCRC = toLE32(*((LPDWORD)((LPBYTE)pChunkHeader + PNG_CHUNK_HEADER_SIZE + chunk.dwLength)));
		unsigned long calculated_crc = crc32((LPBYTE)pChunkHeader+4, chunk.dwLength + 4);
		if (calculated_crc != chunk.ulCRC)
		{
			TRACE0("ReadPngFile()- Invalid chunk CRC\n");
			return PNG_INVALID_CRC;
		}

#ifdef _DEBUG
		TRACE0("--------------------------\n");
		TRACE0("ReadPngFile()- Chunk dump:\n");
		TRACE1(" Length: %u\n", chunk.dwLength);
		TRACE1(" Type: %.4S\n", (char*)&chunk.dwType);
		TRACE1(" CRC: 0x%08X\n", chunk.ulCRC);
#endif
		dwLeadingBytes -= dwTotalChunkSize;
		
		if (chunk.dwType == PNG_CHUNK_TYPE_IHDR)
		{
			// there is only one IHDR and be first
			if (bIHDRPresents || nChunkCounter != 0)
			{
				TRACE0("ReadPngFile()- IHDR chunk must be first and only one\n");
				return PNG_BAD_FORMAT;
			}

			bIHDRPresents = TRUE;
			chunkIHDR = chunk;

			LONG lRet = CheckAndGetImageHeader(&chunkIHDR, &imageHeader);
			if (lRet != PNG_SUCCESS)
				return lRet;
		}
		else if (chunk.dwType == PNG_CHUNK_TYPE_PLTE)
		{
			bPLTEPResents = TRUE;
			chunkPLTE = chunk;
		}
		else if (chunk.dwType == PNG_CHUNK_TYPE_IDAT)
		{
			if (pvCompressedImageData == NULL)
			{
				dwCompressedImageSize = chunk.dwLength;
				pvCompressedImageData = malloc(dwCompressedImageSize);
				if (pvCompressedImageData == NULL)
				{
					TRACE0("ReadPngFile()- Not enough memory\n");
					return PNG_NOT_ENOUGH_MEMORY;
				}

				memcpy(pvCompressedImageData, chunk.pData, chunk.dwLength);
			}
			else // merge to previous IDAT
			{
				DWORD dwNewSize = dwCompressedImageSize + chunk.dwLength;
				void* pret = realloc(pvCompressedImageData, dwNewSize);
				if (pret == NULL)
				{
					free(pvCompressedImageData);
					pvCompressedImageData = NULL;
					TRACE0("ReadPngFile()- Not enough memory\n");
					return PNG_NOT_ENOUGH_MEMORY;
				}

				pvCompressedImageData = pret;
				memcpy((LPBYTE)pvCompressedImageData + dwCompressedImageSize, chunk.pData, chunk.dwLength);
				dwCompressedImageSize = dwNewSize;
			}

			bIDATPresents = TRUE;
			
			if (imageHeader.ihColorType == PNG_COLOR_TYPE_INDEXED && !bPLTEPResents)
			{
				TRACE0("ReadPngFile()- Indexed color type requires palette chunk\n");
				return PNG_BAD_FORMAT;
			}
		}
		else if (chunk.dwType == PNG_CHUNK_TYPE_IEND)
		{
			if (
				!bIHDRPresents || !bIDATPresents ||
				(imageHeader.ihColorType == PNG_COLOR_TYPE_INDEXED && !bPLTEPResents)
				)
			{
				TRACE0("ReadPngFile()- IHDR or IDAT or PLTE not presents\n");
				return PNG_BAD_FORMAT;
			}

			break; // end of chunks, exit loop
		}

		// advance to next chunk
		nChunkCounter++;
		pChunkHeader = (PNG_CHUNK_HEADER*)((LPBYTE)pChunkHeader + dwTotalChunkSize);
	}
	// end of enum chunks ==========================

	if (
		!bIHDRPresents || !bIDATPresents ||
		(imageHeader.ihColorType == PNG_COLOR_TYPE_INDEXED && !bPLTEPResents)
		)
	{
		TRACE0("ReadPngFile()- IHDR or IDAT or PLTE not presents\n");
		return PNG_BAD_FORMAT;
	}

	if (pOutImageHeader != NULL)
	{
		*pOutImageHeader = imageHeader;
	}

#ifdef _DEBUG
	TRACE0("------------------\n");
	TRACE0("Image Header dump:\n");
	TRACE1("Bit depth: %u\n", imageHeader.ihBitDepth);
	TRACE1("Color type: %u\n", imageHeader.ihColorType);
	TRACE1("Width: %u\n", imageHeader.ihWidth);
	TRACE1("Height: %u\n", imageHeader.ihHeight);
	TRACE1("Filter: %u\n", imageHeader.ihFilterMethod);
	TRACE1("Compression: %u\n", imageHeader.ihCompressionMethod);
	TRACE1("Interlace: %u\n", imageHeader.ihInterlaceMethod);
#endif

	LONG lRet = PNG_FAIL;

	// Extract image data from IDAT chunk
	DWORD dwImageSize = 0;
	void* pvImageData = NULL; // pointer to uncompressed image data
	lRet = Uncompress(pvCompressedImageData, dwCompressedImageSize, &pvImageData, &dwImageSize);
	if (lRet != PNG_SUCCESS)
	{
		TRACE1("ReadPngFile()- UncompressImageData() failed, %d\n", lRet);
		return lRet;
	}
	ASSERT(pvImageData);
	free(pvCompressedImageData);
	pvCompressedImageData = NULL;


	lRet = UnfilterImageData(&imageHeader, (BYTE*)pvImageData);
	if (lRet != PNG_SUCCESS)
	{
		TRACE1("ReadPngFile()- UnfilterImageData() failed, %d\n", lRet);
		return lRet;
	}
	
	// Create an empty DIB used for writing pixels from png image data
	DWORD dwDIBSize = 0;
	void* pvDIBData = NULL;
	lRet = CreateEmptyDIB(&imageHeader, bPLTEPResents ? &chunkPLTE : NULL, &pvDIBData, &dwDIBSize);
	if (lRet != PNG_SUCCESS)
	{
		return lRet;
	}
	ASSERT(pvDIBData);
	
	void* pvDIBits = DIBGetBits((LPBITMAPINFO)pvDIBData);
	lRet = CopyImageDataToDIB(&imageHeader, pvImageData, dwImageSize, (LPBITMAPINFO)pvDIBData, pvDIBits);

	// clean up
	free(pvImageData);

	if (lRet != PNG_SUCCESS)
	{
		TRACE1("ReadPngFile() failed: %d\n", lRet);
		free(pvDIBData);
		return lRet;
	}

	*ppvDIBData = pvDIBData;
	*pdwDIBSize = dwDIBSize;
	return PNG_SUCCESS;
}
