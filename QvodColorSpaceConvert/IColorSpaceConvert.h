//-------------------------------------------------
//	Desc: IColorSpaceConvert.h
//  Date: 2010.12.13
//-------------------------------------------------

#ifndef __COLORSPACECONVERT_H__
#define __COLORSPACECONVERT_H__
#include <streams.h>
#ifdef __cplusplus
extern "C" {
#endif

	struct MediaType
	{
		int RGB24 ;
		int RGB32 ;
		int YUY2 ;
		int YV12 ;
		int Default;
	};
	struct ColorParams {
		int    inYV12;
		int    inYUY2;
		int    inRGB24;
		int    inRGB32;
		int    outYV12;
		int    outYUY2;
		int    outRGB24;
		int    outRGB32;
	};
	
	DECLARE_INTERFACE_(IColorSpaceConvert, IUnknown)
	{
		STDMETHOD(SetInpinSubType)(THIS_
			MediaType nType
			)PURE;
		STDMETHOD(SetOutpinSubType)(THIS_
			MediaType nType
			)PURE;
		STDMETHOD(Reset)(THIS_
			)PURE;
		STDMETHOD(GetParams)(THIS_
			ColorParams *irp)PURE;
		// Set the parameters of the filter
		STDMETHOD(SetParams)(THIS_
			ColorParams *irp)PURE;
	};

#ifdef __cplusplus
}
#endif

#endif 
