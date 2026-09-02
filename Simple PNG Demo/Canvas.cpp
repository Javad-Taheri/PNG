#include "stdafx.h"
#include "SimplePNG.h"
#include "Canvas.h"
#include "png\\png.h"

// CCanvas

IMPLEMENT_DYNAMIC(CCanvas, CWnd)

CCanvas::CCanvas()
{
	m_pvDIB = NULL;
}

CCanvas::~CCanvas()
{
}

void CCanvas::SetDIB(void* pvDIB)
{
	m_pvDIB = pvDIB;

	SetupScrolls();

	Invalidate();
	UpdateWindow();
}

void CCanvas::SetupScrolls()
{
	SCROLLINFO si;
	ZeroMemory(&si, sizeof(SCROLLINFO));
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;

	if (m_pvDIB != NULL)
	{
		CRect rc;
		GetClientRect(&rc);

		LPBITMAPINFO pbmi = (LPBITMAPINFO)m_pvDIB;

		si.nMax = pbmi->bmiHeader.biHeight;
		si.nPage = rc.bottom;
		SetScrollInfo(SB_VERT, &si);

		si.nMax = pbmi->bmiHeader.biWidth;
		si.nPage = rc.right;
		SetScrollInfo(SB_HORZ, &si);
	}
	else
	{
		SetScrollInfo(SB_VERT, &si);
		SetScrollInfo(SB_HORZ, &si);
	}
}

BEGIN_MESSAGE_MAP(CCanvas, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()	
	ON_WM_CREATE()
END_MESSAGE_MAP()

// CCanvas message handlers

void CCanvas::OnPaint()
{
	CPaintDC dc(this);

	CRect rc;
	GetClientRect(rc);
	dc.FillSolidRect(&rc, 0x00ffffff);

	if (m_pvDIB != NULL)
	{
		LPBITMAPINFO pbi = (LPBITMAPINFO)m_pvDIB;
		BYTE* pBits = DIBGetBits(pbi);

		int y = GetScrollPos(SB_VERT);
		int x = GetScrollPos(SB_HORZ);

		if (pbi->bmiHeader.biBitCount != 32)
		{
			SetDIBitsToDevice(dc.m_hDC, -x, -y, pbi->bmiHeader.biWidth, pbi->bmiHeader.biHeight,
				0, 0, 0, pbi->bmiHeader.biHeight, pBits, pbi, DIB_RGB_COLORS);
		}
		else
		{
			int stride = (((4 * pbi->bmiHeader.biWidth) + 3) / 4) * 4;
			pBits += (pbi->bmiHeader.biHeight - 1) * stride;

			Gdiplus::Bitmap bitmap(pbi->bmiHeader.biWidth, pbi->bmiHeader.biHeight, -stride, PixelFormat32bppARGB, pBits);
			if (bitmap.GetLastStatus() == Gdiplus::Ok)
			{
				Gdiplus::Graphics g(dc.m_hDC);
				g.DrawImage(&bitmap, -x, -y, pbi->bmiHeader.biWidth, pbi->bmiHeader.biHeight);
			}
		}
	}
}


void CCanvas::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	GetScrollInfo(SB_VERT, &si);

	int nNewPos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINEUP:
		if (nNewPos > 0) nNewPos--;
		break;

	case SB_LINEDOWN:
		if (nNewPos < si.nMax) nNewPos++;
		break;

	case SB_PAGEUP:
		if (nNewPos >= si.nPage) nNewPos-= si.nPage;
		break;

	case SB_PAGEDOWN:
		if (nNewPos <= si.nMax - si.nPage) nNewPos += si.nPage;
		break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		nNewPos = nPos;
		break;

	case SB_TOP:
		nNewPos = 0;
		break;

	case SB_BOTTOM:
		nNewPos = si.nMax;
		break;

	case SB_ENDSCROLL:
		return;
	}

	SetScrollPos(SB_VERT, nNewPos);

	Invalidate(FALSE);
	
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CCanvas::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	GetScrollInfo(SB_HORZ, &si);

	int nNewPos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINELEFT:
		if (nNewPos > 0) nNewPos--;
		break;

	case SB_LINERIGHT:
		if (nNewPos < si.nMax) nNewPos++;
		break;

	case SB_PAGELEFT:
		if (nNewPos >= si.nPage) nNewPos -= si.nPage;
		break;

	case SB_PAGEDOWN:
		if (nNewPos <= si.nMax - si.nPage) nNewPos += si.nPage;
		break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		nNewPos = nPos;
		break;

	case SB_LEFT:
		nNewPos = 0;
		break;

	case SB_RIGHT:
		nNewPos = si.nMax;
		break;

	case SB_ENDSCROLL:
		return;
	}

	SetScrollPos(SB_HORZ, nNewPos);

	Invalidate(FALSE);

	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CCanvas::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}


int CCanvas::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetupScrolls();

	return 0;
}
