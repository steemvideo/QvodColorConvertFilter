// QvodColorSpaceConvert.cpp : 定义 DLL 应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <assert.h>
#include <streams.h>
#include <initguid.h>
#include "QvodColorSpaceConvert.h"
#include "QvodColorSpaceConvertGuid.h"
#include "QvodColorSpaceConvertProp.h"
#include "resource.h"
#include "constant.h"
#include "ISP.h"


#include <Dvdmedia.h>

DefineVar();


#ifdef _MANAGED
#pragma managed(push, off)
#endif

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |				\
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define TESTHR(hr)  if(FAILED(hr)) return hr;

#define CALC_BI_STRIDE(width, ncount)  ((((width * ncount) + 31) & ~31) >> 3)





const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
	&MEDIATYPE_Video,       // Major type
	&MEDIASUBTYPE_NULL      // Minor type
};

const AMOVIESETUP_PIN sudpPins[] =
{
	{ L"Input",             // Pins string name
	FALSE,                // Is it rendered
	FALSE,                // Is it an output
	FALSE,                // Are we allowed none
	FALSE,                // And allowed many
	&CLSID_NULL,          // Connects to filter
	NULL,                 // Connects to pin
	1,                    // Number of types
	&sudPinTypes          // Pin information
	},

	{ L"Output",            // Pins string name
	FALSE,                // Is it rendered
	TRUE,                 // Is it an output
	FALSE,                // Are we allowed none
	FALSE,                // And allowed many
	&CLSID_NULL,          // Connects to filter
	NULL,                 // Connects to pin
	1,                    // Number of types
	&sudPinTypes          // Pin information
	}
};

const AMOVIESETUP_FILTER sudAZImage =
{
	&CLSID_ColorSpaceConvert,			// Filter CLSID
	L"QvodColorSpaceConvert", // String name
	MERIT_DO_NOT_USE,       // Filter merit
	2,                      // Number of pins
	sudpPins                // Pin information
};


CFactoryTemplate g_Templates[2] = 
{
	{ L"QvodColorSpaceConvert"
	, &CLSID_ColorSpaceConvert
	, CColorSpaceConvert::CreateInstance
	, NULL
	, &sudAZImage }
	,{ 
		L"QvodPostVideoPage"
		, &CLSID_ColorSpaceConvertPropPage
		, CColorSpaceConvertProperties::CreateInstance 
	}
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);



STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2( TRUE );

} // DllRegisterServer

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2( FALSE );
} 

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL APIENTRY DllMain(HANDLE hModule, 
					  DWORD  dwReason, 
					  LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

CColorSpaceConvert::CColorSpaceConvert(TCHAR *tszName,
							 LPUNKNOWN punk,
							 HRESULT *phr) :
CTransformFilter(tszName, punk, CLSID_ColorSpaceConvert),
CPersistStream(punk, phr)
{
	//属性页
	//属性页 的初始值将按这里的值设置
	//OpenFile();
	InitPropertyPage();				
	m_pAlignSrc = NULL;		
	m_pAlignDest = NULL;
	CPUTest();
	
	//memset(&ColorPara,0,sizeof(ColorParams));
	ReadConfig();                  // read default value from registry
	SetParams(&ColorPara); // Set Filter parameters
	MakeYUVToRGBTable();

	InitInMediaype();
	InitOutMediatype();
}


CColorSpaceConvert::~CColorSpaceConvert()
{
	//CloseFile();
}

void CColorSpaceConvert::InitPropertyPage()
{                    
}
STDMETHODIMP CColorSpaceConvert::Reset()
{
	return S_OK;
}
STDMETHODIMP CColorSpaceConvert::SetInpinSubType(MediaType nType)
{
	m_InType = nType;
	return S_OK;
}

STDMETHODIMP CColorSpaceConvert::SetOutpinSubType(MediaType nType)
{
	m_OutType = nType;
	return S_OK;
}

STDMETHODIMP CColorSpaceConvert::GetClassID(CLSID *pClsid)
{
	return CBaseFilter::GetClassID(pClsid);

} // GetClassID

///////////////////////////////////////////////////////////////////////
// GetParams: Get the filter parameters to a given destination
///////////////////////////////////////////////////////////////////////
STDMETHODIMP CColorSpaceConvert::GetParams(ColorParams *irp)
{
	CAutoLock cAutolock(&m_VideoLock);
	CheckPointer(irp,E_POINTER);

	*irp = ColorPara;

	return NOERROR;
}

///////////////////////////////////////////////////////////////////////
// SetParams: Set the filter parameters
///////////////////////////////////////////////////////////////////////
STDMETHODIMP CColorSpaceConvert::SetParams(ColorParams *irp)
{
	CAutoLock cAutolock(&m_VideoLock);
	ColorPara = *irp;
	SetDirty(TRUE);
	SaveConfig();
	return NOERROR;
} 

void CColorSpaceConvert::SwapPoiter()
{
}
CUnknown *CColorSpaceConvert::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	CColorSpaceConvert *pNewObject = new CColorSpaceConvert(L"PostVideo Filter", punk, phr);

	if (pNewObject == NULL)
	{
		if (phr)
		{
			*phr = E_OUTOFMEMORY;
		}
	}
	return pNewObject;
} // CreateInstance


//
// NonDelegatingQueryInterface
//
STDMETHODIMP CColorSpaceConvert::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv,E_POINTER);

	if (riid == IID_IColorSpaceConvert) 
	{
		return GetInterface((IColorSpaceConvert *) this, ppv);
	} 
	else if (riid == IID_ISpecifyPropertyPages)
	{
		return GetInterface((ISpecifyPropertyPages *) this, ppv);
	}
	else
	{
		return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
	}
}

STDMETHODIMP CColorSpaceConvert::GetPages(CAUUID *pPages)
{
	if(pPages == NULL) 
	{
		return E_POINTER;
	}
	pPages->cElems = 1;
	pPages->pElems = (GUID *)CoTaskMemAlloc(sizeof(GUID));

	if(pPages->pElems == NULL)
	{
		return E_OUTOFMEMORY;
	}
	pPages->pElems[0] = CLSID_ColorSpaceConvertPropPage;
	return NOERROR;
}

//
// Check the input type
//
bool  CColorSpaceConvert::AllocateTempBuffer()
{
	if(m_pAlignSrc != NULL)
	{
		_aligned_free(m_pAlignSrc);
	}
	if(m_pAlignDest != NULL)
	{
		_aligned_free(m_pAlignDest);
	}
	if(m_nScaleAlign < 0)
	{
		m_nScaleAlign = -m_nScaleAlign;
	}
	if(m_nHeight < 0)
	{
		m_nHeight = -m_nHeight;
	}
	m_pAlignSrc =  (BYTE*)_aligned_malloc(m_nScaleAlign * m_nHeight * 2,MEMORY_ALIGN);	
	m_pAlignDest = (BYTE*)_aligned_malloc(m_nScaleAlign * m_nHeight * 2,MEMORY_ALIGN);
	if(m_pAlignSrc && m_pAlignDest)
	{
		return true;
	}
	else
	{
		return false;
	}
}
HRESULT CColorSpaceConvert::CheckInputType(const CMediaType *mtIn)
{
	CheckPointer(mtIn,E_POINTER);
	if (*mtIn->Type() != MEDIATYPE_Video)
	{
		return VFW_E_TYPE_NOT_ACCEPTED ;
	}

	BITMAPINFOHEADER bih;
	bool b = GetBmpHeader(*mtIn,bih);
	assert(b);
	m_nHeight = bih.biHeight;
	m_nWidth = bih.biWidth;
	m_nScaleAlign = GET_ALIGN_PITCH(m_nWidth);

	if(m_InType.Default)
	{
		if( (*mtIn->Subtype() == MEDIASUBTYPE_YUY2)
			|| (*mtIn->Subtype() == MEDIASUBTYPE_YV12)
			||(*mtIn->Subtype() == MEDIASUBTYPE_RGB24)
			||(*mtIn->Subtype() == MEDIASUBTYPE_RGB32)) 
		{
			return S_OK;
		}
		else
		{
			return VFW_E_TYPE_NOT_ACCEPTED;
		}
	}
	else
	{
		if(m_InType.YV12)
		{
			if( *mtIn->Subtype() == MEDIASUBTYPE_YV12) 
			{
				return S_OK;
			}
			else
			{
				return VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
		if(m_InType.YUY2)
		{
			if( *mtIn->Subtype() == MEDIASUBTYPE_YUY2) 
			{
				return S_OK;
			}
			else
			{
				return VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
		if(m_InType.RGB24)
		{
			if( *mtIn->Subtype() == MEDIASUBTYPE_RGB24) 
			{			
				return S_OK;
			}
			else
			{
				return VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
		if(m_InType.RGB32)
		{
			if( *mtIn->Subtype() == MEDIASUBTYPE_RGB32) 
			{
				return S_OK;
			}
			else
			{
				return VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
	}
	return S_OK;
}

//
// Checktransform
// Check a transform can be done between these formats
//
HRESULT CColorSpaceConvert::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	CheckPointer(mtIn,E_POINTER);
	CheckPointer(mtOut,E_POINTER);

	if (*mtOut->Type() != MEDIATYPE_Video || *mtIn->Type() != MEDIATYPE_Video)
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	if (*mtIn->Subtype() == MEDIASUBTYPE_YUY2 
		|| *mtIn->Subtype() == MEDIASUBTYPE_YV12
		|| *mtIn->Subtype() == MEDIASUBTYPE_RGB24
		||*mtIn->Subtype() == MEDIASUBTYPE_RGB32)
	{
		//if(MEDIASUBTYPE_YUY2 == *mtOut->Subtype() )
		//{	
		//	m_MediType = YUY2;
		//}
		//else if(MEDIASUBTYPE_YV12 == *mtOut->Subtype())
		//{
		//	m_MediType = YV12;
		//}
		//else if(MEDIASUBTYPE_RGB24 == *mtOut->Subtype() )
		//{	
		//	m_MediType = RGB24;
		//}
		//else if(MEDIASUBTYPE_RGB32== *mtOut->Subtype())
		//{
		//	m_MediType = RGB32;
		//}
		//else
		//{
		//	return VFW_E_TYPE_NOT_ACCEPTED;
		//}
		return S_OK;
	}
	else
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	

	//CheckScaleSize();
	return S_OK;
} // CheckTransform

// DecideBufferSize
HRESULT CColorSpaceConvert::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
	if(m_pInput->IsConnected() == FALSE)
		return E_UNEXPECTED;

	CMediaType curMt = m_pOutput->CurrentMediaType();
	BITMAPINFOHEADER bih;
	if(!GetBmpHeader(curMt,bih))
		return E_UNEXPECTED;
	int width = abs(bih.biWidth);
	int height = abs(bih.biHeight);

	if(m_nWidth == 0)
	{
		m_nWidth = width;
	}
	if(m_nHeight == 0)
	{
		m_nHeight = height;
	}

	if(curMt.subtype == MEDIASUBTYPE_YV12)
	{
		pProperties->cbBuffer =  width * height * 3/2;
	}
	else if(curMt.subtype == MEDIASUBTYPE_YUY2)
	{
		pProperties->cbBuffer =  m_nWidth * m_nHeight * 2;	
	}
	else if(curMt.subtype == MEDIASUBTYPE_RGB24)
	{
		pProperties->cbBuffer =  m_nWidth * m_nHeight * 3;
	}
	else if(curMt.subtype == MEDIASUBTYPE_RGB32)
	{
		pProperties->cbBuffer =  m_nWidth * m_nHeight * 4;
	}

	//pProperties->cbBuffer =  1000 * height * 2;
	//assert(pProperties->cbBuffer);

	pProperties->cbPrefix = 0;
	pProperties->cBuffers = 10;
	HRESULT hr;
	ALLOCATOR_PROPERTIES Actual;
	if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) 
		return hr;
	return  pProperties->cbBuffer > Actual.cbBuffer? E_FAIL: NOERROR;
}


// SetMediaType: Override from CTransformFilter. 
HRESULT CColorSpaceConvert::SetMediaType( PIN_DIRECTION direction, const CMediaType *pmt)
{
	return CTransformFilter::SetMediaType(direction,pmt);
}
void CColorSpaceConvert::CheckScaleSize()
{
	if(!m_nHeight || !m_nWidth)
	{
		InitScaleSize();
	}
}
void CColorSpaceConvert::InitScaleSize()
{

	CMediaType imt;
	if(S_OK == m_pInput->ConnectionMediaType(&imt))
	{
		BITMAPINFOHEADER bih;
		bool b = GetBmpHeader(imt,bih);
		assert(b);
		m_nHeight = bih.biHeight;
		m_nWidth = bih.biWidth;
		m_nScaleAlign = GET_ALIGN_PITCH(m_nWidth);
	}
	else
	{
		m_nHeight = 0;
		m_nWidth = 0;
		m_nScaleAlign = 0;
	}
}


//GetMediaType()只有在媒体类型改变时调用才会成功。 你可以在类型改变时，自已保存一个值。 
HRESULT CColorSpaceConvert::GetMediaType(int iPosition, CMediaType *pmt)
{
	CheckPointer(pmt,E_POINTER);
	// Is the input pin connected
	if (m_pInput->IsConnected() == FALSE) 
	{
		return E_UNEXPECTED;
	}
	// This should never happen
	if (iPosition < 0)
	{
		return E_INVALIDARG;
	}

	return GetActualMediaType(iPosition,pmt);
} // GetMediaType

HRESULT CColorSpaceConvert::GetActualMediaType(int iPosition, CMediaType *pmt)
{
	assert(m_nHeight);
	assert(m_nScaleAlign);
	
	BITMAPINFOHEADER bihOut;
	memset(&bihOut, 0, sizeof(BITMAPINFOHEADER));
	bihOut.biSize = sizeof(BITMAPINFOHEADER);
	bihOut.biHeight = m_nHeight;
	bihOut.biWidth  = m_nWidth;//告诉后面filter一个真实的宽
	{
		CMediaType&		pmtInput	= m_pInput->CurrentMediaType();
		const GUID& subtype = pmtInput.subtype;
		VIDEOINFOHEADER2* vihInput = (VIDEOINFOHEADER2*)pmtInput.Format();
		VIDEOINFOHEADER2 vih;
		memset(&vih,0,sizeof(VIDEOINFOHEADER2));
		VIDEOINFOHEADER* vihInput1 = (VIDEOINFOHEADER*)pmtInput.Format();
		VIDEOINFOHEADER vih1;
		memset(&vih1,0,sizeof(VIDEOINFOHEADER));
		{
			if(iPosition == m_OutType.YV12)
			{
				bihOut.biBitCount =  12;
				bihOut.biCompression = MAKEFOURCC('Y','V','1','2'); 
				pmt->SetSubtype(&MEDIASUBTYPE_YV12); 
				bihOut.biPlanes = 3;	

				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih.bmiHeader = bihOut;

				vih.dwPictAspectRatioX = m_nWidth;
				vih.dwPictAspectRatioY = m_nHeight;
				vih.rcSource = vihInput->rcSource;

				vih.rcTarget = vihInput->rcTarget;

				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo2); 
				pmt->SetFormat((BYTE*) & vih, sizeof(VIDEOINFOHEADER2)); 
				pmt->SetSampleSize(vih.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);	
			}
			else if(iPosition == (m_OutType.YV12 + 1))
			{
				bihOut.biBitCount =  12;
				bihOut.biCompression =  MAKEFOURCC('Y','V','1','2'); 
				pmt->SetSubtype(&MEDIASUBTYPE_YV12); 

				bihOut.biPlanes = 3;
				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih1.bmiHeader = bihOut;
				vih1.rcSource = vihInput1->rcSource;
				vih1.rcTarget = vihInput1->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo); 
				pmt->SetFormat((BYTE*) & vih1, sizeof(VIDEOINFOHEADER)); 
				pmt->SetSampleSize(vih1.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);
			}
			else if(iPosition == m_OutType.YUY2)
			{
				bihOut.biBitCount =  16;
				bihOut.biCompression = MAKEFOURCC('Y','U','Y','2'); 
				pmt->SetSubtype(&MEDIASUBTYPE_YUY2); 
				bihOut.biPlanes = 1;	

				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih.bmiHeader = bihOut;

				vih.dwPictAspectRatioX = m_nWidth;
				vih.dwPictAspectRatioY = m_nHeight;
				vih.rcSource = vihInput->rcSource;

				vih.rcTarget = vihInput->rcTarget;

				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo2); 
				pmt->SetFormat((BYTE*) & vih, sizeof(VIDEOINFOHEADER2)); 
				pmt->SetSampleSize(vih.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);	
			}
			else if(iPosition == (m_OutType.YUY2 + 1))
			{
				bihOut.biBitCount =  16;
				bihOut.biCompression =  MAKEFOURCC('Y','U','Y','2'); 
				pmt->SetSubtype(&MEDIASUBTYPE_YUY2); 

				bihOut.biPlanes = 1;
				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih1.bmiHeader = bihOut;
				vih1.rcSource = vihInput1->rcSource;
				vih1.rcTarget = vihInput1->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo); 
				pmt->SetFormat((BYTE*) & vih1, sizeof(VIDEOINFOHEADER)); 
				pmt->SetSampleSize(vih1.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);
			}
			else if(iPosition == m_OutType.RGB24)
			{
				bihOut.biBitCount =  24;
				bihOut.biCompression = BI_RGB; 
				pmt->SetSubtype(&MEDIASUBTYPE_RGB24); 
				bihOut.biPlanes = 1;	

				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih.bmiHeader = bihOut;
				vih.dwPictAspectRatioX = m_nWidth;
				vih.dwPictAspectRatioY = m_nHeight;
				vih.rcSource = vihInput->rcSource;
				vih.rcTarget = vihInput->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo2); 
				pmt->SetFormat((BYTE*) & vih, sizeof(VIDEOINFOHEADER2)); 
				pmt->SetSampleSize(vih.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);	
			}
			else if(iPosition == (m_OutType.RGB24 + 1))
			{
				bihOut.biBitCount =  24;
				bihOut.biCompression = BI_RGB; 
				pmt->SetSubtype(&MEDIASUBTYPE_RGB24); 

				bihOut.biPlanes = 1;
				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih1.bmiHeader = bihOut;
				vih1.rcSource = vihInput1->rcSource;
				vih1.rcTarget = vihInput1->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo); 
				pmt->SetFormat((BYTE*) & vih1, sizeof(VIDEOINFOHEADER)); 
				pmt->SetSampleSize(vih1.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);
			}
			else if(iPosition == m_OutType.RGB32)
			{
				bihOut.biBitCount =  32;
				bihOut.biCompression = BI_RGB; 
				pmt->SetSubtype(&MEDIASUBTYPE_RGB32); 
				bihOut.biPlanes = 1;	

				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih.bmiHeader = bihOut;
				vih.dwPictAspectRatioX = m_nWidth;
				vih.dwPictAspectRatioY = m_nHeight;
				vih.rcSource = vihInput->rcSource;
				vih.rcTarget = vihInput->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo2); 
				pmt->SetFormat((BYTE*) & vih, sizeof(VIDEOINFOHEADER2)); 
				pmt->SetSampleSize(vih.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);	
			}
			else if(iPosition == (m_OutType.RGB32 + 1))
			{
				bihOut.biBitCount =  32;
				bihOut.biCompression = BI_RGB; 
				pmt->SetSubtype(&MEDIASUBTYPE_RGB32); 

				bihOut.biPlanes = 1;
				bihOut.biSizeImage = CalcImageSize(bihOut.biWidth,bihOut.biHeight, bihOut.biBitCount);
				vih1.bmiHeader = bihOut;
				vih1.rcSource = vihInput1->rcSource;
				vih1.rcTarget = vihInput1->rcTarget;
				pmt->SetType(&MEDIATYPE_Video); 
				pmt->SetFormatType(&FORMAT_VideoInfo); 
				pmt->SetFormat((BYTE*) & vih1, sizeof(VIDEOINFOHEADER)); 
				pmt->SetSampleSize(vih1.bmiHeader.biSizeImage);
				pmt->SetTemporalCompression(TRUE);
			}
			else
			{
				return VFW_S_NO_MORE_ITEMS;
			}
		}
    }
	return S_OK;
}


BOOL CColorSpaceConvert::CanPerformVideo(const CMediaType *pMediaType) const
{
	CheckPointer(pMediaType,FALSE);

	if (IsEqualGUID(*pMediaType->Type(), MEDIATYPE_Video)) 
	{
		if (IsEqualGUID(*pMediaType->Subtype(), MEDIASUBTYPE_YUY2)) 
		{
			VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) pMediaType->Format();
			return (pvi->bmiHeader.biBitCount == 16);
		}
	}
	return FALSE;
} // CanPerformVideo


HRESULT CColorSpaceConvert::Receive(IMediaSample* pIn)
{
	CAutoLock cAutoLock(&m_VideoLock);

	HRESULT hr;

	REFERENCE_TIME	rtStart;
	REFERENCE_TIME	rtStop;

	if(S_OK !=m_pInput->CheckStreaming())
		return S_FALSE;

	AM_SAMPLE2_PROPERTIES*  pProps = m_pInput->SampleProps();
	if(pProps->dwStreamId != AM_STREAM_MEDIA)
		return m_pOutput->Deliver(pIn);

	if(FAILED(hr = pIn->GetPointer(&m_Inpin.pData))) 
	{
		return hr;
	}
	hr	= pIn->GetTime(&rtStart, &rtStop);

	if (rtStop <= rtStart)
	{
		rtStop = rtStart ;
	}

#ifdef _DEBUG
	const GUID& subtype = m_pInput->CurrentMediaType().subtype;
	assert(subtype == MEDIASUBTYPE_YUY2 || 
		subtype == MEDIASUBTYPE_YV12 ||
		subtype == MEDIASUBTYPE_RGB24 ||
		subtype == MEDIASUBTYPE_RGB32);
#endif
	//得到输入宽高参数
	m_Inpin.nSize = pIn->GetActualDataLength();
	CMediaType mtIn = m_pInput->CurrentMediaType();
	BITMAPINFOHEADER bih;
	if (!GetBmpHeader(mtIn,bih))
	{
		return E_UNEXPECTED;
	}
	m_Inpin.nWidth = bih.biWidth;
	m_Inpin.nHeight = bih.biHeight;
	if(m_Inpin.nHeight < 0)
	{
		m_Inpin.nHeight = -m_Inpin.nHeight;
	}
	if(m_Inpin.nWidth < 0)
	{
		m_Inpin.nWidth = -m_Inpin.nWidth;
	}
	m_Inpin.nPitch = m_Inpin.nSize / m_Inpin.nHeight;

	hr = Deliver(m_Inpin.pData,rtStart,rtStop);
	return NOERROR;
}

HRESULT CColorSpaceConvert::Deliver(BYTE* pData,REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	HRESULT hr;
	CMediaType newMt;

	CComPtr<IMediaSample>pOut;
	if(FAILED(hr = m_pOutput->GetDeliveryBuffer(&pOut, NULL, NULL, 0)) || FAILED(hr = pOut->GetPointer(&m_Outpin.pData)))
		return hr;

	
	pOut->SetTime(&rtStart, &rtStop);
	CMediaType mtOut = m_pOutput->CurrentMediaType();
	BITMAPINFOHEADER bih;
	if (!GetBmpHeader(mtOut,bih))
	{
		return E_UNEXPECTED;
	}
	
	m_Outpin.nWidth = bih.biWidth;
	m_Outpin.nHeight = bih.biHeight;
	m_Outpin.nSize = pOut->GetActualDataLength();
	m_Outpin.nPitch = m_Outpin.nSize / m_Outpin.nHeight;

	
	int nConvertType = SelectConvertType();
	if(nConvertType != -1)
	{
		const GUID& SubtypeIn = m_pInput->CurrentMediaType().subtype;
		if(SubtypeIn == MEDIASUBTYPE_RGB24)	
		{
			if(m_pAlignSrc != NULL)
			{
				_aligned_free(m_pAlignSrc);
			}
			m_pAlignSrc =  (BYTE*)_aligned_malloc(m_nWidth * m_nHeight * 3+16,MEMORY_ALIGN);	
			
			CopyFrame(m_pAlignSrc,pData,m_Inpin.nPitch,m_Inpin.nPitch
				,m_nHeight,m_Inpin.nHeight);
			ColorConvert (m_pAlignSrc, m_Outpin.pData,m_nWidth,m_nHeight,
				m_Inpin.nPitch,m_Outpin.nPitch,nConvertType);
		}
		else
		{
			ColorConvert (pData, m_Outpin.pData,m_nWidth,m_nHeight,
				m_Inpin.nPitch,m_Outpin.nPitch,nConvertType);
		}
		
	}
	else
	{
		const GUID& SubtypeIn = m_pInput->CurrentMediaType().subtype;
		if(SubtypeIn == MEDIASUBTYPE_YV12)
		{
			m_Outpin.nPitch = m_Outpin.nPitch * 2 / 3;
			m_Inpin.nPitch = m_Inpin.nPitch * 2 / 3; 
			CopyYV12Frame(m_Outpin.pData
				,m_Outpin.nPitch
				,m_Outpin.nHeight
				,pData
				,m_Inpin.nPitch
				,m_Inpin.nHeight);
		}
		else
		{
			if(SubtypeIn == MEDIASUBTYPE_RGB32)
			{
				int width = m_Inpin.nWidth;
				int height = m_Inpin.nHeight;
				BYTE *pTemp = new BYTE[width * 4];
				for(int i = 0;i<height/2;i++)
				{ 
					CopyMemory(pTemp,pData+i * width*4,width * 4);
					CopyMemory(pData+i * width*4,pData+(height-i-1)* width*4,width * 4);
					CopyMemory(pData+(height-i-1)* width*4,pTemp,width * 4);
				}
				delete []pTemp;
				
			}
			CopyFrame(m_Outpin.pData, pData,m_Outpin.nPitch,m_Inpin.nPitch,
				m_Outpin.nHeight,m_Inpin.nHeight);
		}		
	}
	return m_pOutput->Deliver(pOut);

}


void CColorSpaceConvert::CopyFrame(
							  unsigned char* pDest
							  ,unsigned char * pSrc
							  ,int nDestPitch
							  ,int nSrcPitch
							  ,int nDestHeight
							  ,int nSrcHeight)
{
	int minPitch = min(nDestPitch,nSrcPitch);
	int minHeight = min(nDestHeight,nSrcHeight);

	if((nDestPitch == nSrcPitch) && (nDestHeight == nSrcHeight) )
	{
		memcpy(pDest,pSrc,nSrcPitch * nSrcHeight);
	}
	else
	{
		for(int i = 0;i<minHeight;i++)
		{
			memcpy(pDest,pSrc,nSrcPitch);
			pSrc += nSrcPitch;
			//pSrc += nDestPitch;
			pDest +=  nDestPitch;
			//pDest +=  nSrcPitch;
		}
	}
}

void CColorSpaceConvert::CopyYV12Frame(BYTE* pDst,int nDstPitch,int nDstHeight,BYTE*pSrc,int nSrcPitch,int nSrcHeight)
{
	if ((nSrcPitch < 0) || (nSrcHeight < 0)|| (nDstPitch < 0)|| (nDstHeight < 0))
	{
		return;
	}
	if ((nSrcHeight == nDstHeight)&&(nSrcPitch == nDstPitch))
	{
		memcpy(pDst,pSrc,nSrcHeight * nSrcPitch * 3/2);
		return;
	}
	//copy y

	BYTE *pDstY = pDst ;//this important;
	BYTE *pSrcY = pSrc ;
	int nPitchY = min(nSrcPitch,nDstPitch);
	int nHeightY = min(nSrcHeight,nDstHeight);
	for (int i = 0; i < nHeightY; ++i)
	{
		memcpy(pDstY,pSrcY,nPitchY);
		pDstY += nDstPitch;
		pSrcY += nSrcPitch;
	}

	int nPitchUV = nPitchY / 2;
	int nHeightUV = nHeightY / 2;
	int nHalfSrcPic = nSrcPitch / 2;
	int nHalfDstPic = nDstPitch / 2;


	//copy u		
	BYTE *pDstU = pDst + nDstPitch * nDstHeight;//this important;
	BYTE *pSrcU = pSrc + nSrcPitch * nSrcHeight;
	for (int i = 0; i < nHeightUV; ++i)
	{
		memcpy(pDstU,pSrcU,nPitchUV);
		pDstU += nHalfDstPic;
		pSrcU += nHalfSrcPic;
	}


	//copy v
	BYTE *pDstV =  pDst  + nDstPitch * nDstHeight * 5/4;//this important;
	BYTE *pSrcV =  pSrc  + nSrcPitch * nSrcHeight * 5/4;
	for (int i = 0; i < nHeightUV; ++i)
	{
		memcpy(pDstV,pSrcV,nPitchUV);
		pDstV += nHalfDstPic;
		pSrcV += nHalfSrcPic;
	}
}


void CColorSpaceConvert::CopyFrameRGB32(
								   unsigned char* pDest
								   ,unsigned char * pSrc
								   ,int nDestPitch
								   ,int nSrcPitch
								   ,int nDestHeight
								   ,int nSrcHeight)
{
	int minPitch = min(nDestPitch,nSrcPitch);
	int minHeight = min(nDestHeight,nSrcHeight);

	if((nDestPitch == nSrcPitch) && (nDestHeight == nSrcHeight) )
	{
		memcpy(pDest,pSrc,nSrcPitch * nSrcHeight * 4 );
	}
	else
	{

		for(int i = 0;i<minHeight;i++)
		{
			memcpy(pDest,pSrc,minPitch*4);
			pSrc += nSrcPitch * 4;
			pDest +=  nDestPitch * 4 ;
		}
	}
}

void CColorSpaceConvert::InitInMediaype()
{
	m_InType.YV12 = 0;
	m_InType.YUY2 = 0;
	m_InType.RGB24 = 0;
	m_InType.RGB32 = 0;
	m_InType.Default = 1;
}
void CColorSpaceConvert::InitOutMediatype()
{
	m_OutType.YV12 = 0;
	m_OutType.YUY2 = 2;
	m_OutType.RGB24 = 4;
	m_OutType.RGB32 =6;
	m_OutType.Default = 1;
}




HRESULT CColorSpaceConvert::StartStreaming()
{
	return S_OK;
}

HRESULT CColorSpaceConvert::StopStreaming()
{
	return S_OK;
}
bool CColorSpaceConvert::GetBmpHeader(const CMediaType &mt,BITMAPINFOHEADER & bph)
{
	if(*mt.FormatType() == FORMAT_VideoInfo) 
	{
		VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) mt.Format();
		bph = pvi->bmiHeader;
		return true;
	}
	else if(*mt.FormatType() == FORMAT_VideoInfo2) 
	{
		VIDEOINFOHEADER2 *pvi = (VIDEOINFOHEADER2 *) mt.Format();
		bph = pvi->bmiHeader;
		return true;
	}
	else
	{
		return false;
	}
}
bool CColorSpaceConvert::SetBphToVph( CMediaType &mt,const BITMAPINFOHEADER & bph)
{
	if(*mt.FormatType() == FORMAT_VideoInfo) 
	{
		VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) mt.Format();
		pvi->bmiHeader = bph;
		mt.SetFormat((BYTE *)pvi,sizeof(VIDEOINFOHEADER));
		return true;
	}
	else if(*mt.FormatType() == FORMAT_VideoInfo2) 
	{
		VIDEOINFOHEADER2 *pvi = (VIDEOINFOHEADER2 *) mt.Format();
		pvi->bmiHeader = bph;
		mt.SetFormat((BYTE *)pvi,sizeof(VIDEOINFOHEADER2));
		return true;
	}
	else
	{
		return false;
	}	
}



#define WRITEOUT(var)  hr = pStream->Write(&var, sizeof(var), NULL); \
	if (FAILED(hr)) return hr;

#define READIN(var)    hr = pStream->Read(&var, sizeof(var), NULL); \
	if (FAILED(hr)) return hr;

//
// ScribbleToStream
//
// Overriden to write our state into a stream
//
HRESULT CColorSpaceConvert::ScribbleToStream(IStream *pStream)
{
	//HRESULT hr;

	//WRITEOUT(m_nEffect);

	return NOERROR;

} // ScribbleToStream


HRESULT CColorSpaceConvert::ReadFromStream(IStream *pStream)
{
	//HRESULT hr;

	//READIN(m_nEffect);

	return NOERROR;

} // ReadFromStream




int  CColorSpaceConvert::CalcImageSize(int width,int heigth,int bitCount)
{
	return CALC_BI_STRIDE(width,bitCount) * heigth;
}

int  CColorSpaceConvert::SelectConvertType()
{
	const GUID& SubtypeIn = m_pInput->CurrentMediaType().subtype;
	const GUID& SubtypeOut = m_pOutput->CurrentMediaType().subtype;
	if(SubtypeIn == MEDIASUBTYPE_YV12 && SubtypeOut == MEDIASUBTYPE_RGB24)
	{
		return 0;
	}
	else if(SubtypeIn == MEDIASUBTYPE_YV12 && SubtypeOut == MEDIASUBTYPE_RGB32)
	{
		return 1;
	}
	else if(SubtypeIn == MEDIASUBTYPE_YV12 && SubtypeOut == MEDIASUBTYPE_YUY2)
	{
		return 2;
	}
	else if(SubtypeIn == MEDIASUBTYPE_YUY2 && SubtypeOut == MEDIASUBTYPE_RGB24)
	{
		return 3;
	}
	else if(SubtypeIn == MEDIASUBTYPE_YUY2 && SubtypeOut == MEDIASUBTYPE_RGB32)
	{
		return 4;
	}
	else if(SubtypeIn == MEDIASUBTYPE_YUY2 && SubtypeOut == MEDIASUBTYPE_YV12)
	{
		return 5;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB24 && SubtypeOut == MEDIASUBTYPE_RGB32)
	{
		return 6;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB24 && SubtypeOut == MEDIASUBTYPE_YUY2)
	{
		return 7;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB24 && SubtypeOut == MEDIASUBTYPE_YV12)
	{
		return 8;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB32 && SubtypeOut == MEDIASUBTYPE_RGB24)
	{
		return 9;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB32 && SubtypeOut == MEDIASUBTYPE_YUY2)
	{
		return 10;
	}
	else if(SubtypeIn == MEDIASUBTYPE_RGB32 && SubtypeOut == MEDIASUBTYPE_YV12)
	{
		return 11;
	}
	return -1;
}

void CColorSpaceConvert::ReadConfig()
{
	// read integer from registry
	ColorPara.inYV12 = 
		GetProfileInt(L"ColorConvert", L"inYV12", 0);
	ColorPara.inYUY2 = 
		GetProfileInt(L"ColorConvert",L"inYUY2", 0);
	ColorPara.inRGB24 = 
		GetProfileInt(L"ColorConvert",L"inRGB24", 0);
	ColorPara.inRGB32 = 
		GetProfileInt(L"ColorConvert",L"inRGB32", 0);
	ColorPara.outYV12 = 
		GetProfileInt(L"ColorConvert",L"outYV12", 0);
	ColorPara.outYUY2 = 
		GetProfileInt(L"ColorConvert",L"outYUY2", 0);
	ColorPara.outRGB24 = 
		GetProfileInt(L"ColorConvert",L"outRGB24", 0);
	ColorPara.outRGB32 = 
		GetProfileInt(L"ColorConvert",L"outRGB32", 0);
}

void CColorSpaceConvert::SaveConfig()
{
	char sz[STR_MAX_LENGTH];
	// write int into registry
	sprintf(sz, "%d", ColorPara.inYV12);
	WriteProfileStringA("ColorConvert", "inYV12", sz);

	sprintf(sz, "%d", ColorPara.inYUY2);
	WriteProfileStringA("ColorConvert", "inYUY2", sz);

	sprintf(sz, "%d", ColorPara.inRGB24);
	WriteProfileStringA("ColorConvert", "inRGB24", sz);

	sprintf(sz, "%d", ColorPara.inRGB32);
	WriteProfileStringA("ColorConvert", "inRGB32", sz);

	sprintf(sz, "%d", ColorPara.outYV12);
	WriteProfileStringA("ColorConvert", "outYV12", sz);

	sprintf(sz, "%d", ColorPara.outYUY2);
	WriteProfileStringA("ColorConvert", "outYUY2", sz);

	sprintf(sz, "%d", ColorPara.outRGB24);
	WriteProfileStringA("ColorConvert", "outRGB24", sz);

	sprintf(sz, "%d", ColorPara.outRGB32);
	WriteProfileStringA("ColorConvert", "outRGB32", sz);

}

#ifdef _MANAGED
#pragma managed(pop)
#endif

