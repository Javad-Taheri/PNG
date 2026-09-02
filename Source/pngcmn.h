#ifndef _PNGCMN_H_
#define _PNGCMN_H_

#pragma once

/*===========================================================================*/
/*			                    PNG Commons                                  */
/*---------------------------------------------------------------------------*/
/* Author: Javad Taheri
/* e-mail: mrjavadtaheri@gmail.com
/* License: Free to use.
/*===========================================================================*/


/* If SAL (source code annotations language) not supported */
#ifndef _SAL_VERSION
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Outptr_result_maybenull_
#endif


// PNG status codes returned by functions
#define PNG_SUCCESS				0L
#define PNG_FAIL				1L
#define PNG_INVALID_ARG			2L
#define PNG_WIN32_ERROR			3L
#define PNG_NOT_PNG				4L
#define PNG_INVALID_CRC			5L
#define PNG_BAD_FORMAT			6L
#define PNG_UNSUPPORTED			7L
#define PNG_NOT_ENOUGH_MEMORY	8L


//
// PNG definiations
//

static const struct
{
	union {
		unsigned char bytes[8];
		struct {
			DWORD dw1;
			DWORD dw2;
		};
	};

} PngSignature = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

#define PNG_SIGNATURE_SIZE		8
#define PNG_EMPTY_CHUNK_SIZE	12
#define TOTAL_CHUNK_SIZE(datalen) ((datalen) + PNG_EMPTY_CHUNK_SIZE)

// Color types
#define PNG_COLOR_TYPE_GRAYSCALE		0x00
#define PNG_COLOR_TYPE_RGB_TRIPLE		0x02
#define PNG_COLOR_TYPE_INDEXED			0x03
#define PNG_COLOR_TYPE_GRAYSCALE_ALPHA	0x04
#define PNG_COLOR_TYPE_RGBA_QUAD		0x06

// There is only one compression method
#define PNG_COMPRESSION_METHOD_INFALTE_DEFLATE	0x00

// There is only one filter method
#define PNG_FILTER_METHOD_ADAPTIVE	0x00

// Filter sub types
#define PNG_FILTER_TYPE_NONE		0x00
#define PNG_FILTER_TYPE_SUB			0x01
#define PNG_FILTER_TYPE_UP			0x02
#define PNG_FILTER_TYPE_AVERAGE		0x03
#define PNG_FILTER_TYPE_PAETH		0x04

// Interlace methods
#define PNG_INTERLACE_SEQUENTIAL	0x00 // no interlaced
#define PNG_INTERLACE_ADAM7			0x01


//
// PNG structures
//

#pragma pack(push, 1)

// Chunk type format
typedef struct tagPNG_CHUNK_TYPE
{
	union {
		char	bytes[4];
		DWORD	dw;
	};
} PNG_CHUNK_TYPE;

#define PNG_CHUNK_TYPE_SIZE		4

// Chunk header format
typedef struct tagPNG_CHUNK_HEADER
{
	unsigned long	chLength;
	PNG_CHUNK_TYPE	chType;
} PNG_CHUNK_HEADER;

#define PNG_CHUNK_HEADER_SIZE	8

// IHDR chunck format
typedef struct tagPNG_IMAGE_HEADER
{
	unsigned long ihWidth;
	unsigned long ihHeight;
	unsigned char ihBitDepth;
	unsigned char ihColorType;
	unsigned char ihCompressionMethod;
	unsigned char ihFilterMethod;
	unsigned char ihInterlaceMethod;
} PNG_IMAGE_HEADER;

#define PNG_IHDR_SIZE			13

// PLTE chunk format
typedef struct tagPNG_PALETTE_ENTRY
{
	unsigned char peRed;
	unsigned char peGreen;
	unsigned char peBlue;
} PNG_PALETTE_ENTRY, *PPNG_PALETTE_ENTRY;

// PNG image grayscale+alpha pixel format
typedef struct tagPNG_GRAYSCALE_ALPHA
{
	unsigned char gaGray;
	unsigned char gaAlpha;
} PNG_GRAYSCALE_ALPHA;

typedef struct tagPNG_GRAYSCALE_ALPHA_16BIT
{
	unsigned short gaGray;
	unsigned short gaAlpha;
} PNG_GRAYSCALE_ALPHA_16BIT;


// PNG RGB pixel format
typedef struct tagPNG_RGB_TRIPLE
{
	unsigned char rgbRed;
	unsigned char rgbGreen;
	unsigned char rgbBlue;
} PNG_RGB_TRIPLE;

typedef struct tagPNG_RGB_TRIPLE_16BIT
{
	unsigned short rgbRed;
	unsigned short rgbGreen;
	unsigned short rgbBlue;
} PNG_RGB_TRIPLE_16BIT;


// PNG RGA+Alpha pixel format
typedef struct tagPNG_RGBA_QUAD
{
	unsigned char rgbaRed;
	unsigned char rgbaGreen;
	unsigned char rgbaBlue;
	unsigned char rgbaAlpha;
} PNG_RGBA_QUAD;

typedef struct tagPNG_RGBA_QUAD_16BIT
{
	unsigned short rgbaRed;
	unsigned short rgbaGreen;
	unsigned short rgbaBlue;
	unsigned short rgbaAlpha;
} PNG_RGBA_QUAD_16BIT;


// DIB RGB pixel format
typedef struct tagDIB_RGB_TRIPLE
{
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
} DIB_RGB_TRIPLE;

// DIB RGA+Alpha pixel format
typedef struct tagDIB_RGBA_QUAD
{
	BYTE rgbaBlue;
	BYTE rgbaGreen;
	BYTE rgbaRed;
	BYTE rgbaAlpha;
} DIB_RGBA_QUAD;

#pragma pack(pop)

typedef struct tagPNG_CHUNK_STRUCT
{
	DWORD	dwLength;
	DWORD	dwType;
	LPBYTE	pData;
	ULONG	ulCRC;
} PNG_CHUNK_STRUCT;

#define PNG_MAKETYPE(a, b, c, d) (DWORD)(((DWORD)(d) << 24) | ((DWORD)(c) << 16) | ((DWORD)(b) << 8) | ((DWORD)(a)))

#define PNG_CHUNK_TYPE_IHDR		PNG_MAKETYPE('I', 'H', 'D', 'R')
#define PNG_CHUNK_TYPE_PLTE		PNG_MAKETYPE('P', 'L', 'T', 'E')
#define PNG_CHUNK_TYPE_IDAT		PNG_MAKETYPE('I', 'D', 'A', 'T')
#define PNG_CHUNK_TYPE_IEND		PNG_MAKETYPE('I', 'E', 'N', 'D')


// Byte Convertion Functions
WORD swapbytes16(WORD n);
DWORD swapbytes32(DWORD n);

#define toLE16(x) swapbytes16(x)
#define toLE32(x) swapbytes32(x)

#define toBE16(x) swapbytes16(x)
#define toBE32(x) swapbytes32(x)


// DIB Helper Functions

/*Returns number of colors used in palette array*/
DWORD DIBNumColors(_In_ LPBITMAPINFO pbmi);

/*Returns size of palette in bytes*/
DWORD DIBPalleteSize(_In_ LPBITMAPINFO pbmi);

/*Returns a pointer to DIBits*/
LPBYTE DIBGetBits(_In_ LPBITMAPINFO pbmi);

/*Returns number of bytes in each DIB scanline aligned to DWORD*/
DWORD DIBBytesPerLine(_In_ LPBITMAPINFO pbmi);

/*Returns DIBits (actual bitmap data) size*/
DWORD DIBCalcBitsSize(_In_ LPBITMAPINFO pbmi);

/*Returns number of bytes per each DIB pixel; rounded to 1 byte*/
UINT DIBBytesPerPixel(_In_ LPBITMAPINFO pbmi);


// PNG Helper Functions

/*Returns TRUE if the given bytes is the PNG singnature*/
BOOL PNGVerifySignature(_In_ void* pbytes, _In_ UINT numbytes);

/*Sanity check of PNG_IMAGE_HEADER and returns FALSE if has bad format*/
BOOL PNGVerifyImageHeader(_In_ PNG_IMAGE_HEADER* pihdr);

/*Returns number of sample per each pixel*/
UINT PNGNumSamples(_In_ PNG_IMAGE_HEADER* pihdr);

/*Returns number of bytes per each pixel; rounded to 1 byte*/
DWORD PNGBytesPerPixel(_In_ PNG_IMAGE_HEADER* pihdr);

/*Returns number of bytes per each scanline; excluded filter byte*/
DWORD PNGBytesPerLine(_In_ PNG_IMAGE_HEADER* pihdr);

/*Returns number of scanlines for specified interlace pass*/
UINT PNGPassLines(_In_ PNG_IMAGE_HEADER* pihdr, _In_ int pass);

/*Returns pixels count per scanline for specified pass */
UINT PNGPassLinePixels(_In_ PNG_IMAGE_HEADER* pihdr, _In_ int pass);

/*Returns needed bytes for uncompressed IDAT chunk base on image header data*/
DWORD CalcPngImageSize(_In_ PNG_IMAGE_HEADER* pImageHeader);

/*Calculates and resturns Peath filter value*/
int PaethPredictor(_In_ int a, _In_ int b, _In_ int c);
#endif // _PNGCMN_H_