//------------------------------------------------------------------------------
// File: EZProp.cpp
//
// Desc: DirectShow sample code - implementation of CColorSpaceConvertProperties class.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------
#include "stdafx.h"
#include <windows.h>
#include <windowsx.h>
#include <streams.h>
#include <commctrl.h>
#include <olectl.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include "resource.h"
#include "QvodColorSpaceConvertGuid.h"
#include "IColorSpaceConvert.h"
#include "QvodColorSpaceConvert.h"
#include "QvodColorSpaceConvertProp.h"

#define IN_YV12  IDC_YV12
#define IN_YUY2  IDC_YV12 + 1
#define IN_RGB24 IDC_YV12 + 2
#define IN_RGB32 IDC_YV12 + 3

#define OUT_YV12   IDC_OYV12
#define OUT_YUY2   IDC_OYV12 + 1
#define OUT_RGB24  IDC_OYV12 + 2
#define OUT_RGB32  IDC_OYV12 + 3


//
// CreateInstance
//
// Used by the DirectShow base classes to create instances
//
CUnknown *CColorSpaceConvertProperties::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
	ASSERT(phr);

	CUnknown *punk = new CColorSpaceConvertProperties(lpunk, phr);

	if (punk == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return punk;

} // CreateInstance


//
// Constructor
//
CColorSpaceConvertProperties::CColorSpaceConvertProperties(LPUNKNOWN pUnk, HRESULT *phr) :
CBasePropertyPage(NAME("Special Effects Property Page"), pUnk,
				  IDD_ColorConvertProp, IDS_TITLE),
				  m_pIPEffect(NULL),
				  m_bIsInitialized(FALSE)
{
	ASSERT(phr);
	memset(&ColorPara,0,sizeof(ColorParams));
} // (Constructor)


//
// OnReceiveMessage
//
// Handles the messages for our property window
//
BOOL CColorSpaceConvertProperties::OnReceiveMessage(HWND hwnd,
										  UINT uMsg,
										  WPARAM wParam,
										  LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		{
			if (m_bIsInitialized)
			{
				m_bDirty = TRUE;
				if (m_pPageSite)
				{
					m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
				}
			}
			return (LRESULT) 1;
		}
	}
	return CBasePropertyPage::OnReceiveMessage(hwnd,uMsg,wParam,lParam);

} // OnReceiveMessage


//
// OnConnect
//
// Called when we connect to a transform filter
//
HRESULT CColorSpaceConvertProperties::OnConnect(IUnknown *pUnknown)
{
	CheckPointer(pUnknown,E_POINTER);
	ASSERT(m_pIPEffect == NULL);

	HRESULT hr = pUnknown->QueryInterface(IID_IColorSpaceConvert, (void **) &m_pIPEffect);
	if (FAILED(hr)) {
		return E_NOINTERFACE;
	}

	// Get the initial image FX property
	CheckPointer(m_pIPEffect,E_FAIL);
	m_pIPEffect->SetInpinSubType(m_InSubType);

	m_bIsInitialized = FALSE ;
	return NOERROR;

} // OnConnect


//
// OnDisconnect
//
// Likewise called when we disconnect from a filter
//
HRESULT CColorSpaceConvertProperties::OnDisconnect()
{
	// Release of Interface after setting the appropriate old effect value
	if(m_pIPEffect)
	{
		m_pIPEffect->Release();
		m_pIPEffect = NULL;
	}
	return NOERROR;

} // OnDisconnect


//
// OnActivate
//
// We are being activated
//
HRESULT CColorSpaceConvertProperties::OnActivate()
{
	HWND m_hInYV12	= GetDlgItem(m_Dlg,IDC_YV12);
	HWND m_hInYUY2	= GetDlgItem(m_Dlg,IDC_YUY2);
	HWND m_hInRGB24	= GetDlgItem(m_Dlg,IDC_RGB24);
	HWND m_hInRGB32	= GetDlgItem(m_Dlg,IDC_RGB32);
	HWND m_hInOYV12	= GetDlgItem(m_Dlg,IDC_OYV12);
	HWND m_hInOYUY2	= GetDlgItem(m_Dlg,IDC_OYUY2);
	HWND m_hInORGB24= GetDlgItem(m_Dlg,IDC_ORGB24);
	HWND m_hInORGB32	= GetDlgItem(m_Dlg,IDC_ORGB32);

	m_pIPEffect->GetParams(&ColorPara);

	CheckDlgButton(m_Dlg, IDC_YV12, ColorPara.inYV12);
	CheckDlgButton(m_Dlg, IDC_YUY2, ColorPara.inYUY2);
	CheckDlgButton(m_Dlg, IDC_RGB24, ColorPara.inRGB24);
	CheckDlgButton(m_Dlg, IDC_RGB32, ColorPara.inRGB32);

	CheckDlgButton(m_Dlg, IDC_OYV12, ColorPara.outYV12);
	CheckDlgButton(m_Dlg, IDC_OYUY2, ColorPara.outYUY2);
	CheckDlgButton(m_Dlg, IDC_ORGB24, ColorPara.outRGB24);
	CheckDlgButton(m_Dlg, IDC_ORGB32, ColorPara.outRGB32);

	m_bIsInitialized = TRUE;

	return NOERROR;

} // OnActivate


//
// OnDeactivate
//
// We are being deactivated
//
HRESULT CColorSpaceConvertProperties::OnDeactivate(void)
{
	ASSERT(m_pIPEffect);

	m_bIsInitialized = FALSE;
	GetControlValues();

	return NOERROR;

} // OnDeactivate

//
// OnApplyChanges
//
// Apply any changes so far made
//
HRESULT CColorSpaceConvertProperties::OnApplyChanges()
{
	GetControlValues();

	CheckPointer(m_pIPEffect,E_POINTER);
	m_pIPEffect->SetInpinSubType(m_InSubType);
	m_pIPEffect->SetOutpinSubType(m_OutSubType);
	m_pIPEffect->SetParams(&ColorPara);

	return NOERROR;

} // OnApplyChanges


void CColorSpaceConvertProperties::GetControlValues()
{
	memset(&ColorPara,0,sizeof(ColorParams));
	for (int i = IDC_YV12; i <= IDC_RGB32; i++) 
	{
		if (IsDlgButtonChecked(m_Dlg, i)) 
		{
			if(i == IDC_YV12)
			{
				m_InSubType.YV12 = 1;
				m_InSubType.YUY2 = 0;
				m_InSubType.RGB24 = 0;
				m_InSubType.RGB32 = 0;
				m_InSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.inYV12 = 1;
			}
			if(i == IDC_YUY2)
			{
				m_InSubType.YV12 = 0;
				m_InSubType.YUY2 = 1;
				m_InSubType.RGB24 = 0;
				m_InSubType.RGB32 = 0;
				m_InSubType.Default = 0;
				m_pIPEffect->SetInpinSubType(m_InSubType);
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.inYUY2 = 1;
			}
			if(i == IDC_RGB24)
			{
				m_InSubType.YV12 = 0;
				m_InSubType.YUY2 = 0;
				m_InSubType.RGB24 = 1;
				m_InSubType.RGB32 = 0;
				m_InSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.inRGB24 = 1;
			}
			if(i == IDC_RGB32)
			{
				m_InSubType.YV12 = 0;
				m_InSubType.YUY2 = 0;
				m_InSubType.RGB24 = 0;
				m_InSubType.RGB32 = 1;
				m_InSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.inRGB32 = 1;
			}
			break;
		}
	}
	for (int i = IDC_OYV12; i <= IDC_ORGB32; i++) 
	{
		if (IsDlgButtonChecked(m_Dlg, i)) 
		{
			if(i == IDC_OYV12)
			{
				m_OutSubType.YV12 = 0;
				m_OutSubType.YUY2 = 2;
				m_OutSubType.RGB24 = 4;
				m_OutSubType.RGB32 = 6;
				m_OutSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.outYV12 = 1;
			}
			if(i == IDC_OYUY2)
			{
				m_OutSubType.YV12 = 2;
				m_OutSubType.YUY2 = 0;
				m_OutSubType.RGB24 = 4;
				m_OutSubType.RGB32 = 6;
				m_OutSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.outYUY2 = 1;
			}
			if(i == IDC_ORGB24)
			{
				m_OutSubType.YV12 = 2;
				m_OutSubType.YUY2 = 4;
				m_OutSubType.RGB24 = 0;
				m_OutSubType.RGB32 = 6;
				m_OutSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.outRGB24 = 1;
			}
			if(i == IDC_ORGB32)
			{
				m_OutSubType.YV12 = 2;
				m_OutSubType.YUY2 = 4;
				m_OutSubType.RGB24 = 6;
				m_OutSubType.RGB32 = 0;
				m_OutSubType.Default = 0;
				//memset(&ColorPara,0,sizeof(ColorParams));
				ColorPara.outRGB32 = 1;
			}
			break;
		}
	}
}