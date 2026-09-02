#pragma once

#include "Canvas.h"

// CMainDlg dialog
class CMainDlg : public CDialog
{
// Construction
public:
	CMainDlg(CWnd* pParent = NULL);	// standard constructor	
	~CMainDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SIMPLEPNG_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	// Generated message map functions
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
	afx_msg void OnClickPaste();
	afx_msg void OnClickOpen();
	afx_msg void OnClickSave();
	afx_msg void OnClickAbout();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

	CCanvas m_wndCanvas;

	void* m_pvDIBData;
	DWORD m_dwDIBSize;

	void ShowErrorMessage(LONG err);
	BOOL DoPaste();
};
