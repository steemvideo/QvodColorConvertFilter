#include "stdafx.h"
#include "constant.h"
#include "ISP.h"
#include <assert.h>
#include <xmmintrin.h>

CPUType        g_cpuType;

#define CPUID_STD_MMX          0x00800000
#define CPUID_STD_SSE          0x02000000
#define CPUID_STD_SSE2         0x04000000
#define CPUID_EXT_3DNOW        0x80000000

static const int csY_coeff_16 = (int)(1.164383*(1<<16));
static const int csU_blue_16  = (int)(2.017232*(1<<16));
static const int csU_green_16 = (int)((-0.391762)*(1<<16)); 
static const int csV_green_16 = (int)((-0.812968)*(1<<16));
static const int csV_red_16   = (int)(1.596027*(1<<16));

static unsigned char  nColor_Table[256*3];
static const unsigned char * Color_Table = &nColor_Table[256];



void   InitRGBToYUVMMX(int   coeffs) 
{ 
	int   i; 
	i   =   coeffs; 
	if   (i   >   8) 
		i   =   3; 

	ycoefs   =   &ycoef[i-1][0]; 
	ucoefs   =   &ucoef[i-1][0]; 
	vcoefs   =   &vcoef[i-1][0]; 
}

BOOL CPUTest()
{
	
	__try 
	{
		__asm xor    eax, eax
		__asm xor    ebx, ebx
		__asm xor    ecx, ecx
		__asm xor    edx, edx
		__asm cpuid
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		g_cpuType = CPU_UNKNOW;
		return false;
	}
	unsigned int cpuFeature = 0;

	//探测CPU类型
	__asm
	{
		xor     eax, eax                      // CPUID function #0
		cpuid                                 // largest std func/vendor string

		test    eax, eax                      // largest standard function==0?
		jz      $no_standard_features         // yes, no standard features func

		mov eax, 1
		cpuid
		//测试是否支持MMX
		mov ecx,CPUID_STD_MMX
		and ecx,edx
		or  cpuFeature,ecx
		//测试是否支持SSE
		mov ecx,CPUID_STD_SSE
		and ecx,edx
		or  cpuFeature,ecx
		//测试是否支持SSE2
		mov ecx,CPUID_STD_SSE2
		and ecx,edx
		or  cpuFeature,ecx

$no_standard_features:
	}
	if(cpuFeature >= (CPUID_STD_MMX + CPUID_STD_SSE + CPUID_STD_SSE2))
	{
		g_cpuType = CPU_SSE2;
	}
	else if(cpuFeature >= (CPUID_STD_MMX))
	{
		g_cpuType = CPU_MMX;
	}
	else 
	{
		g_cpuType = CPU_UNKNOW;
		return false;
	}

	//测试操作系统是否支持SIMD
	_try {
		__asm xorps xmm0, xmm0 ;Streaming SIMD Extension
	} 
	_except(EXCEPTION_EXECUTE_HANDLER) {
		if (_exception_code()==STATUS_ILLEGAL_INSTRUCTION)
		{
			/* SSE not supported */
			g_cpuType = CPU_UNKNOW;
			return false;
		}
	}
	return true;
}

void YUV2ToYV12MMX(BYTE *const pYUV, BYTE *pYV12,const int nWidth,const int  nHeight,int SrcPitch,int DstPitch)
{
	const int nDestPitch  = (DstPitch<<1) / 3 ;

	const int nSrcPitch = nWidth * 2;
	const int nHalfHeight = nHeight / 2;

	BYTE *const pY = pYV12;
	BYTE *const pV = pYV12  + nDestPitch * nHeight;
	BYTE *const pU = pYV12  + nDestPitch * nHeight + nDestPitch  * nHeight / 4;
	// BYTE *const pSrc = pYUV;

	const int nYPitch = nDestPitch ;
	const int nUPitch = nDestPitch / 2;
	const int nVPitch = nDestPitch / 2;

	const int minWidth = min(nWidth,nDestPitch);
	//ASSERT(nWidth <= nDestPitch);

	const int nCycle = minWidth / 8;       // calc 8 pix
	BYTE *pSrcTemp = pYUV;
	BYTE *pYTemp = pY;
	BYTE *pUTemp = pU;
	BYTE *pVTemp = pV;

	static const __int64 maskY = {0x00ff00ff00ff00ff};
	static const __int64 maskU = {0x000000ff000000ff};

	for(int i = 0;i< nHeight; i++)
	{
		if(i %2 == 0)
		{
			__asm
			{
				mov ecx,nCycle;
				mov	esi,pSrcTemp
				mov edi,pYTemp;
				mov eax,pUTemp;
				mov edx,pVTemp;
LOOP1:	
				movq    mm0,[esi];
				movq    mm1,[esi+8];
				pxor	mm7,mm7;
				movq    mm2,mm0;
				movq    mm3,mm1;
				pand    mm0,maskY;
				pand    mm1,maskY;
				packuswb mm0,mm1; // 0000yyyy ...

				movntq	[edi],mm0

				psrlw    mm2,8; // 0 v 0 u ....
				psrlw    mm3,8; // 0 v 0  u ...
				movq     mm0,mm2;
				movq     mm1,mm3;
				pand     mm2,maskU;
				pand     mm3,maskU;
				packssdw mm2,mm3;
				packuswb mm2,mm7;// 00000...uuuu...

				movd		[eax],mm2;

				psrld	 mm0,16;
				psrld	 mm1,16;
				packssdw mm0,mm1;
				packuswb mm0,mm7; //0000...vvvv...

				movd    [edx],mm0;

				add    esi,16;
				add    edi,8;
				add    eax,4;
				add    edx,4;

				dec    ecx;
				jnz    LOOP1
			}
			pUTemp += nUPitch;
			pVTemp += nVPitch;
		}
		else
		{
			__asm
			{
				mov ecx,nCycle;
				mov	esi,pSrcTemp
				mov edi,pYTemp;
LOOP2:	
				movq  mm0,[esi];
				movq  mm1,[esi+8];
				pxor	mm7,mm7;
				movq   mm2,mm0;
				movq   mm3,mm1;
				pand   mm0,maskY;
				pand   mm1,maskY;
				packuswb mm0,mm1; // 0000yyyy ...
				movntq	[edi],mm0

				add    esi,16;
				add    edi,8;

				dec    ecx;
				jnz    LOOP2	
			}
		}
		pSrcTemp += nSrcPitch;
		pYTemp += nYPitch;
	}
	__asm emms
}
void YUV2ToYV12SSE2(BYTE *const pYUV, BYTE *pYV12,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	const int nDestPitch  = (DstPitch<<1) / 3 ;
	//const int nDestPitch  = nWidth ;

	const int nSrcPitch = nWidth * 2;

	const int nHalfHeight = nHeight / 2;

	BYTE *const pY = pYV12;
	BYTE *const pV = pYV12  + nDestPitch * nHeight;
	BYTE *const pU = pYV12  + nDestPitch * nHeight + nDestPitch  * nHeight / 4;
	BYTE *const pSrc = pYUV;

	const int nYPitch = nDestPitch ;
	const int nUPitch = nDestPitch / 2;
	const int nVPitch = nDestPitch / 2;

	const int minWidth = min(nWidth,nDestPitch);
	//ASSERT(nWidth <= nDestPitch);

	const int nCycle = minWidth / 16;       // calc 16 pix
	int  nRemain = minWidth % 16*2;			//不对齐的数量
	int  nOffset = minWidth - nRemain;
	BYTE *pSrcTemp = pSrc;
	BYTE *pYTemp = pY;
	BYTE *pUTemp = pU;
	BYTE *pVTemp = pV;

	int  nYdiff =nYPitch -  nCycle * 16;
	int  nUdiff =nYPitch -  nCycle * 8;
	int  nVdiff =nYPitch -  nCycle * 8;

	__declspec(align(16))
		static const __int64 maskY[2] = {0x00ff00ff00ff00ff,0x00ff00ff00ff00ff};
	__declspec(align(16))
		static const __int64 maskU[2] = {0x000000ff000000ff,0x000000ff000000ff};
#ifdef GRAYMASK
	__declspec(align(16))
		static const __int64 GRAY[2] =  {0x8080808080808080,0x8080808080808080};
#endif

	for(int i = 0;i< nHeight; i++)
	{
		if(i %2 == 0)
		{
			__asm
			{
				mov ecx,nCycle;
				mov	esi,pSrcTemp
				mov edi,pYTemp;
				mov eax,pUTemp;
				mov edx,pVTemp;

#ifdef GRAYMASK
				movdqa xmm6,GRAY;
#endif

LOOP1:	
				movdqu xmm0,[esi];
				movdqu xmm1,[esi+16];
				pxor	xmm7,xmm7;
				movdqu xmm2,xmm0;
				movdqu xmm3,xmm1;
				pand   xmm0,maskY;
				pand   xmm1,maskY;
				packuswb xmm0,xmm1; // 0000yyyy ...

				movdqu	[edi],xmm0

				psrlw   xmm2,8; // 0 v 0 u ....
				psrlw   xmm3,8; // 0 v 0  u ...
				movdqu  xmm0,xmm2;
				movdqu  xmm1,xmm3;
				pand     xmm2,maskU;
				pand     xmm3,maskU;
				packssdw xmm2,xmm3;
				packuswb xmm2,xmm7;// 00000...uuuu...	

#ifdef GRAYMASK
				movlpd   [eax],xmm6;
#else
				movlpd   [eax],xmm2;
#endif
				psrld	 xmm0,16;
				psrld	 xmm1,16;

				packssdw xmm0,xmm1;
				packuswb xmm0,xmm7; //0000...vvvv...

#ifdef GRAYMASK
				movlpd   [edx],xmm6;
#else
				movlpd   [edx],xmm0;
#endif
				add    esi,32;
				add    edi,16;
				add    eax,8;
				add    edx,8;

				dec    ecx;
				jnz    LOOP1

			}
			if(nRemain > 0)//不对齐处理
			{

				BYTE *pU = pUTemp + nOffset/2;
				BYTE *pV = pVTemp + nOffset/2;
				BYTE *pT = pSrcTemp + nOffset * 2;
				for(int i = 0 ; i < nRemain * 2; i+=4)
				{
					*pU = *(pT + 1);
					*pV = *(pT + 3);
					++pU;
					++pV;
					pT += 4;
				}
			}
			pUTemp += nUPitch;
			pVTemp += nVPitch;
		}
		else
		{
			__asm
			{
				mov ecx,nCycle;
				mov	esi,pSrcTemp
				mov edi,pYTemp;
LOOP2:	
				movdqu xmm0,[esi];
				movdqu xmm1,[esi+16];
				pand   xmm0,maskY;
				pand   xmm1,maskY;
				packuswb xmm0,xmm1; // 0000yyyy ...
				movdqu	[edi],xmm0
				add    esi,32;
				add    edi,16;

				dec    ecx;
				jnz    LOOP2	
			}
		}

		if(nRemain > 0)//不对齐处理
		{
			BYTE * pT = pSrcTemp + nOffset*2;
			BYTE  * pY = pYTemp + nOffset;
			for(int i = 0 ; i < nRemain * 2; i+=4)
			{
				*pY = *pT;
				++pY;
				*pY = *pT + 2;
				++pY;
				pT += 4;
			}
		}
		pSrcTemp += nSrcPitch;
		pYTemp += nYPitch;
	}
}


BOOL YV12ToRGB24C(BYTE* pYV12, BYTE* pRGB24,int nWidth,int nHeight)
{
	if(!pYV12 || !pRGB24)
		return FALSE;

	const LONG nYLen = LONG(nWidth * nHeight);
	const int nHfWidth = (nWidth>>1);

	if(nYLen<1 || nHfWidth<1)
		return FALSE;

	// Y data
	BYTE* yData = pYV12;
	// v data
	BYTE* vData = &yData[nYLen];
	// u data
	BYTE* uData = &vData[nYLen>>2];

	if(!uData || !vData)
		return FALSE;

	int rgb[3];
	int i, j, m, n, x, y,  py, rdif, invgdif, bdif;
	m = -nWidth;
	n = -nHfWidth;

	BOOL addhalf = TRUE;
	for(y=0; y<nHeight;y++) 
	{
		m += nWidth;
		if( addhalf )
		{
			n+=nHfWidth;
			addhalf = FALSE;
		} 
		else
		{
			addhalf = TRUE;
		}
		for(x=0; x<nWidth;x++)  
		{
			i = m + x;
			j = n + (x>>1);

			py = yData[i];

			// search tables to get rdif invgdif and bidif
			rdif = Table_fv1[vData[j]];    // fv1
			invgdif = Table_fu1[uData[j]] + Table_fv2[vData[j]]; // fu1+fv2
			bdif = Table_fu2[uData[j]]; // fu2

			rgb[2] = py+rdif;    // R
			rgb[1] = py-invgdif; // G
			rgb[0] = py+bdif;    // B

			j = nYLen - nWidth - m + x;
			i = (j<<1) + j;

			// copy this pixel to rgb data
			for(j=0; j<3; j++)
			{
				if(rgb[j]>=0 && rgb[j]<=255)
				{
					pRGB24[i + j] = rgb[j];
				}
				else
				{
					pRGB24[i + j] = (rgb[j] < 0)? 0 : 255;
				}
			}
		}
	}
	return TRUE;
}


void YV12ToRGB24MMX(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
					BYTE *puc_out, int width_y,  int height_y,int stride_out)
{
	int y, horiz_count;
	BYTE *puc_out_remembered;
	//int stride_out = width_y * 3;

	if (height_y > 0) 
	{
		//we are flipping our output upside-down
		height_y  = height_y;
		puc_y     += (height_y   - 1) * stride_y ;
		puc_u     += (height_y/2 - 1) * stride_uv;
		puc_v     += (height_y/2 - 1) * stride_uv;
		stride_y  = -stride_y;
		stride_uv = -stride_uv;
	}

	horiz_count = -(width_y >> 3);
	int w=  width_y + ( horiz_count<<3);
	int stride = (width_y >> 3 ) <<3;

	for (y=0; y<height_y; y++)
	{
		if (y == height_y-1) 
		{
			//this is the last output line - we need to be careful not to overrun the end of this line
			BYTE temp_buff[3*MAXIMUM_Y_WIDTH+1];
			puc_out_remembered = puc_out;
			puc_out = temp_buff; //write the RGB to a temporary store
		}
		if(w>0)
		{
			BYTE *pOut ;
			BYTE *pY;
			BYTE *pU;
			BYTE	*pV;
			//pOut = puc_out + y * width_y*3 +3* stride;
			//pY = puc_y + y * width_y + stride;
			pOut = puc_out  +3* stride;
			pY = puc_y  + stride;
			if(y%2 == 0)
			{
				pU = puc_u  + stride/2;
				pV = puc_v +  stride/2;	
			}
			//DecodeYV12ToRgb24Line(pOut  ,pY,pU,pV,w);
		}
		_asm 
		{
			push eax
			push ebx
			push ecx
			push edx
			push edi
			push esi

			mov eax, puc_out       
			mov ebx, puc_y       
			mov ecx, puc_u       
			mov edx, puc_v
			mov edi, horiz_count

horiz_loop:

			movd mm2, [ecx] //;pU
			pxor mm7, mm7

			movd mm3, [edx] //pV
			punpcklbw mm2, mm7       

			movq mm0, [ebx]   //pY      
			punpcklbw mm3, mm7       

			movq mm1, mmw_0x00ff     

			psubusb mm0, mmb_0x10    

			psubw mm2, mmw_0x0080    
			pand mm1, mm0            

			psubw mm3, mmw_0x0080    
			psllw mm1, 3             

			psrlw mm0, 8             
			psllw mm2, 3             

			pmulhw mm1, mmw_mult_Y   
			psllw mm0, 3             

			psllw mm3, 3             
			movq mm5, mm3            

			pmulhw mm5, mmw_mult_V_R 
			movq mm4, mm2            

			pmulhw mm0, mmw_mult_Y   
			movq mm7, mm1            

			pmulhw mm2, mmw_mult_U_G 
			paddsw mm7, mm5

			pmulhw mm3, mmw_mult_V_G
			packuswb mm7, mm7

			pmulhw mm4, mmw_mult_U_B
			paddsw mm5, mm0      

			packuswb mm5, mm5
			paddsw mm2, mm3          

			movq mm3, mm1            
			movq mm6, mm1            

			paddsw mm3, mm4
			paddsw mm6, mm2

			punpcklbw mm7, mm5
			paddsw mm2, mm0

			packuswb mm6, mm6
			packuswb mm2, mm2

			packuswb mm3, mm3
			paddsw mm4, mm0

			packuswb mm4, mm4
			punpcklbw mm6, mm2

			punpcklbw mm3, mm4

			// 32-bit shuffle.
			pxor mm0, mm0

			movq mm1, mm6
			punpcklbw mm1, mm0

			movq mm0, mm3
			punpcklbw mm0, mm7

			movq mm2, mm0

			punpcklbw mm0, mm1
			punpckhbw mm2, mm1

			// 24-bit shuffle and sav
			movd   [eax], mm0
			psrlq mm0, 32

			movd [eax + 3], mm0
			movd  [eax + 6], mm2

			psrlq mm2, 32            

			movd  [eax + 9], mm2        

			// 32-bit shuffle.
			pxor mm0, mm0            

			movq mm1, mm6            
			punpckhbw mm1, mm0       

			movq mm0, mm3            
			punpckhbw mm0, mm7       

			movq mm2, mm0            

			punpcklbw mm0, mm1       
			punpckhbw mm2, mm1       

			// 24-bit shuffle and sav
			movd [eax + 12], mm0        
			psrlq mm0, 32            

			movd [eax +15], mm0        
			add ebx, 8               

			movd [eax +18], mm2        
			psrlq mm2, 32            

			add ecx, 4               
			add edx, 4               

			movd [eax + 21], mm2        
			add eax, 24 

			inc edi
			jne horiz_loop

			pop esi		
			pop edi
			pop edx
			pop ecx
			pop ebx
			pop eax

			emms
		}

		if (y == height_y-1) 
		{
			//last line of output - we have used the temp_buff and need to copy
			int x = 3 * width_y;                  //interation counter
			BYTE *ps = puc_out;                // source pointer (temporary line store)
			BYTE *pd = puc_out_remembered;     // dest pointer
			while (x--) *(pd++) = *(ps++);          // copy the line
		}

		puc_y   += stride_y;
		if (y%2)
		{
			puc_u   += stride_uv;
			puc_v   += stride_uv;
		}
		puc_out += stride_out; 
	}

}

void YV12ToRGB24SSE2(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
					BYTE *puc_out, int width_y,  int height_y,int stride_out)
{
	int y, horiz_count;
	BYTE *puc_out_remembered;
	 __declspec(align(16))
	static const uint64_t mmw_0x00ff[2] = {0x00ff00ff00ff00ff,0x00ff00ff00ff00ff};
	  __declspec(align(16))
	static const uint64_t mmb_0x10[2]  = {0x1010101010101010,0x1010101010101010}; 
	   __declspec(align(16))
	static const uint64_t mmw_0x0080[2] = {0x0080008000800080,0x0080008000800080};
	    __declspec(align(16))
	static const uint64_t mmw_mult_Y[2]    ={ 0x2568256825682568,0x2568256825682568};
		 __declspec(align(16))
	static const uint64_t mmw_mult_V_R[2]  ={ 0x3343334333433343,0x3343334333433343};
	  __declspec(align(16))
	static const uint64_t mmw_mult_U_G[2]  = {0xf36ef36ef36ef36e,0xf36ef36ef36ef36e};
     __declspec(align(16))
	static const uint64_t mmw_mult_U_B[2]  = {0x40cf40cf40cf40cf,0x40cf40cf40cf40cf};
	  __declspec(align(16))
	static const uint64_t mmw_mult_V_G[2]  = {0xe5e2e5e2e5e2e5e2,0xe5e2e5e2e5e2e5e2};

	if (height_y > 0) 
	{
		height_y  = height_y;
		puc_y     += (height_y   - 1) * stride_y ;
		puc_u     += (height_y/2 - 1) * stride_uv;
		puc_v     += (height_y/2 - 1) * stride_uv;
		stride_y  = -stride_y;
		stride_uv = -stride_uv;
	}

	horiz_count = -(width_y >> 4);
	int w=  width_y + ( horiz_count<<4);
	int stride = (width_y >> 4 ) <<4;

	for (y=0; y<height_y; y++)
	{
		if (y == height_y-1) 
		{
			//this is the last output line - we need to be careful not to overrun the end of this line
			BYTE temp_buff[3*MAXIMUM_Y_WIDTH+1];
			puc_out_remembered = puc_out;
			puc_out = temp_buff; //write the RGB to a temporary store
		}
		if(w>0)
		{
			BYTE *pOut ;
			BYTE *pY;
			BYTE *pU;
			BYTE	*pV;
			//pOut = puc_out + y * width_y*3 +3* stride;
			//pY = puc_y + y * width_y + stride;
			pOut = puc_out  +3* stride;
			pY = puc_y  + stride;
			if(y%2 == 0)
			{
				pU = puc_u  + stride/2;
				pV = puc_v +  stride/2;	
			}
			
			DecodeYV12ToRgb24Line(pOut  ,pY,pU,pV,w);
		}
		
		_asm 
		{
			push eax
			//push ebx
			push ecx
			push edx
			push edi
			push esi

			mov eax, puc_out       
			mov esi, puc_y       
			mov ecx, puc_u       
			mov edx, puc_v
			mov edi, horiz_count

horiz_loop:

			movlpd xmm2, [ecx] //;pU
			pxor xmm7, xmm7

			movlpd xmm3, [edx] //pV
			punpcklbw xmm2, xmm7       

			movdqu xmm0, [esi]   //pY      
			punpcklbw xmm3, xmm7       

			movdqu xmm1, mmw_0x00ff     

			psubusb xmm0, mmb_0x10    

			psubw xmm2, mmw_0x0080    
			pand xmm1, xmm0            

			psubw xmm3, mmw_0x0080    
			psllw xmm1, 3             

			psrlw xmm0, 8             
			psllw xmm2, 3             

			pmulhw xmm1, mmw_mult_Y   
			psllw xmm0, 3             

			psllw xmm3, 3             
			movdqu xmm5, xmm3            

			pmulhw xmm5, mmw_mult_V_R 
			movdqu xmm4, xmm2            

			pmulhw xmm0, mmw_mult_Y   
			movdqu xmm7, xmm1            

			pmulhw xmm2, mmw_mult_U_G 
			paddsw xmm7, xmm5

			pmulhw xmm3, mmw_mult_V_G
			packuswb xmm7, xmm7

			pmulhw xmm4, mmw_mult_U_B
			paddsw xmm5, xmm0      

			packuswb xmm5, xmm5
			paddsw xmm2, xmm3          

			movdqu xmm3, xmm1            
			movdqu xmm6, xmm1            

			paddsw xmm3, xmm4
			paddsw xmm6, xmm2

			punpcklbw xmm7, xmm5
			paddsw xmm2, xmm0

			packuswb xmm6, xmm6
			packuswb xmm2, xmm2

			packuswb xmm3, xmm3
			paddsw xmm4, xmm0

			packuswb xmm4, xmm4
			punpcklbw xmm6, xmm2

			punpcklbw xmm3, xmm4

			// 32-bit shuffle.
			pxor xmm0, xmm0

			movdqu xmm1, xmm6
			punpcklbw xmm1, xmm0

			movdqu xmm0, xmm3
			punpcklbw xmm0, xmm7

			movdqu xmm2, xmm0

			punpcklbw xmm0, xmm1
			punpckhbw xmm2, xmm1

			// 24-bit shuffle and sav
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm0,0x4e
			movd   [eax], xmm0
			psrlq xmm0, 32
			movd [eax + 3], xmm0
			
			movd [eax + 6], xmm4
			psrlq xmm4, 32
			movd [eax + 9], xmm4

			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm2,0x4e
			movd  [eax + 12], xmm2
			psrlq xmm2, 32            
			movd  [eax + 15], xmm2  
			movd  [eax + 18], xmm4
			psrlq xmm4, 32            
			movd  [eax + 21], xmm4  

			// 32-bit shuffle.
			pxor xmm0, xmm0            

			movdqu xmm1, xmm6            
			punpckhbw xmm1, xmm0       

			movdqu xmm0, xmm3            
			punpckhbw xmm0, xmm7       

			movdqu xmm2, xmm0            

			punpcklbw xmm0, xmm1       
			punpckhbw xmm2, xmm1       

			// 24-bit shuffle and sav
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm0,0x4e
			movd [eax + 24], xmm0        
			psrlq xmm0, 32            

			movd [eax +27], xmm0    
			movd [eax + 30], xmm4        
			psrlq xmm4, 32            
			movd [eax +33], xmm4  
			add esi, 16               
            
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm2,0x4e
			movd [eax +36], xmm2        
			psrlq xmm2, 32            

			add ecx, 8               
			add edx, 8               

			movd [eax + 39], xmm2  
			movd [eax +42], xmm4        
			psrlq xmm4, 32  
			movd [eax +45], xmm4  
			add eax, 48 

			inc edi
			jne horiz_loop

			pop esi		
			pop edi
			pop edx
			pop ecx
			//pop ebx
			pop eax

			emms
		}

		if (y == height_y-1) 
		{
			//last line of output - we have used the temp_buff and need to copy
			int x = 3 * width_y;                  //interation counter
			BYTE *ps = puc_out;                // source pointer (temporary line store)
			BYTE *pd = puc_out_remembered;     // dest pointer
			while (x--) *(pd++) = *(ps++);          // copy the line
		}

		puc_y   += stride_y;
		if (y%2)
		{
			puc_u   += stride_uv;
			puc_v   += stride_uv;
		}
		puc_out += stride_out; 
	}

}

void MakeYUVToRGBTable()
{
	for (int i=0;i<256*3;++i)
		nColor_Table[i]=(unsigned char)BorderColor(i-256);
	Color_Table =  &nColor_Table[256];
}

long BorderColor(long color)
{
	if (color>255)
		return 255;
	else if (color<0)
		return 0;
	else
		return color;
}

void DecodeYV12ToRgb24Line(BYTE* pDstLine,const unsigned char *pY,
											    unsigned char *pU, unsigned char *pV,long width)
{
	static int j = 0;
	for (long x=0;x<width;x+=2)
	{	
		YV12ToRGB24Two(pDstLine,pY[0],pY[1],pU[0],pV[0]);
		pDstLine += 6;
		pY+=2;	
		j++;
		if((j)%2 == 1)
		{
			pU++;
			pV++;			
		}
	}
}
void DecodeYV12ToRgb32Line(BYTE* pDstLine,const unsigned char *pY,
											   const unsigned char *pU,const unsigned char *pV,long width)
{
	for (long x=0;x<width;x+=2)
	{
		YV12ToRGB32Two(pDstLine,pY[0],pY[1],pU[0],pV[0]);
		pDstLine += 8;
		pY+=2;
		pU++;
		pV++;
	}
}

void DecodeYUY2ToRgb32Line(BYTE* pDstLine,const unsigned char *pYUYV,long width)
{
	for (long x=0;x<width;x+=2)
	{
		YV12ToRGB32Two(pDstLine,pYUYV[0],pYUYV[2],pYUYV[1],pYUYV[3]);
		pDstLine += 8;
		pYUYV+=4;
	}
}

void DecodeYUY2ToRgb24Line(BYTE* pDstLine,const unsigned char *pYUYV,long width)
{
	for (long x=0;x<width;x+=2)
	{
		YV12ToRGB24Two(pDstLine,pYUYV[0],pYUYV[2],pYUYV[1],pYUYV[3]);
		pDstLine += 6;
		pYUYV+=4;
	}
}

void YV12ToRGB24Two(unsigned char *pDst,const unsigned char Y0,const unsigned char Y1,
										const unsigned char U,const unsigned char V)
{
	//R = Y + 1.4075 * (V-128)
	//G = Y - 0.3455 * (U-128) - 0.7169 * (V-128)
	//B = Y + 1.779 * (U-128)
	int Ye0=csY_coeff_16 * (Y0 - 16); 
	int Ye1=csY_coeff_16 * (Y1 - 16);
	int Ue=(U-128);
	int Ue_blue=csU_blue_16 *Ue;
	int Ue_green=csU_green_16 *Ue;
	int Ve=(V-128);
	int Ve_green=csV_green_16 *Ve;
	int Ve_red=csV_red_16 *Ve;
	int UeVe_green=Ue_green+Ve_green;
	pDst[0]=Color_Table[ ( Ye0 + Ue_blue )>>16 ] ;
	pDst[1] =Color_Table[ ( Ye0 + UeVe_green )>>16] ;
	pDst[2]= Color_Table[ ( Ye0 + Ve_red )>>16] ;

	pDst[3]=Color_Table[ ( Ye1 + Ue_blue )>>16 ] ;
	pDst[4] =Color_Table[ ( Ye1 + UeVe_green )>>16] ;
	pDst[5]= Color_Table[ ( Ye1 + Ve_red )>>16] ;

	//pDst[0]= Y0 + 1.779 * (U-128);
	//pDst[1] = Y0 - 0.3455 * (U-128) - 0.7169 * (V-128) ;
	//pDst[2]=  Y0 + 1.4075 * (V-128);
	//pDst[3]= Y1 + 1.779 * (U-128);;
	//pDst[4] = Y1 - 0.3455 * (U-128) - 0.7169 * (V-128) ;;
	//pDst[5]=  Y1 + 1.4075 * (V-128);;
}

void YV12ToRGB32Two(unsigned char *pDst,const unsigned char Y0,const unsigned char Y1,
										const unsigned char U,const unsigned char V)
{
	int Ye0=csY_coeff_16 * (Y0 - 16); 
	int Ye1=csY_coeff_16 * (Y1 - 16);
	int Ue=(U-128);
	int Ue_blue=csU_blue_16 *Ue;
	int Ue_green=csU_green_16 *Ue;
	int Ve=(V-128);
	int Ve_green=csV_green_16 *Ve;
	int Ve_red=csV_red_16 *Ve;
	int UeVe_green=Ue_green+Ve_green;
	pDst[0]=Color_Table[ ( Ye0 + Ue_blue )>>16 ] ;
	pDst[1] =Color_Table[ ( Ye0 + UeVe_green )>>16] ;
	pDst[2]= Color_Table[ ( Ye0 + Ve_red )>>16] ;
	pDst[3] =(unsigned char)( 0);
	pDst[4]=Color_Table[ ( Ye1 + Ue_blue )>>16 ] ;
	pDst[5] =Color_Table[ ( Ye1 + UeVe_green )>>16] ;
	pDst[6]= Color_Table[ ( Ye1 + Ve_red )>>16] ;
	pDst[7] =(unsigned char)( 0);

}

void YV12ToRGB32MMX(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
										BYTE *puc_out, int width_y,  int height_y,int stride_out) 
{
	int y, horiz_count;
	BYTE *puc_out_remembered;

	if (height_y > 0) 
	{
		//we are flipping our output upside-down
		height_y  = height_y;
		puc_y     += (height_y   - 1) * stride_y ;
		puc_u     += (height_y/2 - 1) * stride_uv;
		puc_v     += (height_y/2 - 1) * stride_uv;
		stride_y  = -stride_y;
		stride_uv = -stride_uv;
	}

	horiz_count = -(width_y >> 3);

	int w=  width_y + ( horiz_count<<3);
	int stride = (width_y >> 3 ) <<3;

	for (y=0; y<height_y; y++)
	{
		if (y == height_y-1) 
		{
			//this is the last output line - we need to be careful not to overrun the end of this line
			BYTE temp_buff[3*MAXIMUM_Y_WIDTH+1];
			puc_out_remembered = puc_out;
			puc_out = temp_buff; //write the RGB to a temporary store
		}
		if(w>0)
		{
			BYTE *pOut ;
			BYTE *pY;
			BYTE *pU;
			BYTE	*pV;
			//pOut = puc_out + y * width_y*3 +3* stride;
			//pY = puc_y + y * width_y + stride;
			pOut = puc_out  +4* stride;
			pY = puc_y  + stride;
			pU = puc_u  + stride/2;
			pV = puc_v +  stride/2;	
			DecodeYV12ToRgb32Line(pOut  ,pY,pU,pV,w);
		}
		_asm 
		{
			push eax
			push ebx
			push ecx
			push edx
			push edi

			mov eax, puc_out       
			mov ebx, puc_y       
			mov ecx, puc_u       
			mov edx, puc_v
			mov edi, horiz_count

horiz_loop:

			movd mm2, [ecx] //;pU
			pxor mm7, mm7

			movd mm3, [edx] //pV
			punpcklbw mm2, mm7       

			movq mm0, [ebx]      //pY      
			punpcklbw mm3, mm7       

			movq mm1, mmw_0x00ff     

			psubusb mm0, mmb_0x10    

			psubw mm2, mmw_0x0080    
			pand mm1, mm0            

			psubw mm3, mmw_0x0080    
			psllw mm1, 3             

			psrlw mm0, 8             
			psllw mm2, 3             

			pmulhw mm1, mmw_mult_Y   
			psllw mm0, 3             

			psllw mm3, 3             
			movq mm5, mm3            

			pmulhw mm5, mmw_mult_V_R 
			movq mm4, mm2            

			pmulhw mm0, mmw_mult_Y   
			movq mm7, mm1            

			pmulhw mm2, mmw_mult_U_G 
			paddsw mm7, mm5

			pmulhw mm3, mmw_mult_V_G
			packuswb mm7, mm7

			pmulhw mm4, mmw_mult_U_B
			paddsw mm5, mm0      

			packuswb mm5, mm5
			paddsw mm2, mm3          

			movq mm3, mm1            
			movq mm6, mm1            

			paddsw mm3, mm4
			paddsw mm6, mm2

			punpcklbw mm7, mm5
			paddsw mm2, mm0

			packuswb mm6, mm6
			packuswb mm2, mm2

			packuswb mm3, mm3
			paddsw mm4, mm0

			packuswb mm4, mm4
			punpcklbw mm6, mm2

			punpcklbw mm3, mm4

			// 32-bit shuffle.
			pxor mm0, mm0

			movq mm1, mm6
			punpcklbw mm1, mm0

			movq mm0, mm3
			punpcklbw mm0, mm7

			movq mm2, mm0

			punpcklbw mm0, mm1
			punpckhbw mm2, mm1

			// 24-bit shuffle and sav
			movd   [eax], mm0
			psrlq mm0, 32

			movd  [eax + 4], mm0
			movd  [eax + 8], mm2

			psrlq mm2, 32            

			movd  [eax + 12], mm2        

			// 32-bit shuffle.
			pxor mm0, mm0            

			movq mm1, mm6            
			punpckhbw mm1, mm0       

			movq mm0, mm3            
			punpckhbw mm0, mm7       

			movq mm2, mm0            

			punpcklbw mm0, mm1       
			punpckhbw mm2, mm1       

			// 24-bit shuffle and sav
			movd [eax + 16], mm0        
			psrlq mm0, 32            

			movd [eax +20], mm0        
			add ebx, 8               

			movd [eax +24], mm2        
			psrlq mm2, 32            

			add ecx, 4               
			add edx, 4               

			movd [eax + 28], mm2        
			add eax, 32             

			inc edi
			jne horiz_loop

			pop edi
			pop edx
			pop ecx
			pop ebx
			pop eax

			emms
		}

		if (y == height_y-1) 
		{
			//last line of output - we have used the temp_buff and need to copy
			int x = 3 * width_y;                  //interation counter
			BYTE *ps = puc_out;                // source pointer (temporary line store)
			BYTE *pd = puc_out_remembered;     // dest pointer
			while (x--) *(pd++) = *(ps++);          // copy the line
		}

		puc_y   += stride_y;
		if (y%2)
		{
			puc_u   += stride_uv;
			puc_v   += stride_uv;
		}
		puc_out += stride_out; 
	}

}

void YV12ToRGB32SSE2(BYTE *puc_y, int stride_y, BYTE *puc_u,  BYTE *puc_v, int stride_uv, 
					BYTE *puc_out, int width_y,  int height_y,int stride_out) 
{
	int y, horiz_count;
	BYTE *puc_out_remembered;
    __declspec(align(16))
	static const uint64_t xmmw_0x00ff[2] = {0x00ff00ff00ff00ff,0x00ff00ff00ff00ff};
	__declspec(align(16))
	static const uint64_t xmmb_0x10[2]  = {0x1010101010101010,0x1010101010101010}; 
	__declspec(align(16))
	static const uint64_t xmmw_0x0080[2] = {0x0080008000800080,0x0080008000800080};
	__declspec(align(16))
	static const uint64_t xmmw_mult_Y[2]    ={ 0x2568256825682568,0x2568256825682568};
	__declspec(align(16))
	static const uint64_t xmmw_mult_V_R[2]  ={ 0x3343334333433343,0x3343334333433343};
	__declspec(align(16))
	static const uint64_t xmmw_mult_U_G[2]  = {0xf36ef36ef36ef36e,0xf36ef36ef36ef36e};
	__declspec(align(16))
	static const uint64_t xmmw_mult_U_B[2]  = {0x40cf40cf40cf40cf,0x40cf40cf40cf40cf};
	__declspec(align(16))
	static const uint64_t xmmw_mult_V_G[2]  = {0xe5e2e5e2e5e2e5e2,0xe5e2e5e2e5e2e5e2};

	//if (height_y > 0) 
	//{
	//	//we are flipping our output upside-down
	//	height_y  = height_y;
	//	puc_y     += (height_y   - 1) * stride_y ;
	//	puc_u     += (height_y/2 - 1) * stride_uv;
	//	puc_v     += (height_y/2 - 1) * stride_uv;
	//	stride_y  = -stride_y;
	//	stride_uv = -stride_uv;
	//}

	horiz_count = -(width_y >> 4);

	int w=  width_y + ( horiz_count<<4);
	int stride = (width_y >> 4 ) <<4;

	for (y=0; y<height_y; y++)
	{
		if (y == height_y-1) 
		{
			//this is the last output line - we need to be careful not to overrun the end of this line
			BYTE temp_buff[4*MAXIMUM_Y_WIDTH+1];
			puc_out_remembered = puc_out;
			puc_out = temp_buff; //write the RGB to a temporary store
		}
		if(w>0)
		{
			BYTE *pOut ;
			BYTE *pY;
			BYTE *pU;
			BYTE	*pV;

			pOut = puc_out  +4* stride;
			pY = puc_y  + stride;
			pU = puc_u  + stride/2;
			pV = puc_v +  stride/2;	
			DecodeYV12ToRgb32Line(pOut  ,pY,pU,pV,w);
		}
		_asm 
		{
			push eax
			push ecx
			push edx
			push edi
			push esi

			mov eax, puc_out       
			mov esi, puc_y       
			mov ecx, puc_u       
			mov edx, puc_v
			mov edi, horiz_count

horiz_loop:

			movlpd xmm2, [ecx] //;pU
			pxor xmm7, xmm7

			movlpd xmm3, [edx] //pV
			punpcklbw xmm2, xmm7       

			movdqu xmm0, [esi]      //pY      
			punpcklbw xmm3, xmm7       

			movdqu xmm1, xmmw_0x00ff     

			psubusb xmm0, xmmb_0x10    

			psubw xmm2, xmmw_0x0080    
			pand xmm1, xmm0            

			psubw xmm3, xmmw_0x0080    
			psllw xmm1, 3             

			psrlw xmm0, 8             
			psllw xmm2, 3             

			pmulhw xmm1, xmmw_mult_Y   
			psllw xmm0, 3             

			psllw xmm3, 3             
			movdqu xmm5, xmm3            

			pmulhw xmm5, xmmw_mult_V_R 
			movdqu xmm4, xmm2            

			pmulhw xmm0, xmmw_mult_Y   
			movdqu xmm7, xmm1            

			pmulhw xmm2, xmmw_mult_U_G 
			paddsw xmm7, xmm5

			pmulhw xmm3, xmmw_mult_V_G
			packuswb xmm7, xmm7

			pmulhw xmm4, xmmw_mult_U_B
			paddsw xmm5, xmm0      

			packuswb xmm5, xmm5
			paddsw xmm2, xmm3          

			movdqu xmm3, xmm1            
			movdqu xmm6, xmm1            

			paddsw xmm3, xmm4
			paddsw xmm6, xmm2

			punpcklbw xmm7, xmm5
			paddsw xmm2, xmm0

			packuswb xmm6, xmm6
			packuswb xmm2, xmm2

			packuswb xmm3, xmm3
			paddsw xmm4, xmm0

			packuswb xmm4, xmm4
			punpcklbw xmm6, xmm2

			punpcklbw xmm3, xmm4

			// 32-bit shuffle.
			pxor xmm0, xmm0

			movdqu xmm1, xmm6
			punpcklbw xmm1, xmm0

			movdqu xmm0, xmm3
			punpcklbw xmm0, xmm7

			movdqu xmm2, xmm0

			punpcklbw xmm0, xmm1
			punpckhbw xmm2, xmm1

			// 24-bit shuffle and sav
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm0,0x4e
			movd   [eax], xmm0
			PSRLQ  xmm0, 32
			movd  [eax + 4], xmm0
			
			movd  [eax + 8], xmm4
			PSRLQ  xmm4, 32
			movd  [eax + 12], xmm4
			
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm2,0x4e
			movd  [eax + 16], xmm2
			PSRLQ  xmm2, 32
			movd  [eax + 20], xmm2
		          
			movd  [eax + 24], xmm4  
			PSRLQ  xmm4, 32            
			movd  [eax + 28], xmm4 

			// 32-bit shuffle.
			pxor xmm0, xmm0            

			movdqu xmm1, xmm6            
			punpckhbw xmm1, xmm0       

			movdqu xmm0, xmm3            
			punpckhbw xmm0, xmm7       

			movdqu xmm2, xmm0            

			punpcklbw xmm0, xmm1       
			punpckhbw xmm2, xmm1       

			// 24-bit shuffle and sav
			pxor   xmm4, xmm4
			PSHUFD  xmm4,xmm0,0x4e
			movd [eax + 32], xmm0        
			PSRLQ  xmm0, 32            

			movd [eax +36], xmm0               

			movd [eax +40], xmm4  
			PSRLQ  xmm4, 32            

			movd [eax +44], xmm4 
		
			add esi, 16 
            pxor   xmm4, xmm4
            PSHUFD  xmm4,xmm2,0x4e
			movd [eax +48], xmm2        
			PSRLQ  xmm2, 32 
			movd [eax +52], xmm2

			movd [eax +56], xmm4        
			PSRLQ  xmm4, 32  
			movd [eax +60], xmm4        
			

			add ecx, 8              
			add edx, 8               
			
			add eax, 64             

			inc edi
			jne horiz_loop

            pop esi
			pop edi
			pop edx
			pop ecx
			pop eax

			emms
		}

		if (y == height_y-1) 
		{
			//last line of output - we have used the temp_buff and need to copy
			int x = 3 * width_y;                  //interation counter
			BYTE *ps = puc_out;                // source pointer (temporary line store)
			BYTE *pd = puc_out_remembered;     // dest pointer
			while (x--) *(pd++) = *(ps++);          // copy the line
		}

		puc_y   += stride_y;
		if (y%2)
		{
			puc_u   += stride_uv;
			puc_v   += stride_uv;
		}
		puc_out += stride_out; 
	}

}

void YV12ToYUY2MMX(const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, int src_rowsize, int src_pitch, int src_pitch_uv, 
				   BYTE* dst, int dst_pitch,int height) 
{
	static __int64 add_64=0x0002000200020002;
	const BYTE** srcp= new const BYTE*[3];
	int src_pitch_uv2 = src_pitch_uv*2;
	int skipnext = 0;

	int dst_pitch2=dst_pitch*2;
	int src_pitch2 = src_pitch*2;

	int  nRemain = src_rowsize / 8 * 8;			//不对齐的数量
	int  nOffset = src_rowsize - nRemain;

	const BYTE* _srcY=srcY;
	const BYTE* _srcU=srcU;
	const BYTE* _srcV=srcV;
	BYTE* _dst=dst;

	for (int i=0;i<4;i++) 
	{
		switch (i) 
		{
		case 1:
			_srcY+=src_pitch; // Same chroma as in 0
			_dst+=dst_pitch;
			break;
		case 2:
			_srcY=srcY+(src_pitch*(height-2));
			_srcU=srcU+(src_pitch_uv*((height>>1)-1));
			_srcV=srcV+(src_pitch_uv*((height>>1)-1));
			_dst = dst+(dst_pitch*(height-2));
			break;
		case 3: // Same chroma as in 4
			_srcY += src_pitch;
			_dst += dst_pitch;
			break;
		default: // Nothing, case 0
			break;
		}

		__asm 
		{
			mov edi, [_dst]
			mov eax, [_srcY]
			mov ebx, [_srcU]
			mov ecx, [_srcV]
			mov edx,0
			pxor mm7,mm7
			jmp xloop_test_p
xloop_p:
			movq mm0,[eax] //Y
			movd mm1,[ebx] //U
			movq mm3,mm0 
			movd mm2,[ecx] //V
			punpcklbw mm0,mm7 // Y low
			punpckhbw mm3,mm7 // Y high
			punpcklbw mm1,mm7 // 00uu 00uu
			punpcklbw mm2,mm7 // 00vv 00vv
			movq mm4,mm1
			movq mm5,mm2
			punpcklbw mm1,mm7 // 0000 00uu low
			punpcklbw mm2,mm7 // 0000 00vv low
			punpckhbw mm4,mm7 // 0000 00uu high
			punpckhbw mm5,mm7 // 0000 00vv high
			pslld mm1,8
			pslld mm4,8
			pslld mm2,24
			pslld mm5,24
			por mm0, mm1
			por mm3, mm4
			por mm0, mm2
			por mm3, mm5
			movq [edi],mm0
			movq [edi+8],mm3
			add eax,8
			add ebx,4
			add ecx,4
			add edx,8
			add edi, 16
xloop_test_p:
			cmp edx,[src_rowsize]
			jl xloop_p
		}
		if(nOffset > 0)//不对齐处理
		{

			const BYTE *pY = _srcY + nRemain;
			const BYTE *pU = _srcU + nRemain/2;
			const BYTE *pV = _srcV + nRemain / 2;
			BYTE* pDst = _dst + nRemain * 2;
			for(int i = 0 ; i < nOffset; i+=2)
			{
				*(pDst++) = *(pY++);
				*(pDst++) = *pU;
				*(pDst++) =  *(pY++);
				*(pDst++) =  *pV;
				++pU;
				++pV;
			}
		}
	}

	height-=4;

	dst+=dst_pitch2;
	srcY+=src_pitch2;
	srcU+=src_pitch_uv;
	srcV+=src_pitch_uv;

	srcp[0] = srcY;
	srcp[1] = srcU-src_pitch_uv;
	srcp[2] = srcV-src_pitch_uv;

	int y=0;
	int x=0;

	__asm
	{
		mov esi, [srcp]
		mov edi, [dst]

		mov eax,[esi]
		mov ebx,[esi+4]
		mov ecx,[esi+8]
		mov edx,0
		jmp yloop_test
		align 16
yloop:
		mov edx,0 // x counter
		jmp xloop_test
		align 16
xloop:
		mov edx, src_pitch_uv
		movq mm0,[eax] // mm0 = Y current line
		pxor mm7,mm7
		movd mm2,[ebx+edx] // mm2 = U top field
		movd mm3, [ecx+edx] // mm3 = V top field
		movd mm4,[ebx] // U prev top field
		movq mm1,mm0 // mm1 = Y current line
		movd mm5,[ecx] // V prev top field

		punpcklbw mm2,mm7 // U 00uu 00uu 00uu 00uu
		punpcklbw mm3,mm7 // V 00vv 00vv 00vv 00vv
		punpcklbw mm4,mm7 // U 00uu 00uu 00uu 00uu
		punpcklbw mm5,mm7 // V 00vv 00vv 00vv 00vv
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4, [add_64]
		paddusw mm5, [add_64]
		psrlw mm4,2
		psrlw mm5,2


		punpcklbw mm0,mm7 // Y low
		punpckhbw mm1,mm7 // Y high*
		pxor mm6,mm6
		punpcklbw mm6,mm4 // U 0000 uu00 0000 uu00 (low)
		punpckhbw mm7,mm4 // V 0000 uu00 0000 uu00 (high
		por mm0,mm6
		por mm1,mm7
		movq mm6,mm5
		punpcklbw mm5,mm5 // V 0000 vvvv 0000 vvvv (low)
		punpckhbw mm6,mm6 // V 0000 vvvv 0000 vvvv (high)
		pslld mm5,24
		pslld mm6,24
		por mm0,mm5
		por mm1,mm6
		mov edx, src_pitch_uv2
		movq [edi],mm0
		movq [edi+8],mm1

		//Next line 

		movd mm4,[ebx+edx] // U next top field
		movd mm5,[ecx+edx] // V prev top field
		mov edx, [src_pitch]
		pxor mm7,mm7
		movq mm0,[eax+edx] // Next U-line
		movq mm1,mm0 // mm1 = Y current line

		punpcklbw mm4,mm7 // U 00uu 00uu 00uu 00uu
		punpcklbw mm5,mm7 // V 00vv 00vv 00vv 00vv
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4,mm2
		paddusw mm5,mm3
		paddusw mm4, [add_64]
		paddusw mm5, [add_64]
		psrlw mm4,2
		psrlw mm5,2

		punpcklbw mm0,mm7 // Y low
		punpckhbw mm1,mm7 // Y high*
		pxor mm6,mm6
		punpcklbw mm6,mm4 // U 0000 uu00 0000 uu00 (low)
		punpckhbw mm7,mm4 // V 0000 uu00 0000 uu00 (high
		por mm0,mm6
		por mm1,mm7
		movq mm6,mm5
		punpcklbw mm5,mm5 // V 0000 vvvv 0000 vvvv (low)
		punpckhbw mm6,mm6 // V 0000 vvvv 0000 vvvv (high)
		pslld mm5,24
		mov edx,[dst_pitch]
		pslld mm6,24
		por mm0,mm5
		por mm1,mm6
		movq [edi+edx],mm0
		movq [edi+edx+8],mm1
		add edi,16
		mov edx, [x]
		add eax, 8
		add ebx, 4
		add edx, 8
		add ecx, 4
xloop_test:
		cmp edx,[src_rowsize]
		mov x,edx
		jl xloop
		mov edi, dst
		mov eax,[esi]
		mov ebx,[esi+4]
		mov ecx,[esi+8]

		add edi,[dst_pitch2]
		add eax,[src_pitch2]
		add ebx,[src_pitch_uv]
		add ecx,[src_pitch_uv]
		mov edx, [y]
		mov [esi],eax
		mov [esi+4],ebx
		mov [esi+8],ecx
		mov [dst],edi
		add edx, 2

yloop_test:
		cmp edx,[height]
		mov [y],edx
		jl yloop
		emms
	}
	delete[] srcp;
	if(nOffset > 0)//不对齐处理
	{
		const BYTE *pU = _srcU + nRemain/2;
		const BYTE *pV = _srcV + nRemain / 2;
		for(int j = 0;j<2;j++)
		{
			const BYTE *pY = _srcY + src_rowsize * j+ nRemain;		
			BYTE* pDst = _dst + + src_rowsize*2 * j + nRemain * 2;
			for(int i = 0 ; i < nOffset; i+=2)
			{
				*(pDst++) = *(pY++);
				*(pDst++) = *pU;
				*(pDst++) =  *(pY++);
				*(pDst++) =  *pV;
				++pU;
				++pV;
			}
		}
	}
}

void YV12ToYUY2SSE2(const BYTE* srcY, const BYTE* srcU, const BYTE* srcV, int src_rowsize, int src_pitch, int src_pitch_uv, 
				   BYTE* dst, int dst_pitch,int height) 
{
	static const __int64 maskY[2] = {0x0002000200020002,0x0002000200020002};
	const BYTE** srcp= new const BYTE*[3];
	int src_pitch_uv2 = src_pitch_uv*2;
	int skipnext = 0;

	int dst_pitch2=dst_pitch*2;
	int src_pitch2 = src_pitch*2;

	int  nRemain = src_rowsize / 16 * 16;			//不对齐的数量
	int  nOffset = src_rowsize - nRemain;


	const BYTE* _srcY=srcY;
	const BYTE* _srcU=srcU;
	const BYTE* _srcV=srcV;
	BYTE* _dst=dst;

	for (int i=0;i<4;i++) 
	{
		switch (i) 
		{
		case 1:
			_srcY+=src_pitch; 
			_dst+=dst_pitch;
			break;
		case 2:
			_srcY=srcY+(src_pitch*(height-2));
			_srcU=srcU+(src_pitch_uv*((height>>1)-1));
			_srcV=srcV+(src_pitch_uv*((height>>1)-1));
			_dst = dst+(dst_pitch*(height-2));
			break;
		case 3: 
			_srcY += src_pitch;
			_dst += dst_pitch;
			break;
		default: 
			break;
		}

		__asm 
		{
			mov edi, [_dst]
			mov eax, [_srcY]
			mov ebx, [_srcU]
			mov ecx, [_srcV]
			mov edx,0
			pxor xmm7,xmm7
			jmp xloop_test_p
xloop_p:
			movdqu xmm0,[eax] //Y
			movlpd xmm1,[ebx] //U
			movdqu xmm3,xmm0 
			movlpd xmm2,[ecx] //V
			punpcklbw xmm0,xmm7 // Y low
			punpckhbw xmm3,xmm7 // Y high
			punpcklbw xmm1,xmm7 // 00uu 00uu
			punpcklbw xmm2,xmm7 // 00vv 00vv
			movdqu xmm4,xmm1
			movdqu xmm5,xmm2
			punpcklbw xmm1,xmm7 // 0000 00uu low
			punpcklbw xmm2,xmm7 // 0000 00vv low
			punpckhbw xmm4,xmm7 // 0000 00uu high
			punpckhbw xmm5,xmm7 // 0000 00vv high
			pslld xmm1,8
			pslld xmm4,8
			pslld xmm2,24
			pslld xmm5,24
			por xmm0, xmm1
			por xmm3, xmm4
			por xmm0, xmm2
			por xmm3, xmm5
			movdqu [edi],xmm0
			movdqu [edi+16],xmm3
			add eax,16
			add ebx,8
			add ecx,8
			add edx,16
			add edi, 32
xloop_test_p:
			cmp edx,[src_rowsize]
			jl xloop_p
		}
		if(nOffset > 0)//不对齐处理
		{
			const BYTE *pY = _srcY + nRemain;
			const BYTE *pU = _srcU + nRemain/2;
			const BYTE *pV = _srcV + nRemain / 2;
			BYTE* pDst = _dst + nRemain * 2;
			for(int i = 0 ; i < nOffset; i+=2)
			{
				*(pDst++) = *(pY++);
				*(pDst++) = *pU;
				*(pDst++) =  *(pY++);
				*(pDst++) =  *pV;
				++pU;
				++pV;
			}
		}
	}

	height-=4;

	dst+=dst_pitch2;
	srcY+=src_pitch2;
	srcU+=src_pitch_uv;
	srcV+=src_pitch_uv;

	srcp[0] = srcY;
	srcp[1] = srcU-src_pitch_uv;
	srcp[2] = srcV-src_pitch_uv;

	int y=0;
	int x=0;

	__asm
	{
		mov esi, [srcp]
		mov edi, [dst]

		mov eax,[esi]
		mov ebx,[esi+4]
		mov ecx,[esi+8]
		mov edx,0
		jmp yloop_test
		align 16
yloop:
		mov edx,0 // x counter
		jmp xloop_test
		align 16
xloop:
		mov edx, src_pitch_uv
		movdqu xmm0,[eax] // mm0 = Y current line
		pxor xmm7,xmm7
		movlpd xmm2,[ebx+edx] // mm2 = U top field
		movlpd xmm3, [ecx+edx] // mm3 = V top field
		movlpd xmm4,[ebx] // U prev top field
		movdqu xmm1,xmm0 // mm1 = Y current line
		movlpd xmm5,[ecx] // V prev top field

		punpcklbw xmm2,xmm7 // U 00uu 00uu 00uu 00uu
		punpcklbw xmm3,xmm7 // V 00vv 00vv 00vv 00vv
		punpcklbw xmm4,xmm7 // U 00uu 00uu 00uu 00uu
		punpcklbw xmm5,xmm7 // V 00vv 00vv 00vv 00vv
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4, [maskY]
		paddusw xmm5, [maskY]
		psrlw xmm4,2
		psrlw xmm5,2


		punpcklbw xmm0,xmm7 // Y low
		punpckhbw xmm1,xmm7 // Y high*
		pxor xmm6,xmm6
		punpcklbw xmm6,xmm4 // U 0000 uu00 0000 uu00 (low)
		punpckhbw xmm7,xmm4 // V 0000 uu00 0000 uu00 (high
		por xmm0,xmm6
		por xmm1,xmm7
		movdqu xmm6,xmm5
		punpcklbw xmm5,xmm5 // V 0000 vvvv 0000 vvvv (low)
		punpckhbw xmm6,xmm6 // V 0000 vvvv 0000 vvvv (high)
		pslld xmm5,24
		pslld xmm6,24
		por xmm0,xmm5
		por xmm1,xmm6
		mov edx, src_pitch_uv2
		movdqu [edi],xmm0
		movdqu [edi+16],xmm1

		//Next line 

		movlpd xmm4,[ebx+edx] // U next top field
		movlpd xmm5,[ecx+edx] // V prev top field
		mov edx, [src_pitch]
		pxor xmm7,xmm7
		movdqu xmm0,[eax+edx] // Next U-line
		movdqu xmm1,xmm0 // mm1 = Y current line

		punpcklbw xmm4,xmm7 // U 00uu 00uu 00uu 00uu
		punpcklbw xmm5,xmm7 // V 00vv 00vv 00vv 00vv
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4,xmm2
		paddusw xmm5,xmm3
		paddusw xmm4, [maskY]
		paddusw xmm5, [maskY]
		psrlw xmm4,2
		psrlw xmm5,2

		punpcklbw xmm0,xmm7 // Y low
		punpckhbw xmm1,xmm7 // Y high*
		pxor xmm6,xmm6
		punpcklbw xmm6,xmm4 // U 0000 uu00 0000 uu00 (low)
		punpckhbw xmm7,xmm4 // V 0000 uu00 0000 uu00 (high
		por xmm0,xmm6
		por xmm1,xmm7
		movdqu xmm6,xmm5
		punpcklbw xmm5,xmm5 // V 0000 vvvv 0000 vvvv (low)
		punpckhbw xmm6,xmm6 // V 0000 vvvv 0000 vvvv (high)
		pslld xmm5,24
		mov edx,[dst_pitch]
		pslld xmm6,24
		por xmm0,xmm5
		por xmm1,xmm6
		movdqu [edi+edx],xmm0
		movdqu [edi+edx+16],xmm1
		add edi,32
		mov edx, [x]
		add eax, 16
		add ebx, 8
		add edx, 16
		add ecx, 8
xloop_test:
		cmp edx,[src_rowsize]
		mov x,edx
		jl xloop
		mov edi, dst
		mov eax,[esi]
		mov ebx,[esi+4]
		mov ecx,[esi+8]

		add edi,[dst_pitch2]
		add eax,[src_pitch2]
		add ebx,[src_pitch_uv]
		add ecx,[src_pitch_uv]
		mov edx, [y]
		mov [esi],eax
		mov [esi+4],ebx
		mov [esi+8],ecx
		mov [dst],edi
		add edx, 2

yloop_test:
		cmp edx,[height]
		mov [y],edx
		jl yloop
		emms
	}
	delete[] srcp;
	if(nOffset > 0)//不对齐处理
	{
		const BYTE *pU = _srcU + nRemain/2;
		const BYTE *pV = _srcV + nRemain / 2;
		for(int j = 0;j<2;j++)
		{
			const BYTE *pY = _srcY + src_rowsize * j+ nRemain;		
			BYTE* pDst = _dst + + src_rowsize*2 * j + nRemain * 2;
			for(int i = 0 ; i < nOffset; i+=2)
			{
				*(pDst++) = *(pY++);
				*(pDst++) = *pU;
				*(pDst++) =  *(pY++);
				*(pDst++) =  *pV;
				++pU;
				++pV;
			}
		}
	}
}

void YUV_TO_RGB24(    BYTE *puc_y,        int stride_y, 
									  BYTE *puc_u,        BYTE *puc_v, int stride_uv, 
									  BYTE *puc_out,    int width_y,    int height_y,int stride_out)
{
	int y, horiz_count;
	BYTE *puc_out_remembered;
	//stride_out = width_y * 3;

	if (height_y < 0) {
		//we are flipping our output upside-down
		height_y  = -height_y;
		puc_y     += (height_y   - 1) * stride_y ;
		puc_u     += (height_y/2 - 1) * stride_uv;
		puc_v     += (height_y/2 - 1) * stride_uv;
		stride_y  = -stride_y;
		stride_uv = -stride_uv;
	}

	horiz_count = -(width_y >> 3);

	for (y=0; y<height_y; y++) {
		if (y == height_y-1) {
			//this is the last output line - we need to be careful not to overrun the end of this line
			BYTE temp_buff[3*MAXIMUM_Y_WIDTH+1];
			puc_out_remembered = puc_out;
			puc_out = temp_buff; //write the RGB to a temporary store
		}
		_asm {
			push eax
			push ebx
			push ecx
			push edx
			push edi

			mov eax, puc_out       
			mov ebx, puc_y       
			mov ecx, puc_u       
			mov edx, puc_v
			mov edi, horiz_count

horiz_loop:

			movd mm2, [ecx]
			pxor mm7, mm7

			movd mm3, [edx]
			punpcklbw mm2, mm7       

			movq mm0, [ebx]          
			punpcklbw mm3, mm7       

			movq mm1, mmw_0x00ff     

			psubusb mm0, mmb_0x10    

			psubw mm2, mmw_0x0080    
			pand mm1, mm0            

			psubw mm3, mmw_0x0080    
			psllw mm1, 3             

			psrlw mm0, 8             
			psllw mm2, 3             

			pmulhw mm1, mmw_mult_Y   
			psllw mm0, 3             

			psllw mm3, 3             
			movq mm5, mm3            

			pmulhw mm5, mmw_mult_V_R 
			movq mm4, mm2            

			pmulhw mm0, mmw_mult_Y   
			movq mm7, mm1            

			pmulhw mm2, mmw_mult_U_G 
			paddsw mm7, mm5

			pmulhw mm3, mmw_mult_V_G
			packuswb mm7, mm7

			pmulhw mm4, mmw_mult_U_B
			paddsw mm5, mm0      

			packuswb mm5, mm5
			paddsw mm2, mm3          

			movq mm3, mm1            
			movq mm6, mm1            

			paddsw mm3, mm4
			paddsw mm6, mm2

			punpcklbw mm7, mm5
			paddsw mm2, mm0

			packuswb mm6, mm6
			packuswb mm2, mm2

			packuswb mm3, mm3
			paddsw mm4, mm0

			packuswb mm4, mm4
			punpcklbw mm6, mm2

			punpcklbw mm3, mm4

			// 32-bit shuffle.
			pxor mm0, mm0

			movq mm1, mm6
			punpcklbw mm1, mm0

			movq mm0, mm3
			punpcklbw mm0, mm7

			movq mm2, mm0

			punpcklbw mm0, mm1
			punpckhbw mm2, mm1

			// 24-bit shuffle and sav
			movd   [eax], mm0
			psrlq mm0, 32

			movd  3[eax], mm0

			movd  6[eax], mm2


			psrlq mm2, 32            

			movd  9[eax], mm2        

			// 32-bit shuffle.
			pxor mm0, mm0            

			movq mm1, mm6            
			punpckhbw mm1, mm0       

			movq mm0, mm3            
			punpckhbw mm0, mm7       

			movq mm2, mm0            

			punpcklbw mm0, mm1       
			punpckhbw mm2, mm1       

			// 24-bit shuffle and sav
			movd 12[eax], mm0        
			psrlq mm0, 32            

			movd 15[eax], mm0        
			add ebx, 8               

			movd 18[eax], mm2        
			psrlq mm2, 32            

			add ecx, 4               
			add edx, 4               

			movd 21[eax], mm2        
			add eax, 24              

			inc edi
			jne horiz_loop

			pop edi
			pop edx
			pop ecx
			pop ebx
			pop eax

			emms
		}


		if (y == height_y-1) {
			//last line of output - we have used the temp_buff and need to copy
			int x = 3 * width_y;                  //interation counter
			BYTE *ps = puc_out;                // source pointer (temporary line store)
			BYTE *pd = puc_out_remembered;     // dest pointer
			while (x--) *(pd++) = *(ps++);          // copy the line
		}

		puc_y   += stride_y;
		if (y%2) {
			puc_u   += stride_uv;
			puc_v   += stride_uv;
		}
		puc_out += stride_out; 
	}
}

void RGB24ToYUY2MMX(BYTE *dest, BYTE *src, int nWidth, int nHeight,int SrcPitch,int DstPitch)
{
	int w = nWidth * nHeight;
	BYTE *pTemp = new BYTE[nWidth * 3];
	for(int i = 0;i<nHeight/2;i++)
	{ 
		CopyMemory(pTemp,src+i * nWidth*3,nWidth * 3);
		CopyMemory(src+i * nWidth*3,src+(nHeight-i-1)* nWidth*3,nWidth * 3);
		CopyMemory(src+(nHeight-i-1)* nWidth*3,pTemp,nWidth * 3);
	}
	delete []pTemp;
	_asm
	{
		mov esi, src
		mov edi, dest
		mov eax, w
		shr eax, 2

		pxor mm0, mm0              ; mm0 = 0, constant
		movq mm7, const128_0_128_0 ; constant

next4:
		// Process 4 pixels.  First YUYV is a, second is b.
		// Source must be 12-byte aligned.
		movd mm1, [esi+0]      ; mm1 = X X X X B1a R0a G0a B0a
		movd mm2, [esi+4]      ; mm2 = X X X X G0b B0b R1a G1a
		movd mm3, [esi+8]      ; mm3 = X X X X R1b G1b B1b R0b
		add esi, 12

		punpcklbw mm1, mm0     ; mm1 = B1a R0a G0a B0a
		punpcklbw mm2, mm0     ; mm2 = G0b B0b R1a G1a
		punpcklbw mm3, mm0     ; mm3 = R1b G1b B1b R0b


		pshufw mm4, mm1, 0x44  ; mm4 = G0a B0a G0a B0a
		pmaddwd mm4, UgUbYgYb  ; mm4 =   Ugb     Ygb

		pshufw mm5, mm1, 0x88  ; mm5 = R0a XXX R0a XXX
		pmaddwd mm5, Ur0Yr0    ; mm5 =   Ur      Yr

		paddd mm4, mm5         ; mm4 =   Ua      Y0a
		psrad mm4, 15          ; mm4 >>= 15


		pshufw mm5, mm1, 0x0f  ; mm5 = XXX XXX B1a B1a
		pshufw mm6, mm2, 0x00  ; mm6 = XXX XXX G1a G1a

		punpcklwd mm5, mm6     ; mm5 = G1a B1a G1a B1a
		pmaddwd mm5, VgVbYgYb  ; mm5 =   Vgb     Ygb

		pshufw mm6, mm2, 0x44  ; mm6 = R1a XXX R1a XXX
		pmaddwd mm6, Vr0Yr0    ; mm6 =   Vr      Yr

		paddd mm5, mm6         ; mm5 =   Va      Y1a
		psrad mm5, 15          ; mm5 >>= 15


		pshufw mm2, mm2, 0xee  ; mm2 = G0b B0b G0b B0b
		pmaddwd mm2, UgUbYgYb  ; mm2 =   Ugb     Ygb

		pshufw mm1, mm3, 0x00  ; mm1 = R0b XXX R0b XXX
		pmaddwd mm1, Ur0Yr0    ; mm1 =   Ur      Yr

		paddd mm2, mm1         ; mm2 =   Ub      Y0b
		psrad mm2, 15          ; mm2 >>= 15


		pshufw mm6, mm3, 0x99  ; mm6 = G1b B1b G1b B1b
		pmaddwd mm6, VgVbYgYb  ; mm6 =   Vgb     Ygb

		pshufw mm3, mm3, 0xcc  ; mm3 = R1b XXX R1b XXX
		pmaddwd mm3, Vr0Yr0    ; mm3 =   Vr      Yr

		paddd mm6, mm3         ; mm6 =   Vb      Y1b
		psrad mm6, 15          ; mm6 >>= 15


		packssdw mm4, mm5      ; mm4 = Va Y1a Ua Y0a
		packssdw mm2, mm6      ; mm2 = Vb Y1b Ub Y0b

		paddsw mm4, mm7        ; mm4 = Va+128 Y1a Ua+128 Y0a
		paddsw mm2, mm7        ; mm2 = Vb+128 Y1b Ub+128 Y0b

		packuswb mm4, mm2   ; mm4 = Vb+128 Y1b Ub+128 Y0b Va+128 Y1a Ua+128 Y0a


		// Destination must be 8-byte aligned.
		movq [edi], mm4
		add edi, 8


		dec eax
		jnz next4

		//mov src, esi

		emms

	}
}

void RGB24ToYUY2SSE23(BYTE *dest, BYTE *src, int nWidth, int nHeight,int SrcPitch,int DstPitch)
{

	int w = nWidth * nHeight;
	BYTE *pTemp = new BYTE[nWidth * 3];
	for(int i = 0;i<nHeight/2;i++)
	{ 
		CopyMemory(pTemp,src+i * nWidth*3,nWidth * 3);
		CopyMemory(src+i * nWidth*3,src+(nHeight-i-1)* nWidth*3,nWidth * 3);
		CopyMemory(src+(nHeight-i-1)* nWidth*3,pTemp,nWidth * 3);
	}
	delete []pTemp;

	static short const128_0_128_0[8] = { 0, 128, 0, 128, 0, 128, 0, 128 };
	static short UgUbYgYb[8] = { 3736, 19235,  16384, -10879, 3736, 19235,  16384, -10879 };
	static short Ur0Yr0[8] =   {0,  9798, 0, -5505, 0,  9798, 0, -5505 };
	static short VgVbYgYb[8] = { 3736, 19235, -2654,  -13730, 3736, 19235, -2654,  -13730 };
	static short Vr0Yr0[8] =   {0, 9798, 0, 16384, 0, 9798, 0, 16384};

	static const __int64 mask0[2] = {0x0000000000000000,0xffff0000ffff0000};
	_asm
	{
		mov esi, src
		mov edi, dest
		mov eax, w
		shr eax,3

		pxor xmm0, xmm0              ; mm0 = 0, constant
		movdqu xmm7, const128_0_128_0 ; constant

next4:
		// Process 4 pixels.  First YUYV is a, second is b.
		// Source must be 12-byte aligned.
		movdqu xmm4, [esi+0]    
		movq  xmm1,xmm4        // xmm1 = 0000 0000 g3b3r2g2 b2r1g1b1
		pshufd xmm4,xmm4,0x4e
        movq  xmm2, xmm4       // xmm2 = 0000 0000 b6r5g5b5 r4g4b4r3
		movdqu xmm4, [esi+16]      
		movq xmm3, xmm4        //xmm3 =  0000 0000  r8g8b8r7 g7b7r6g6
		add esi, 24

		punpcklbw xmm1, xmm0     //xmm1 =  00 b2 00 r1 00 g1 00 b1  low
		punpcklbw xmm2, xmm0     //xmm2 = G0b B0b R1a G1a
		punpcklbw xmm3, xmm0     // xmm3 = R1b G1b B1b R0b


		pshuflw xmm4, xmm1, 0x44  ; mm4 = 00 g1 00 b1 00 g1 00 b1
		pshufhw xmm4, xmm4, 0x00  ; mm4 = 00 g2 00 g2 00 g2 00 g2
		pshuflw xmm5, xmm1, 0xff  ; mm5 = XXX XXX 00 b2 00 b2
		pshufd xmm5, xmm5,0x4e
		pand  xmm5, mask0
		por  xmm4,xmm5
		pshufhw xmm4, xmm4, 0xb1
		pmaddwd xmm4, UgUbYgYb  // xmm4 = Ug2     Yg2  Ug1     Yg1

		pshuflw xmm5, xmm1, 0x88  
		pshufd xmm6,xmm1,0x64
		pshufhw xmm6, xmm6, 0x77 
		pshuflw xmm6, xmm6, 0x88  
		pmaddwd xmm6, Ur0Yr0    

		paddd xmm4, xmm6        // xmm4 = U2 Y2 U1 Y1
		psrad xmm4, 15          // xmm4 >>= 15


		pshuflw xmm5, xmm1, 0x0f  ; mm5 = XXX XXX B1a B1a
		pshufw mm6, mm2, 0x00  ; mm6 = XXX XXX G1a G1a

		punpcklwd mm5, mm6     ; mm5 = G1a B1a G1a B1a
		pmaddwd mm5, VgVbYgYb  ; mm5 =   Vgb     Ygb

		pshufw mm6, mm2, 0x44  ; mm6 = R1a XXX R1a XXX
		pmaddwd mm6, Vr0Yr0    ; mm6 =   Vr      Yr

		paddd mm5, mm6         ; mm5 =   Va      Y1a
		psrad mm5, 15          ; mm5 >>= 15


		pshufw mm2, mm2, 0xee  ; mm2 = G0b B0b G0b B0b
		pmaddwd mm2, UgUbYgYb  ; mm2 =   Ugb     Ygb

		pshufw mm1, mm3, 0x00  ; mm1 = R0b XXX R0b XXX
		pmaddwd mm1, Ur0Yr0    ; mm1 =   Ur      Yr

		paddd mm2, mm1         ; mm2 =   Ub      Y0b
		psrad mm2, 15          ; mm2 >>= 15


		pshufw mm6, mm3, 0x99  ; mm6 = G1b B1b G1b B1b
		pmaddwd mm6, VgVbYgYb  ; mm6 =   Vgb     Ygb

		pshufw mm3, mm3, 0xcc  ; mm3 = R1b XXX R1b XXX
		pmaddwd mm3, Vr0Yr0    ; mm3 =   Vr      Yr

		paddd mm6, mm3         ; mm6 =   Vb      Y1b
		psrad mm6, 15          ; mm6 >>= 15


		packssdw mm4, mm5      ; mm4 = Va Y1a Ua Y0a
		packssdw mm2, mm6      ; mm2 = Vb Y1b Ub Y0b

		paddsw mm4, mm7        ; mm4 = Va+128 Y1a Ua+128 Y0a
		paddsw mm2, mm7        ; mm2 = Vb+128 Y1b Ub+128 Y0b

		packuswb mm4, mm2   ; mm4 = Vb+128 Y1b Ub+128 Y0b Va+128 Y1a Ua+128 Y0a


		// Destination must be 8-byte aligned.
		movq [edi], mm4
		add edi, 8

		dec eax
		jnz next4
		emms

	}
}

//void RGB24ToYUY2SSE2(BYTE   *src,   BYTE   *desty,    int   srcrowsize,   int   destrowsize, 
//					 int   width,   int   height) 
void RGB24ToYUY2SSE2(BYTE *desty, BYTE *src, int width, int height, int   srcrowsize,   int   destrowsize)
{
	unsigned   char   *yp = desty; 
	unsigned   char   *prow = src; 
	int   i,   j ; 

	int nOffset = width - ((width>>3) << 3);
	int alignSrcWidth = ((width>>3) << 3) * 3; 
	int alignDstWidth = ((width>>3) << 3) << 1;

	int n = height;
	BYTE *pTemp = new BYTE[width * 3];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*3,width * 3);
		CopyMemory(src+i * width*3,src+(height-i-1)* width*3,width * 3);
		CopyMemory(src+(height-i-1)* width*3,pTemp,width * 3);
	}
	delete []pTemp;
     __declspec(align(16))
	static short  int ycoefsf[8] = {2851,   22970,   6947,   0, 2851,   22970,   6947,   0 };
	 __declspec(align(16))
	static short  int ucoefs[8] =   {16384,   -12583,   -3801,   0, 16384,   -12583,   -3801,   0 };
	 __declspec(align(16))
	static short int vcoefs[8] = {-1802,   -14582,   16384,   0, -1802,   -14582,   16384,   0 };
 	 __declspec(align(16))   
	static const __int64 mask0[2] = {0xffffffffffffffff,0x00000000ffffffff};
 	 __declspec(align(16))
	static const __int64 mask1[2] = {0x0d0504030c020100,0x0f0b0a090e080706};
 	 __declspec(align(16))
	static const __int64 mask3[2] = {0x0000000000000000,0x00000000ffffffff};
 	 __declspec(align(16))
	static const __int64 mask4[2] = {0xffffffffffffffff,0x0000000000000000};
 	 __declspec(align(16))
	static const __int64 mask5[2] = {0x0d01000b0c0a0908,0x0f0706050e040302};

	for(int n = 0;n< height; n++)
	{
		_asm   
		{ 
			push eax
			push ebx
			push ecx
			push edx
			push edi
			push esi

			xor    edx,   edx 
			mov    eax,   width 
			sar    eax,3 
			//cmp    edx,   eax 
			//jge    yuvexit 

			mov    j,   eax 
			//
			//mov    eax,   height 

			//sar       eax,   1 

			//mov    i,   eax 
			//cmp    edx,   eax 
			//jge    yuvexit 

			//mov    eax,   desty 
			//mov    yp,   eax 
			//mov    eax,   src 
			//mov    prow,   eax 
            //
			pxor   xmm7,   xmm7 
			mov    eax,   i 

			//heighttop:
			//		mov    i,   eax 
			mov    edi,   j 
			mov    ebx,   prow 
			mov    ecx,   yp 

widthtop: 
            movdqu        xmm0, [ebx] 
			movdqu        xmm2, xmm0
			pand xmm0, mask0
		    pshufb xmm0,mask1
			movdqu        xmm5,   xmm0     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
			pshufd   xmm2, xmm2,0x30
			pand xmm2,mask3
			movdqu        xmm0,   [ebx+16]
			pand  xmm0, mask4
			por  xmm0, xmm2
			pshufb xmm0,mask5
			movdqu        xmm3,   xmm0
			add           ebx,   24 

			movdqu     xmm0,xmm5
			pshufd       xmm0,xmm0,0x4e
			movq        xmm6,   xmm5    // xmm6 0   r2   g2   b2 0   r1   g1   b1
			movq        xmm5,   xmm0    // xmm5 0   r4   g4   b4 0   r3   g3   b3

			movdqu     xmm0,xmm3       // xmm0 0 r8...b4
			pshufd       xmm0,xmm0,0x4e
			movq        xmm4,   xmm3    // xmm4  0   r6   g6   b6  0   r5   g5   b5
			movq        xmm3,   xmm0    // xmm3  0   r8   g8   b8  0   r7   g7   b7
			punpcklbw   xmm6,   xmm7   // xmm6  0   r2   g2   b2  0   r1   g1   b1 
			punpcklbw  xmm5,   xmm7  // xmm5   0   r4   g4   b4  0   r3   g3   b3 
			punpcklbw   xmm4,   xmm7   // xmm4  0   r6   g6   b6  0   r5   g5   b5
			punpcklbw   xmm3,   xmm7  // xmm3  0   r8   g8   b8  0   r7   g7   b7

			movdqu      xmm0,   xmm6 
			movdqu      xmm1,   xmm5 

			pmaddwd   xmm0,   ycoefsf   //   xmm0   r2*cr   and   g2*cg+b2*cb   r1*cr   and   g1*cg+b1*cb 
			movdqu      xmm2,  xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2       //   xmm0  y2   y1   in   lower   32   bits

			///y1
			movd      eax,    xmm0 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx],   al 

			pshufd       xmm0,xmm0,0x4e
			//y2
			movd      eax,    xmm0 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+2],   al 

			pmaddwd   xmm1,   ycoefsf   
			movdqu      xmm2,   xmm1 
			psrlq     xmm2,   32 
			paddd    xmm1,   xmm2     
			///y3
			movd      eax,   xmm1 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+4],   al 

			pshufd       xmm1,xmm1,0x4e
			//y4
			movd      eax,   xmm1 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+6],   al 

			movdqu      xmm0,   xmm6 
			movdqu      xmm1,   xmm5 
			//u1
			pmaddwd   xmm0,   ucoefs   
			movdqu      xmm2,   xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2      
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+1],   al 
			//u2
			pshufd       xmm0,xmm0,0x4e
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+5],   al

			//v1
			movdqu      xmm0,   xmm6 
			pmaddwd   xmm0,  vcoefs  
			movdqu      xmm2,   xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2     

			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+3],   al 
			//v2
			pshufd       xmm0,xmm0,0x4e
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+7],   al 

			movdqu      xmm0,   xmm4  
			movdqu      xmm1,   xmm3

			pmaddwd   xmm0,   ycoefsf   
			movdqu      xmm2,  xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2      

			//y5
			movd      eax,    xmm0 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+8],   al 
			//y6
			pshufd       xmm0,xmm0,0x4e
			movd      eax,    xmm0 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+10],   al 
			//y7
			pmaddwd   xmm1,   ycoefsf  
			movdqu      xmm2,   xmm1 
			psrlq     xmm2,   32 
			paddd    xmm1,   xmm2       

			movd      eax,   xmm1 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+12],   al 

			//y8
			pshufd       xmm1,xmm1,0x4e
			movd      eax,   xmm1 
			imul      eax,   219 
			shr       eax,   8 
			add       eax,   540672 
			shr       eax,   15 
			mov       [ecx+14],   al 

			movdqu      xmm0,   xmm4 
			//u3
			pmaddwd   xmm0,   ucoefs   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
			movdqu      xmm2,   xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2       //   MM0   has   u1   in   lower   32   bits 
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+9],   al 
			//u4
			pshufd       xmm0,xmm0,0x4e
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+13],   al

			//v3
			movdqu      xmm0,   xmm4 
			pmaddwd   xmm0,  vcoefs   //   MM5   has   r1*cr   and   g1*cg+b1*cb 
			movdqu      xmm2,   xmm0 
			psrlq     xmm2,   32 
			paddd     xmm0,   xmm2       //   MM5   has   v1   in   lower   32   bits 

			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+11],   al 
			//v4
			pshufd       xmm0,xmm0,0x4e
			movd      eax,   xmm0 
			imul      eax,   224 
			sar       eax,   8 
			add       eax,   4210688 
			shr       eax,   15 
			mov       [ecx+15],   al 

			add      ecx, 16
			dec       edi 	
			jnz       widthtop 

			//mov       eax,   destrowsize 
			//add       yp,   eax 

			//mov       eax,   srcrowsize 

			//add       prow,   eax 
			//mov       eax,   i 
			//dec       eax 
			//jnz       heighttop 

			pop esi
			pop edi
			pop edx
			pop ecx
			pop ebx
			pop eax
			//yuvexit:
			emms 
		} 

		//Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16
		//Cr = V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128
		//Cb = U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128
		if(nOffset > 0)//不对齐处理
		{
			const BYTE *pSrcTemp = prow + alignSrcWidth;
			BYTE *pDstTemp = yp + alignDstWidth;
			BYTE Y1,U1,V1,Y2;

			for(int i = 0 ; i < nOffset; i+=2)
			{
				Y1 =  (*pSrcTemp*98 + *(pSrcTemp+1) * 504 + *(pSrcTemp+2)*257) / 1000 + 16;
				if(Y1>255) Y1 = 255;
				if(Y1<0) Y1 = 0;
				*pDstTemp = Y1;
				Y2 =  (*(pSrcTemp+3)*98 + *(pSrcTemp+4) * 504 + *(pSrcTemp+5) * 257) / 1000 + 16;
				if(Y2>255) Y2 = 255;
				if(Y2<0) Y2 = 0;
				*(pDstTemp+2) = Y2;			
				pSrcTemp += 6;
				pDstTemp += 4;				
			}

			pSrcTemp = prow + alignSrcWidth;
			pDstTemp = yp + alignDstWidth;
				
			for(int i = 0 ; i < nOffset/2; i++)
			{
				U1 =  (*pSrcTemp*439 + *(pSrcTemp+1) * (-291) + *(pSrcTemp+2)*(-148)) / 1000 + 128;
				V1 =  (*pSrcTemp*(-71) + *(pSrcTemp+1) * (-368) + *(pSrcTemp+2)*439) / 1000 + 128;
				*(pDstTemp+1) = U1;				
				*(pDstTemp+3) = V1;						
				pSrcTemp += 3;
				pDstTemp += 4;
			}			
		}
		prow += srcrowsize;
		yp += destrowsize;
	}
}


void RGB32ToYUY2MMX(BYTE   *src,   BYTE   *desty,    int   srcrowsize,   int   destrowsize, 
										int   width,   int   height) 
{
	unsigned   char   *yp; 
	unsigned   char   *prow; 
	int   i,   j ; 

	int n = height;
	BYTE *pTemp = new BYTE[width * 4];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*4,width * 4);
		CopyMemory(src+i * width*4,src+(height-i-1)* width*4,width * 4);
		CopyMemory(src+(height-i-1)* width*4,pTemp,width * 4);
	}
	delete []pTemp;

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,2 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   desty 
		mov    yp,   eax 

		mov    eax,   src 
		mov    prow,   eax 
		pxor   MM7,   MM7 
		mov    eax,   i 

heighttop:
		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   yp 

widthtop: 
		movq        MM5,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		movq        MM3,   [ebx+8]
		add           ebx,   16 
		movq        MM6,   MM5 
		movq        MM4,   MM3
		punpcklbw   MM5,   MM7   //   MM5   has   0   r1   g1   b1 
		punpckhbw   MM6,   MM7   //   MM6   has   0   r2   g2   b2 
		punpcklbw   MM3,   MM7   //   MM3   has   0   r3   g3   b3 
		punpckhbw   MM4,   MM7   //   MM4   has   0   r4   g4   b4 

		movq      MM0,   MM5 
		movq      MM1,   MM6 
		mov       eax,   ycoefs 
		pmaddwd   MM0,   [eax]   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   y1   in   lower   32   bits 
		pmaddwd   MM1,   [eax]   //   MM1   has   r2*cr   and   g2*cg+b2*cb 
		movq      MM2,   MM1 
		psrlq     MM2,   32 
		paddd     MM1,   MM2       //   MM1   has   y2   in   lower   32   bits 
		///y1
		movd      eax,   MM0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		///y2
		movd      eax,   MM1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+2],   al 


		movq      MM0,   MM3 
		movq      MM1,   MM4 
		mov       eax,   ycoefs 
		pmaddwd   MM0,   [eax]   //   MM0   has   r3*cr   and   g3*cg+b3*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   y3  in   lower   32   bits 
		pmaddwd   MM1,   [eax]   //   MM1   has   r4*cr   and   g4*cg+b4*cb 
		movq      MM2,   MM1 
		psrlq     MM2,   32 
		paddd     MM1,   MM2       //   MM1   has   y4  in   lower   32   bits 

		///y3
		movd      eax,   MM0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+4],   al 
		///y4
		movd      eax,   MM1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+6],   al 


		////////u1
		movq      MM0,   MM5 
		mov       eax,   ucoefs 
		pmaddwd   MM0,   [eax]   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   u1   in   lower   32   bits 
		movd      eax,   MM0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+1],   al 

		////u2
		movq      MM0,   MM6
		mov       eax,   ucoefs 
		pmaddwd   MM0,   [eax]   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   u1   in   lower   32   bits 
		movd      eax,   MM0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+5],   al 

		///v1
		mov       eax,   vcoefs 
		pmaddwd   MM5,   [eax]   //   MM5   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM5 
		psrlq     MM2,   32 
		paddd     MM5,   MM2       //   MM5   has   v1   in   lower   32   bits 

		pmaddwd   MM6,   [eax]   //   MM6   has   r2*cr   and   g2*cg+b2*cb 
		movq      MM2,   MM6 
		psrlq     MM6,   32 
		paddd     MM6,   MM2       //   MM6   has   v2   in   lower   32   bits 

		movd      eax,   MM5 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+3],   al 

		movd      eax,   MM6 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+7],   al 

		add      ecx, 8


		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       yp,   eax 



		mov       eax,   srcrowsize 
		//sub       prow,   eax 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		emms 
	} 
}

void RGB32ToYUY2SSE2(BYTE   *src,   BYTE   *desty,    int   srcrowsize,   int   destrowsize, 
					int   width,   int   height) 
{
	unsigned   char   *yp = desty; 
	unsigned   char   *prow = src; 
	int   i,   j ; 

	int nOffset = width - ((width>>3) << 3);
	int alignSrcWidth = ((width>>3) << 3) << 2; 
	int alignDstWidth = ((width>>3) << 3) << 1;

	int n = height;
	BYTE *pTemp = new BYTE[width * 4];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*4,width * 4);
		CopyMemory(src+i * width*4,src+(height-i-1)* width*4,width * 4);
		CopyMemory(src+(height-i-1)* width*4,pTemp,width * 4);
	}
	delete []pTemp;

  __declspec(align(16))
	static short int ycoefs[8] = {2851,   22970,   6947,   0, 2851,   22970,   6947,   0 };
  __declspec(align(16))
	static short int ucoefs[8] =   {16384,   -12583,   -3801,   0, 16384,   -12583,   -3801,   0 };
  __declspec(align(16))
	static short int vcoefs[8] = {-1802,   -14582,   16384,   0, -1802,   -14582,   16384,   0 };

	for(int n = 0;n< height; n++)
	{
	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,3 
		cmp    edx,   eax 
		//jge    yuvexit 

		mov    j,   eax 
		//mov    eax,   height 

		//sar       eax,   1 

		//mov    i,   eax 
		//cmp    edx,   eax 
		//jge    yuvexit 

		//mov    eax,   desty 
		//mov    yp,   eax 
		//mov    eax,   src 
		//mov    prow,   eax 
		pxor   xmm7,   xmm7 
		mov    eax,   i 

//heighttop:
//		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   yp 

widthtop: 
		movdqu        xmm5,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		movdqu        xmm3,   [ebx+16]
		add           ebx,   32 

        movdqu     xmm0,xmm5
		pshufd       xmm0,xmm0,0x4e
		movq        xmm6,   xmm5    // xmm6 0   r2   g2   b2 0   r1   g1   b1
		movq        xmm5,   xmm0    // xmm5 0   r4   g4   b4 0   r3   g3   b3

        movdqu     xmm0,xmm3       // xmm0 0 r8...b4
		pshufd       xmm0,xmm0,0x4e
		movq        xmm4,   xmm3    // xmm4  0   r6   g6   b6  0   r5   g5   b5
		movq        xmm3,   xmm0    // xmm3  0   r8   g8   b8  0   r7   g7   b7
		punpcklbw   xmm6,   xmm7   // xmm6  0   r2   g2   b2  0   r1   g1   b1 
		punpcklbw  xmm5,   xmm7  // xmm5   0   r4   g4   b4  0   r3   g3   b3 
		punpcklbw   xmm4,   xmm7   // xmm4  0   r6   g6   b6  0   r5   g5   b5
		punpcklbw   xmm3,   xmm7  // xmm3  0   r8   g8   b8  0   r7   g7   b7

		movdqu      xmm0,   xmm6 
		movdqu      xmm1,   xmm5 

		pmaddwd   xmm0,   ycoefs   //   xmm0   r2*cr   and   g2*cg+b2*cb   r1*cr   and   g1*cg+b1*cb 
		movdqu      xmm2,  xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2       //   xmm0  y2   y1   in   lower   32   bits

		///y1
		movd      eax,    xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 

		pshufd       xmm0,xmm0,0x4e
		//y2
		movd      eax,    xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+2],   al 

		pmaddwd   xmm1,   ycoefs   
		movdqu      xmm2,   xmm1 
		psrlq     xmm2,   32 
		paddd    xmm1,   xmm2     
		///y3
		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+4],   al 

		pshufd       xmm1,xmm1,0x4e
		//y4
		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+6],   al 

		movdqu      xmm0,   xmm6 
		movdqu      xmm1,   xmm5 
		//u1
		pmaddwd   xmm0,   ucoefs   
		movdqu      xmm2,   xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2      
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+1],   al 
        //u2
		pshufd       xmm0,xmm0,0x4e
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+5],   al

		//v1
		movdqu      xmm0,   xmm6 
		pmaddwd   xmm0,  vcoefs  
		movdqu      xmm2,   xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2     

		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+3],   al 
        //v2
		pshufd       xmm0,xmm0,0x4e
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+7],   al 

		movdqu      xmm0,   xmm4  
		movdqu      xmm1,   xmm3

		pmaddwd   xmm0,   ycoefs   
		movdqu      xmm2,  xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2      

		//y5
		movd      eax,    xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+8],   al 
        //y6
		pshufd       xmm0,xmm0,0x4e
		movd      eax,    xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+10],   al 
        //y7
		pmaddwd   xmm1,   ycoefs  
		movdqu      xmm2,   xmm1 
		psrlq     xmm2,   32 
		paddd    xmm1,   xmm2       
		
		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+12],   al 

        //y8
		pshufd       xmm1,xmm1,0x4e
		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx+14],   al 

		movdqu      xmm0,   xmm4 
		//u3
		pmaddwd   xmm0,   ucoefs   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movdqu      xmm2,   xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2       //   MM0   has   u1   in   lower   32   bits 
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+9],   al 
		//u4
		pshufd       xmm0,xmm0,0x4e
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+13],   al

		//v3
		movdqu      xmm0,   xmm4 
		pmaddwd   xmm0,  vcoefs   //   MM5   has   r1*cr   and   g1*cg+b1*cb 
		movdqu      xmm2,   xmm0 
		psrlq     xmm2,   32 
		paddd     xmm0,   xmm2       //   MM5   has   v1   in   lower   32   bits 

		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+11],   al 
		//v4
		pshufd       xmm0,xmm0,0x4e
		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [ecx+15],   al 

		add      ecx, 16
		dec       edi 	
		jnz       widthtop 

		//mov       eax,   destrowsize 
		//add       yp,   eax 

		//mov       eax,   srcrowsize 

		//add       prow,   eax 
		//mov       eax,   i 
		//dec       eax 
		//jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
//yuvexit:
		emms 
	} 

	//Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16
	//Cr = V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128
	//Cb = U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128
	if(nOffset > 0)//不对齐处理
	{
		const BYTE *pSrcTemp = prow + alignSrcWidth;
		 BYTE *pDstTemp = yp + alignDstWidth;
		BYTE Y1,U1,V1,Y2,U2,V2;

		for(int i = 0 ; i < nOffset; i+=2)
		{
			Y1 =  (*pSrcTemp*98 + *(pSrcTemp+1) * 504 + *(pSrcTemp+2)*257) / 1000 + 16;
			if(Y1>255) Y1 = 255;
			if(Y1<0) Y1 = 0;
			*pDstTemp = Y1;
			Y2 =  (*(pSrcTemp+4)*98 + *(pSrcTemp+5) * 504 + *(pSrcTemp+6) * 257) / 1000 + 16;
			if(Y2>255) Y2 = 255;
			if(Y2<0) Y2 = 0;
			*(pDstTemp+2) = Y2;
			
			if(i / 2 == 0)
			{
				U1 =  (*pSrcTemp*439 + *(pSrcTemp+1) * (-291) + *(pSrcTemp+2)*(-148)) / 1000 + 128;
				*(pDstTemp+1) = U1;
				V1 =  (*pSrcTemp*(-71) + *(pSrcTemp+1) * (-368) + *(pSrcTemp+2)*439) / 1000 + 128;
				*(pDstTemp+3) = V1;
			}
			if(i / 2 == 1)
		    {
				U2 =  (*(pSrcTemp+4)*439 + *(pSrcTemp+5) * (-291) + *(pSrcTemp+5)*(-148)) / 1000 + 128;
				*(pDstTemp+1) = U2;
				V2 =  (*(pSrcTemp+4)*(-71) + *(pSrcTemp+5) * (-368) + *(pSrcTemp+6)*439) / 1000 + 128;
				*(pDstTemp+3) = V2;
			}
			pSrcTemp += 8;
			pDstTemp += 4;
		}
	}
	prow += srcrowsize;
	yp += destrowsize;
	}
}

void   RGB24toYV12MMX(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, int   srcrowsize, int   destrowsize, 
										  int   width,   int   height) 
{ 
	BYTE   *yp,   *up,   *vp; 
	BYTE   *prow; 

	BYTE *pTemp = new BYTE[width * 3];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*3,width * 3);
		CopyMemory(src+i * width*3,src+(height-i-1)* width*3,width * 3);
		CopyMemory(src+(height-i-1)* width*3,pTemp,width * 3);
	}
	delete []pTemp;

	unsigned int      j ,z;

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,2 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov   z,   eax 

		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   desty 
		mov    yp,   eax 
		mov    eax,   destu 
		mov    up,   eax 
		mov    eax,   destv 
		mov    vp,   eax 
		mov    eax,   src 
		mov    prow,   eax 
		pxor mm0, mm0              ; mm0 = 0, constant
		movq mm7, const128_0_128_0 ; constant
		mov    eax,   z 

heighttop:

		mov    z,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   yp 
		mov    edx,   up 
		mov    esi,   vp 

widthtop: 
		pxor mm0, mm0              ; mm0 = 0, constant
		movq mm7, const128_0_128_0 ; constant
		movd        MM1,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		movd        MM2,   [ebx+4]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		movd        MM3,   [ebx+8]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		add           ebx,   12

		punpcklbw mm1, mm0     ; mm1 = B1a R0a G0a B0a
		punpcklbw mm2, mm0     ; mm2 = G0b B0b R1a G1a
		punpcklbw mm3, mm0     ; mm3 = R1b G1b B1b R0b

		pshufw mm4, mm1, 0x44  ; mm4 = G0a B0a G0a B0a
		pmaddwd mm4, UgUbYgYb  ; mm4 =   Ugb     Ygb

		pshufw mm5, mm1, 0x88  ; mm5 = R0a XXX R0a XXX
		pmaddwd mm5, Ur0Yr0    ; mm5 =   Ur      Yr

		paddd mm4, mm5         ; mm4 =   Ua      Y0a
		psrad mm4, 15          ; mm4 >>= 15


		pshufw mm5, mm1, 0x0f  ; mm5 = XXX XXX B1a B1a
		pshufw mm6, mm2, 0x00  ; mm6 = XXX XXX G1a G1a

		punpcklwd mm5, mm6     ; mm5 = G1a B1a G1a B1a
		pmaddwd mm5, VgVbYgYb  ; mm5 =   Vgb     Ygb

		pshufw mm6, mm2, 0x44  ; mm6 = R1a XXX R1a XXX
		pmaddwd mm6, Vr0Yr0    ; mm6 =   Vr      Yr

		paddd mm5, mm6         ; mm5 =   Va      Y1a
		psrad mm5, 15          ; mm5 >>= 15


		pshufw mm2, mm2, 0xee  ; mm2 = G0b B0b G0b B0b
		pmaddwd mm2, UgUbYgYb  ; mm2 =   Ugb     Ygb

		pshufw mm1, mm3, 0x00  ; mm1 = R0b XXX R0b XXX
		pmaddwd mm1, Ur0Yr0    ; mm1 =   Ur      Yr

		paddd mm2, mm1         ; mm2 =   Ub      Y0b
		psrad mm2, 15          ; mm2 >>= 15


		pshufw mm6, mm3, 0x99  ; mm6 = G1b B1b G1b B1b
		pmaddwd mm6, VgVbYgYb  ; mm6 =   Vgb     Ygb

		pshufw mm3, mm3, 0xcc  ; mm3 = R1b XXX R1b XXX
		pmaddwd mm3, Vr0Yr0    ; mm3 =   Vr      Yr

		paddd mm6, mm3         ; mm6 =   Vb      Y1b
		psrad mm6, 15          ; mm6 >>= 15


		packssdw mm4, mm5      ; mm4 = Va Y1a Ua Y0a
		packssdw mm2, mm6      ; mm2 = Vb Y1b Ub Y0b

		paddsw mm4, mm7        ; mm4 = Va+128 Y1a Ua+128 Y0a
		paddsw mm2, mm7        ; mm2 = Vb+128 Y1b Ub+128 Y0b

		movq  mm5,mm4
		movq  mm6,mm2

		movd  eax  ,mm5
		mov   [ecx]  , al    //y1
		psrlq     mm5,   32 
		movd  eax  ,mm5
		mov   [ecx+1]  , al   //y2

		movd  eax  ,mm6
		mov   [ecx+2]  , al     //y3
		psrlq     mm6,   32 
		movd  eax  ,mm6
		mov   [ecx+3]  , al   //y4
		add ecx ,4

		test  z, 0x00000001
		jnz  L1

		movd  eax  ,mm4
		sar     eax,16
		mov   [edx],al      //u1
		psrlq     mm4,   32 
		movd  eax  ,mm4
		sar     eax,16
		mov   [esi]  , al   //v1


		movd  eax  ,mm2
		sar     eax,16
		mov   [edx+1],al     //u2
		psrlq     mm2,   32 
		movd  eax  ,mm2
		sar     eax,16
		mov   [esi+1]  , al  //v2

		add edx,2
		add  esi ,2
L1:
		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       yp,   eax 


		test  z, 0x00000001
		jnz  L2

		sar        eax,1
		add       up,   eax 
		add       vp,   eax 
L2:
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   z
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax

yuvexit:
		emms 
	} 
}

void   RGB24toYV12SSE2(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, int   srcrowsize, int   destrowsize, 
					   int   width,   int   height) 
{ 
	BYTE   *yp,   *up,   *vp; 
	BYTE   *prow; 
	int   i,   j ; 

	int n = height;
	BYTE *pTemp = new BYTE[width * 3];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*3,width * 3);
		CopyMemory(src+i * width*3,src+(height-i-1)* width*3,width * 3);
		CopyMemory(src+(height-i-1)* width*3,pTemp,width * 3);
	}
	delete []pTemp;

  __declspec(align(16))
	static short int ycoefs[8] = {2851,   22970,   6947,   0, 2851,   22970,   6947,   0 };
  __declspec(align(16))
	static short int ucoefs[8] =   {16384,   -12583,   -3801,   0, 16384,   -12583,   -3801,   0 };
  __declspec(align(16))
	static short int vcoefs[8] = {-1802,   -14582,   16384,   0, -1802,   -14582,   16384,   0 };
  __declspec(align(16))
	static const __int64 mask0[2] = {0xffffffffffffffff,0x00000000ffffffff};
  __declspec(align(16))
	static const __int64 mask1[2] = {0x0d0504030c020100,0x0f0b0a090e080706};

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,2
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   desty 
		mov    yp,   eax 
		mov    eax,   destu 
		mov    up,   eax 
		mov    eax,   destv 
		mov    vp,   eax 
		mov    eax,   src 
		mov    prow,   eax 
		pxor    xmm7,   xmm7 
		mov    eax,   i 

heighttop:

		mov    i,   eax 
		mov    ebx,   j 
		mov    edi,   prow 
		mov    ecx,   yp 
		mov    edx,   up 
		mov    esi,   vp 

widthtop: 
		movdqu   xmm0,   [edi]     
		pand xmm0 ,mask0
		pshufb xmm0, mask1
		movq      xmm5,   xmm0 // xmm1  = 00..00 g3 b3 r2 g2 b2 r1 g1 b1
		pshufd     xmm0 ,xmm0 ,0x4e
		movq       xmm4,xmm0
		add           edi,   12 

		punpcklbw   xmm5,   xmm7  
		punpcklbw   xmm4,   xmm7   

		movdqu      xmm0,  xmm5 
		movdqu      xmm1,   xmm4 

		pmaddwd   xmm0,   ycoefs 
		movdqu      xmm2,   xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2      
		pmaddwd   xmm1,   ycoefs   
		movdqu      xmm2,   xmm1 
		PSRLQ     xmm2,   32 
		paddd     xmm1,   xmm2      

		movd      eax,  xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		pshufd     xmm0 ,xmm0 ,0x4e
		movd      eax,  xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		pshufd     xmm1 ,xmm1 ,0x4e
		movd      eax,  xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		test  i, 0x00000001
		jnz  L1


		movdqu      xmm0,   xmm5 
		movdqu      xmm1,   xmm4 

		pmaddwd   xmm0,   ucoefs   
		movdqu      xmm2,  xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2      


		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [edx],   al 
		inc       edx 

		movdqu      xmm0,   xmm5 
		pshufd     xmm0 ,xmm0 ,0x4e

		pmaddwd   xmm0,   vcoefs    
		movdqa      xmm2,  xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2        

		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [esi],   al 
		inc       esi 

		pmaddwd   xmm1,   ucoefs   
		movdqu     xmm2,   xmm1 
		PSRLQ     xmm2,   32 
		paddd     xmm1,   xmm2        

		movd      eax,   xmm1 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [edx],   al 
		inc       edx 

		movdqu      xmm1,   xmm4 
		pmaddwd   xmm1,   vcoefs   

		movdqu     xmm2,   xmm1 
		PSRLQ     xmm2,   32 
		paddd     xmm1,   xmm2       

		movd      eax,   xmm1 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [esi],   al 
		inc       esi 

L1:
		dec       ebx 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       yp,   eax 
		sar        eax,1

		test  i, 0x00000001
		jnz  L2

		add       up,   eax 
		add       vp,   eax 
L2:
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		
yuvexit:
		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
		emms 
	} 
}



void   RGB32toYV12MMX(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, int   srcrowsize, int   destrowsize, 
										  int   width,   int   height) 
{ 
	unsigned   char   *yp,   *up,   *vp; 
	unsigned   char   *prow; 
	int   i,   j ; 

	int n = height;
	BYTE *pTemp = new BYTE[width * 4];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*4,width * 4);
		CopyMemory(src+i * width*4,src+(height-i-1)* width*4,width * 4);
		CopyMemory(src+(height-i-1)* width*4,pTemp,width * 4);
	}
	delete []pTemp;

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,1 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   desty 
		mov    yp,   eax 
		mov    eax,   destu 
		mov    up,   eax 
		mov    eax,   destv 
		mov    vp,   eax 
		mov    eax,   src 
		mov    prow,   eax 
		pxor   MM7,   MM7 
		mov    eax,   i 
heighttop:

		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   yp 
		mov    edx,   up 
		mov    esi,   vp 
widthtop: 
		movq        MM5,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		add           ebx,   8 
		movq        MM6,   MM5 
		punpcklbw   MM5,   MM7   //   MM5   has   0   r1   g1   b1 
		punpckhbw   MM6,   MM7   //   MM6   has   0   r2   g2   b2 

		movq      MM0,   MM5 
		movq      MM1,   MM6 
		mov       eax,   ycoefs 
		pmaddwd   MM0,   [eax]   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   y1   in   lower   32   bits 
		pmaddwd   MM1,   [eax]   //   MM1   has   r2*cr   and   g2*cg+b2*cb 
		movq      MM2,   MM1 
		psrlq     MM2,   32 
		paddd     MM1,   MM2       //   MM1   has   y2   in   lower   32   bits 
		movd      eax,   MM0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 
		movd      eax,   MM1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		test  i, 0x00000001
		jnz  L1


		movq      MM0,   MM5 
		//		movq      MM1,   MM6 
		movq      MM1,   MM5 
		mov       eax,   ucoefs
		pmaddwd   MM0,   [eax]   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM0 
		psrlq     MM2,   32 
		paddd     MM0,   MM2       //   MM0   has   u1   in   lower   32   bits 


		movd      eax,   MM0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [edx],   al 
		inc       edx 

		mov       eax,   vcoefs 
		pmaddwd   MM5,   [eax]   //   MM5   has   r1*cr   and   g1*cg+b1*cb 
		movq      MM2,   MM5 
		psrlq     MM2,   32 
		paddd     MM5,   MM2       //   MM5   has   v1   in   lower   32   bits 

		movd      eax,   MM5 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [esi],   al 
		inc       esi 

L1:
		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       yp,   eax 
		sar        eax,1

		test  i, 0x00000001
		jnz  L2

		add       up,   eax 
		add       vp,   eax 
L2:
		mov       eax,   srcrowsize 
		//sub       prow,   eax 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		//pop  ebx
		emms 
	} 
}

void   RGB32toYV12SSE2(BYTE   *src,   BYTE   *desty, BYTE   *destu,BYTE   *destv, int   srcrowsize, int   destrowsize, 
					  int   width,   int   height) 
{ 
	BYTE   *yp,   *up,   *vp; 
	BYTE   *prow; 
	int   i,   j ; 

	int n = height;
	BYTE *pTemp = new BYTE[width * 4];
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*4,width * 4);
		CopyMemory(src+i * width*4,src+(height-i-1)* width*4,width * 4);
		CopyMemory(src+(height-i-1)* width*4,pTemp,width * 4);
	}
	delete []pTemp;

  __declspec(align(16))
	static short int ycoefs[8] = {2851,   22970,   6947,   0, 2851,   22970,   6947,   0 };
  __declspec(align(16))
	static short int ucoefs[8] =   {16384,   -12583,   -3801,   0, 16384,   -12583,   -3801,   0 };
  __declspec(align(16))
	static short int vcoefs[8] = {-1802,   -14582,   16384,   0, -1802,   -14582,   16384,   0 };

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,2 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   desty 
		mov    yp,   eax 
		mov    eax,   destu 
		mov    up,   eax 
		mov    eax,   destv 
		mov    vp,   eax 
		mov    eax,   src 
		mov    prow,   eax 
		pxor    xmm7,   xmm7 
		mov    eax,   i 

heighttop:

		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   yp 
		mov    edx,   up 
		mov    esi,   vp 

widthtop: 
		movdqu   xmm0,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   4   pixels  
		movq      xmm5,   xmm0 // xmm1  = 00..00 g3 b3 r2 g2 b2 r1 g1 b1
		pshufd     xmm0 ,xmm0 ,0x4e
		movq       xmm4,xmm0
		add           ebx,   16 

		punpcklbw   xmm5,   xmm7   //   MM5   has   0   r2   g2   b2 0   r1   g1   b1 
		punpcklbw   xmm4,   xmm7   //   MM4   has  0   r4   g4   b4 0   r3   g3   b3

		movdqa      xmm0,  xmm5 
		movdqa      xmm1,   xmm4 

		pmaddwd   xmm0,   ycoefs   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movdqa      xmm2,   xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2       //   MM0   has   y1 y2  in   lower   32   bits 
		pmaddwd   xmm1,   ycoefs   //   MM1   has   r2*cr   and   g2*cg+b2*cb 
		movdqa      xmm2,   xmm1 
		PSRLQ    xmm2,   32 
		paddd     xmm1,   xmm2       //   MM1   has   y3 y4   in   lower   32   bits 

		movd      eax,  xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		pshufd     xmm0 ,xmm0 ,0x4e
		movd      eax,  xmm0 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		movd      eax,   xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		pshufd     xmm1 ,xmm1 ,0x4e
		movd      eax,  xmm1 
		imul      eax,   219 
		shr       eax,   8 
		add       eax,   540672 
		shr       eax,   15 
		mov       [ecx],   al 
		inc       ecx 

		test  i, 0x00000001
		jnz  L1


		movdqa      xmm0,   xmm5 
		movdqa      xmm1,   xmm4 

		pmaddwd   xmm0,   ucoefs   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movdqa      xmm2,  xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2       //   MM0   has   u1   in   lower   32   bits 


		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [edx],   al 
		inc       edx 
        
		movdqa      xmm0,   xmm5 
		pshufd     xmm0 ,xmm0 ,0x4e

		pmaddwd   xmm0,   vcoefs   //   MM0   has   r1*cr   and   g1*cg+b1*cb 
		movdqa      xmm2,  xmm0 
		PSRLQ     xmm2,   32 
		paddd     xmm0,   xmm2       //   MM0   has   u1   in   lower   32   bits 

		movd      eax,   xmm0 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [esi],   al 
		inc       esi 

		pmaddwd   xmm1,   ucoefs   //   MM5   has   r1*cr   and   g1*cg+b1*cb 
		movdqa     xmm2,   xmm1 
		PSRLQ     xmm2,   32 
		paddd     xmm1,   xmm2       //   MM5   has   v1   in   lower   32   bits 

		movd      eax,   xmm1 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [edx],   al 
		inc       edx 
        
		movdqa      xmm1,   xmm4 
		pmaddwd   xmm1,   vcoefs   //   MM5   has   r1*cr   and   g1*cg+b1*cb 

		movdqa     xmm2,   xmm1 
		PSRLQ     xmm2,   32 
		paddd     xmm1,   xmm2       //   MM5   has   v1   in   lower   32   bits 

       
		movd      eax,   xmm1 
		imul      eax,   224 
		sar       eax,   8 
		add       eax,   4210688 
		shr       eax,   15 
		mov       [esi],   al 
		inc       esi 


L1:
		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       yp,   eax 
		sar        eax,1

		test  i, 0x00000001
		jnz  L2

		add       up,   eax 
		add       vp,   eax 
L2:
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		emms 
	} 
}

void   RGB32toRGB24C(BYTE   *src,   BYTE   *dest, int   width,   int   height) 
{
	BYTE *pSrcTemp = src;
	BYTE *pDstTemp = dest;
	long nPixel = width * height;
	for(int i = 0;i<nPixel;i+=2)
	{
		*pDstTemp = *pSrcTemp;
		*(pDstTemp+1) = *(pSrcTemp+1);
		*(pDstTemp+2 )= *(pSrcTemp+2);
		*(pDstTemp+3) = *(pSrcTemp+4);
		*(pDstTemp+4) = *(pSrcTemp+5);
		*(pDstTemp+5 )= *(pSrcTemp+6);
		pDstTemp+=6;
		pSrcTemp+=8;
	}
}

void   RGB24toRGB32C(BYTE   *src,   BYTE   *dest, int   width,   int   height) 
{
	BYTE *pSrcTemp = src;
	BYTE *pDstTemp = dest;
	long nPixel = width * height;
	for(int i = 0;i<nPixel;i+=2)
	{
		*pDstTemp = *pSrcTemp;
		*(pDstTemp+1) = *(pSrcTemp+1);
		*(pDstTemp+2 )= *(pSrcTemp+2);
		*(pDstTemp+4) = *(pSrcTemp+3);
		*(pDstTemp+5) = *(pSrcTemp+4);
		*(pDstTemp+6 )= *(pSrcTemp+5);
		pDstTemp+=8;
		pSrcTemp+=6;
	}
}

void RGB32ToRGB24MMX(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch)
{
	unsigned   char   *desttemp;
	unsigned   char   *prow; 
	long destrowsize = DstPitch;
	long srcrowsize = SrcPitch;
	int   i,   j ; 
	static const __int64 maskX ={ 0x00ffffff00ffffff};

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,1 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   dest 
		mov    desttemp,   eax  
		mov    eax,   src 
		mov    prow,   eax 
		pxor   MM7,   MM7 
		mov    eax,   i 

heighttop:
		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   desttemp 

widthtop: 
		movq        MM4,   [ebx]     //   MM5   has   0   r2   g2   b2   0   r1   g1   b1,   two   pixels 
		add           ebx,   8 
		pand         MM4, maskX
		movq        MM5,   MM4 
		movq        MM6,   MM4 

		movd     [ecx]  ,mm5
		psrlq     MM6,   32
		movd     eax  ,mm6
		mov      [ecx+3],al
		mov      [ecx+4],ah
		sar  eax,16
		mov      [ecx+5],al

		add   ecx ,6
		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       desttemp,   eax 
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		emms 
	} 
}

void RGB32ToRGB24SSE2(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch)
{
	unsigned   char   *desttemp;
	unsigned   char   *prow; 
	long destrowsize = DstPitch;
	long srcrowsize = SrcPitch;
	int   i,   j ; 

  __declspec(align(16))
	static const __int64 mask0[2] ={ 0x00ffffff00ffffff,0x00ffffff00ffffff};

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,2 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   dest 
		mov    desttemp,   eax  
		mov    eax,   src 
		mov    prow,   eax 
		pxor   xmm7,   xmm7 
		mov    eax,   i 

heighttop:
		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   desttemp 

widthtop: 
		movdqu        xmm4,   [ebx]     //   xmm4    0   r4   g4   b4   0   r3   g3   b3 0   r2   g2   b2   0   r1   g1   b1,   4   pixels 
		add           ebx,  16 
		//por         xmm4, mask0
		movdqu       xmm5,   xmm4 		
		movq        xmm6,   xmm4
		pshufd     xmm5, xmm5,0x4e
		movq        xmm3,   xmm5

		movd     [ecx]  ,xmm6
		psrlq     xmm6,   32
		movd     eax  ,xmm6
		mov      [ecx+3],al
		mov      [ecx+4],ah
		sar  eax,16
		mov      [ecx+5],al

		movd     eax  ,xmm3
		mov      [ecx+6],al
		mov      [ecx+7],ah
		sar  eax,16
		mov      [ecx+8],al

		psrlq     xmm3,   32
		movd     eax  ,xmm3
		mov      [ecx+9],al
		mov      [ecx+10],ah
		sar  eax,16
		mov      [ecx+11],al

		add   ecx ,12
		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       desttemp,   eax 
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		emms 
	} 
}

void RGB24ToRGB32MMX(BYTE   *src,   BYTE   *dest, int   width,   int   height,int SrcPitch,int DstPitch)
{
	unsigned   char   *desttemp;
	unsigned   char   *prow; 
	long destrowsize = width<<2;
	long srcrowsize = width * 3;
	int   i,   j ; 
	static const __int64 mask0 ={ 0x0000ffffffffffff};
	static const __int64 mask1 ={ 0x0705040306020100};
	static const __int64 mask2 ={ 0x0507060304020100};

	_asm   
	{ 
		push eax
		push ebx
		push ecx
		push edx
		push edi
		push esi

		xor    edx,   edx 
		mov    eax,   width 
		sar    eax,3 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    j,   eax 
		mov    eax,   height 

		mov    i,   eax 
		cmp    edx,   eax 
		jge    yuvexit 

		mov    eax,   dest 
		mov    desttemp,   eax  
		mov    eax,   src 
		mov    prow,   eax 

		mov    eax,   i 

heighttop:

		mov    i,   eax 
		mov    edi,   j 
		mov    ebx,   prow 
		mov    ecx,   desttemp 

widthtop: 
		movq        MM4,   [ebx]     //   MM4   has   g3   b3   r2   g2   b2   r1   g1   b1,   
		movq        MM5,   [ebx+8]     //   MM5   has   b6   r5   g5   b5  r4   g4   b4   r3,   
		movq        MM6,   [ebx+16]     //   MM6   has   r8   g8   b8   r7   g7   b7   r6   g6,
		add           ebx,   24 
		movq        MM1,   MM4  
		pand         MM1,   mask0
		movq        MM2,   MM5    
		movq        MM3,   MM6


		pshufb      MM1, mask1

		movq     [ecx]  ,MM1

		movq        MM1,   MM4  
		psrlq     MM1,   48

		psllq     MM2,   16
		paddq    MM2,   MM1
		movq    MM7,   MM2
		pand         MM7,   mask0
		pshufb      MM7, mask1
		movq     [ecx+8]  ,MM7

		movq        MM2,   MM5
		psrlq     MM2,   32
		psllq     MM3,   48
		paddq         MM2,   MM3
		pshufb      MM2,mask2
		movq     [ecx+16]  ,MM2

		psrlq     MM6, 16
		pshufb      MM6,mask1
		movq     [ecx+24]  ,MM6
		add ecx, 32

		dec       edi 	
		jnz       widthtop 

		mov       eax,   destrowsize 
		add       desttemp,   eax 
		mov       eax,   srcrowsize 
		add       prow,   eax 
		mov       eax,   i 
		dec       eax 
		jnz       heighttop 

		pop esi
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
yuvexit:
		emms 
	} 
}

void RGB24ToRGB32SSE2(BYTE  *src,   BYTE *dest, int width,   int height,int SrcPitch,int DstPitch)
{
	unsigned   char   *desttemp = dest;
	unsigned   char   *prow = src; 
	long destrowsize = DstPitch;
	long srcrowsize = SrcPitch;
	int  j ; 
	int nOffset = width - (( width>>4 ) << 4);

  __declspec(align(16))
	static const __int64 mask0[2] ={ 0xffffffffffffffff,0x00000000ffffffff};
  __declspec(align(16))
	static const __int64 mask1[2] ={ 0x0d0504030c020100,0x0f0b0a090e080706};
  __declspec(align(16))
	static const __int64 mask2[2] ={ 0x00000000ffffffff,0x000000000000};
  __declspec(align(16))
	static const __int64 mask3[2] ={ 0xffffffff00000000,0xffffffffffffffff};
  __declspec(align(16))
	static const __int64 mask4[2] ={ 0x0109080700060504,0x030f0e0d020c0b0a};

	int n = height;
	//BYTE *pTemp = new BYTE[width * 3];
	BYTE *pTemp =  (BYTE*)_aligned_malloc(width * 3,16);	
	for(int i = 0;i<height/2;i++)
	{ 
		CopyMemory(pTemp,src+i * width*3,width * 3);
		CopyMemory(src+i * width*3,src+(height-i-1)* width*3,width * 3);
		CopyMemory(src+(height-i-1)* width*3,pTemp,width * 3);
	}
	delete []pTemp;

	for(int i = 0;i < height; i++)
	{
		_asm   
		{ 
			push eax
			//push ebx
			push ecx
			push edx
			push edi
			push esi

			xor    edx,   edx 
			mov    eax,   width 
			sar    eax,4 
			cmp    edx,   eax 
			jge    yuvexit 

			mov    j,   eax 
			mov    edi,   j 
			mov    eax,   prow 
			mov    ecx,   desttemp 

		widthtop: 
			movdqu        xmm4,   [eax]     //xmm4   b6 r5g5b5  r4g4b4 r3g3b3 r2g2b2 r1g1b1,   
			movdqu        xmm5,   [eax+16]  //xmm5  g11  b11r10g10 b10r9g9 b9r8g8 b8r7g7 b7r6g6
			movdqu        xmm6,   [eax+32]  //xmm6  r16  g16b16r15 g15b15r14 g14b14r13 g13b13r12 g12b12r11
			add           eax,   48 
			movdqu        xmm1,   xmm4  
			pand         xmm1,   mask0
			movdqu        xmm2,   xmm5    
			movdqu        xmm3,   xmm6

			pshufb      xmm1, mask1

			movdqu     [ecx]  ,xmm1

			movdqu       xmm1,   xmm4  
			psrlq     xmm1,   32

			pshufd xmm1,xmm1,0x4e
			pand         xmm1,   mask2

			pshufd xmm2,xmm2,0x93
			pand   xmm2,mask3

			por   xmm1,xmm2
			pand xmm1, mask0
			pshufb      xmm1, mask1
			movdqu     [ecx+16]  ,xmm1

			movdqu        xmm2,   xmm5
			pshufd xmm2,xmm2,0x4e
			movq  xmm1,xmm2

			pand xmm3,mask2
			pshufd xmm3,xmm3,0x87
			por   xmm1,xmm3
			pshufb      xmm1, mask1
			movdqu     [ecx+32]  ,xmm1

			movdqu        xmm3,   xmm6
			pand xmm3, mask3
			pshufb      xmm3, mask4
			movdqu     [ecx+48]  ,xmm3

			add ecx, 64

			dec       edi 	
			jnz       widthtop 

			pop esi
			pop edi
			pop edx
			pop ecx
			//pop ebx
			pop eax
		yuvexit:
			emms 
		} 
		if(nOffset>0)
		{
			const BYTE *pSrcTemp = prow + (( width>>4 ) << 4) * 3;
			BYTE *pDstTemp = desttemp +(( width>>4 ) << 4) * 4;
			for(int i = 0;i < nOffset;i++)
			{
				*pDstTemp = *pSrcTemp;
				*(pDstTemp+1) = *(pSrcTemp+1);
				*(pDstTemp+2) = *(pSrcTemp+2);
				*(pDstTemp+3) = 0;
				pSrcTemp+=3;
				pDstTemp+=4;
			}
		}
		prow += srcrowsize;
		desttemp += destrowsize;
	}
}

void YUY2ToRGB32MMXLine(BYTE* pDstLine,BYTE* pYUYV,long width)
{
	const  __int64   csMMX_16_b = 0x1010101010101010; // byte{16,16,16,16,16,16,16,16}
	const  __int64   csMMX_128_w  = 0x0080008000800080; //short{  128,  128,  128,  128}
	const  __int64   csMMX_0x00FF_w = 0x00FF00FF00FF00FF ; //掩码
	const  __int64   csMMX_Y_coeff_w = 0x2543254325432543; //short{ 9539, 9539, 9539, 9539} =1.164383*(1<<13)
	const  __int64   csMMX_U_blue_w = 0x408D408D408D408D ; //short{16525,16525,16525,16525} =2.017232*(1<<13)
	const  __int64   csMMX_U_green_w = 0xF377F377F377F377 ; //short{-3209,-3209,-3209,-3209} =(-0.391762)*(1<<13)
	const  __int64   csMMX_V_green_w = 0xE5FCE5FCE5FCE5FC; //short{-6660,-6660,-6660,-6660} =(-0.812968)*(1<<13)
	const  __int64   csMMX_V_red_w = 0x3313331333133313; //short{13075,13075,13075,13075} =1.596027*(1<<13)
	long expand8_width=(width>>3)<<3;
	if (expand8_width>0)
	{
		__asm
		{
			push    eax
			//push    ebx
			push    ecx
			push    edx
			mov     ecx,expand8_width
			mov     eax,pYUYV
			mov     edx,pDstLine
			lea     eax,[eax+ecx*2]
			lea     edx,[edx+ecx*4]
			neg     ecx
loop_beign:
			movq        mm0,[eax+ecx*2 ] /*mm0=V1 Y3 U1 Y2 V0 Y1 U0 Y0  */                
			movq        mm4,[eax+ecx*2+8] /*mm4=V3 Y7 U3 Y6 V2 Y5 U2 Y4  */                
			movq        mm1,mm0                                                             
			movq        mm5,mm4                                                             
			psrlw       mm1,8              /*mm1=00 V1 00 U1 00 V0 00 U0  */                
			psrlw       mm5,8              /*mm5=00 V3 00 U3 00 V2 00 U2  */                
			pand        mm0,csMMX_0x00FF_w /*mm0=00 Y3 00 Y2 00 Y1 00 Y0  */                
			pand        mm4,csMMX_0x00FF_w /*mm4=00 Y7 00 Y6 00 Y5 00 Y4  */                
			packuswb    mm1,mm5            /*mm1=V3 U3 V2 U2 V1 U1 V0 U0  */                
			movq        mm2,mm1                                                             
			packuswb    mm0,mm4            /*mm0=Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  */                
			psllw       mm1,8              /*mm1=U3 00 U2 00 U1 00 U0 00  */                
			psrlw       mm2,8              /*mm2=00 V3 00 V2 00 V1 00 V0  */                
			psrlw       mm1,8              /*mm1=00 U3 00 U2 00 U1 00 U0  */     

			prefetchnta [eax+ecx*2+64*4]  //预读


			psubusb     mm0,csMMX_16_b        /* mm0 : Y -= 16                       */     
			psubsw      mm1,csMMX_128_w       /* mm1 : u -= 128                      */      
			movq        mm7,mm0                                                              
			psubsw      mm2,csMMX_128_w       /* mm2 : v -= 128                      */      
			pand        mm0,csMMX_0x00FF_w    /* mm0 = 00 Y6 00 Y4 00 Y2 00 Y0       */      
			psllw       mm1,3                 /* mm1 : u *= 8                        */      
			psllw       mm2,3                 /* mm2 : v *= 8                        */      
			psrlw       mm7,8                 /* mm7 = 00 Y7 00 Y5 00 Y3 00 Y1       */      
			movq        mm3,mm1                                                              
			movq        mm4,mm2                                                              
			pmulhw      mm1,csMMX_U_green_w   /* mm1 = u * U_green                   */      
			psllw       mm0,3                 /* y*=8                                */      
			pmulhw      mm2,csMMX_V_green_w   /* mm2 = v * V_green                   */      
			psllw       mm7,3                 /* y*=8                                */      
			pmulhw      mm3,csMMX_U_blue_w                                                   
			paddsw      mm1,mm2                                                              
			pmulhw      mm4,csMMX_V_red_w                                                    
			movq        mm2,mm3                                                              
			pmulhw      mm0,csMMX_Y_coeff_w                                                  
			movq        mm6,mm4                                                              
			pmulhw      mm7,csMMX_Y_coeff_w                                                  
			movq        mm5,mm1                                                              
			paddsw      mm3,mm0               /* mm3 = B6 B4 B2 B0       */                  
			paddsw      mm2,mm7               /* mm2 = B7 B5 B3 B1       */                  
			paddsw      mm4,mm0               /* mm4 = R6 R4 R2 R0       */                  
			paddsw      mm6,mm7               /* mm6 = R7 R5 R3 R1       */                  
			paddsw      mm1,mm0               /* mm1 = G6 G4 G2 G0       */                  
			paddsw      mm5,mm7               /* mm5 = G7 G5 G3 G1       */                  
			packuswb    mm3,mm4               /* mm3 = R6 R4 R2 R0 B6 B4 B2 B0 to [0-255] */ 
			packuswb    mm2,mm6               /* mm2 = R7 R5 R3 R1 B7 B5 B3 B1 to [0-255] */ 
			packuswb    mm5,mm1               /* mm5 = G6 G4 G2 G0 G7 G5 G3 G1 to [0-255] */ 
			movq        mm4,mm3                                                              
			punpcklbw   mm3,mm2               /* mm3 = B7 B6 B5 B4 B3 B2 B1 B0     */        
			punpckldq   mm1,mm5               /* mm1 = G7 G5 G3 G1 xx xx xx xx     */        
			punpckhbw   mm4,mm2               /* mm4 = R7 R6 R5 R4 R3 R2 R1 R0     */        
			punpckhbw   mm5,mm1               /* mm5 = G7 G6 G5 G4 G3 G2 G1 G0     */                                                          
			pcmpeqb     mm2,mm2               /* mm2 = FF FF FF FF FF FF FF FF     */        
			movq        mm0,mm3                                                              
			movq        mm7,mm4                                                              
			punpcklbw   mm0,mm5             /* mm0 = G3 B3 G2 B2 G1 B1 G0 B0       */        
			punpcklbw   mm7,mm2             /* mm7 = FF R3 FF R2 FF R1 FF R0       */        
			movq        mm1,mm0                                                              
			movq        mm6,mm3                                                              
			punpcklwd   mm0,mm7             /* mm0 = FF R1 G1 B1 FF R0 G0 B0       */        
			punpckhwd   mm1,mm7             /* mm1 = FF R3 G3 B3 FF R2 G2 B2       */        
			movntq     [edx+ecx*4],mm0                                                    
			movq        mm7,mm4                                                              
			punpckhbw   mm6,mm5             /* mm6 = G7 B7 G6 B6 G5 B5 G4 B4       */        
			 movntq  [edx+ecx*4+8],mm1                                                  
			punpckhbw   mm7,mm2             /* mm7 = FF R7 FF R6 FF R5 FF R4      */         
			movq        mm0,mm6                                                              
			punpcklwd   mm6,mm7             /* mm6 = FF R5 G5 B5 FF R4 G4 B4      */         
			punpckhwd   mm0,mm7             /* mm0 = FF R7 G7 B7 FF R6 G6 B6      */         
			movntq  [edx+ecx*4+8*2],mm6                                                 
			movntq  [edx+ecx*4+8*3],mm0    
			add     ecx,8
			jnz     loop_beign
			mov     pYUYV,eax
			mov     pDstLine,edx
			pop edx
			pop ecx
			//pop ebx
			pop eax
		}
	}
	//处理边界
	DecodeYUY2ToRgb32Line(pDstLine,pYUYV,width-expand8_width);
	__asm emms;
}

void YUY2ToRGB32SSE2Line(BYTE* pDstLine,BYTE* pYUYV,long width)
{
	__m128  csSSE2_16_b; // byte{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}
	csSSE2_16_b.m128_i64[0] =  0x1010101010101010;
	csSSE2_16_b.m128_i64[1] =  0x1010101010101010;

	__m128   csSSE2_128_w ; //short{  128,  128,  128,  128,  128,  128,  128,  128}
	csSSE2_128_w.m128_i64[0] = 0x0080008000800080;
	csSSE2_128_w.m128_i64[1] = 0x0080008000800080;

	__m128   csSSE2_0x00FF_w ;//= 0x00FF00FF00FF00FF 00FF00FF00FF00FF ; //掩码
	csSSE2_0x00FF_w.m128_i64[0] = 0x00FF00FF00FF00FF;
	csSSE2_0x00FF_w.m128_i64[1] = 0x00FF00FF00FF00FF;

	__m128   csSSE2_Y_coeff_w ;//short{ 9539, 9539, 9539, 9539, 9539, 9539, 9539, 9539} =1.164383*(1<<13)
	csSSE2_Y_coeff_w.m128_i64[0] = 0x2543254325432543; 
	csSSE2_Y_coeff_w.m128_i64[1] = 0x2543254325432543; 

	__m128  csSSE2_U_blue_w ;//short{16525,16525,16525,16525,16525,16525,16525,16525} =2.017232*(1<<13)
	csSSE2_U_blue_w.m128_i64[0] = 0x408D408D408D408D; 
	csSSE2_U_blue_w.m128_i64[1] = 0x408D408D408D408D; 

	__m128   csSSE2_U_green_w;//short{-3209,-3209,-3209,-3209,-3209,-3209,-3209,-3209} =(-0.391762)*(1<<13)
	csSSE2_U_green_w.m128_i64[0] = 0xF377F377F377F377;
	csSSE2_U_green_w.m128_i64[1] = 0xF377F377F377F377;

	__m128   csSSE2_V_green_w; //short{-6660,-6660,-6660,-6660,-6660,-6660,-6660,-6660} =(-0.812968)*(1<<13)
	csSSE2_V_green_w.m128_i64[0] = 0xE5FCE5FCE5FCE5FC;
	csSSE2_V_green_w.m128_i64[1] = 0xE5FCE5FCE5FCE5FC;

	__m128   csSSE2_V_red_w; //short{13075,13075,13075,13075,13075,13075,13075,13075} =1.596027*(1<<13)
	csSSE2_V_red_w.m128_i64[0] = 0x3313331333133313;
	csSSE2_V_red_w.m128_i64[1] = 0x3313331333133313;
	long expand8_width=(width>>4)<<4;
	if (expand8_width>0)
	{
		__asm
		{
			push    eax
			push    ecx
			push    edx
			mov     ecx,expand8_width
			mov     eax,pYUYV
			mov     edx,pDstLine
			lea     eax,[eax+ecx*2]
			lea     edx,[edx+ecx*4]
			neg     ecx
loop_beign:
			movdqu        xmm0, [eax+ecx*2 ] /*mm0=V3 Y7 .... V0 Y1 U0 Y0  */                
			movdqu        xmm4,[eax+ecx*2+16] /*mm4=V7 Y15 .... V4 Y9 U4 Y8  */                
			movdqa        xmm1,xmm0                                                             
			movdqa        xmm5,xmm4                                                             
			psrlw       xmm1,8              /*mm1=00 V3 00 U3 .... 00 V0 00 U0  */                
			psrlw       xmm5,8              /*mm5=00 V7 00 U7 .... 00 V4 00 U4  */                
			pand        xmm0,csSSE2_0x00FF_w /*mm0=00 Y7 .... Y2 00 Y1 00 Y0   */                
			pand        xmm4,csSSE2_0x00FF_w /*mm4=00 Y15 .... Y10 00 Y9 00 Y8  */                
			packuswb    xmm1,xmm5            /*xmm1=V7 U7 V6 U6 V5 U5 V4 U4 V3 U3 V2 U2 V1 U1 V0 U0  */                
			movdqa        xmm2,xmm1                                                             
			packuswb    xmm0,xmm4            /*mm0=Y15 .... Y8 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  */                
			psllw       xmm1,8              /*mm1=U7 00 U2 00 U1 00 U0 00  */             
			psrlw       xmm2,8              /*mm2=00 V7 00 V2 00 V1 00 V0  */                
			psrlw       xmm1,8              /*mm1=00 U7 00 U2 00 U1 00 U0  */     

			prefetchnta [eax+ecx*2+64*4]  //预读


			psubusb     xmm0,csSSE2_16_b        /* mm0 : Y -= 16                       */     
			psubsw      xmm1,csSSE2_128_w       /* mm1 : u -= 128                      */      
			movdqa        xmm7,xmm0                                                              
			psubsw      xmm2,csSSE2_128_w       /* mm2 : v -= 128                      */      
			pand        xmm0,csSSE2_0x00FF_w    /* mm0 = 00 Y14 00 Y4 00 Y2 00 Y0       */      
			psllw       xmm1,3                 /* mm1 : u *= 8                        */      
			psllw       xmm2,3                 /* mm2 : v *= 8                        */      
			psrlw       xmm7,8                 /* mm7 = 00 15 00 Y5 00 Y3 00 Y1       */      
			movdqa        xmm3,xmm1                                                              
			movdqa        xmm4,xmm2                                                              
			pmulhw      xmm1,csSSE2_U_green_w   /* mm1 = u * U_green                   */      
			psllw       xmm0,3                 /* y*=8                                */      
			pmulhw      xmm2,csSSE2_V_green_w   /* mm2 = v * V_green                   */      
			psllw       xmm7,3                 /* y*=8                                */      
			pmulhw      xmm3,csSSE2_U_blue_w                                                   
			paddsw      xmm1,xmm2                                                              
			pmulhw      xmm4,csSSE2_V_red_w                                                    
			movdqa        xmm2,xmm3  

			pmulhw      xmm0,csSSE2_Y_coeff_w                                                  
			movdqa        xmm6,xmm4                                                              
			pmulhw      xmm7,csSSE2_Y_coeff_w                                                  
			movdqa        xmm5,xmm1                                                              
			paddsw      xmm3,xmm0               /* mm3 = B14 .... B0       */                  
			paddsw      xmm2,xmm7               /* mm2 = B15 .... B1       */                  
			paddsw      xmm4,xmm0               /* mm4 = R14....  R0       */                  
			paddsw      xmm6,xmm7               /* mm6 = R15 ..... R1       */                  
			paddsw      xmm1,xmm0               /* mm1 = G14....  G0       */                  
			paddsw      xmm5,xmm7               /* mm5 = G15 .... G1       */                  
			packuswb    xmm3,xmm4               /* mm3 = R14 .... R0 B14 .... B0 to [0-255] */ 
			packuswb    xmm2,xmm6               /* mm2 = R15 .... R1 B15 .... B1 to [0-255] */ 
			packuswb    xmm5,xmm1               /* mm5 = G14 .... G0 G15 .... G1 to [0-255] */ 
			movdqa        xmm4,xmm3                                                              
			punpcklbw   xmm3,xmm2               /* mm3 = B15 .... B0     */  

			punpcklqdq   xmm1,xmm5               /* mm1 = G15 G13 .... G3 G1 xx xx xx xx     */

			punpckhbw   xmm4,xmm2               /* mm4 = R15 .... R0     */        
			punpckhbw   xmm5,xmm1               /* mm5 = G15 .... G0     */                                                          
			pcmpeqb     xmm2,xmm2               /* mm2 = FF FF FF FF FF FF FF FF     */        
			movdqa        xmm0,xmm3                                                              
			movdqa        xmm7,xmm4                                                              
			punpcklbw   xmm0,xmm5             /* mm0 = G3 B3 G2 B2 G1 B1 G0 B0       */        
			punpcklbw   xmm7,xmm2             /* mm7 = FF R3 FF R2 FF R1 FF R0       */        
			movdqa        xmm1,xmm0                                                              
			movdqa        xmm6,xmm3                                                              
			punpcklwd   xmm0,xmm7             /* mm0 = FF R1 G1 B1 FF R0 G0 B0       */        
			punpckhwd   xmm1,xmm7             /* mm1 = FF R3 G3 B3 FF R2 G2 B2       */        
			movdqu     [edx+ecx*4],xmm0                                                    
			movdqa        xmm7,xmm4                                                              
			punpckhbw   xmm6,xmm5             /* mm6 = G7 B7 G6 B6 G5 B5 G4 B4       */        
			movdqu  [edx+ecx*4+16],xmm1                                                  
			punpckhbw   xmm7,xmm2             /* mm7 = FF R7 FF R6 FF R5 FF R4      */         
			movdqa        xmm0,xmm6                                                              
			punpcklwd   xmm6,xmm7             /* mm6 = FF R5 G5 B5 FF R4 G4 B4      */         
			punpckhwd   xmm0,xmm7             /* mm0 = FF R7 G7 B7 FF R6 G6 B6      */         
			movdqu  [edx+ecx*4+32],xmm6                                                 
			movdqu  [edx+ecx*4+48],xmm0    
			add     ecx,16
			jnz     loop_beign
			mov     pYUYV,eax
			mov     pDstLine,edx
			pop edx
			pop ecx
			pop eax
		}
	}
	//处理边界
	DecodeYUY2ToRgb32Line(pDstLine,pYUYV,width-expand8_width);
}

void YUY2ToRGB24SSE2Line(BYTE* pDstLine,BYTE* pYUYV,long width ,long height)
{
	__m128  csSSE2_16_b; // byte{16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}
	csSSE2_16_b.m128_i64[0] =  0x1010101010101010;
	csSSE2_16_b.m128_i64[1] =  0x1010101010101010;

	__m128   csSSE2_128_w ; //short{  128,  128,  128,  128,  128,  128,  128,  128}
	csSSE2_128_w.m128_i64[0] = 0x0080008000800080;
	csSSE2_128_w.m128_i64[1] = 0x0080008000800080;

	__m128   csSSE2_0x00FF_w ;//= 0x00FF00FF00FF00FF 00FF00FF00FF00FF ; //掩码
	csSSE2_0x00FF_w.m128_i64[0] = 0x00FF00FF00FF00FF;
	csSSE2_0x00FF_w.m128_i64[1] = 0x00FF00FF00FF00FF;

	__m128   csSSE2_Y_coeff_w ;//short{ 9539, 9539, 9539, 9539, 9539, 9539, 9539, 9539} =1.164383*(1<<13)
	csSSE2_Y_coeff_w.m128_i64[0] = 0x2543254325432543; 
	csSSE2_Y_coeff_w.m128_i64[1] = 0x2543254325432543; 

	__m128  csSSE2_U_blue_w ;//short{16525,16525,16525,16525,16525,16525,16525,16525} =2.017232*(1<<13)
	csSSE2_U_blue_w.m128_i64[0] = 0x408D408D408D408D; 
	csSSE2_U_blue_w.m128_i64[1] = 0x408D408D408D408D; 

	__m128   csSSE2_U_green_w;//short{-3209,-3209,-3209,-3209,-3209,-3209,-3209,-3209} =(-0.391762)*(1<<13)
	csSSE2_U_green_w.m128_i64[0] = 0xF377F377F377F377;
	csSSE2_U_green_w.m128_i64[1] = 0xF377F377F377F377;

	__m128   csSSE2_V_green_w; //short{-6660,-6660,-6660,-6660,-6660,-6660,-6660,-6660} =(-0.812968)*(1<<13)
	csSSE2_V_green_w.m128_i64[0] = 0xE5FCE5FCE5FCE5FC;
	csSSE2_V_green_w.m128_i64[1] = 0xE5FCE5FCE5FCE5FC;

	__m128   csSSE2_V_red_w; //short{13075,13075,13075,13075,13075,13075,13075,13075} =1.596027*(1<<13)
	csSSE2_V_red_w.m128_i64[0] = 0x3313331333133313;
	csSSE2_V_red_w.m128_i64[1] = 0x3313331333133313;

	__m128   mask0; //short{13075,13075,13075,13075,13075,13075,13075,13075} =1.596027*(1<<13)
	mask0.m128_i64[0] = 0x0908060504020100;
	mask0.m128_i64[1] = 0x0f0b07030e0d0c0a;

	long expand8_width=(width>>4)<<4;

	

	if (expand8_width>0)
	{
		__asm
		{
			push    eax
			push    ecx
			push    edx
			mov     ecx,expand8_width
			mov     eax,pYUYV
			mov     edx,pDstLine

loop_beign:
			movdqu        xmm0, [eax/*+ecx*2*/ ] /*mm0=V3 Y7 .... V0 Y1 U0 Y0  */                
			movdqu        xmm4,[eax/*+ecx*2*/+16] /*mm4=V7 Y15 .... V4 Y9 U4 Y8  */
			add  eax,32
			movdqa        xmm1,xmm0                                                             
			movdqa        xmm5,xmm4                                                             
			psrlw       xmm1,8              /*mm1=00 V3 00 U3 .... 00 V0 00 U0  */                
			psrlw       xmm5,8              /*mm5=00 V7 00 U7 .... 00 V4 00 U4  */                
			pand        xmm0,csSSE2_0x00FF_w /*mm0=00 Y7 .... Y2 00 Y1 00 Y0   */                
			pand        xmm4,csSSE2_0x00FF_w /*mm4=00 Y15 .... Y10 00 Y9 00 Y8  */                
			packuswb    xmm1,xmm5            /*xmm1=V7 U7 V6 U6 V5 U5 V4 U4 V3 U3 V2 U2 V1 U1 V0 U0  */                
			movdqa        xmm2,xmm1                                                             
			packuswb    xmm0,xmm4            /*mm0=Y15 .... Y8 Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  */                
			psllw       xmm1,8              /*mm1=U7 00 U2 00 U1 00 U0 00  */             
			psrlw       xmm2,8              /*mm2=00 V7 00 V2 00 V1 00 V0  */                
			psrlw       xmm1,8              /*mm1=00 U7 00 U2 00 U1 00 U0  */     

			prefetchnta [eax+48*4]  //预读


			psubusb     xmm0,csSSE2_16_b        /* mm0 : Y -= 16                       */     
			psubsw      xmm1,csSSE2_128_w       /* mm1 : u -= 128                      */      
			movdqa        xmm7,xmm0                                                              
			psubsw      xmm2,csSSE2_128_w       /* mm2 : v -= 128                      */      
			pand        xmm0,csSSE2_0x00FF_w    /* mm0 = 00 Y14 00 Y4 00 Y2 00 Y0       */      
			psllw       xmm1,3                 /* mm1 : u *= 8                        */      
			psllw       xmm2,3                 /* mm2 : v *= 8                        */      
			psrlw       xmm7,8                 /* mm7 = 00 15 00 Y5 00 Y3 00 Y1       */      
			movdqa        xmm3,xmm1                                                              
			movdqa        xmm4,xmm2                                                              
			pmulhw      xmm1,csSSE2_U_green_w   /* mm1 = u * U_green                   */      
			psllw       xmm0,3                 /* y*=8                                */      
			pmulhw      xmm2,csSSE2_V_green_w   /* mm2 = v * V_green                   */      
			psllw       xmm7,3                 /* y*=8                                */      
			pmulhw      xmm3,csSSE2_U_blue_w                                                   
			paddsw      xmm1,xmm2                                                              
			pmulhw      xmm4,csSSE2_V_red_w                                                    
			movdqa        xmm2,xmm3  

			pmulhw      xmm0,csSSE2_Y_coeff_w                                                  
			movdqa        xmm6,xmm4                                                              
			pmulhw      xmm7,csSSE2_Y_coeff_w                                                  
			movdqa        xmm5,xmm1                                                              
			paddsw      xmm3,xmm0               /* mm3 = B14 .... B0       */                  
			paddsw      xmm2,xmm7               /* mm2 = B15 .... B1       */                  
			paddsw      xmm4,xmm0               /* mm4 = R14....  R0       */                  
			paddsw      xmm6,xmm7               /* mm6 = R15 ..... R1       */                  
			paddsw      xmm1,xmm0               /* mm1 = G14....  G0       */                  
			paddsw      xmm5,xmm7               /* mm5 = G15 .... G1       */                  
			packuswb    xmm3,xmm4               /* mm3 = R14 .... R0 B14 .... B0 to [0-255] */ 
			packuswb    xmm2,xmm6               /* mm2 = R15 .... R1 B15 .... B1 to [0-255] */ 
			packuswb    xmm5,xmm1               /* mm5 = G14 .... G0 G15 .... G1 to [0-255] */ 
			movdqa        xmm4,xmm3                                                              
			punpcklbw   xmm3,xmm2               /* mm3 = B15 .... B0     */  

			punpcklqdq   xmm1,xmm5               /* mm1 = G15 G13 .... G3 G1 xx xx xx xx     */

			punpckhbw   xmm4,xmm2               /* mm4 = R15 .... R0     */        
			punpckhbw   xmm5,xmm1               /* mm5 = G15 .... G0     */                                                          
			pxor     xmm2,xmm2               /* mm2 = FF FF FF FF FF FF FF FF     */        
			movdqa        xmm0,xmm3                                                              
			movdqa        xmm7,xmm4                                                              
			punpcklbw   xmm0,xmm5             /* mm0 = G3 B3 G2 B2 G1 B1 G0 B0       */        
			punpcklbw   xmm7,xmm2             /* mm7 = FF R3 FF R2 FF R1 FF R0       */        
			movdqa        xmm1,xmm0                                                              
			movdqa        xmm6,xmm3                                                              
			punpcklwd   xmm0,xmm7             /* mm0 = FF R1 G1 B1 FF R0 G0 B0       */ 
			pshufb     xmm0, mask0
			punpckhwd   xmm1,xmm7             /* mm1 = FF R3 G3 B3 FF R2 G2 B2       */
			pshufb     xmm1, mask0
			movdqa        xmm3,xmm1    
            pshufd     xmm3,xmm3,0x3f
			por    xmm0,xmm3		
			movdqu     [edx],xmm0  
			pshufd xmm1, xmm1,0xf9
			movdqa        xmm7,xmm4                                                              
			punpckhbw   xmm6,xmm5             /* mm6 = G7 B7 G6 B6 G5 B5 G4 B4       */
			                                             
			punpckhbw   xmm7,xmm2             /* mm7 = FF R7 FF R6 FF R5 FF R4      */         
			movdqa        xmm0,xmm6                                                              
			punpcklwd   xmm6,xmm7             /* mm6 = FF R5 G5 B5 FF R4 G4 B4      */
			movdqa        xmm3,xmm6 
			pshufb     xmm3, mask0
			movdqa        xmm6,xmm3
			pshufd     xmm3,xmm3,0x4f
			por  xmm1 ,xmm3
			movdqu  [edx+16],xmm1
			pshufd  xmm6,xmm6,0xfe
			punpckhwd   xmm0,xmm7             /* mm0 = FF R7 G7 B7 FF R6 G6 B6      */
			pshufb     xmm0, mask0
			pshufd     xmm0,xmm0,0x93
			por  xmm0 ,xmm6                                           
			movdqu  [edx+32],xmm0 

			//punpckhbw   xmm4,xmm2               /* mm4 = R15 .... R0     */        
			//punpckhbw   xmm5,xmm1               /* mm5 = G15 .... G0     */                                                          
			//pcmpeqb     xmm2,xmm2               /* mm2 = FF FF FF FF FF FF FF FF     */        
			//movdqa        xmm0,xmm3                                                              
			//movdqa        xmm7,xmm4                                                              
			//punpcklbw   xmm0,xmm5             /* mm0 = G3 B3 G2 B2 G1 B1 G0 B0       */        
			//punpcklbw   xmm7,xmm2             /* mm7 = FF R3 FF R2 FF R1 FF R0       */        
			//movdqa        xmm1,xmm0                                                              
			//movdqa        xmm6,xmm3                                                              
			//punpcklwd   xmm0,xmm7             /* mm0 = FF R1 G1 B1 FF R0 G0 B0       */        
			//punpckhwd   xmm1,xmm7             /* mm1 = FF R3 G3 B3 FF R2 G2 B2       */ 
			//pshufb     xmm0, mask0
			//movdqu     [edx],xmm0                                                    
			//movdqa        xmm7,xmm4                                                              
			//punpckhbw   xmm6,xmm5             /* mm6 = G7 B7 G6 B6 G5 B5 G4 B4       */
			//pshufb     xmm1, mask0
			//movdqu  [edx+12],xmm1                                                  
			//punpckhbw   xmm7,xmm2             /* mm7 = FF R7 FF R6 FF R5 FF R4      */         
			//movdqa        xmm0,xmm6                                                              
			//punpcklwd   xmm6,xmm7             /* mm6 = FF R5 G5 B5 FF R4 G4 B4      */
			//pshufb     xmm6, mask0
			//punpckhwd   xmm0,xmm7             /* mm0 = FF R7 G7 B7 FF R6 G6 B6      */  
			//pshufb     xmm0, mask0
			//movdqu  [edx+24],xmm6                                                 
			//movdqu  [edx+36],xmm0    
			add edx,48
			sub ecx,16
			cmp ecx,0
			jnz     loop_beign
			mov     pYUYV,eax
			mov     pDstLine,edx
			pop edx
			pop ecx
			pop eax
		}
	}
	//处理边界
	DecodeYUY2ToRgb24Line(pDstLine,pYUYV,width-expand8_width);
}





typedef void (*pColorConvert)( BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch);
pColorConvert  ColorConvertFun [12] =
{
	YV12ToRGB24Space,
	YV12ToRGB32Space,
	YV12ToYUY2Space,
	YUY2ToRGB24Space,
	YUY2ToRGB32Space,
	YUY2ToYV12Space,
	RGB24ToRGB32Space,
	RGB24ToYUY2Space,
	RGB24ToYV12Space,
	RGb32ToRGB24Space,
	RGb32ToYUY2Space,
	RGb32ToYV12Space,
};
void ColorConvert (BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch,int nOrder)
{
	ColorConvertFun[nOrder](pSrc,pDst,nWidth,nHeight, SrcPitch, DstPitch);
}


void YV12ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE* pY = pSrc;
	BYTE *pV = pY +nWidth * nHeight;
	BYTE * pU = pV +nWidth/2 * nHeight/2;
	if(g_cpuType == CPU_SSE2)
	{
		YV12ToRGB24SSE2(pY, nWidth, pU,  pV, nWidth/2, 
			pDst, nWidth,   nHeight,DstPitch) ;
	}
	else
	{
		YV12ToRGB24MMX(pY, nWidth, pU,  pV, nWidth/2, 
			pDst, nWidth,   nHeight,DstPitch) ;
	}
}
void YV12ToRGB32Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE* pY = pSrc;
	BYTE *pV = pY +nWidth * nHeight;
	BYTE * pU = pV +nWidth/2 * nHeight/2;
	if(g_cpuType == CPU_SSE2)
	{
		YV12ToRGB32SSE2(pY, nWidth, pU,  pV, nWidth/2, 
			pDst, nWidth, nHeight,DstPitch) ;
	}
	else
	{
		YV12ToRGB32MMX(pY, nWidth, pU,  pV, nWidth/2, 
			pDst, nWidth, nHeight,DstPitch) ;
	}
}
void YV12ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE* pY = pSrc;
	BYTE *pV = pY +nWidth * nHeight;
	BYTE * pU = pV +nWidth/2 * nHeight/2;
	if(g_cpuType == CPU_SSE2)
	{
		YV12ToYUY2SSE2(pY, pU, pV, nWidth, nWidth, nWidth/2, 
			pDst, DstPitch , nHeight)	;
	}
	else
	{
		YV12ToYUY2MMX(pY, pU, pV, nWidth, nWidth, nWidth/2, 
			pDst, DstPitch , nHeight)	;
	}
}
void YUY2ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert((nWidth & 1)==0); 

	long YUV_byte_width= (nWidth>>1) << 2;
	long byte_width =((nWidth*32+31)/32)*4;
	BYTE* pDstLine = pDst;

	MakeYUVToRGBTable();
	int n = nHeight;
	BYTE *pTemp = new BYTE[nWidth * 2];
	for(int i = 0;i<nHeight/2;i++)
	{ 
		CopyMemory(pTemp,pSrc+i * nWidth*2,nWidth * 2);
		CopyMemory(pSrc+i * nWidth*2,pSrc+(nHeight-i-1)* nWidth*2,nWidth * 2);
		CopyMemory(pSrc+(nHeight-i-1)* nWidth*2,pTemp,nWidth * 2);
	}
	delete []pTemp;

	for (long y = 0; y < nHeight; ++y)
	{
		if(g_cpuType == CPU_SSE2)
		{
			YUY2ToRGB24SSE2Line( pDstLine,pSrc,nWidth ,nHeight);
		}
		else
		{
			YUY2ToRGB24SSE2Line( pDstLine,pSrc,nWidth, nHeight );
		}		
		pSrc += SrcPitch;
		pDstLine+= DstPitch;
	}  
	__asm sfence
	__asm emms
	
	
}
void YUY2ToRGB32Space(BYTE* pSrc,BYTE *pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert((nWidth & 1)==0); 

	long YUV_byte_width= (nWidth>>1) << 2;
	long byte_width =((nWidth*32+31)/32)*4;
	BYTE* pDstLine = pDst;

	MakeYUVToRGBTable();

	for (long y = 0; y < nHeight; ++y)
	{
		if(g_cpuType == CPU_SSE2)
		{
			YUY2ToRGB32SSE2Line( pDstLine,pSrc,nWidth );
		}
		else
		{
			YUY2ToRGB32MMXLine( pDstLine,pSrc,nWidth );
		}		
		pSrc += SrcPitch;
		pDstLine+= DstPitch;
	}  
	__asm sfence
		__asm emms
}
void YUY2ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE *pSrcTemp = pSrc;
	BYTE *pDstTemp = pDst;

	if(g_cpuType == CPU_SSE2)
	{
		YUV2ToYV12SSE2(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);
	}
	else
	{
		YUV2ToYV12MMX(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);	
	}	
}
void RGB24ToRGB32Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE *pSrcTemp = pSrc;
	BYTE *pDstTemp = pDst;

	if(g_cpuType == CPU_SSE2)
	{
		RGB24ToRGB32SSE2(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);
	}
	else
	{
		RGB24ToRGB32MMX(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);		
	}
	
}
void RGB24ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE *pSrcTemp = pSrc;
	BYTE *pDstTemp = pDst;

	if(g_cpuType == CPU_SSE2)
	{
		RGB24ToYUY2SSE2(pDstTemp,pSrc,nWidth,  nHeight,SrcPitch,DstPitch);
	}
	else
	{
		RGB24ToYUY2MMX(pDstTemp,pSrc,nWidth,  nHeight,SrcPitch,DstPitch);
	}

}
void RGB24ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	int nRenderWidth = (DstPitch<<1) / 3;
	BYTE* pY = pDst;
	BYTE *pV = pY +nRenderWidth * nHeight;
	BYTE * pU = pV +nRenderWidth/2 * nHeight/2;
	BYTE *pSrcTemp = pSrc;
	

	if(g_cpuType == CPU_SSE2)
	{
		RGB24toYV12SSE2(pSrcTemp,   pY, pU,pV, SrcPitch, nRenderWidth, 
			nWidth,  nHeight) ;
	}
	else
	{
		RGB24toYV12MMX(pSrcTemp,   pY, pU,pV,SrcPitch, nRenderWidth, 
			nWidth,  nHeight) ;
	}
}
void RGb32ToRGB24Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	BYTE *pSrcTemp = pSrc;
	BYTE *pDstTemp = pDst;

	if(g_cpuType == CPU_SSE2)
	{
		RGB32ToRGB24SSE2(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);
	}
	else
	{
		RGB32ToRGB24MMX(pSrcTemp,pDstTemp,nWidth,  nHeight,SrcPitch,DstPitch);
	}
}
void RGb32ToYUY2Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	
	BYTE *pSrcTemp = pSrc;
	BYTE *pDstTemp = pDst;

	InitRGBToYUVMMX(7);

	if(g_cpuType == CPU_SSE2)
	{
		RGB32ToYUY2SSE2(pSrcTemp,  pDst,SrcPitch, DstPitch, 
			nWidth,  nHeight);
	}
	else
	{
		RGB32ToYUY2MMX(pSrcTemp,  pDst, SrcPitch, DstPitch, 
			nWidth,  nHeight);
	}
}
void RGb32ToYV12Space(BYTE * pSrc, BYTE * pDst,const int nWidth,const int nHeight,int SrcPitch,int DstPitch)
{
	assert(pSrc);
	assert(pDst);
	int nRenderWidth = (DstPitch<<1) / 3;
	BYTE* pY = pDst;
	BYTE *pV = pY +nRenderWidth * nHeight;
	BYTE * pU = pV +nRenderWidth/2 * nHeight/2;
	BYTE *pSrcTemp = pSrc;

	InitRGBToYUVMMX(7);


	if(g_cpuType == CPU_SSE2)
	{
		//LogData("nWidth*4: %d, ",nWidth*4);
		//LogData("nRenderWidth: %d, ",nRenderWidth);
		//LogData("nWidth: %d, ",nWidth);
		//LogData("nHeight: %d\n,",nHeight);
		RGB32toYV12SSE2(pSrcTemp,   pY, pU,pV, nWidth * 4, nRenderWidth, 
			nWidth,  nHeight) ;
	}
	else
	{
		RGB32toYV12MMX(pSrcTemp,   pY, pU,pV, nWidth*4, nRenderWidth, 
			nWidth,  nHeight) ;
	}
}
