#include "stdafx.h"
#include "host_comm.h"

host_comm::host_comm()
	: m_hwnd(nullptr)
	, m_hdc(nullptr)
	, m_width(0)
	, m_height(0)
	, m_gr_bmp(nullptr)
	, m_suppress_drawing(false)
	, m_paint_pending(false)
	, m_instance_type(KInstanceTypeCUI)
	, m_script_info()
	, m_panel_tooltip_param_ptr(new panel_tooltip_param)
{
	m_max_size = { INT_MAX, INT_MAX };
	m_min_size = { 0, 0 };
}

host_comm::~host_comm() {}

HDC host_comm::get_hdc()
{
	return m_hdc;
}

HWND host_comm::get_hwnd()
{
	return m_hwnd;
}

int host_comm::get_height()
{
	return m_height;
}

int host_comm::get_width()
{
	return m_width;
}

panel_tooltip_param_ptr& host_comm::panel_tooltip()
{
	return m_panel_tooltip_param_ptr;
}

t_size host_comm::get_instance_type()
{
	return m_instance_type;
}

void host_comm::redraw()
{
	m_paint_pending = false;
	RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void host_comm::refresh_background(LPRECT lprcUpdate)
{
	HWND wnd_parent = GetAncestor(m_hwnd, GA_PARENT);

	if (!wnd_parent || IsIconic(core_api::get_main_window()) || !IsWindowVisible(m_hwnd))
		return;

	// HACK: for Tab control
	HWND hwnd = FindWindowEx(wnd_parent, nullptr, nullptr, nullptr);
	while (hwnd != nullptr)
	{
		pfc::string8_fast name;
		uGetClassName(hwnd, name);
		if (name.equals("SysTabControl32"))
		{
			wnd_parent = hwnd;
			break;
		}
		hwnd = FindWindowEx(wnd_parent, hwnd, nullptr, nullptr);
	}

	HDC dc_parent = GetDC(wnd_parent);
	HDC hdc_bk = CreateCompatibleDC(dc_parent);
	POINT pt = { 0, 0 };
	RECT rect_child = { 0, 0, m_width, m_height };
	RECT rect_parent;
	HRGN rgn_child = nullptr;

	if (lprcUpdate)
	{
		HRGN rgn = CreateRectRgnIndirect(lprcUpdate);
		rgn_child = CreateRectRgnIndirect(&rect_child);
		CombineRgn(rgn_child, rgn_child, rgn, RGN_DIFF);
		DeleteRgn(rgn);
	}
	else
	{
		rgn_child = CreateRectRgn(0, 0, 0, 0);
	}

	ClientToScreen(m_hwnd, &pt);
	ScreenToClient(wnd_parent, &pt);

	CopyRect(&rect_parent, &rect_child);
	ClientToScreen(m_hwnd, (LPPOINT)&rect_parent);
	ClientToScreen(m_hwnd, (LPPOINT)&rect_parent + 1);
	ScreenToClient(wnd_parent, (LPPOINT)&rect_parent);
	ScreenToClient(wnd_parent, (LPPOINT)&rect_parent + 1);

	// Force Repaint
	m_suppress_drawing = true;
	SetWindowRgn(m_hwnd, rgn_child, FALSE);
	RedrawWindow(wnd_parent, &rect_parent, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW | RDW_UPDATENOW);

	// Background bitmap
	HBITMAP old_bmp = SelectBitmap(hdc_bk, m_gr_bmp_bk);

	BitBlt(hdc_bk, rect_child.left, rect_child.top, rect_child.right - rect_child.left, rect_child.bottom - rect_child.top, dc_parent, pt.x, pt.y, SRCCOPY);

	SelectBitmap(hdc_bk, old_bmp);
	DeleteDC(hdc_bk);
	ReleaseDC(wnd_parent, dc_parent);
	DeleteRgn(rgn_child);
	SetWindowRgn(m_hwnd, nullptr, FALSE);
	m_suppress_drawing = false;
	if (m_edge_style) SendMessage(m_hwnd, WM_NCPAINT, 1, 0);
	repaint(true);
}

void host_comm::repaint(bool force)
{
	m_paint_pending = true;

	if (force)
	{
		RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
	else
	{
		InvalidateRect(m_hwnd, nullptr, FALSE);
	}
}

void host_comm::repaint_rect(int x, int y, int w, int h, bool force)
{
	RECT rc = { x, y, x + w, y + h };
	m_paint_pending = true;

	if (force)
	{
		RedrawWindow(m_hwnd, &rc, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}
	else
	{
		InvalidateRect(m_hwnd, &rc, FALSE);
	}
}
