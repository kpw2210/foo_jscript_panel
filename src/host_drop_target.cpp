#include "stdafx.h"
#include "host_comm.h"
#include "host_drop_target.h"
#include "js_panel_window.h"

IDropSourceImpl::IDropSourceImpl() : m_refCount(0), m_dwLastEffect(DROPEFFECT_NONE) {}
IDropSourceImpl::~IDropSourceImpl() {}

STDMETHODIMP IDropSourceImpl::GiveFeedback(DWORD dwEffect)
{
	m_dwLastEffect = dwEffect;
	return DRAGDROP_S_USEDEFAULTCURSORS;
}

STDMETHODIMP IDropSourceImpl::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{
	if (fEscapePressed || (grfKeyState & MK_RBUTTON) || (grfKeyState & MK_MBUTTON))
	{
		return DRAGDROP_S_CANCEL;
	}

	if (!(grfKeyState & MK_LBUTTON))
	{
		return m_dwLastEffect == DROPEFFECT_NONE ? DRAGDROP_S_CANCEL : DRAGDROP_S_DROP;
	}

	return S_OK;
}

ULONG STDMETHODCALLTYPE IDropSourceImpl::AddRef()
{
	return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE IDropSourceImpl::Release()
{
	LONG rv = InterlockedDecrement(&m_refCount);
	if (!rv)
	{
		delete this;
	}
	return rv;
}

IDropTargetImpl::IDropTargetImpl(HWND hWnd) : m_hWnd(hWnd)
{
	CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper, (LPVOID*)&m_dropTargetHelper);
}

IDropTargetImpl::~IDropTargetImpl()
{
	RevokeDragDrop();
}

HRESULT IDropTargetImpl::OnDragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return S_OK;
}

HRESULT IDropTargetImpl::OnDragLeave()
{
	return S_OK;
}

HRESULT IDropTargetImpl::OnDragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return S_OK;
}

HRESULT IDropTargetImpl::OnDrop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	return S_OK;
}

HRESULT IDropTargetImpl::RegisterDragDrop()
{
	return ::RegisterDragDrop(m_hWnd, this);
}

HRESULT IDropTargetImpl::RevokeDragDrop()
{
	return ::RevokeDragDrop(m_hWnd);
}

STDMETHODIMP IDropTargetImpl::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	if (pDataObj == nullptr) return E_FAIL;
	if (pdwEffect == nullptr) return E_POINTER;

	HRESULT hr = S_OK;
	try
	{
		if (m_dropTargetHelper)
		{
			POINT point = { pt.x, pt.y };
			m_dropTargetHelper->DragEnter(m_hWnd, pDataObj, &point, *pdwEffect);
		}

		hr = OnDragEnter(pDataObj, grfKeyState, pt, pdwEffect);
		if (FAILED(hr)) return hr;
	}
	catch (...)
	{
		return E_FAIL;
	}
	return hr;
}

STDMETHODIMP IDropTargetImpl::DragLeave()
{
	HRESULT hr = S_OK;

	try
	{
		if (m_dropTargetHelper)
		{
			m_dropTargetHelper->DragLeave();
		}

		hr = OnDragLeave();
		if (FAILED(hr)) return hr;
	}
	catch (...)
	{
		return E_FAIL;
	}
	return hr;
}

STDMETHODIMP IDropTargetImpl::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	if (pdwEffect == nullptr) return E_POINTER;

	HRESULT hr = S_OK;
	try
	{
		if (m_dropTargetHelper)
		{
			POINT point = { pt.x, pt.y };
			m_dropTargetHelper->DragOver(&point, *pdwEffect);
		}

		hr = OnDragOver(grfKeyState, pt, pdwEffect);
		if (FAILED(hr)) return hr;
	}
	catch (...)
	{
		return E_FAIL;
	}
	return hr;
}

STDMETHODIMP IDropTargetImpl::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	HRESULT hr = S_OK;

	if (pDataObj == nullptr) return E_FAIL;
	if (pdwEffect == nullptr) return E_POINTER;

	try
	{
		if (m_dropTargetHelper)
		{
			POINT point = { pt.x, pt.y };
			m_dropTargetHelper->Drop(pDataObj, &point, *pdwEffect);
		}

		hr = OnDrop(pDataObj, grfKeyState, pt, pdwEffect);
		if (FAILED(hr)) return hr;
	}
	catch (...)
	{
		return E_FAIL;
	}
	return hr;
}

host_drop_target::host_drop_target(js_panel_window* host) : IDropTargetImpl(host->GetHWND()), m_host(host), m_action(new com_object_impl_t<DropSourceAction, true>()) {}

host_drop_target::~host_drop_target()
{
	m_action->Release();
}

HRESULT host_drop_target::OnDragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	if (!pdwEffect) return E_POINTER;

	m_action->Reset();

	bool native;
	if (SUCCEEDED(ole_interaction::get()->check_dataobject(pDataObj, m_fb2kAllowedEffect, native)))
	{
		if (native && (*pdwEffect & DROPEFFECT_MOVE))
		{
			m_fb2kAllowedEffect |= DROPEFFECT_MOVE; // Remove check_dataobject move suppression for intra fb2k interactions
		}
	}
	else
	{
		m_fb2kAllowedEffect = DROPEFFECT_NONE;
	}

	m_action->m_effect = *pdwEffect & m_fb2kAllowedEffect;

	ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&pt));
	on_drag_enter(grfKeyState, pt, m_action);

	*pdwEffect = m_action->m_effect;
	return S_OK;
}

HRESULT host_drop_target::OnDragLeave()
{
	on_drag_leave();
	return S_OK;
}

HRESULT host_drop_target::OnDragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	if (!pdwEffect) return E_POINTER;

	m_action->m_effect = *pdwEffect & m_fb2kAllowedEffect;

	ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&pt));
	on_drag_over(grfKeyState, pt, m_action);

	*pdwEffect = m_action->m_effect;
	return S_OK;
}

HRESULT host_drop_target::OnDrop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	if (!pdwEffect) return E_POINTER;

	m_action->m_effect = *pdwEffect & m_fb2kAllowedEffect;

	ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&pt));
	on_drag_drop(grfKeyState, pt, m_action);

	if (*pdwEffect == DROPEFFECT_NONE || m_action->m_effect == DROPEFFECT_NONE)
	{
		*pdwEffect = DROPEFFECT_NONE;
		return S_OK;
	}

	dropped_files_data_impl droppedData;
	HRESULT hr = ole_interaction::get()->parse_dataobject(pDataObj, droppedData);
	if (SUCCEEDED(hr))
	{
		droppedData.to_handles_async_ex(playlist_incoming_item_filter_v2::op_flag_delay_ui,
			core_api::get_main_window(),
			fb2k::service_new<helpers::js_process_locations>(m_action->m_to_select, m_action->m_base, m_action->m_playlist_idx));
	}

	*pdwEffect = m_action->m_effect;
	return S_OK;
}

void host_drop_target::on_drag_enter(DWORD keyState, POINTL& pt, IDropSourceAction* action)
{
	VARIANTARG args[4];
	args[0].vt = VT_I4;
	args[0].lVal = keyState;
	args[1].vt = VT_I4;
	args[1].lVal = pt.y;
	args[2].vt = VT_I4;
	args[2].lVal = pt.x;
	args[3].vt = VT_DISPATCH;
	args[3].pdispVal = action;
	m_host->script_invoke_v(CallbackIds::on_drag_enter, args, _countof(args));
}

void host_drop_target::on_drag_leave()
{
	m_host->script_invoke_v(CallbackIds::on_drag_leave);
}

void host_drop_target::on_drag_over(DWORD keyState, POINTL& pt, IDropSourceAction* action)
{
	VARIANTARG args[4];
	args[0].vt = VT_I4;
	args[0].lVal = keyState;
	args[1].vt = VT_I4;
	args[1].lVal = pt.y;
	args[2].vt = VT_I4;
	args[2].lVal = pt.x;
	args[3].vt = VT_DISPATCH;
	args[3].pdispVal = action;
	m_host->script_invoke_v(CallbackIds::on_drag_over, args, _countof(args));
}

void host_drop_target::on_drag_drop(DWORD keyState, POINTL& pt, IDropSourceAction* action)
{
	VARIANTARG args[4];
	args[0].vt = VT_I4;
	args[0].lVal = keyState;
	args[1].vt = VT_I4;
	args[1].lVal = pt.y;
	args[2].vt = VT_I4;
	args[2].lVal = pt.x;
	args[3].vt = VT_DISPATCH;
	args[3].pdispVal = action;
	m_host->script_invoke_v(CallbackIds::on_drag_drop, args, _countof(args));
}
