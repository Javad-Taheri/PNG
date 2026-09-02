#include "stdafx.h"
#include "pngencoder.h"
#include "crc.h"

#if defined(_WIN32)
#define ZLIB_WINAPI
#endif

#include <zlib.h>

#define ZLIB_BUF_SIZE 131072 // 128K

//=========================================================
//			   Local Functions Prototype
//
LONG Compress(_In_ void* pvIn, _In_ DWORD dwSizeIn, _Out_ void** ppvOut, _Out_ LPDWORD pdwSizeOut, _In_ int level);
BYTE SelectFilterType(_In_ BYTE* pline, _In_ UINT nBytesPerLine, _In_ UINT nBpp);
LONG FilterImageData(_In_ PNG_IMAGE_HEADER* pihdr, _In_ void* pvImage, _In_ BYTE filterType, _In_ BOOL bBest);
LONG FillPngImageHeaderFromBitmapInfo(_In_ LPBITMAPINFO pbmi, _In_ BOOL bGrayscale, _In_ BOOL bInterlace, _Out_ PNG_IMAGE_HEADER* pImageHeader);
LONG CopyPaletteFromDIB(_In_ LPBITMAPINFO pbmi, _Out_ void* pPal);
void RGB16to24(_In_ WORD rgb16, _Out_ BYTE& r, _Out_ BYTE& g, _Out_ BYTE& b);
void CopyPngPixelFromDIB(_In_ LPBITMAPINFO pbmi, _In_ BYTE* pDib, _In_ PNG_IMAGE_HEADER* pihdr, _Out_ BYTE* pPng);
LONG CopyImageDataFromDIB(_In_ LPBITMAPINFO pbmi, _In_ void* pvBits, _In_ PNG_IMAGE_HEADER* pihdr, _Out_ void* pidat, _In_ DWORD dwAvailSize);
LONG MakeChunk(_In_ DWORD dwType, _In_ void* pbytes, _In_ DWORD dwSizeofBytes, _Out_ void* pbuff, _In_ DWORD dwAvailSize);
//
//=========================================================

/*
 Compresses input buffer
*/
LONG Compress(
	_In_ void* pvIn,
	_In_ DWORD dwSizeIn,
	_Out_ void** ppvOut,
	_Out_ LPDWORD pdwSizeOut,
	_In_ int level)
{
	int ret, flush;
	unsigned have;
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

	pdest = (unsigned char*)malloc(destSize);
	if (pdest == NULL)
	{
		free(buf_in);
		free(buf_out);
		return PNG_NOT_ENOUGH_MEMORY;
	}

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, level);
	if (ret != Z_OK)
	{
		free(buf_in);
		free(buf_out);
		free(pdest);
		return PNG_FAIL;
	}

	psrc = (unsigned char*) pvIn;
	srcSize = dwSizeIn;

	/* compress until end of data */
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

		flush = (bytesRead >= dwSizeIn) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = buf_in;

		/* run deflate() on input until output buffer not full, finish
		compression if all of source has been read in */
		do {
			strm.avail_out = ZLIB_BUF_SIZE;
			strm.next_out = buf_out;
			ret = deflate(&strm, flush);    /* no bad return value */
			ASSERT(ret != Z_STREAM_ERROR);  /* state not clobbered */
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
		} while (strm.avail_out == 0);

		/* all input will be used */
		ASSERT(strm.avail_in == 0); 

	/* done when last data in file processed */
	} while (flush != Z_FINISH);
	/* stream will be complete */
	ASSERT(ret == Z_STREAM_END);

    /* clean up and return */
	deflateEnd(&strm);
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

	return PNG_FAIL;
}

/*
 Filters one byte
*/
LONG FilterByte(
	_In_ UINT lineNum,	// scanline number, 0 based
	_In_ UINT bytePos,	// byte position in scanline
	_In_ int type,		// filter type
	_In_ BYTE* pbyte,	// byte to be filter
	_In_ UINT stride,	// stride of scanline
	_In_ UINT bpp,		// byte per pixel
	_Out_ BYTE* pout	// filter byte result (pointer can be same with pbyte)
	)
{
	BYTE a = 0;
	BYTE b = 0;
	BYTE c = 0;
	BYTE prior = 0;

	switch (type)
	{
	case PNG_FILTER_TYPE_NONE:
		// prior = 0;
		break;

	case PNG_FILTER_TYPE_SUB:
		if (bytePos >= bpp) {
			prior = *(pbyte - bpp);
		}
		break;

	case PNG_FILTER_TYPE_UP:
		if (lineNum > 0) {
			prior = *(pbyte - stride - 1);
		}
		break;

	case PNG_FILTER_TYPE_AVERAGE:
		if (bytePos >= bpp) {
			a = *(pbyte - bpp);
		}

		if (lineNum > 0) {
			b = *(pbyte - stride - 1);
		}

		prior = (a + b) / 2;
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

		prior = PaethPredictor(a, b, c);
		break;

	default:
		TRACE0("FilterByte()- Wrong filter type\n");
		ASSERT(FALSE);
//		return PNG_BAD_FORMAT;
		break;
	}

	*pout = *pbyte - prior;

	return PNG_SUCCESS;
}

/*
 Returns best filter type for input scanline.
 pline must point to the first of scanline after the filter byte
 
 To select best filter type we calc each scanline by all types.
 Whitch filter is better that have smallest sum result of filtered bytes.
*/
BYTE SelectFilterType(_In_ BYTE* pline, _In_ UINT nBytesPerLine, _In_ UINT nBpp)
{
	struct filter_result_pair {
		BYTE	type;
		DWORD	result;
	} filters[5] = {
		{ PNG_FILTER_TYPE_NONE,		0 },
		{ PNG_FILTER_TYPE_SUB,		0 },
		{ PNG_FILTER_TYPE_UP,		0 },
		{ PNG_FILTER_TYPE_AVERAGE,	0 },
		{ PNG_FILTER_TYPE_PAETH,	0 },
	};

	UINT f;
	for (f = 0; f < 5; f++)
	{
		BYTE* pin = (BYTE*)pline + nBytesPerLine - 1;
		for (UINT n = nBytesPerLine; n > 0; n--)
		{
			BYTE b;
			FilterByte(0, n - 1, filters[f].type, pin--, nBytesPerLine, nBpp, &b);
			filters[f].result += b;
		}
	}

	BYTE best = 0;
	for (f = 0; f < 5; f++) {
		if (filters[f].result < filters[best].result)
			best = f;
	}

	return filters[best].type;
}

/*
 Filters png image data
*/
LONG FilterImageData(
	_In_ PNG_IMAGE_HEADER* pihdr, 
	_In_ void* pvImageData,  
	_In_ BYTE filterType, 
	_In_ BOOL bBest)
{
	ASSERT(pihdr != NULL && pvImageData != NULL);

	UINT nBpp = PNGBytesPerPixel(pihdr);
	if (nBpp == 0)
	{
		TRACE0("FilterImageData()- Zero Bpp\n");
		return PNG_BAD_FORMAT;
	}

	BYTE* ptr = NULL;

	// to filtering image data, we must start from bottom-right pixel

	if (pihdr->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
	{
		UINT nBytesPerLine = PNGBytesPerLine(pihdr);
		for (UINT nLine = pihdr->ihHeight; nLine > 0; nLine--)
		{
			ptr = (BYTE*)pvImageData + ((nLine - 1) * (nBytesPerLine + 1/*1 for filter byte*/));
			BYTE* pFilterType = ptr++;

			if (bBest)
				*pFilterType = SelectFilterType(ptr, nBytesPerLine, nBpp);
			else
				*pFilterType = filterType;

			// set pointer to end of scanline
			ptr += (nBytesPerLine - 1);

			for (UINT nByte = nBytesPerLine; nByte > 0; nByte--)
			{
				if (FilterByte(nLine - 1, nByte - 1, *pFilterType, ptr, nBytesPerLine, nBpp, ptr) != PNG_SUCCESS)
					return PNG_BAD_FORMAT;
				ptr--;
			}
		}
	}
	/*end of if (PNG_INTERLACE_SEQUENTIAL)*/
	else if (pihdr->ihInterlaceMethod == PNG_INTERLACE_ADAM7)
	{
		ASSERT(pihdr->ihBitDepth >= 8);

		BYTE* pPassData = (BYTE*)pvImageData;

		for (int pass = 0; pass < 7; pass++)
		{
			UINT nPassLines = PNGPassLines(pihdr, pass);
			UINT nPassPixels = PNGPassLinePixels(pihdr, pass);

			// don't process empty passes
			if (nPassLines > 0 && nPassPixels > 0)
			{
				UINT nBytesInLine = nPassPixels * nBpp;
				UINT nPassSize = nPassLines * (nBytesInLine + 1/*1 for filter byte*/);

				for (UINT nLine = nPassLines; nLine > 0; nLine--)
				{
					ptr = (BYTE*)pPassData + ((nLine - 1) * (nBytesInLine + 1));
					BYTE* pFilterType = ptr++;

					if (bBest)
						*pFilterType = SelectFilterType(ptr, nBytesInLine, nBpp);
					else
						*pFilterType = filterType;

					// set pointer to end of pass scanline
					ptr += (nBytesInLine - 1);

					for (UINT nByte = nBytesInLine; nByte > 0; nByte--)
					{
						if (FilterByte(nLine - 1, nByte - 1, *pFilterType, ptr, nBytesInLine, nBpp, ptr) != PNG_SUCCESS)
							return PNG_BAD_FORMAT;
						ptr--;
					}
				}

				pPassData += nPassSize;
			}
		}/*end of for(pass)*/
	}
	/*end of if (PNG_INTERLACE_ADAM7)*/
	else
	{
		TRACE0("FilterImageData()- Invalid interlace method\n");
		return PNG_BAD_FORMAT;
	}
	/*end of if (interlace_method)*/

	return PNG_SUCCESS;
}

/*
 Fill pImageHeader with equvalent values from pbmi
*/
LONG FillPngImageHeaderFromBitmapInfo(
	_In_ LPBITMAPINFO pbmi, 
	_In_ BOOL bGrayscale, 
	_In_ BOOL bInterlace,
	_Out_ PNG_IMAGE_HEADER* pImageHeader)
{
	ASSERT(pbmi != NULL && pImageHeader != NULL);
	ASSERT(pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER));

	if (pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
	{
		TRACE0("FillPngImageHeaderFromBitmapInfo()- Invalid BITMAPINFOHEADER size\n");
		return PNG_BAD_FORMAT;
	}

	pImageHeader->ihWidth = pbmi->bmiHeader.biWidth;
	pImageHeader->ihHeight = (pbmi->bmiHeader.biHeight >= 0) ? pbmi->bmiHeader.biHeight : -pbmi->bmiHeader.biHeight;
	
	switch (pbmi->bmiHeader.biBitCount)
	{
	case 1:
	case 4:
	case 8:
		pImageHeader->ihBitDepth = pbmi->bmiHeader.biBitCount;
		pImageHeader->ihColorType = !bGrayscale ? PNG_COLOR_TYPE_INDEXED : PNG_COLOR_TYPE_GRAYSCALE;
		break;

	case 16:
	case 24:	
		pImageHeader->ihBitDepth = 8;
		pImageHeader->ihColorType = !bGrayscale ? PNG_COLOR_TYPE_RGB_TRIPLE : PNG_COLOR_TYPE_GRAYSCALE;
		break;

	case 32:
		pImageHeader->ihBitDepth = 8;
		pImageHeader->ihColorType = !bGrayscale ? PNG_COLOR_TYPE_RGBA_QUAD : PNG_COLOR_TYPE_GRAYSCALE_ALPHA;
		break;

	default:
		TRACE0("FillPngImageHeaderFromBitmapInfo()- Invalid BITMAPINFOHEADER bit count\n");
		return PNG_BAD_FORMAT;
		break;
	}

	pImageHeader->ihFilterMethod = PNG_FILTER_METHOD_ADAPTIVE;
	pImageHeader->ihCompressionMethod = PNG_COMPRESSION_METHOD_INFALTE_DEFLATE;
	if (!bInterlace)
		pImageHeader->ihInterlaceMethod = PNG_INTERLACE_SEQUENTIAL;
	else if (pbmi->bmiHeader.biBitCount >= 8)
		pImageHeader->ihInterlaceMethod = PNG_INTERLACE_ADAM7;
	else
	{
		TRACE0("FillPngImageHeaderFromBitmapInfo()- Less than 8-bit image can not be interlaced\n");
		return PNG_BAD_FORMAT;
	}

	return PNG_SUCCESS;
}

/*
 Copy palette entries from DIB to PNG palette
*/
LONG CopyPaletteFromDIB(_In_ LPBITMAPINFO pbmi, _Out_ void* pPal)
{
	ASSERT(pbmi != NULL && pPal != NULL);
	ASSERT(pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER));
	ASSERT(pbmi->bmiHeader.biBitCount <= 8);

	// is there palette?
	if (pbmi->bmiHeader.biBitCount > 8)
	{
		TRACE0("CopyPaletteFromDIB()- Invalid bit count. bit count must less than 16\n");
		return PNG_BAD_FORMAT;
	}

	// max palette entry is 256
	UINT numColors = min(256, DIBNumColors(pbmi));
	PNG_PALETTE_ENTRY* pEntry = (PNG_PALETTE_ENTRY*)pPal;
	for (UINT n = 0; n < 256; n++)
	{
		if (n < numColors)
		{
			pEntry->peRed = pbmi->bmiColors[n].rgbRed;
			pEntry->peGreen = pbmi->bmiColors[n].rgbGreen;
			pEntry->peBlue = pbmi->bmiColors[n].rgbBlue;
		}
		else
		{
			pEntry->peRed = 0;
			pEntry->peGreen = 0;
			pEntry->peBlue = 0;
		}

		pEntry++;
	}

	return PNG_SUCCESS;
}

/*
 Convert 16bit rgb to 24 bit
*/
void RGB16to24(_In_ WORD rgb16, _Out_ BYTE& r, _Out_ BYTE& g, _Out_ BYTE& b)
{
	WORD R_Mask = 0x00007c00;
	WORD G_Mask = 0x000003e0;
	WORD B_Mask = 0x0000001f;

	r = 8 * ((rgb16 & R_Mask) >> 10);
	g = 8 * ((rgb16 & G_Mask) >> 5);
	b = 8 * (rgb16 & B_Mask);
}

/*
 Copy and convert one DIB pixel into PNG format
*/
void CopyPngPixelFromDIB(_In_ LPBITMAPINFO pbmi, _In_ BYTE* pDib, _In_ PNG_IMAGE_HEADER* pihdr, _Out_ BYTE* pPng)
{
	if (pihdr->ihBitDepth <= 8)
	{
		if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE)
		{
			BYTE gray = 0;
			if (pbmi->bmiHeader.biBitCount == 24)
			{
				DIB_RGB_TRIPLE* prgbDib = (DIB_RGB_TRIPLE*)pDib;
				gray = (prgbDib->rgbRed + prgbDib->rgbGreen + prgbDib->rgbBlue) / 3;
			}
			else if (pbmi->bmiHeader.biBitCount == 16)
			{
				BYTE r, g, b;
				RGB16to24(*(WORD*)pDib, r, g, b);
				gray = (r + g + b) / 3;
			}
			else if (pbmi->bmiHeader.biBitCount == 8)
			{
				gray = (pbmi->bmiColors[*pDib].rgbRed + pbmi->bmiColors[*pDib].rgbGreen + pbmi->bmiColors[*pDib].rgbBlue) / 3;
			}
			else if (pbmi->bmiHeader.biBitCount == 4)
			{
				BYTE b = *pDib;
				gray = ((b & 0x30) << 2) | ((b & 0x03) << 4);
				b = *(pDib + 1);
				gray |= ((b & 0x30) >> 2) | (b & 0x03);
			}

			*pPng = gray;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGB_TRIPLE)
		{
			PNG_RGB_TRIPLE* prgbPng = (PNG_RGB_TRIPLE*)pPng;

			if (pbmi->bmiHeader.biBitCount == 24)
			{
				DIB_RGB_TRIPLE* prgbDib = (DIB_RGB_TRIPLE*)pDib;

				prgbPng->rgbRed = prgbDib->rgbRed;
				prgbPng->rgbGreen = prgbDib->rgbGreen;
				prgbPng->rgbBlue = prgbDib->rgbBlue;
			}
			else if (pbmi->bmiHeader.biBitCount == 16)
			{
				RGB16to24(*(WORD*)pDib, prgbPng->rgbRed, prgbPng->rgbGreen, prgbPng->rgbBlue);
			}
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_INDEXED)
		{
			if (pihdr->ihBitDepth != 2)
			{
				*pPng = *pDib;
			}
			else
			{
				BYTE b = *pDib;
				*pPng = ((b & 0x30) << 2) | ((b & 0x03) << 4);
				b = *(pDib + 1);
				*pPng |= ((b & 0x30) >> 2) | (b & 0x03);
			}
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE_ALPHA)
		{
			PNG_GRAYSCALE_ALPHA* pgaPng = (PNG_GRAYSCALE_ALPHA*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			pgaPng->gaGray = (prgbaDib->rgbaRed + prgbaDib->rgbaGreen + prgbaDib->rgbaBlue) / 3;
			pgaPng->gaAlpha = prgbaDib->rgbaAlpha;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGBA_QUAD)
		{
			PNG_RGBA_QUAD* prgbaPng = (PNG_RGBA_QUAD*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			prgbaPng->rgbaRed = prgbaDib->rgbaRed;
			prgbaPng->rgbaGreen = prgbaDib->rgbaGreen;
			prgbaPng->rgbaBlue = prgbaDib->rgbaBlue;
			prgbaPng->rgbaAlpha = prgbaDib->rgbaAlpha;
		}
	}
	else if (pihdr->ihBitDepth == 16)
	{
		if (pihdr->ihColorType == PNG_COLOR_TYPE_RGB_TRIPLE)
		{
			PNG_RGB_TRIPLE_16BIT* prgbPng = (PNG_RGB_TRIPLE_16BIT*)pPng;

			if (pbmi->bmiHeader.biBitCount == 24)
			{
				DIB_RGB_TRIPLE* prgbDib = (DIB_RGB_TRIPLE*)pDib;

				prgbPng->rgbRed = prgbDib->rgbRed;
				prgbPng->rgbGreen = prgbDib->rgbGreen;
				prgbPng->rgbBlue = prgbDib->rgbBlue;
			}
			else if (pbmi->bmiHeader.biBitCount == 16)
			{
				BYTE r, g, b;
				RGB16to24(*(WORD*)pDib, r, g, b);
				prgbPng->rgbRed = r;
				prgbPng->rgbGreen = g;
				prgbPng->rgbBlue = b;
			}
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_GRAYSCALE_ALPHA)
		{
			PNG_GRAYSCALE_ALPHA_16BIT* pgaPng = (PNG_GRAYSCALE_ALPHA_16BIT*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			pgaPng->gaGray = (prgbaDib->rgbaRed + prgbaDib->rgbaGreen + prgbaDib->rgbaBlue) / 3;
			pgaPng->gaAlpha = prgbaDib->rgbaAlpha;
		}
		else if (pihdr->ihColorType == PNG_COLOR_TYPE_RGBA_QUAD)
		{
			PNG_RGBA_QUAD_16BIT* prgbaPng = (PNG_RGBA_QUAD_16BIT*)pPng;
			DIB_RGBA_QUAD* prgbaDib = (DIB_RGBA_QUAD*)pDib;

			prgbaPng->rgbaRed = prgbaDib->rgbaRed;
			prgbaPng->rgbaGreen = prgbaDib->rgbaGreen;
			prgbaPng->rgbaBlue = prgbaDib->rgbaBlue;
			prgbaPng->rgbaAlpha = prgbaDib->rgbaAlpha;
		}
	}
}

/*
 Copy image data (IDAT) from DIB without filtering
*/
LONG CopyImageDataFromDIB(
	_In_ LPBITMAPINFO pbmi,
	_In_ void* pvBits,
	_In_ PNG_IMAGE_HEADER* pihdr,
	_Out_ void* pidat,
	_In_ DWORD dwAvailSize)
{
	ASSERT(pbmi != NULL && pvBits != NULL && pihdr != NULL && pidat != NULL);
	ASSERT(pbmi->bmiHeader.biWidth == pihdr->ihWidth);
	ASSERT(abs(pbmi->bmiHeader.biHeight) == pihdr->ihHeight);

	DWORD dwImageSize = CalcPngImageSize(pihdr);
	if (dwAvailSize < dwImageSize)
	{
		ASSERT(FALSE);
		return PNG_FAIL;
	}

	static const UINT pass_first_pixel_x[7] = { 0, 4, 0, 2, 0, 1, 0 };
	static const UINT pass_next_pixel_x[7]  = { 8, 8, 4, 4, 2, 2, 1 };
	static const UINT pass_first_pixel_y[7] = { 0, 0, 4, 0, 2, 0, 1 };
	static const UINT pass_next_pixel_y[7]  = { 8, 8, 8, 4, 4, 2, 2 };

	UINT dibBpp = DIBBytesPerPixel(pbmi);
	UINT pngBpp = PNGBytesPerPixel(pihdr);
	if (dibBpp == 0 || pngBpp == 0)
	{
		TRACE0("CopyImageDataFromDIB()- Zero Bpp\n");
		return PNG_BAD_FORMAT;
	}

	UINT dibStride = DIBBytesPerLine(pbmi);

	BYTE* pDibByte = NULL;
	BYTE* pPngByte = (BYTE*)pidat;

	if (pihdr->ihBitDepth >= 8)
	{
		if (pihdr->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
		{
			for (UINT y = 0; y < pihdr->ihHeight; y++)
			{
				// write filter byte
				*pPngByte++ = PNG_FILTER_TYPE_NONE;
				
				pDibByte = (BYTE*)pvBits;				
				if (pbmi->bmiHeader.biHeight >= 0) // if bottom-top bitmap
					pDibByte += (pbmi->bmiHeader.biHeight - y - 1) * dibStride;
				else
					pDibByte += y * dibStride;

				for (UINT x = 0; x < pihdr->ihWidth ; x++)
				{
					CopyPngPixelFromDIB(pbmi, pDibByte, pihdr, pPngByte);

					pDibByte += dibBpp;
					pPngByte += pngBpp;
				}
			}
		}
		else if (pihdr->ihInterlaceMethod == PNG_INTERLACE_ADAM7)
		{
			for (int pass = 0; pass < 7; pass++)
			{
				UINT nPassLines = PNGPassLines(pihdr, pass);
				UINT nPassPixels = PNGPassLinePixels(pihdr, pass);

				// don't process empty passes
				if (nPassLines > 0 && nPassPixels > 0)
				{
					UINT y = pass_first_pixel_y[pass];

					for (UINT nLine = 0; nLine < nPassLines; nLine++)
					{
						// write filter byte
						*pPngByte++ = PNG_FILTER_TYPE_NONE;
						
						pDibByte = (BYTE*)pvBits;
						if (pbmi->bmiHeader.biHeight >= 0) // if bottom-top bitmap
							pDibByte += (pbmi->bmiHeader.biHeight - y - 1) * dibStride;
						else
							pDibByte += y * dibStride;

						pDibByte += pass_first_pixel_x[pass] * dibBpp;

						for (UINT nPixel = 0; nPixel < nPassPixels; nPixel++)
						{
							CopyPngPixelFromDIB(pbmi, pDibByte, pihdr, pPngByte);

							pDibByte += pass_next_pixel_x[pass] * dibBpp;

							pPngByte += pngBpp;
						}

						y += pass_next_pixel_y[pass];
					}
				}
			}/*end of for(pass)*/
		}
		/*end of if (PNG_INTERLACE_ADAM7)*/
		else
		{
			TRACE0("CopyImageDataFromDIB()- Invalid interlace method\n");
			return PNG_BAD_FORMAT;
		}
		/*end of if(interlaceMethod)*/
	}
	/*end of if (bith-depth >= 8)*/
	else if (pihdr->ihBitDepth < 8)
	{
		if (pihdr->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
		{
			UINT dibIncrement = pihdr->ihBitDepth != 2 ? 1 : 2;
			UINT nBytesPerLine = PNGBytesPerLine(pihdr);
			if (nBytesPerLine == 0)
				return PNG_BAD_FORMAT;

			for (UINT y = 0; y < pihdr->ihHeight; y++)
			{
				*pPngByte = PNG_FILTER_TYPE_NONE;
				pPngByte++;

				pDibByte = (BYTE*)pvBits;
				if (pbmi->bmiHeader.biHeight >= 0) // if bottom-top bitmap
					pDibByte += (pbmi->bmiHeader.biHeight - y - 1) * dibStride;
				else
					pDibByte += y * dibStride;

				for (UINT nByte = 0; nByte < nBytesPerLine; nByte++)
				{
					CopyPngPixelFromDIB(pbmi, pDibByte, pihdr, pPngByte);

					pPngByte++;
					pDibByte+= dibIncrement;
				}
			}
		}
		else
		{
			TRACE0("CopyImageDataFromDIB()- Less than 8-bit image can not be interlaced\n");
			return PNG_BAD_FORMAT;
		}
	}
	/*end of if (bith-depth < 8)*/

	return PNG_SUCCESS;
}


/*
 Make the PNG chunk from bytes.
 If pbytes is not NULL, the function copeis bytes from.
 If pbytes is NULL, the function skip bytes and only write chunk header and crc
*/
LONG MakeChunk(
	_In_ DWORD dwType,	// chunk type
	_In_ void* pbytes,	// bytes to write
	_In_ DWORD dwSizeofBytes,	// size of pBytes
	_Out_ void* pbuff,		// pointer to buffer that chunk bytes writes on
	_In_ DWORD dwAvailSize // size of available pbuff
	)
{
	ASSERT(pbuff != NULL);
	ASSERT(dwAvailSize >= TOTAL_CHUNK_SIZE(dwSizeofBytes));

	PNG_CHUNK_HEADER chdr;
	LPBYTE ptr = (LPBYTE)pbuff;
	ULONG crc;

	if (dwAvailSize < TOTAL_CHUNK_SIZE(dwSizeofBytes))
	{
		TRACE0("MakeChunk()- Insufficient buffer size\n");
		return PNG_FAIL;
	}

	// write the header
	chdr.chLength = toBE32(dwSizeofBytes);
	chdr.chType.dw = dwType;		
	memcpy(ptr, &chdr, PNG_CHUNK_HEADER_SIZE);
	ptr += PNG_CHUNK_HEADER_SIZE;

	// write the data
	if (pbytes != NULL) {
		memcpy(ptr, pbytes, dwSizeofBytes);
	}
	ptr += dwSizeofBytes;

	// write CRC
	crc = crc32((LPBYTE)pbuff + 4, dwSizeofBytes + 4);
	*((ULONG*)ptr) = toBE32(crc);

	return PNG_SUCCESS;
}

LONG WritePngFile(
	_In_ LPCTSTR lpszFileName, 
	_In_ LPVOID pvDIBData, 
	_In_ BOOL bGrayscale,
	_In_ BOOL bInterlace,
	_In_ int nFilter,
	_In_ int nCompression
)
{
	ASSERT(lpszFileName != NULL && pvDIBData != NULL);
	if (lpszFileName == NULL || pvDIBData == NULL)
		return PNG_INVALID_ARG;


	TRACE1("WritePngFile()- FileName: %s\n", lpszFileName);

	LONG lRet = PNG_FAIL;
	DWORD dwLastError = GetLastError();
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD dwWritten = 0;
	void* pvPngData = NULL;
	DWORD dwPngSize = 0;

	__try
	{
		hFile = CreateFile(lpszFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			dwLastError = GetLastError();
			TRACE1("WritePngFile()- Failed to CreateFile, error: 0x%08x\n", dwLastError);
			lRet = PNG_WIN32_ERROR;
			return lRet;
		}

		lRet = WritePngFile(&pvPngData, &dwPngSize, pvDIBData, bGrayscale, bInterlace, nFilter, nCompression);
		if (lRet != PNG_SUCCESS)
		{
			return lRet;
		}

		if (!WriteFile(hFile, pvPngData, dwPngSize, &dwWritten, NULL) || dwWritten != dwPngSize)
		{
			dwLastError = GetLastError();
			TRACE1("WritePngFile()- Failed to WriteFile, error: 0x%08x\n", dwLastError);
			lRet = PNG_WIN32_ERROR;
			return lRet;
		}

		// all things done!
		lRet = PNG_SUCCESS;
	}
	__finally
	{
		if (hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);

		if (pvPngData != NULL)
			free(pvPngData);

		// if wrong file, delete it!
		if (lRet != PNG_SUCCESS)
			DeleteFile(lpszFileName);

		SetLastError(dwLastError);
	}

	return lRet;
}

LONG WritePngFile(
	_In_ LPVOID* ppvPNGData, 
	_In_ LPDWORD pdwPNGSize, 
	_In_ LPVOID pvDIBData, 
	_In_ BOOL bGrayscale,
	_In_ BOOL bInterlace,
	_In_ int nFilter,
	_In_ int nCompression
	)
{
	ASSERT(ppvPNGData != NULL && pdwPNGSize != NULL && pvDIBData != NULL);
	if (ppvPNGData == NULL || pdwPNGSize == NULL || pvDIBData == NULL)
		return PNG_INVALID_ARG;

	*ppvPNGData = NULL;
	*pdwPNGSize = 0;

	LONG lRet = PNG_FAIL;
	DWORD dwPngSize = 0;
	LPBYTE pPngData = NULL;
	LPBITMAPINFO pbmi = (LPBITMAPINFO)pvDIBData;
	PNG_IMAGE_HEADER ihdr;
	void* pvPalette = NULL;
	DWORD dwPaletteSize = 0;
	void* pvImageData = NULL;
	DWORD dwImageSize = 0;
	void* pvCompressedImageData = NULL;
	DWORD dwCompressedImageSize = 0;
	DWORD dwTotalChunkSize = 0;


	// select proper filter type
	BOOL bBestFilter = FALSE;
	BYTE pngFilterType = PNG_FILTER_TYPE_PAETH;
	switch (nFilter)
	{
	case WPF_FilterNone: pngFilterType = PNG_FILTER_TYPE_NONE; break;
	case WPF_FilterSub: pngFilterType = PNG_FILTER_TYPE_SUB; break;
	case WPF_FilterUp: pngFilterType = PNG_FILTER_TYPE_UP; break;
	case WPF_FilterAverage: pngFilterType = PNG_FILTER_TYPE_AVERAGE; break;
	case WPF_FilterPaeth: pngFilterType = PNG_FILTER_TYPE_PAETH; break;	
	case WPF_FilterBest: bBestFilter = TRUE; break;
	}

	// select proper zlib compression level
	int zlibLevel = Z_DEFAULT_COMPRESSION;
	switch (nCompression)
	{
	case WPF_NoCompression: zlibLevel = Z_NO_COMPRESSION; break;
	case WPF_BestSpeed: zlibLevel = Z_BEST_SPEED; break;
	case WPF_BestCompression: zlibLevel = Z_BEST_COMPRESSION; break;
	}

	__try
	{
		// fill png image header fields by bitmap info
		lRet = FillPngImageHeaderFromBitmapInfo(pbmi, bGrayscale, bInterlace, &ihdr);
		if (lRet != PNG_SUCCESS)
			return lRet;

		if (PNGVerifyImageHeader(&ihdr) == FALSE)
		{
			TRACE0("WritePngFile()- Not valid IHDR\n");
			return PNG_BAD_FORMAT;
		}

		dwPngSize = PNG_SIGNATURE_SIZE + TOTAL_CHUNK_SIZE(PNG_IHDR_SIZE);

		// do we need palette?
		if (ihdr.ihColorType == PNG_COLOR_TYPE_INDEXED)
		{
			// Create palette
			dwPaletteSize = 256 * sizeof(PNG_PALETTE_ENTRY);
			pvPalette = malloc(dwPaletteSize);
			if (pvPalette == NULL)
			{
				lRet = PNG_NOT_ENOUGH_MEMORY;
				return lRet;
			}

			lRet = CopyPaletteFromDIB(pbmi, pvPalette);
			if (lRet != PNG_SUCCESS)
				return lRet;

			dwPngSize += TOTAL_CHUNK_SIZE(dwPaletteSize);
		}


		// Create image data
		dwImageSize = CalcPngImageSize(&ihdr);
		pvImageData = malloc(dwImageSize);
		if (pvImageData == NULL)
		{
			lRet = PNG_NOT_ENOUGH_MEMORY;
			return lRet;
		}

		// Copy image pixels from input DIB
		lRet = CopyImageDataFromDIB(pbmi, DIBGetBits(pbmi), &ihdr, pvImageData, dwImageSize);
		if (lRet != PNG_SUCCESS)
			return lRet;

		lRet = FilterImageData(&ihdr, pvImageData, pngFilterType, bBestFilter);
		if (lRet != PNG_SUCCESS)
			return lRet;

		// Compress image data
		lRet = Compress(pvImageData, dwImageSize, &pvCompressedImageData, &dwCompressedImageSize, zlibLevel);
		if (lRet != PNG_SUCCESS)
			return lRet;

		free(pvImageData);
		pvImageData = NULL;

		dwPngSize += TOTAL_CHUNK_SIZE(dwCompressedImageSize);


		// for IEND
		dwPngSize += PNG_EMPTY_CHUNK_SIZE;

		// allocate memory for whole png file
		pPngData = (LPBYTE)malloc(dwPngSize);

		// pointer to memory equvalent to png file position
		LPBYTE pFilePtr = pPngData;
		
		// Write png file signature =======================
		memcpy(pFilePtr, PngSignature.bytes, PNG_SIGNATURE_SIZE);
		pFilePtr += PNG_SIGNATURE_SIZE;
		// ================================================


		// Write IHDR chunk ===============================
		// temporary changes byte order of some fields...
		dwTotalChunkSize = TOTAL_CHUNK_SIZE(PNG_IHDR_SIZE);
		ihdr.ihWidth = toBE32(ihdr.ihWidth);
		ihdr.ihHeight = toBE32(ihdr.ihHeight);
		lRet = MakeChunk(PNG_CHUNK_TYPE_IHDR, &ihdr, PNG_IHDR_SIZE, pFilePtr, dwPngSize - (pFilePtr - pPngData));
		// restore byte order
		ihdr.ihWidth = toLE32(ihdr.ihWidth);
		ihdr.ihHeight = toLE32(ihdr.ihHeight);
		if (lRet != PNG_SUCCESS)
			return lRet;
		pFilePtr += dwTotalChunkSize;
		// ================================================


		// Write PLTE chunk ===============================
		// do we need palette?
		if (ihdr.ihColorType == PNG_COLOR_TYPE_INDEXED)
		{
			ASSERT(pvPalette != NULL && dwPaletteSize != 0);
			dwTotalChunkSize = TOTAL_CHUNK_SIZE(dwPaletteSize);

			lRet = MakeChunk(PNG_CHUNK_TYPE_PLTE, pvPalette, dwPaletteSize, pFilePtr, dwPngSize - (pFilePtr - pPngData));
			if (lRet != PNG_SUCCESS)
				return lRet;

			free(pvPalette);
			pvPalette = NULL;

			pFilePtr += dwTotalChunkSize;
		}
		// ================================================


		// Write IDAT chunk ===============================
		dwTotalChunkSize = TOTAL_CHUNK_SIZE(dwCompressedImageSize);

		lRet = MakeChunk(PNG_CHUNK_TYPE_IDAT, pvCompressedImageData, dwCompressedImageSize, pFilePtr, dwPngSize - (pFilePtr - pPngData));
		if (lRet != PNG_SUCCESS)
			return lRet;

		free(pvCompressedImageData);
		pvCompressedImageData = NULL;

		pFilePtr += dwTotalChunkSize;
		// ================================================


		// Write IEND chunk ===============================
		dwTotalChunkSize = PNG_EMPTY_CHUNK_SIZE;
		lRet = MakeChunk(PNG_CHUNK_TYPE_IEND, NULL, 0, pFilePtr, dwPngSize - (pFilePtr - pPngData));
		if (lRet != PNG_SUCCESS)
			return lRet;
		// ================================================


		// all things done!
		*ppvPNGData = pPngData;
		*pdwPNGSize = dwPngSize;
		lRet = PNG_SUCCESS;
	}
	__finally
	{
		// Clean up

		if (pvPalette != NULL)
			free(pvPalette);

		if (pvImageData != NULL)
			free(pvImageData);

		if (pvCompressedImageData != NULL)
			free(pvCompressedImageData);

		if (lRet != PNG_SUCCESS && pPngData != NULL)
		{
			free(pPngData);
		}
	}

	return lRet;
}
