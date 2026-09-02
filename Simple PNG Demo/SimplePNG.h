
// SimplePNG.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CSimplePNGDemoApp:
// See SimplePNG.cpp for the implementation of this class
//

class CSimplePNGDemoApp : public CWinApp
{
public:
	CSimplePNGDemoApp();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()

// GDI+ stuffs
	ULONG_PTR m_gdiplusToken;
	Gdiplus::GdiplusStartupInput m_gdiplusStartupInput;
};

extern CSimplePNGDemoApp theApp;