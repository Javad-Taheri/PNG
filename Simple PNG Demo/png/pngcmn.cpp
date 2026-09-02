#include "stdafx.h"
#include "pngcmn.h"


///////////////////////////////////////////////////////////
// Byte Convertion Helpers
//
// These functions swap bytes between MSB and LSB

WORD swapbytes16(WORD n)
{
	return (WORD)(((n & 0x00FF) << 8) | ((n & 0xFF00) >> 8));
}

DWORD swapbytes32(DWORD n)
{
	DWORD result = 0;

	result |= (n & 0xFF000000) >> 24;
	result |= (n & 0x00FF0000) >> 8;
	result |= (n & 0x0000FF00) << 8;
	result |= (n & 0x000000FF) << 24;

	return result;
}
//
// End of Byte Convertion Helpers /////////////////////////


///////////////////////////////////////////////////////////
// DIB Helpers
//

/*Returns number of colors used in palette array*/
DWORD DIBNumColors(_In_ LPBITMAPINFO pbmi)
{
	if (pbmi->bmiHeader.biClrUsed)
		return pbmi->bmiHeader.biClrUsed;

	switch (pbmi->bmiHeader.biBitCount)
	{
	case 1: return 2;
	case 4: return 16;
	case 8: return 256;
	}

	return 0;
}

/*Returns size of palette in bytes*/
DWORD DIBPalleteSize(_In_ LPBITMAPINFO pbmi)
{
	return (DIBNumColors(pbmi) * sizeof(RGBQUAD));
}

/*Returns a pointer to DIBits*/
LPBYTE DIBGetBits(_In_ LPBITMAPINFO pbmi)
{
	return ((LPBYTE)pbmi + pbmi->bmiHeader.biSize + DIBPalleteSize(pbmi));
}

/*Returns number of bytes in each DIB scanline aligned to DWORD*/
DWORD DIBBytesPerLine(_In_ LPBITMAPINFO pbmi)
{
	DWORD stride = 0;

	switch (pbmi->bmiHeader.biBitCount)
	{
	case 1:
		stride = (pbmi->bmiHeader.biWidth + 7) / 8;
		break;
	case 4:
		stride = (pbmi->bmiHeader.biWidth + 1) / 2;
		break;
	case 8:
		stride = pbmi->bmiHeader.biWidth;
		break;
	case 16: stride = pbmi->bmiHeader.biWidth * 2;
		break;
	case 24:
		stride = pbmi->bmiHeader.biWidth * 3;
		break;
	case 32:
		stride = pbmi->bmiHeader.biWidth * 4;
		break;
	default:
		return 0;
	}

	// DWORD align
	stride = ((stride + 3) / 4) * 4;

	return stride;
}

/*Returns DIBits (actual bitmap data) size*/
DWORD DIBCalcBitsSize(_In_ LPBITMAPINFO pbmi)
{
	return DIBBytesPerLine(pbmi) * (pbmi->bmiHeader.biHeight >= 0 ? pbmi->bmiHeader.biHeight : -pbmi->bmiHeader.biHeight);
}

/*Returns number of bytes per each DIB pixel; rounded to 1 byte*/
UINT DIBBytesPerPixel(LPBITMAPINFO pbmi)
{
	return (pbmi->bmiHeader.biBitCount + 7) / 8;
}
//
// End of DIB Helpers /////////////////////////////////////


///////////////////////////////////////////////////////////
// PNG Helpers
//
/*Returns TRUE if the given bytes is the PNG singnature*/
BOOL PNGVerifySignature(_In_ void* pbytes, _In_ UINT numbytes)
{
	if (numbytes < PNG_SIGNATURE_SIZE ||
		*((DWORD*)pbytes) != PngSignature.dw1 ||
		*(((DWORD*)pbytes) + 1) != PngSignature.dw2
		)
	{
		return FALSE;
	}

	return TRUE;
}

/*Sanity check of PNG_IMAGE_HEADER and returns FALSE if has bad format*/
BOOL PNGVerifyImageHeader(_In_ PNG_IMAGE_HEADER* pihdr)
{
	ASSERT(pihdr);

	// zero width and height is not valid
	if (pihdr->ihWidth == 0 || pihdr->ihHeight == 0)
		return FALSE;

	if (pihdr->ihBitDepth < 8 && pihdr->ihInterlaceMethod != PNG_INTERLACE_SEQUENTIAL)
		return PNG_BAD_FORMAT;

	// check colortypes and bit-depths
	switch (pihdr->ihColorType)
	{
	case PNG_COLOR_TYPE_GRAYSCALE:
		if (pihdr->ihBitDepth != 1 && pihdr->ihBitDepth != 2 && pihdr->ihBitDepth != 4 && pihdr->ihBitDepth != 8 && pihdr->ihBitDepth != 16)
			return FALSE;
		break;

	case PNG_COLOR_TYPE_RGB_TRIPLE:
		if (pihdr->ihBitDepth != 8 && pihdr->ihBitDepth != 16)
			return FALSE;
		break;

	case PNG_COLOR_TYPE_INDEXED:
		if (pihdr->ihBitDepth != 1 && pihdr->ihBitDepth != 2 && pihdr->ihBitDepth != 4 && pihdr->ihBitDepth != 8)
			return FALSE;
		break;

	case PNG_COLOR_TYPE_RGBA_QUAD:
	case PNG_COLOR_TYPE_GRAYSCALE_ALPHA:
		if (pihdr->ihBitDepth != 8 && pihdr->ihBitDepth != 16)
			return FALSE;
		break;

	default:
		return FALSE;
		break;
	}

	// check compression and filter method
	if (
		pihdr->ihCompressionMethod != PNG_COMPRESSION_METHOD_INFALTE_DEFLATE ||
		pihdr->ihFilterMethod != PNG_FILTER_METHOD_ADAPTIVE
		)
	{
		return FALSE;
	}

	// check interlace method
	if (
		pihdr->ihInterlaceMethod != PNG_INTERLACE_SEQUENTIAL &&
		pihdr->ihInterlaceMethod != PNG_INTERLACE_ADAM7
		)
	{
		return FALSE;
	}

	return TRUE;
}

/*Returns number of sample per each pixel*/
UINT PNGNumSamples(_In_ PNG_IMAGE_HEADER* pihdr)
{
	switch (pihdr->ihColorType)
	{
	case PNG_COLOR_TYPE_GRAYSCALE: return 1;
	case PNG_COLOR_TYPE_RGB_TRIPLE: return 3;
	case PNG_COLOR_TYPE_INDEXED: return 1;
	case PNG_COLOR_TYPE_GRAYSCALE_ALPHA: return 2;
	case PNG_COLOR_TYPE_RGBA_QUAD: return 4;
	}

	return 0;
}

/*Returns number of bytes per each pixel; rounded to 1 byte*/
DWORD PNGBytesPerPixel(_In_ PNG_IMAGE_HEADER* pihdr)
{
	return PNGNumSamples(pihdr) * ((pihdr->ihBitDepth + 7) / 8);
}

/*Returns number of bytes per each scanline; excluded filter byte*/
DWORD PNGBytesPerLine(_In_ PNG_IMAGE_HEADER* pihdr)
{
	ASSERT(pihdr->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL);
	DWORD bytesPerLine = 0; // must be byte align
	switch (pihdr->ihBitDepth)
	{
	case 1:
		bytesPerLine = (pihdr->ihWidth + 7) / 8;
		break;

	case 2:
		bytesPerLine = (pihdr->ihWidth + 3) / 4;
		break;

	case 4:
		bytesPerLine = (pihdr->ihWidth + 1) / 2;
		break;

	case 8:
	case 16:
		bytesPerLine = pihdr->ihWidth * (PNGNumSamples(pihdr) * (pihdr->ihBitDepth / 8));
		break;
	}

	return bytesPerLine;
}

/*Returns number of scanlines for specified interlace pass*/
UINT PNGPassLines(_In_ PNG_IMAGE_HEADER* pihdr, _In_ int pass)
{
	ASSERT(pihdr->ihInterlaceMethod == PNG_INTERLACE_ADAM7);
	static const UINT pass_line_count[7][8] = {
	// %  0  1  2  3  4  5  6  7
		{ 1, 1, 1, 1, 1, 1, 1, 1 }, // pass1
		{ 1, 1, 1, 1, 1, 1, 1, 1 }, // pass2
		{ 1, 0, 0, 0, 0, 1, 1, 1 }, // pass3
		{ 2, 1, 1, 1, 1, 2, 2, 2 }, // pass4
		{ 2, 0, 0, 1, 1, 1, 1, 2 }, // pass5
		{ 4, 1, 1, 2, 2, 3, 3, 4 }, // pass6
		{ 4, 0, 1, 1, 2, 2, 3, 3 }, // pass7
	};

	UINT nPassLines = 0;

	if (pihdr->ihHeight >= 8) {
		nPassLines = ((pihdr->ihHeight / 8) * pass_line_count[pass][0]);
	}
	
	UINT remain = pihdr->ihHeight % 8;
	if (remain > 0) {
		nPassLines += pass_line_count[pass][remain];
	}

	return nPassLines;
}

/*Returns pixels count per scanline for specified pass */
UINT PNGPassLinePixels(_In_ PNG_IMAGE_HEADER* pihdr, _In_ int pass)
{
	static const UINT pass_pixel_count[7][8] = {
	// %  0  1  2  3  4  5  6  7
		{ 1, 1, 1, 1, 1, 1, 1, 1 }, // pass1
		{ 1, 0, 0, 0, 0, 1, 1, 1 }, // pass2
		{ 2, 1, 1, 1, 1, 2, 2, 2 }, // pass3
		{ 2, 0, 0, 1, 1, 1, 1, 2 }, // pass4
		{ 4, 1, 1, 2, 2, 3, 3, 4 }, // pass5
		{ 4, 0, 1, 1, 2, 2, 3, 3 }, // pass6
		{ 8, 1, 2, 3, 4, 5, 6, 7 }, // pass7
	};

	UINT nPassPixels = 0;

	if (pihdr->ihWidth >= 8) {
		nPassPixels = (pihdr->ihWidth / 8) * pass_pixel_count[pass][0];
	}

	UINT remain = pihdr->ihWidth % 8;
	if (remain > 0) {
		nPassPixels += pass_pixel_count[pass][remain];
	}
	
	return nPassPixels;
}

/*
Returns needed bytes for uncompressed IDAT chunk base on image header data
*/
DWORD CalcPngImageSize(_In_ PNG_IMAGE_HEADER* pImageHeader)
{
	ASSERT(pImageHeader != NULL);

	DWORD dwResult = 0;
	UINT nPixelSize = 0; // in bits

	switch (pImageHeader->ihColorType)
	{
	case PNG_COLOR_TYPE_GRAYSCALE:
		nPixelSize = pImageHeader->ihBitDepth;
		break;

	case PNG_COLOR_TYPE_RGB_TRIPLE:
		nPixelSize = pImageHeader->ihBitDepth * 3;
		break;

	case PNG_COLOR_TYPE_INDEXED:
		nPixelSize = pImageHeader->ihBitDepth;
		break;

	case PNG_COLOR_TYPE_GRAYSCALE_ALPHA:
		nPixelSize = pImageHeader->ihBitDepth * 2;
		break;

	case PNG_COLOR_TYPE_RGBA_QUAD:
		nPixelSize = pImageHeader->ihBitDepth * 4;
		break;
	}

	// convert to bytes & byte align
		
	if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_SEQUENTIAL)
	{
		dwResult = ((pImageHeader->ihWidth * nPixelSize * pImageHeader->ihHeight) + 7) / 8;
		// add filter bytes
		dwResult += pImageHeader->ihHeight;
	}
	else if (pImageHeader->ihInterlaceMethod == PNG_INTERLACE_ADAM7)
	{
		for (int pass = 0; pass < 7; pass++)
		{			
			UINT nLines = PNGPassLines(pImageHeader, pass);
			if (nLines > 0)
			{
				UINT nPixels = PNGPassLinePixels(pImageHeader, pass);
				if (nPixels > 0) 
				{
					dwResult += nLines * (((nPixels * nPixelSize) + 7) / 8);
					dwResult += nLines; // add filter bytes
				}
			}
		}
	}

	TRACE1("CalcPngImageSize()- Ret: %u\n", dwResult);
	return dwResult;
}

/*
 Calculates and returns Paeth filter value
*/
int PaethPredictor(_In_ int a, _In_ int b, _In_ int c)
{
	int p = a + b - c;
	int pa = abs(p - a);
	int pb = abs(p - b);
	int pc = abs(p - c);

	int pr = 0;
	if (pa <= pb && pa <= pc)
		pr = a;
	else if (pb <= pc)
		pr = b;
	else
		pr = c;

	return pr;
}
//
// End of PNG Helpers /////////////////////////////////////
