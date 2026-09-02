#include "stdafx.h"
#include "SimplePNG.h"
#include "MainDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSimplePNGDemoApp

BEGIN_MESSAGE_MAP(CSimplePNGDemoApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CSimplePNGDemoApp construction

CSimplePNGDemoApp::CSimplePNGDemoApp()
{
}


// The one and only CSimplePNGDemoApp object

CSimplePNGDemoApp theApp;


// CSimplePNGDemoApp initialization

BOOL CSimplePNGDemoApp::InitInstance()
{
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	Gdiplus::GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, NULL);

	CMainDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	return FALSE;
}


int CSimplePNGDemoApp::ExitInstance()
{
	Gdiplus::GdiplusShutdown(m_gdiplusToken);
	return CWinApp::ExitInstance();
}

