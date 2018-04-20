#ifndef __ISP_H
#define __ISP_H
#include "log.h"

enum CPUType{CPU_MMX = 1,CPU_SSE2,CPU_UNKNOW};

BOOL CPUTest();
void  InitRGBToYUVMMX(int   coeffs) ;

void YUV2ToYV12MMX(BYTE *const pYUV, BYTE *pYV12,const int nWidth,const int  nHeight,int SrcPitch,int DstPitch);
void YUV2ToYV12SSE2(BYTE *const pYUV, BYTE *pYV12,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void YUY2ToRGB24SSE2Line(BYTE* pDstLine,BYTE* pYUYV,long width ,long height);
void YUY2ToRGB32MMXLine(BYTE* pDstLine,BYTE* pYUYV,long width);
void YUY2ToRGB32SSE2Line(BYTE* pDstLine,BYTE* pYUYV,long width);

void YV12ToRGB24MMX(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
					BYTE *puc_out, int width_y,  int height_y,int stride_out) ;
void YV12ToRGB32MMX(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
					BYTE *puc_out, int width_y,  int height_y,int stride_out) ;
BOOL YV12ToRGB24C(BYTE* pYV12, BYTE* pRGB24,int nWidth,int nHeight);

void  YV12ToYUY2MMX(const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, int src_rowsize, int src_pitch, int src_pitch_uv, 
					BYTE* dst, int dst_pitch,int height) ;
void  YUV_TO_RGB24(    BYTE *puc_y,        int stride_y, 
				  BYTE *puc_u,        BYTE *puc_v, int stride_uv, 
				  BYTE *puc_out,    int width_y,    int height_y,int stride_out) ;

void  RGB24ToYUY2MMX(BYTE *dest, BYTE *src, int nWidth, int nHeight,int SrcPitch,int DstPitch);
void  RGB24ToYUY2SSE2(BYTE *desty, BYTE *src, int width, int height, int   srcrowsize,   int   destrowsize);
void  RGB32ToYUY2MMX(BYTE   *src,   BYTE   *desty,    int   srcrowsize,   int   destrowsize, 
					int   width,   int   height) ;
void  RGB32ToYUY2SSE2(BYTE   *src,   BYTE   *desty,    int   srcrowsize,   int   destrowsize, 
					int   width,   int   height) ;

void  RGB24toYV12MMX(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, 
					  int   srcrowsize, int   destrowsize, int   width,   int   height) ;
void  RGB24toYV12SSE2(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, int   srcrowsize, int   destrowsize, 
					   int   width,   int   height) ;

void  RGB32toYV12MMX(BYTE   *src,   BYTE   *desty,  BYTE   *destu,BYTE   *destv,
					  int   srcrowsize,   int   destrowsize, int   width,   int   height) ;
void  RGB32toYV12SSE2(BYTE   *src,   BYTE   *desty,  BYTE   *destu,BYTE   *destv,
					  int   srcrowsize,   int   destrowsize, int   width,   int   height) ;

void  RGB32toRGB24C(BYTE   *src,   BYTE   *dest, int   width,   int   height) ;
void  RGB24toRGB32C(BYTE   *src,   BYTE   *dest, int   width,   int   height) ;
void  RGB32ToRGB24MMX(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch);
void  RGB32ToRGB24SSE2(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch);

void  RGB24ToRGB32MMX(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch);
void  RGB24ToRGB32SSE2(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch);


void  MakeYUVToRGBTable();
void  DecodeYV12ToRgb24Line(BYTE* pDstLine,const BYTE *pY,
						    BYTE *pU, BYTE *pV,long width);
void  YV12ToRGB24Two(BYTE *pDst,const BYTE Y0,const BYTE Y1,
					const BYTE U,const BYTE V);
void  DecodeYV12ToRgb32Line(BYTE* pDstLine,const unsigned char *pY,
						   const BYTE *pU,const BYTE *pV,long width);
void  DecodeYUY2ToRgb32Line(BYTE* pDstLine,const unsigned char *pYUYV,long width);
void  DecodeYUY2ToRgb24Line(BYTE* pDstLine,const unsigned char *pYUYV,long width);
void  YV12ToRGB32Two(BYTE *pDst,const BYTE Y0,const BYTE Y1,
					const BYTE U,const BYTE V);
long  BorderColor(long color);



void  YV12ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  YV12ToRGB32Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  YV12ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  YUY2ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  YUY2ToRGB32Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  YUY2ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGB24ToRGB32Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGB24ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGB24ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGb32ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGb32ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
void  RGb32ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);

void  ColorConvert (BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int nOrder,int SrcPitch,int DstPitch);



#endif