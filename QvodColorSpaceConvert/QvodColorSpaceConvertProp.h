#pragma once
#include "IColorSpaceConvert.h"
class CColorSpaceConvertProperties : public CBasePropertyPage
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);

private:
	BOOL OnReceiveMessage(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
	HRESULT OnConnect(IUnknown *pUnknown);
	HRESULT OnDisconnect();
	HRESULT OnActivate();
	HRESULT OnDeactivate();
	HRESULT OnApplyChanges();

	void    GetControlValues();

	CColorSpaceConvertProperties(LPUNKNOWN lpunk, HRESULT *phr);

	BOOL m_bIsInitialized;      // Used to ignore startup messages
	MediaType  m_InSubType;               // Which effect are we processing
	MediaType  m_OutSubType;
	REFTIME m_start;            // When the effect will begin
	REFTIME m_length;           // And how long it will last for
	IColorSpaceConvert *m_pIPEffect;     // The custom interface on the filter
	ColorParams  ColorPara;

}; // EZrgb24Properties