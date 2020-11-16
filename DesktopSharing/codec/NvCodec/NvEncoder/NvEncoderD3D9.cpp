/*
* Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/


#ifndef WIN32
#include <dlfcn.h>
#endif
#include "NvEncoder/NvEncoderD3D9.h"
#include <D3D9Types.h>
#include <utility>

#ifndef MAKEFOURCC
#define MAKEFOURCC(a,b,c,d) (((unsigned int)a) | (((unsigned int)b)<< 8) | (((unsigned int)c)<<16) | (((unsigned int)d)<<24) )
#endif

D3DFORMAT GetD3D9Format(NV_ENC_BUFFER_FORMAT eBufferFormat)
{
    switch (eBufferFormat)
    {
    case NV_ENC_BUFFER_FORMAT_NV12:
        return (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
    case NV_ENC_BUFFER_FORMAT_ARGB:
        return D3DFMT_A8R8G8B8;
    default:
        return D3DFMT_UNKNOWN;
    }
}

NvEncoderD3D9::NvEncoderD3D9(IDirect3DDevice9* pD3D9Device, uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat,
    IDirectXVideoAccelerationService* pDXVAService, uint32_t nExtraOutputDelay, bool bMotionEstimationOnly) :
    NvEncoder(NV_ENC_DEVICE_TYPE_DIRECTX, pD3D9Device, nWidth, nHeight, eBufferFormat, nExtraOutputDelay, bMotionEstimationOnly)
{
    if (!pD3D9Device) 
    {
        NVENC_THROW_ERROR("Bad d3d9device ptr", NV_ENC_ERR_INVALID_PTR);
    }

    if (GetD3D9Format(GetPixelFormat()) == D3DFMT_UNKNOWN)
    {
        NVENC_THROW_ERROR("Unsupported Buffer format", NV_ENC_ERR_INVALID_PARAM);
    }

    if (!m_hEncoder)
    {
        NVENC_THROW_ERROR("Encoder Initialization failed", NV_ENC_ERR_INVALID_DEVICE);
    }

    m_pD3D9Device = pD3D9Device;
    m_pD3D9Device->AddRef();

    m_pDXVAService = pDXVAService;
    if (m_pDXVAService)
    {
        m_pDXVAService->AddRef();
    }
}

NvEncoderD3D9::~NvEncoderD3D9()
{
    ReleaseD3D9Resources();
}

void NvEncoderD3D9::AllocateInputBuffers(int32_t numInputBuffers)
{
    if (!IsHWEncoderInitialized())
    {
        NVENC_THROW_ERROR("Encoder intialization failed", NV_ENC_ERR_ENCODER_NOT_INITIALIZED);
    }


    // for MEOnly mode we need to allocate seperate set of buffers for reference frame
    int numCount = m_bMotionEstimationOnly ? 2 : 1;

    for (int count = 0; count < numCount; count++)
    {
        std::vector<void*> inputFrames;
        for (int i = 0; i < numInputBuffers; i++)
        {
            IDirect3DSurface9* pD3D9Surface;
            HRESULT res = S_OK;
            if (m_pDXVAService)
            {
                res = m_pDXVAService->CreateSurface(GetMaxEncodeWidth(), GetMaxEncodeHeight(), 0, GetD3D9Format(GetPixelFormat()), D3DPOOL_DEFAULT, 0, DXVA2_VideoProcessorRenderTarget, &pD3D9Surface, nullptr);
            }
            else
            {
                res = m_pD3D9Device->CreateOffscreenPlainSurface(GetMaxEncodeWidth(), GetMaxEncodeHeight(), GetD3D9Format(GetPixelFormat()), D3DPOOL_DEFAULT, &pD3D9Surface, nullptr);
            }
            if (res != S_OK)
            {
                NVENC_THROW_ERROR("Failed to create d3d9Surfaces", NV_ENC_ERR_OUT_OF_MEMORY);
            }
            inputFrames.push_back(pD3D9Surface);
        }
        RegisterResources(inputFrames, NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX, GetMaxEncodeWidth(), GetMaxEncodeHeight(), 0, GetPixelFormat(), count == 1 ? true : false);
    }
}

void NvEncoderD3D9::ReleaseInputBuffers()
{
    ReleaseD3D9Resources();
}

void NvEncoderD3D9::ReleaseD3D9Resources()
{
    if (!m_hEncoder)
    {
        return;
    }

    UnregisterResources();

    for (uint32_t i = 0; i < m_vInputFrames.size(); ++i)
    {
        if (m_vInputFrames[i].inputPtr)
        {
            reinterpret_cast<IDirect3DSurface9*>(m_vInputFrames[i].inputPtr)->Release();
        }
    }
    m_vInputFrames.clear();

    for (uint32_t i = 0; i < m_vReferenceFrames.size(); ++i)
    {
        if (m_vReferenceFrames[i].inputPtr)
        {
            reinterpret_cast<IDirect3DSurface9*>(m_vReferenceFrames[i].inputPtr)->Release();
        }
    }
    m_vReferenceFrames.clear();

    if (m_pDXVAService)
    {
        m_pDXVAService->Release();
        m_pDXVAService = nullptr;
    }

    if (m_pD3D9Device)
    {
        m_pD3D9Device->Release();
        m_pD3D9Device = nullptr;
    }
}
