#pragma once


// CCanvas

class CCanvas : public CWnd
{
	DECLARE_DYNAMIC(CCanvas)

public:
	CCanvas();
	virtual ~CCanvas();

	void SetDIB(void* pvDIB);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	void* m_pvDIB;

	void SetupScrolls();
public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};


