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

#pragma once

#include <assert.h>
#include <stdint.h>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <string.h>
#include "nvcuvid.h"

/**
* @brief Exception class for error reporting from the decode API.
*/
class NVDECException : public std::exception
{
public:
    NVDECException(const std::string& errorStr, const CUresult errorCode)
        : m_errorString(errorStr), m_errorCode(errorCode) {}

    virtual ~NVDECException() throw() {}
    virtual const char* what() const throw() { return m_errorString.c_str(); }
    CUresult  getErrorCode() const { return m_errorCode; }
    const std::string& getErrorString() const { return m_errorString; }
    static NVDECException makeNVDECException(const std::string& errorStr, const CUresult errorCode,
        const std::string& functionName, const std::string& fileName, int lineNo);
private:
    std::string m_errorString;
    CUresult m_errorCode;
};

inline NVDECException NVDECException::makeNVDECException(const std::string& errorStr, const CUresult errorCode, const std::string& functionName,
    const std::string& fileName, int lineNo)
{
    std::ostringstream errorLog;
    errorLog << functionName << " : " << errorStr << " at " << fileName << ":" << lineNo << std::endl;
    NVDECException exception(errorLog.str(), errorCode);
    return exception;
}

#define NVDEC_THROW_ERROR( errorStr, errorCode )                                                         \
    do                                                                                                   \
    {                                                                                                    \
        throw NVDECException::makeNVDECException(errorStr, errorCode, __FUNCTION__, __FILE__, __LINE__); \
    } while (0)


#define NVDEC_API_CALL( cuvidAPI )                                                                                 \
    do                                                                                                             \
    {                                                                                                              \
        CUresult errorCode = cuvidAPI;                                                                             \
        if( errorCode != CUDA_SUCCESS)                                                                             \
        {                                                                                                          \
            std::ostringstream errorLog;                                                                           \
            errorLog << #cuvidAPI << " returned error " << errorCode;                                              \
            throw NVDECException::makeNVDECException(errorLog.str(), errorCode, __FUNCTION__, __FILE__, __LINE__); \
        }                                                                                                          \
    } while (0)

struct Rect {
    int l, t, r, b;
};

struct Dim {
    int w, h;
};

/**
* @brief Base class for decoder interface.
*/
class NvDecoder {

public:
    /**
    *  @brief This function is used to initialize the decoder session.
    *  Application must call this function to initialize the decoder, before
    *  starting to decode any frames.
    */
    NvDecoder(CUcontext cuContext, int nWidth, int nHeight, bool bUseDeviceFrame, cudaVideoCodec eCodec, std::mutex *pMutex = NULL,
        bool bLowLatency = false, bool bDeviceFramePitched = false, const Rect *pCropRect = NULL, const Dim *pResizeDim = NULL, int maxWidth = 0, int maxHeight = 0);
    ~NvDecoder();

    /**
    *  @brief  This function is used to get the current CUDA context.
    */
    CUcontext GetContext() { return m_cuContext; }

    /**
    *  @brief  This function is used to get the current decode width.
    */
    int GetWidth() { assert(m_nWidth); return m_nWidth; }

    /**
    *  @brief  This function is used to get the current decode height.
    */
    int GetHeight() { assert(m_nHeight); return m_nHeight; }

    /**
    *   @brief  This function is used to get the current frame size based on pixel format.
    */
    int GetFrameSize() { assert(m_nWidth); return m_nWidth * m_nHeight * 3 / (m_nBitDepthMinus8 ? 1 : 2); }

    /**
    *  @brief  This function is used to get the pitch of the device buffer holding the decoded frame.
    */
    int GetDeviceFramePitch() { assert(m_nWidth); return m_nDeviceFramePitch ? (int)m_nDeviceFramePitch : m_nWidth * (m_nBitDepthMinus8 ? 2 : 1); }

    /**
    *   @brief  This function is used to get the bit depth associated with the pixel format.
    */
    int GetBitDepth() { assert(m_nWidth); return m_nBitDepthMinus8 + 8; }

    /**
    *   @brief  This function is used to get information about the video stream (codec, display parameters etc)
    */
    CUVIDEOFORMAT GetVideoFormatInfo() { assert(m_nWidth); return m_videoFormat; }

    /**
    *   @brief  This function is used to print information about the video stream
    */
    std::string GetVideoInfo() const { return m_videoInfo.str(); }

    /**
    *   @brief  This function decodes a frame and returns frames that are available for display.
        The frames should be used or buffered before making subsequent calls to the Decode function again
    */
    bool Decode(const uint8_t *pData, int nSize, uint8_t ***pppFrame, int *pnFrameReturned, uint32_t flags = 0, int64_t **ppTimestamp = NULL, int64_t timestamp = 0, CUstream stream = 0);

    /**
    *   @brief  This function decodes a frame and returns the locked frame buffers
    *   This makes the buffers available for use by the application without the buffers
    *   getting overwritten, even if subsequent decode calls are made. The frame buffers
    *   remain locked, until ::UnlockFrame() is called
    */
    bool DecodeLockFrame(const uint8_t *pData, int nSize, uint8_t ***pppFrame, int *pnFrameReturned, uint32_t flags = 0, int64_t **ppTimestamp = NULL, int64_t timestamp = 0, CUstream stream = 0);

    /**
    *   @brief  This function unlocks the frame buffer and makes the frame buffers available for write again
    */
    void UnlockFrame(uint8_t **ppFrame, int nFrame);

    /**
    *   @brief  This function allow app to set decoder reconfig params
    */
    int setReconfigParams(const Rect * pCropRect, const Dim * pResizeDim);

private:
    /**
    *   @brief  Callback function to be registered for getting a callback when decoding of sequence starts
    */
    static int CUDAAPI HandleVideoSequenceProc(void *pUserData, CUVIDEOFORMAT *pVideoFormat) { return ((NvDecoder *)pUserData)->HandleVideoSequence(pVideoFormat); }

    /**
    *   @brief  Callback function to be registered for getting a callback when a decoded frame is ready to be decoded
    */
    static int CUDAAPI HandlePictureDecodeProc(void *pUserData, CUVIDPICPARAMS *pPicParams) { return ((NvDecoder *)pUserData)->HandlePictureDecode(pPicParams); }

    /**
    *   @brief  Callback function to be registered for getting a callback when a decoded frame is available for display
    */
    static int CUDAAPI HandlePictureDisplayProc(void *pUserData, CUVIDPARSERDISPINFO *pDispInfo) { return ((NvDecoder *)pUserData)->HandlePictureDisplay(pDispInfo); }

    /**
    *   @brief  This function gets called when a sequence is ready to be decoded. The function also gets called
        when there is format change
    */
    int HandleVideoSequence(CUVIDEOFORMAT *pVideoFormat);

    /**
    *   @brief  This function gets called when a picture is ready to be decoded. cuvidDecodePicture is called from this function
    *   to decode the picture
    */
    int HandlePictureDecode(CUVIDPICPARAMS *pPicParams);

    /**
    *   @brief  This function gets called after a picture is decoded and available for display. Frames are fetched and stored in 
        internal buffer
    */
    int HandlePictureDisplay(CUVIDPARSERDISPINFO *pDispInfo);

    /**
    *   @brief  This function reconfigure decoder if there is a change in sequence params.
    */
    int ReconfigureDecoder(CUVIDEOFORMAT *pVideoFormat);

private:
    CUcontext m_cuContext = NULL;
    CUvideoctxlock m_ctxLock;
    std::mutex *m_pMutex;
    CUvideoparser m_hParser = NULL;
    CUvideodecoder m_hDecoder = NULL;
    bool m_bUseDeviceFrame;
    // dimension of the output
    unsigned int m_nWidth = 0, m_nHeight = 0;
    // height of the mapped surface 
    int m_nSurfaceHeight = 0;
    int m_nSurfaceWidth = 0;
    cudaVideoCodec m_eCodec = cudaVideoCodec_NumCodecs;
    cudaVideoChromaFormat m_eChromaFormat;
    int m_nBitDepthMinus8 = 0;
    CUVIDEOFORMAT m_videoFormat = {};
    Rect m_displayRect = {};
    // stock of frames
    std::vector<uint8_t *> m_vpFrame; 
    // decoded frames for return
    std::vector<uint8_t *> m_vpFrameRet;
    // timestamps of decoded frames
    std::vector<int64_t> m_vTimestamp;
    int m_nDecodedFrame = 0, m_nDecodedFrameReturned = 0;
    int m_nDecodePicCnt = 0, m_nPicNumInDecodeOrder[32];
    bool m_bEndDecodeDone = false;
    std::mutex m_mtxVPFrame;
    int m_nFrameAlloc = 0;
    CUstream m_cuvidStream = 0;
    bool m_bDeviceFramePitched = false;
    size_t m_nDeviceFramePitch = 0;
    Rect m_cropRect = {};
    Dim m_resizeDim = {};

    std::ostringstream m_videoInfo;
    unsigned int m_nMaxWidth = 0, m_nMaxHeight = 0;
    bool m_bReconfigExternal = false;
    bool m_bReconfigExtPPChange = false;
};
