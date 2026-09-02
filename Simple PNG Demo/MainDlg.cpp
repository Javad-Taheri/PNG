#include "stdafx.h"
#include "SimplePNG.h"
#include "MainDlg.h"
#include "png\\png.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IDW_CANVAS		100

// CMainDlg dialog


CMainDlg::CMainDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_SIMPLEPNG_DIALOG, pParent)
{
	m_pvDIBData = NULL;
	m_dwDIBSize = 0;
}

CMainDlg::~CMainDlg()
{
	if (m_pvDIBData != NULL)
	{
		free(m_pvDIBData);
	}
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialog)
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_OPEN, &CMainDlg::OnClickOpen)	
	ON_BN_CLICKED(IDC_SAVE, &CMainDlg::OnClickSave)
	ON_BN_CLICKED(ID_APP_ABOUT, &CMainDlg::OnClickAbout)
	ON_BN_CLICKED(IDC_PASTE, &CMainDlg::OnClickPaste)
	ON_WM_CREATE()
END_MESSAGE_MAP()


// CMainDlg message handlers

BOOL CMainDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CRect rc;
	GetDlgItem(IDC_CANVAS_PLACEHOLDER)->GetWindowRect(rc);
	ScreenToClient(rc);
	m_wndCanvas.SetWindowPos(NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOACTIVATE | SWP_NOZORDER);

	CComboBox* pwndFiltering = (CComboBox*)GetDlgItem(IDC_FILTERING);
	pwndFiltering->SetCurSel(5);

	CComboBox* pwndCompression = (CComboBox*)GetDlgItem(IDC_COMPRESSION);
	pwndCompression->SetCurSel(0);

	return TRUE;
}

void CMainDlg::ShowErrorMessage(LONG err)
{
	CString strMsg;
	switch (err)
	{
	case PNG_INVALID_ARG:
		strMsg = _T("Invalid argument.");
		break;

	case PNG_WIN32_ERROR: {
		DWORD dwErr = GetLastError();
		strMsg.Format(_T("Win32 error occurred: 0x%08x"), dwErr);
	}
		break;

	case PNG_NOT_PNG:
		strMsg = _T("This file in not PNG file.");
		break;

	case PNG_INVALID_CRC:
		strMsg = _T("This file is damaged.");
		break;

	case PNG_BAD_FORMAT:
		strMsg = _T("Bad PNG file format");
		break;

	case PNG_UNSUPPORTED:
		strMsg = _T("This type of PNG file format is not supported.");
		break;

	case PNG_NOT_ENOUGH_MEMORY:
		strMsg = _T("Not enough memory available to perform the operation.");
		break;

	case PNG_FAIL:
	default:
		strMsg = _T("Failed to read/write the file.");
		break;
	}

	AfxMessageBox(strMsg);
}

void CMainDlg::OnClickPaste()
{
	CString s;
	CString strInfo;

	if (DoPaste())
	{
		m_wndCanvas.SetDIB(m_pvDIBData);

		if (m_pvDIBData != NULL)
		{
			LPBITMAPINFO pbmi = (LPBITMAPINFO)m_pvDIBData;
			strInfo += _T("DIB Information\r\n");
			s.Format(_T("Bit-count: %d\r\n"), pbmi->bmiHeader.biBitCount);
			strInfo += s;
			s.Format(_T("Dimension: %dx%d\r\n"), pbmi->bmiHeader.biWidth, pbmi->bmiHeader.biHeight);
			strInfo += s;
			s.Format(_T("DIB size: %u bytes\r\n"), m_dwDIBSize);
			strInfo += s;
		}

		SetDlgItemText(IDC_EDIT_INFO, strInfo);
	}	
}

void CMainDlg::OnClickOpen()
{
	CFileDialog dlgOpen(TRUE, NULL, NULL,
		OFN_DONTADDTORECENT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST,
		_T("PNG files (*.png)|*.png||"));

	if (dlgOpen.DoModal() == IDOK)
	{
		// clean up previous DIB
		if (m_pvDIBData != NULL)
		{
			free(m_pvDIBData);
			m_pvDIBData = NULL;
		}

		PNG_IMAGE_HEADER ihdr;
		LONG lRet = ReadPngFile(dlgOpen.GetPathName(), &m_pvDIBData, &m_dwDIBSize, &ihdr);
		if (lRet != PNG_SUCCESS)
		{
			ShowErrorMessage(lRet);
		}
		else
		{
			CString s;
			CString strInfo;

			strInfo = _T("PNG Image Header\r\n");
			s.Format(_T("Dimension: %dx%d\r\n"), ihdr.ihWidth, ihdr.ihHeight);
			strInfo += s;

			s.Format(_T("Bit-depth: %d\r\n"), ihdr.ihBitDepth);
			strInfo += s;

			s.Format(_T("Color type: %d\r\n"), ihdr.ihColorType);
			strInfo += s;

			s.Format(_T("Compresstion method: %d\r\n"), ihdr.ihCompressionMethod);
			strInfo += s;

			s.Format(_T("Filter method: %d\r\n"), ihdr.ihFilterMethod);
			strInfo += s;

			s.Format(_T("Interlace method: %d\r\n"), ihdr.ihInterlaceMethod);
			strInfo += s;

			LPBITMAPINFO pbmi = (LPBITMAPINFO)m_pvDIBData;
			strInfo += _T("\r\nDIB Information\r\n");
			s.Format(_T("Bit-count: %d\r\n"), pbmi->bmiHeader.biBitCount);
			strInfo += s;

			s.Format(_T("DIB size: %u bytes\r\n"), m_dwDIBSize);
			strInfo += s;

			SetDlgItemText(IDC_EDIT_INFO, strInfo);
		}

		m_wndCanvas.SetDIB(m_pvDIBData);
	}
}

void CMainDlg::OnClickSave()
{
	CFileDialog dlgSave(FALSE, _T(".png"), NULL,
		OFN_DONTADDTORECENT | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT,
		_T("PNG files (*.png)|*.png||"));

	if (m_pvDIBData == NULL)
	{
		AfxMessageBox(_T("Before saving open an image."));
		return;
	}

	if (dlgSave.DoModal() == IDOK)
	{
		BOOL bGrayscale = FALSE;
		BOOL bInterlace = FALSE;
		int nFiltering = WPF_FilterBest;
		int nCompression = WPF_DefaultCompression;

		bGrayscale = IsDlgButtonChecked(IDC_CHECK_GRAYSCALE) == BST_CHECKED;
		bInterlace = IsDlgButtonChecked(IDC_CHECK_INTERLACE) == BST_CHECKED;

		CComboBox* pwndFiltering = (CComboBox*)GetDlgItem(IDC_FILTERING);
		switch (pwndFiltering->GetCurSel())
		{
		case 0: nFiltering = WPF_FilterNone; break;
		case 1: nFiltering = WPF_FilterSub; break;
		case 2: nFiltering = WPF_FilterUp; break;
		case 3: nFiltering = WPF_FilterAverage; break;
		case 4: nFiltering = WPF_FilterPaeth; break;
		case 5: nFiltering = WPF_FilterBest; break;
		}

		CComboBox* pwndCompression = (CComboBox*)GetDlgItem(IDC_COMPRESSION);
		switch (pwndCompression->GetCurSel())
		{
		case 0: nCompression = WPF_DefaultCompression; break;
		case 1: nCompression = WPF_NoCompression ; break;
		case 2: nCompression = WPF_BestSpeed; break;
		case 3: nCompression = WPF_BestCompression; break;
		}

		CString strFileName;
		strFileName = dlgSave.GetPathName();
		LONG lRet = WritePngFile(strFileName, m_pvDIBData, bGrayscale, bInterlace, nFiltering, nCompression);
		if (lRet == PNG_SUCCESS)
		{
			ShellExecute(AfxGetMainWnd()->m_hWnd, _T("open"), strFileName, NULL, NULL, SW_NORMAL);
		}
		else
		{
			ShowErrorMessage(lRet);
		}
	}
}

void CMainDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
}

void CMainDlg::OnClickAbout()
{
	AfxMessageBox(_T("Simple PNG, Version: 2.0\n\nAuthor: Javad Taheri.\nmrjavadtaheri@gmail.com"), MB_OK | MB_ICONINFORMATION);
}

BOOL CMainDlg::DoPaste()
{
	if (!IsClipboardFormatAvailable(CF_DIB))
	{
		AfxMessageBox(_T("No DIB available on the clipboard."));
		return FALSE;
	}

	if (!OpenClipboard())
	{
		AfxMessageBox(_T("Failed to open clipboard."));
		return FALSE;
	}

	HGLOBAL hglb = GetClipboardData(CF_DIB);
	if (hglb == NULL)
	{
		CloseClipboard();
		AfxMessageBox(_T("Failed to get clipboard data."));
		return FALSE;
	}

	LPVOID pvData = GlobalLock(hglb);
	if (pvData != NULL)
	{
		//
		// duplicate CF_DIB
		//

		LPBITMAPINFO pbmi = (LPBITMAPINFO)pvData;

		// Check Bitmap Info Header sanity
		if (pbmi->bmiHeader.biSize < sizeof(BITMAPINFOHEADER) || pbmi->bmiHeader.biWidth == 0 || pbmi->bmiHeader.biHeight == 0 || pbmi->bmiHeader.biPlanes != 1)
		{
			CloseClipboard();
			return FALSE;
		}

		// Check bit-depth
		if (pbmi->bmiHeader.biBitCount != 1 &&
			pbmi->bmiHeader.biBitCount != 4 &&
			pbmi->bmiHeader.biBitCount != 8 &&
			pbmi->bmiHeader.biBitCount != 16 &&
			pbmi->bmiHeader.biBitCount != 24 &&
			pbmi->bmiHeader.biBitCount != 32)
		{
			CloseClipboard();
			return FALSE;
		}

		DWORD dwPaletteSize = DIBPalleteSize(pbmi);
		DWORD dwBitsSize = DIBCalcBitsSize(pbmi);
		DWORD dwDIBSize = pbmi->bmiHeader.biSize + dwPaletteSize + dwBitsSize;

		m_dwDIBSize = dwDIBSize;

		m_pvDIBData = malloc(dwDIBSize);
		if (m_pvDIBData == NULL)
		{
			GlobalUnlock(hglb);
			CloseClipboard();
			AfxMessageBox(_T("Not enough memory."));
			return FALSE;
		}
			
		memcpy(m_pvDIBData, pbmi, dwDIBSize);

		GlobalUnlock(hglb);
	}

	CloseClipboard();

	return TRUE;
}

int CMainDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_wndCanvas.Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_BORDER, 
		CRect(0, 0, 0, 0), this, IDW_CANVAS))
	{
		TRACE0("Failed to create canvas\n");
		return -1;
	}

	return 0;
}

