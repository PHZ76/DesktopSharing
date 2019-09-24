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

#include "NvEncoder/NvEncoderGL.h"

NvEncoderGL::NvEncoderGL(uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat,
    uint32_t nExtraOutputDelay, bool bMotionEstimationOnly) :
    NvEncoder(NV_ENC_DEVICE_TYPE_OPENGL, nullptr, nWidth, nHeight, eBufferFormat,
        nExtraOutputDelay, bMotionEstimationOnly)
{
    if (!m_hEncoder)
    {
        return;
    }
}

NvEncoderGL::~NvEncoderGL()
{
    ReleaseGLResources();
}

void NvEncoderGL::ReleaseInputBuffers()
{
    ReleaseGLResources();
}

void NvEncoderGL::AllocateInputBuffers(int32_t numInputBuffers)
{
    if (!IsHWEncoderInitialized())
    {
        NVENC_THROW_ERROR("Encoder device not initialized", NV_ENC_ERR_ENCODER_NOT_INITIALIZED);
    }
    int numCount = m_bMotionEstimationOnly ? 2 : 1;

    for (int count = 0; count < numCount; count++)
    {
        std::vector<void*> inputFrames;
        for (int i = 0; i < numInputBuffers; i++)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = new NV_ENC_INPUT_RESOURCE_OPENGL_TEX;
            uint32_t tex;

            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_RECTANGLE, tex);

            uint32_t chromaHeight = GetNumChromaPlanes(GetPixelFormat()) * GetChromaHeight(GetPixelFormat(), GetMaxEncodeHeight());
            if (GetPixelFormat() == NV_ENC_BUFFER_FORMAT_YV12 || GetPixelFormat() == NV_ENC_BUFFER_FORMAT_IYUV)
                chromaHeight = GetChromaHeight(GetPixelFormat(), GetMaxEncodeHeight());

            glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8,
                GetWidthInBytes(GetPixelFormat(), GetMaxEncodeWidth()),
                GetMaxEncodeHeight() + chromaHeight,
                0, GL_RED, GL_UNSIGNED_BYTE, NULL);

            glBindTexture(GL_TEXTURE_RECTANGLE, 0);

            pResource->texture = tex;
            pResource->target = GL_TEXTURE_RECTANGLE;
            inputFrames.push_back(pResource);
        }
        RegisterResources(inputFrames, NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX,
            GetMaxEncodeWidth(),
            GetMaxEncodeHeight(),
            GetWidthInBytes(GetPixelFormat(), GetMaxEncodeWidth()),
            GetPixelFormat(), count == 1 ? true : false);
    }
}

void NvEncoderGL::ReleaseGLResources()
{
    if (!m_hEncoder)
    {
        return;
    }

    UnregisterResources();

    for (int i = 0; i < m_vInputFrames.size(); ++i)
    {
        if (m_vInputFrames[i].inputPtr)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)m_vInputFrames[i].inputPtr;
            if (pResource)
            {
                glDeleteTextures(1, &pResource->texture);
                delete pResource;
            }
        }
    }
    m_vInputFrames.clear();

    for (int i = 0; i < m_vReferenceFrames.size(); ++i)
    {
        if (m_vReferenceFrames[i].inputPtr)
        {
            NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)m_vReferenceFrames[i].inputPtr;
            if (pResource)
            {
                glDeleteTextures(1, &pResource->texture);
                delete pResource;
            }
        }
    }
    m_vReferenceFrames.clear();
}
