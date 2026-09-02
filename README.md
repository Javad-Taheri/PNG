# PNG
## Simple PNG Decoder/Encoder PNG source code in VC++

### --------------------------- (Decoder Section) -----------------------------
### Introduction: 
        This decoder code is really simple and is understandable for
        reading PNG file format. It creates DIB (CF_DIB) format from 
        supported PNG files according to the following table:
       
         +-----------------------------------------------+---------------+
         |               PNG FILE (in)                   |               |
         +------------------+----------------+-----------+   DIB (out)   +
         |    color type    | bit-depth      | interlace |   bit-depth   |
         +------------------+----------------+-----------+---------------+
         | grayscale(0)     | 1, 2, 4, 8, 16 |   8, 16   | 1, 4, 4, 8, 8 |
         | rgb triple(2)    | 8, 16          |   8, 16   | 24, 24        |
         | indexed(3)       | 1, 2, 4, 8     |   8       | 1, 4, 4, 8    |
         | grayscale+alpa(4)| 8, 16          |   8, 16   | 32, 32        |
         | rgb+alpha(6)     | 8, 16          |   8, 16   | 32, 32        |
         +------------------+----------------+-----------+---------------+
         
         Notes: 
         -----------------------
         For compiling this code you need zlib.
 
         This decoder only use main PNG chunks: IHDR, PLTE, IDAT & IEND
         the ancillary chunks is not used. Also multi-IDAT chunks are 
         supported.

### Usage:
         Main function is ReadPngFile() and overloaded to read from 
         file or memory.

         Syntax:
         -----------------------
         LONG	ReadPngFile(
			_In_ LPCTSTR lpszFileName,
			_Outptr_result_maybenull_ LPVOID* ppvDIBData,
			_Out_ LPDWORD pdwDIBSize,
			_Out_opt_ PNG_IMAGE_HEADER* pOutImageHeader = NULL);

         Parameters:
 
         [in]lpszFileName
			       The full qualified path of the PNG file

         [out]ppvDIBData   
                 Pointer to result DIB data. You must use
                 free() function to release this memory after
                 no longer needed it.

         [out]pdwDIBSize
                 Pointer to a DWORD that recieve the entire DIB size.

         [out, optional]pOutImageHeader
                 You can recieve the IHDR data as a PNG_IMAGE_HEADER
                 structure for details about the PNG file.

         Return value: 
                 Returns PNG_SUCCESS if operation is successful or other
                 defined png status codes. 
                 If the return value is PNG_WIN32_ERROR you can call GetLastError()
                 for more information.

         ### Remarks:
         -----------------------
                 You must release the out DIB memory after use or no longer 
                 needed it by free() function.

         ### Sample:
         -----------------------
         DWORD dwDIBSize = 0;
         LPVOID pvDIBData = NULL;
         PNG_IMAGE_HEADER imageHeader;
         if (ReadPngFile(_T("c:\\sample.png"), &pvDIBData, &dwDIBSize, &imageHeader) == PNG_SUCCESS)
         {
                 // TODO: you can paint the DIB by SetDIBitsToDevice() API function
                 free(pvDIBData);
         }


### --------------------------- (Encoder Section) -----------------------------
### Introduction:
         This encoder creates PNG files from DIB data. DIB data must be in 
         CF_DIB format. The following table shows input DIB formats and out-
         put corresponding PNG file.

         +-----------------------------------+----------------------------+
         |             DIB (in)              |            PNG (out)       |
         +-----------+-----------+-----------+----------------+-----------+
         | bit-depth | grayscale | interlace |   color type   | bit-depth |
         +-----------+-----------+-----------+----------------+-----------+
         | 1         |    yes    |    no     | indexed/grays. | 1         |
         | 4         |    yes    |    no     | indexed/grays. | 4         |
         | 8         |    yes    |    yes    | indexed/grays. | 8         |
         | 16        |    yes    |    yes    | rgb/grayscale  | 8         |
         | 24        |    yes    |    yes    | rgb/grayscale  | 8         |
         | 32        |    yes    |    yes    | rgb+alpha/     | 8         |
         |           |           |           | grayscale+alpha|           |
         +-----------+-----------------------+----------------+-----------+

         Notes:
         -----------------------
         This encoder only use main PNG chunks: IHDR, PLTE, IDAT & IEND
         the ancillary chunks is not used.

         Input DIB can be bottom-top or top-bottom line order.
         1-bit & 4-bit DIBs can not be interlaced.

         For compiling this code you need zlib.

### Usage:
        Main function is WritePngFile() and overloaded to write to
        file or memory.

         Syntax:
         -----------------------
         LONG
         WritePngFile(
             _In_ LPCTSTR lpszFileName,
                    - or -
             _Out_ LPVOID* ppvPNGData,
             _Out_ LPDWORD pdwPNGSize,

             _In_ LPVOID pvDIBData,
             _In_ BOOL bGrayscale  = FALSE,
             _In_ BOOL bInterlace  = FALSE,
             _In_ int nFilter      = WPF_FilterBest,
             _In_ int nCompression = WPF_DefaultCompression);

         Parameters:

         [in]lpszFileName
			       The full qualified path of the PNG file.
         
         [out]ppvPNGData
                 Pointer to the result PNG file. You must use
                 free() function to release this memory after
                 no longer needed it.
        
         [out]pdwPNGSize
                 Pointer to a DWORD that recieve the entire PNG file size.

         [in]pvDIBData
                 Pointer to buffer that contains input DIB data in CF_DIB 
                 format.

         [in]bGrayscale
                 If this flag is set TRUE, the result PNG will be in grayscale
                 color type. Also the function converts color pixels to grayscale.

         [in]bInterlace
                 If this flag is set TRUE, the result PNG will be interlaced.

         [in]nFilter
                 Specifies the filtering method. It can be one of the PngFilterTypes
                 enum element.

         [in]nCompression
                 Specifies the compression level. It can be one of the PngCompressionLevels
                 enum element.

         Return Value:
                 Returns PNG_SUCCESS if operation is successful or other
                 defined png status codes.
                 If the return value is PNG_WIN32_ERROR you can call GetLastError()
                 for more information.

         Remarks:
         -----------------------
                 You must release the out PNG memory (ppvPNGData) after use or no 
                 longer needed it by free() function.

                 If the specified filter type is WPF_FilterBest, the function 
                 selects best filter type for each scanline.

                 If the bGrayscale is TRUE, the result PNG file will be in colortype 
                 grayscale(0) for DIBs less than 32-bit and color type grayscale+alpha(4)
                 for 32-bit DIBs.
                 Used formula for converting color pixels to grayscale is simply:
                 gray = (r + g + b) / 3;

