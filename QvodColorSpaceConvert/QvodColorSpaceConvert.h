//-------------------------------------------------
//	Desc: VideoProcess.h
//  Date: 2010.12.13
//-------------------------------------------------

#include "IColorSpaceConvert.h"

#define MEMORY_ALIGN		16
#define MAX_SCALE_HEIGHT    3000
#define MAX_SCALE_WIDTH     2000
#define MIN_SCALE_HEIGHT    64
#define MIN_SCALE_WIDTH     64
#define GET_ALIGN_PITCH(width)   ((((width) + 15) >> 4) <<4)




class CColorSpaceConvert : public CTransformFilter,
	public IColorSpaceConvert,
	public CPersistStream,
	public ISpecifyPropertyPages
{	
public:
	DECLARE_IUNKNOWN;
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

	STDMETHODIMP GetPages(CAUUID *pPages);
	// CPersistStream stuff
	HRESULT ScribbleToStream(IStream *pStream);
	HRESULT ReadFromStream(IStream *pStream);

	HRESULT Receive(IMediaSample* pIn);
	HRESULT Deliver(BYTE* pBuff,REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);

	// Overrriden from CTransformFilter base class
	HRESULT CheckInputType(const CMediaType *mtIn);
	HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
	HRESULT DecideBufferSize(IMemAllocator *pAlloc,	ALLOCATOR_PROPERTIES *pProperties);
	HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
	HRESULT SetMediaType( PIN_DIRECTION direction, const CMediaType *pmt);
	HRESULT StopStreaming();
	HRESULT StartStreaming();


	//Overriden from IEFFect
	STDMETHODIMP SetInpinSubType(MediaType nType);
	STDMETHODIMP SetOutpinSubType(MediaType nType);

	STDMETHODIMP Reset();

	// CPersistStream override
	STDMETHODIMP GetClassID(CLSID *pClsid);

	STDMETHODIMP GetParams(ColorParams *irp);
	// Set the parameters of the filter
	STDMETHODIMP SetParams(ColorParams *irp);
	//DECLARE_IUNKNOWN;

private:
	CColorSpaceConvert(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr);
	~CColorSpaceConvert();

	HRESULT GetBitmapInfoHeader(CMediaType &mt,BITMAPINFOHEADER ** outBitmapInfo);

	BOOL CanPerformVideo(const CMediaType *pMediaType) const;

	void SwapPoiter();
	void InitPropertyPage();
	bool AllocateTempBuffer();
	bool GetBmpHeader(const CMediaType &mt,BITMAPINFOHEADER & bph);
	bool SetBphToVph( CMediaType &mt,const BITMAPINFOHEADER & bph);
	HRESULT GetActualMediaType(int iPosition, CMediaType *pMediaType);
	void CopyFrame(unsigned char* pDest
		,unsigned char * pSrc
		,int nDestPitch
		,int nSrcPitch
		,int nDestHeight
		,int nSrcHeight);
	void CopyYV12Frame(BYTE* pDst
		,int nDstPitch
		,int nDstHeight
		,BYTE*pSrc
		,int nSrcPitch
		,int nSrcHeight);
	void CopyFrameRGB32(
		unsigned char* pDest
		,unsigned char * pSrc
		,int nDestPitch
		,int nSrcPitch
		,int nDestHeight
		,int nSrcHeight);
	void InitScaleSize();
	void CheckScaleSize();
	int  CalcImageSize(int width,int heigth,int bitCount);
	void SaveConfig();
	void ReadConfig();
	int SelectConvertType();

private:
	void InitInMediaype();
	void InitOutMediatype();
private:
	CCritSec			m_VideoLock;			 // Private play critical section
	
	MediaType      m_MediType;


private://宽 高
	struct tagPinInfo
	{
		tagPinInfo()
			:nWidth(0)
			,nHeight(0)
			,nPitch(0)
			,nSize(0)
			,pData(NULL){}

		int nWidth;
		int nHeight;
		int nPitch;
		int nSize;
		BYTE *pData;
	};
	tagPinInfo m_Inpin;
	tagPinInfo m_Outpin;

	unsigned char		*m_pAlignSrc;		//对齐的内存指针
	unsigned char		*m_pAlignDest;		//对齐的内存指针
	int					m_nHeight;		//源高
	int					m_nWidth;		//源宽
	int					m_nScaleAlign;	
	MediaType     m_InType;
	MediaType     m_OutType;
	ColorParams  ColorPara;
}; 
