//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer11.cpp: Implements a back-end specific class for the D3D11 renderer.

#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"

#include <EGL/eglext.h>
#include <versionhelpers.h>
#include <sstream>

#include "common/tls.h"
#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Program.h"
#include "libANGLE/State.h"
#include "libANGLE/Surface.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/DisplayD3D.h"
#include "libANGLE/renderer/d3d/FramebufferD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/SurfaceD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/d3d11/Blit11.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Clear11.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"
#include "libANGLE/renderer/d3d/d3d11/Fence11.h"
#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Image11.h"
#include "libANGLE/renderer/d3d/d3d11/IndexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/PixelTransfer11.h"
#include "libANGLE/renderer/d3d/d3d11/Program11.h"
#include "libANGLE/renderer/d3d/d3d11/Query11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/ShaderExecutable11.h"
#include "libANGLE/renderer/d3d/d3d11/StreamProducerD3DTexture.h"
#include "libANGLE/renderer/d3d/d3d11/SwapChain11.h"
#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"
#include "libANGLE/renderer/d3d/d3d11/TransformFeedback11.h"
#include "libANGLE/renderer/d3d/d3d11/Trim11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/dxgi_support_table.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "third_party/trace_event/trace_event.h"

#ifdef ANGLE_ENABLE_WINDOWS_STORE
#    include "libANGLE/renderer/d3d/d3d11/winrt/NativeWindow11WinRT.h"
#else
//#    include "libANGLE/renderer/d3d/d3d11/converged/CompositorNativeWindow11.h"
#    include "libANGLE/renderer/d3d/d3d11/win32/NativeWindow11Win32.h"
#endif

// Include the D3D9 debug annotator header for use by the desktop D3D11 renderer
// because the D3D11 interface method ID3DUserDefinedAnnotation::GetStatus
// doesn't work with the Graphics Diagnostics tools in Visual Studio 2013.
#ifdef ANGLE_ENABLE_D3D9
#    include "libANGLE/renderer/d3d/d3d9/DebugAnnotator9.h"
#endif

// Enable ANGLE_SKIP_DXGI_1_2_CHECK if there is not a possibility of using cross-process
// HWNDs or the Windows 7 Platform Update (KB2670838) is expected to be installed.
#ifndef ANGLE_SKIP_DXGI_1_2_CHECK
#    define ANGLE_SKIP_DXGI_1_2_CHECK 0
#endif

namespace rx
{

namespace
{

enum
{
    MAX_TEXTURE_IMAGE_UNITS_VTF_SM4 = 16
};

enum ANGLEFeatureLevel
{
    ANGLE_FEATURE_LEVEL_INVALID,
    ANGLE_FEATURE_LEVEL_9_3,
    ANGLE_FEATURE_LEVEL_10_0,
    ANGLE_FEATURE_LEVEL_10_1,
    ANGLE_FEATURE_LEVEL_11_0,
    ANGLE_FEATURE_LEVEL_11_1,
    NUM_ANGLE_FEATURE_LEVELS
};

ANGLEFeatureLevel GetANGLEFeatureLevel(D3D_FEATURE_LEVEL d3dFeatureLevel)
{
    switch (d3dFeatureLevel)
    {
        case D3D_FEATURE_LEVEL_9_3:
            return ANGLE_FEATURE_LEVEL_9_3;
        case D3D_FEATURE_LEVEL_10_0:
            return ANGLE_FEATURE_LEVEL_10_0;
        case D3D_FEATURE_LEVEL_10_1:
            return ANGLE_FEATURE_LEVEL_10_1;
        case D3D_FEATURE_LEVEL_11_0:
            return ANGLE_FEATURE_LEVEL_11_0;
        case D3D_FEATURE_LEVEL_11_1:
            return ANGLE_FEATURE_LEVEL_11_1;
        default:
            return ANGLE_FEATURE_LEVEL_INVALID;
    }
}

void SetLineLoopIndices(GLuint *dest, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        dest[i] = static_cast<GLuint>(i);
    }
    dest[count] = 0;
}

template <typename T>
void CopyLineLoopIndices(const void *indices, GLuint *dest, size_t count)
{
    const T *srcPtr = static_cast<const T *>(indices);
    for (size_t i = 0; i < count; ++i)
    {
        dest[i] = static_cast<GLuint>(srcPtr[i]);
    }
    dest[count] = static_cast<GLuint>(srcPtr[0]);
}

void SetTriangleFanIndices(GLuint *destPtr, size_t numTris)
{
    for (size_t i = 0; i < numTris; i++)
    {
        destPtr[i * 3 + 0] = 0;
        destPtr[i * 3 + 1] = static_cast<GLuint>(i) + 1;
        destPtr[i * 3 + 2] = static_cast<GLuint>(i) + 2;
    }
}

template <typename T>
void CopyLineLoopIndicesWithRestart(const void *indices,
                                    size_t count,
                                    gl::DrawElementsType indexType,
                                    std::vector<GLuint> *bufferOut)
{
    GLuint restartIndex    = gl::GetPrimitiveRestartIndex(indexType);
    GLuint d3dRestartIndex = static_cast<GLuint>(d3d11::GetPrimitiveRestartIndex());
    const T *srcPtr        = static_cast<const T *>(indices);
    Optional<GLuint> currentLoopStart;

    bufferOut->clear();

    for (size_t indexIdx = 0; indexIdx < count; ++indexIdx)
    {
        GLuint value = static_cast<GLuint>(srcPtr[indexIdx]);

        if (value == restartIndex)
        {
            if (currentLoopStart.valid())
            {
                bufferOut->push_back(currentLoopStart.value());
                bufferOut->push_back(d3dRestartIndex);
                currentLoopStart.reset();
            }
        }
        else
        {
            bufferOut->push_back(value);
            if (!currentLoopStart.valid())
            {
                currentLoopStart = value;
            }
        }
    }

    if (currentLoopStart.valid())
    {
        bufferOut->push_back(currentLoopStart.value());
    }
}

void GetLineLoopIndices(const void *indices,
                        gl::DrawElementsType indexType,
                        GLuint count,
                        bool usePrimitiveRestartFixedIndex,
                        std::vector<GLuint> *bufferOut)
{
    if (indexType != gl::DrawElementsType::InvalidEnum && usePrimitiveRestartFixedIndex)
    {
        switch (indexType)
        {
            case gl::DrawElementsType::UnsignedByte:
                CopyLineLoopIndicesWithRestart<GLubyte>(indices, count, indexType, bufferOut);
                break;
            case gl::DrawElementsType::UnsignedShort:
                CopyLineLoopIndicesWithRestart<GLushort>(indices, count, indexType, bufferOut);
                break;
            case gl::DrawElementsType::UnsignedInt:
                CopyLineLoopIndicesWithRestart<GLuint>(indices, count, indexType, bufferOut);
                break;
            default:
                UNREACHABLE();
                break;
        }
        return;
    }

    // For non-primitive-restart draws, the index count is static.
    bufferOut->resize(static_cast<size_t>(count) + 1);

    switch (indexType)
    {
        // Non-indexed draw
        case gl::DrawElementsType::InvalidEnum:
            SetLineLoopIndices(&(*bufferOut)[0], count);
            break;
        case gl::DrawElementsType::UnsignedByte:
            CopyLineLoopIndices<GLubyte>(indices, &(*bufferOut)[0], count);
            break;
        case gl::DrawElementsType::UnsignedShort:
            CopyLineLoopIndices<GLushort>(indices, &(*bufferOut)[0], count);
            break;
        case gl::DrawElementsType::UnsignedInt:
            CopyLineLoopIndices<GLuint>(indices, &(*bufferOut)[0], count);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

template <typename T>
void CopyTriangleFanIndices(const void *indices, GLuint *destPtr, size_t numTris)
{
    const T *srcPtr = static_cast<const T *>(indices);

    for (size_t i = 0; i < numTris; i++)
    {
        destPtr[i * 3 + 0] = static_cast<GLuint>(srcPtr[0]);
        destPtr[i * 3 + 1] = static_cast<GLuint>(srcPtr[i + 1]);
        destPtr[i * 3 + 2] = static_cast<GLuint>(srcPtr[i + 2]);
    }
}

template <typename T>
void CopyTriangleFanIndicesWithRestart(const void *indices,
                                       GLuint indexCount,
                                       gl::DrawElementsType indexType,
                                       std::vector<GLuint> *bufferOut)
{
    GLuint restartIndex    = gl::GetPrimitiveRestartIndex(indexType);
    GLuint d3dRestartIndex = gl::GetPrimitiveRestartIndex(gl::DrawElementsType::UnsignedInt);
    const T *srcPtr        = static_cast<const T *>(indices);
    Optional<GLuint> vertexA;
    Optional<GLuint> vertexB;

    bufferOut->clear();

    for (size_t indexIdx = 0; indexIdx < indexCount; ++indexIdx)
    {
        GLuint value = static_cast<GLuint>(srcPtr[indexIdx]);

        if (value == restartIndex)
        {
            bufferOut->push_back(d3dRestartIndex);
            vertexA.reset();
            vertexB.reset();
        }
        else
        {
            if (!vertexA.valid())
            {
                vertexA = value;
            }
            else if (!vertexB.valid())
            {
                vertexB = value;
            }
            else
            {
                bufferOut->push_back(vertexA.value());
                bufferOut->push_back(vertexB.value());
                bufferOut->push_back(value);
                vertexB = value;
            }
        }
    }
}

void GetTriFanIndices(const void *indices,
                      gl::DrawElementsType indexType,
                      GLuint count,
                      bool usePrimitiveRestartFixedIndex,
                      std::vector<GLuint> *bufferOut)
{
    if (indexType != gl::DrawElementsType::InvalidEnum && usePrimitiveRestartFixedIndex)
    {
        switch (indexType)
        {
            case gl::DrawElementsType::UnsignedByte:
                CopyTriangleFanIndicesWithRestart<GLubyte>(indices, count, indexType, bufferOut);
                break;
            case gl::DrawElementsType::UnsignedShort:
                CopyTriangleFanIndicesWithRestart<GLushort>(indices, count, indexType, bufferOut);
                break;
            case gl::DrawElementsType::UnsignedInt:
                CopyTriangleFanIndicesWithRestart<GLuint>(indices, count, indexType, bufferOut);
                break;
            default:
                UNREACHABLE();
                break;
        }
        return;
    }

    // For non-primitive-restart draws, the index count is static.
    GLuint numTris = count - 2;
    bufferOut->resize(numTris * 3);

    switch (indexType)
    {
        // Non-indexed draw
        case gl::DrawElementsType::InvalidEnum:
            SetTriangleFanIndices(&(*bufferOut)[0], numTris);
            break;
        case gl::DrawElementsType::UnsignedByte:
            CopyTriangleFanIndices<GLubyte>(indices, &(*bufferOut)[0], numTris);
            break;
        case gl::DrawElementsType::UnsignedShort:
            CopyTriangleFanIndices<GLushort>(indices, &(*bufferOut)[0], numTris);
            break;
        case gl::DrawElementsType::UnsignedInt:
            CopyTriangleFanIndices<GLuint>(indices, &(*bufferOut)[0], numTris);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

bool IsArrayRTV(ID3D11RenderTargetView *rtv)
{
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    rtv->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY &&
        desc.Texture1DArray.ArraySize > 1)
        return true;
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY &&
        desc.Texture2DArray.ArraySize > 1)
        return true;
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY &&
        desc.Texture2DMSArray.ArraySize > 1)
        return true;
    return false;
}

GLsizei GetAdjustedInstanceCount(const ProgramD3D *program, GLsizei instanceCount)
{
    if (!program->getState().usesMultiview())
    {
        return instanceCount;
    }
    if (instanceCount == 0)
    {
        return program->getState().getNumViews();
    }
    return program->getState().getNumViews() * instanceCount;
}

const uint32_t ScratchMemoryBufferLifetime = 1000;

void PopulateFormatDeviceCaps(ID3D11Device *device,
                              DXGI_FORMAT format,
                              UINT *outSupport,
                              UINT *outMaxSamples)
{
    if (FAILED(device->CheckFormatSupport(format, outSupport)))
    {
        *outSupport = 0;
    }

    *outMaxSamples = 0;
    for (UINT sampleCount = 2; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount *= 2)
    {
        UINT qualityCount = 0;
        if (FAILED(device->CheckMultisampleQualityLevels(format, sampleCount, &qualityCount)) ||
            qualityCount == 0)
        {
            break;
        }

        *outMaxSamples = sampleCount;
    }
}

}  // anonymous namespace

Renderer11DeviceCaps::Renderer11DeviceCaps() = default;

Renderer11::Renderer11(egl::Display *display)
    : RendererD3D(display),
      mCreateDebugDevice(false),
      mStateCache(),
      mStateManager(this),
      mLastHistogramUpdateTime(
          ANGLEPlatformCurrent()->monotonicallyIncreasingTime(ANGLEPlatformCurrent())),
      mDebug(nullptr),
      mScratchMemoryBuffer(ScratchMemoryBufferLifetime),
      mAnnotator(nullptr)
{
    mLineLoopIB    = nullptr;
    mTriangleFanIB = nullptr;

    mBlit          = nullptr;
    mPixelTransfer = nullptr;

    mClear = nullptr;

    mTrim = nullptr;

    mRenderer11DeviceCaps.supportsClearView                      = false;
    mRenderer11DeviceCaps.supportsConstantBufferOffsets          = false;
    mRenderer11DeviceCaps.supportsVpRtIndexWriteFromVertexShader = false;
    mRenderer11DeviceCaps.supportsDXGI1_2                        = false;
    mRenderer11DeviceCaps.B5G6R5support                          = 0;
    mRenderer11DeviceCaps.B4G4R4A4support                        = 0;
    mRenderer11DeviceCaps.B5G5R5A1support                        = 0;

    mD3d11Module          = nullptr;
    mDxgiModule           = nullptr;
    mDCompModule          = nullptr;
    mCreatedWithDeviceEXT = false;

    mDevice         = nullptr;
    mDeviceContext  = nullptr;
    //mDeviceContext1 = nullptr;
    //mDeviceContext3 = nullptr;
    mDxgiAdapter    = nullptr;
    mDxgiFactory    = nullptr;

    ZeroMemory(&mAdapterDescription, sizeof(mAdapterDescription));

    const auto &attributes = mDisplay->getAttributeMap();

    if (mDisplay->getPlatform() == EGL_PLATFORM_ANGLE_ANGLE)
    {
        EGLint requestedMajorVersion = static_cast<EGLint>(
            attributes.get(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE));
        EGLint requestedMinorVersion = static_cast<EGLint>(
            attributes.get(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE));

        if (requestedMajorVersion == EGL_DONT_CARE || requestedMajorVersion >= 11)
        {
            if (requestedMinorVersion == EGL_DONT_CARE || requestedMinorVersion >= 1)
            {
                // This could potentially lead to failed context creation if done on a system
                // without the platform update which installs DXGI 1.2. Currently, for Chrome users
                // D3D11 contexts are only created if the platform update is available, so this
                // should not cause any issues.
                mAvailableFeatureLevels.push_back(D3D_FEATURE_LEVEL_11_1);
            }
            if (requestedMinorVersion == EGL_DONT_CARE || requestedMinorVersion >= 0)
            {
                mAvailableFeatureLevels.push_back(D3D_FEATURE_LEVEL_11_0);
            }
        }

        if (requestedMajorVersion == EGL_DONT_CARE || requestedMajorVersion >= 10)
        {
            if (requestedMinorVersion == EGL_DONT_CARE || requestedMinorVersion >= 1)
            {
                mAvailableFeatureLevels.push_back(D3D_FEATURE_LEVEL_10_1);
            }
            if (requestedMinorVersion == EGL_DONT_CARE || requestedMinorVersion >= 0)
            {
                mAvailableFeatureLevels.push_back(D3D_FEATURE_LEVEL_10_0);
            }
        }

        if (requestedMajorVersion == 9 && requestedMinorVersion == 3)
        {
            mAvailableFeatureLevels.push_back(D3D_FEATURE_LEVEL_9_3);
        }

        EGLint requestedDeviceType = static_cast<EGLint>(attributes.get(
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE));
        switch (requestedDeviceType)
        {
            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
                mRequestedDriverType = D3D_DRIVER_TYPE_HARDWARE;
                break;

            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE:
                mRequestedDriverType = D3D_DRIVER_TYPE_WARP;
                break;

            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE:
                mRequestedDriverType = D3D_DRIVER_TYPE_REFERENCE;
                break;

            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
                mRequestedDriverType = D3D_DRIVER_TYPE_NULL;
                break;

            default:
                UNREACHABLE();
        }

        mCreateDebugDevice = false;
    }
    else if (mDisplay->getPlatform() == EGL_PLATFORM_DEVICE_EXT)
    {
        ASSERT(mDisplay->getDevice() != nullptr);
        mCreatedWithDeviceEXT = true;

        // Also set EGL_PLATFORM_ANGLE_ANGLE variables, in case they're used elsewhere in ANGLE
        // mAvailableFeatureLevels defaults to empty
        mRequestedDriverType = D3D_DRIVER_TYPE_UNKNOWN;
    }

    const EGLenum presentPath = static_cast<EGLenum>(attributes.get(
        EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE, EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE));
    mPresentPathFastEnabled   = (presentPath == EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE);

// The D3D11 renderer must choose the D3D9 debug annotator because the D3D11 interface
// method ID3DUserDefinedAnnotation::GetStatus on desktop builds doesn't work with the Graphics
// Diagnostics tools in Visual Studio 2013.
// The D3D9 annotator works properly for both D3D11 and D3D9.
// Incorrect status reporting can cause ANGLE to log unnecessary debug events.
//#ifdef ANGLE_ENABLE_D3D9
    mAnnotator = new DebugAnnotator9();
/*#else
    mAnnotator = new DebugAnnotator11();
#endif*/
    ASSERT(mAnnotator);
    gl::InitializeDebugAnnotations(mAnnotator);
}

Renderer11::~Renderer11()
{
    release();
}

#ifndef __d3d11_1_h__
#    define D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET ((D3D11_MESSAGE_ID)3146081)
#endif

egl::Error Renderer11::initialize()
{
    HRESULT result = S_OK;

    ANGLE_TRY(initializeD3DDevice());

#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
#    if !ANGLE_SKIP_DXGI_1_2_CHECK
    {
        TRACE_EVENT0("gpu.angle", "Renderer11::initialize (DXGICheck)");
        // In order to create a swap chain for an HWND owned by another process, DXGI 1.2 is
        // required.
        // The easiest way to check is to query for a IDXGIDevice2.
        bool requireDXGI1_2 = false;
        HWND hwnd           = WindowFromDC(mDisplay->getNativeDisplayId());
        if (hwnd)
        {
            DWORD currentProcessId = GetCurrentProcessId();
            DWORD wndProcessId;
            GetWindowThreadProcessId(hwnd, &wndProcessId);
            requireDXGI1_2 = (currentProcessId != wndProcessId);
        }
        else
        {
            requireDXGI1_2 = true;
        }

        if (requireDXGI1_2)
        {
            IDXGIDevice2 *dxgiDevice2 = nullptr;
            result = mDevice->QueryInterface(__uuidof(IDXGIDevice2), (void **)&dxgiDevice2);
            if (FAILED(result))
            {
                return egl::EglNotInitialized(D3D11_INIT_INCOMPATIBLE_DXGI)
                       << "DXGI 1.2 required to present to HWNDs owned by another process.";
            }
            SafeRelease(dxgiDevice2);
        }
    }
#    endif
#endif

    {
        TRACE_EVENT0("gpu.angle", "Renderer11::initialize (ComQueries)");
        // Cast the DeviceContext to a DeviceContext1 and DeviceContext3.
        // This could fail on Windows 7 without the Platform Update.
        // Don't error in this case- just don't use mDeviceContext1 or mDeviceContext3.
        //mDeviceContext1 = d3d11::DynamicCastComObject<ID3D11DeviceContext1>(mDeviceContext);
        //mDeviceContext3 = d3d11::DynamicCastComObject<ID3D11DeviceContext3>(mDeviceContext);

        IDXGIDevice *dxgiDevice = nullptr;
        result = mDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);

        if (FAILED(result))
        {
            return egl::EglNotInitialized(D3D11_INIT_OTHER_ERROR) << "Could not query DXGI device.";
        }

        result = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&mDxgiAdapter);

        if (FAILED(result))
        {
            return egl::EglNotInitialized(D3D11_INIT_OTHER_ERROR)
                   << "Could not retrieve DXGI adapter";
        }

        SafeRelease(dxgiDevice);

        IDXGIAdapter2 *dxgiAdapter2 = d3d11::DynamicCastComObject<IDXGIAdapter2>(mDxgiAdapter);

        // On D3D_FEATURE_LEVEL_9_*, IDXGIAdapter::GetDesc returns "Software Adapter" for the
        // description string.
        // If DXGI1.2 is available then IDXGIAdapter2::GetDesc2 can be used to get the actual
        // hardware values.
        if (mRenderer11DeviceCaps.featureLevel <= D3D_FEATURE_LEVEL_9_3 && dxgiAdapter2 != nullptr)
        {
            DXGI_ADAPTER_DESC2 adapterDesc2 = {};
            result                          = dxgiAdapter2->GetDesc2(&adapterDesc2);
            if (SUCCEEDED(result))
            {
                // Copy the contents of the DXGI_ADAPTER_DESC2 into mAdapterDescription (a
                // DXGI_ADAPTER_DESC).
                memcpy(mAdapterDescription.Description, adapterDesc2.Description,
                       sizeof(mAdapterDescription.Description));
                mAdapterDescription.VendorId              = adapterDesc2.VendorId;
                mAdapterDescription.DeviceId              = adapterDesc2.DeviceId;
                mAdapterDescription.SubSysId              = adapterDesc2.SubSysId;
                mAdapterDescription.Revision              = adapterDesc2.Revision;
                mAdapterDescription.DedicatedVideoMemory  = adapterDesc2.DedicatedVideoMemory;
                mAdapterDescription.DedicatedSystemMemory = adapterDesc2.DedicatedSystemMemory;
                mAdapterDescription.SharedSystemMemory    = adapterDesc2.SharedSystemMemory;
                mAdapterDescription.AdapterLuid           = adapterDesc2.AdapterLuid;
            }
        }
        else
        {
            result = mDxgiAdapter->GetDesc(&mAdapterDescription);
        }

        SafeRelease(dxgiAdapter2);

        if (FAILED(result))
        {
            return egl::EglNotInitialized(D3D11_INIT_OTHER_ERROR)
                   << "Could not read DXGI adaptor description.";
        }

        memset(mDescription, 0, sizeof(mDescription));
        wcstombs(mDescription, mAdapterDescription.Description, sizeof(mDescription) - 1);

        result = mDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&mDxgiFactory);

        if (!mDxgiFactory || FAILED(result))
        {
            return egl::EglNotInitialized(D3D11_INIT_OTHER_ERROR)
                   << "Could not create DXGI factory.";
        }
    }

    // Disable some spurious D3D11 debug warnings to prevent them from flooding the output log
    if (mCreateDebugDevice)
    {
        TRACE_EVENT0("gpu.angle", "Renderer11::initialize (HideWarnings)");
        ID3D11InfoQueue *infoQueue;
        result = mDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infoQueue);

        if (SUCCEEDED(result))
        {
            D3D11_MESSAGE_ID hideMessages[] = {
                D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,

                // Robust access behaviour makes out of bounds messages safe
                D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_TOO_SMALL,
            };

            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs         = static_cast<unsigned int>(ArraySize(hideMessages));
            filter.DenyList.pIDList        = hideMessages;

            infoQueue->AddStorageFilterEntries(&filter);
            SafeRelease(infoQueue);
        }
    }

#if !defined(NDEBUG)
    mDebug = d3d11::DynamicCastComObject<ID3D11Debug>(mDevice);
#endif

    ANGLE_TRY(initializeDevice());

    return egl::NoError();
}

HRESULT Renderer11::callD3D11CreateDevice(PFN_D3D11_CREATE_DEVICE createDevice, bool debug)
{
    return createDevice(
        nullptr, mRequestedDriverType, nullptr, debug ? D3D11_CREATE_DEVICE_DEBUG : 0,
        mAvailableFeatureLevels.data(), static_cast<unsigned int>(mAvailableFeatureLevels.size()),
        D3D11_SDK_VERSION, &mDevice, &(mRenderer11DeviceCaps.featureLevel), &mDeviceContext);
}

egl::Error Renderer11::initializeD3DDevice()
{
    HRESULT result = S_OK;

    if (!mCreatedWithDeviceEXT)
    {
#if !defined(ANGLE_ENABLE_WINDOWS_STORE)
        PFN_D3D11_CREATE_DEVICE D3D11CreateDevice = nullptr;
        {
            SCOPED_ANGLE_HISTOGRAM_TIMER("GPU.ANGLE.Renderer11InitializeDLLsMS");
            TRACE_EVENT0("gpu.angle", "Renderer11::initialize (Load DLLs)");
            mDxgiModule  = LoadLibrary(TEXT("dxgi.dll"));
            mD3d11Module = LoadLibrary(TEXT("d3d11.dll"));
            mDCompModule = LoadLibrary(TEXT("dcomp.dll"));

            if (mD3d11Module == nullptr || mDxgiModule == nullptr)
            {
                return egl::EglNotInitialized(D3D11_INIT_MISSING_DEP)
                       << "Could not load D3D11 or DXGI library.";
            }

            // create the D3D11 device
            ASSERT(mDevice == nullptr);
            D3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
                GetProcAddress(mD3d11Module, "D3D11CreateDevice"));

            if (D3D11CreateDevice == nullptr)
            {
                return egl::EglNotInitialized(D3D11_INIT_MISSING_DEP)
                       << "Could not retrieve D3D11CreateDevice address.";
            }
        }
#endif

        if (mCreateDebugDevice)
        {
            TRACE_EVENT0("gpu.angle", "D3D11CreateDevice (Debug)");
            result = callD3D11CreateDevice(D3D11CreateDevice, true);

            if (result == E_INVALIDARG && mAvailableFeatureLevels.size() > 1u &&
                mAvailableFeatureLevels[0] == D3D_FEATURE_LEVEL_11_1)
            {
                // On older Windows platforms, D3D11.1 is not supported which returns E_INVALIDARG.
                // Try again without passing D3D_FEATURE_LEVEL_11_1 in case we have other feature
                // levels to fall back on.
                mAvailableFeatureLevels.erase(mAvailableFeatureLevels.begin());
                result = callD3D11CreateDevice(D3D11CreateDevice, true);
            }

            if (!mDevice || FAILED(result))
            {
                WARN() << "Failed creating Debug D3D11 device - falling back to release runtime.";
            }
        }

        if (!mDevice || FAILED(result))
        {
            SCOPED_ANGLE_HISTOGRAM_TIMER("GPU.ANGLE.D3D11CreateDeviceMS");
            TRACE_EVENT0("gpu.angle", "D3D11CreateDevice");

            result = callD3D11CreateDevice(D3D11CreateDevice, false);

            if (result == E_INVALIDARG && mAvailableFeatureLevels.size() > 1u &&
                mAvailableFeatureLevels[0] == D3D_FEATURE_LEVEL_11_1)
            {
                // On older Windows platforms, D3D11.1 is not supported which returns E_INVALIDARG.
                // Try again without passing D3D_FEATURE_LEVEL_11_1 in case we have other feature
                // levels to fall back on.
                mAvailableFeatureLevels.erase(mAvailableFeatureLevels.begin());
                result = callD3D11CreateDevice(D3D11CreateDevice, false);
            }

            // Cleanup done by destructor
            if (!mDevice || FAILED(result))
            {
                ANGLE_HISTOGRAM_SPARSE_SLOWLY("GPU.ANGLE.D3D11CreateDeviceError",
                                              static_cast<int>(result));
                return egl::EglNotInitialized(D3D11_INIT_CREATEDEVICE_ERROR)
                       << "Could not create D3D11 device.";
            }
        }
    }
    else
    {
        DeviceD3D *deviceD3D = GetImplAs<DeviceD3D>(mDisplay->getDevice());
        ASSERT(deviceD3D != nullptr);

        // We should use the inputted D3D11 device instead
        void *device = nullptr;
        ANGLE_TRY(deviceD3D->getDevice(&device));

        ID3D11Device *d3dDevice = static_cast<ID3D11Device *>(device);
        if (FAILED(d3dDevice->GetDeviceRemovedReason()))
        {
            return egl::EglNotInitialized() << "Inputted D3D11 device has been lost.";
        }

        if (d3dDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_9_3)
        {
            return egl::EglNotInitialized()
                   << "Inputted D3D11 device must be Feature Level 9_3 or greater.";
        }

        // The Renderer11 adds a ref to the inputted D3D11 device, like D3D11CreateDevice does.
        mDevice = d3dDevice;
        mDevice->AddRef();
        mDevice->GetImmediateContext(&mDeviceContext);
        mRenderer11DeviceCaps.featureLevel = mDevice->GetFeatureLevel();
    }

    mResourceManager11.setAllocationsInitialized(mCreateDebugDevice);

    d3d11::SetDebugName(mDeviceContext, "DeviceContext");

    return egl::NoError();
}

// do any one-time device initialization
// NOTE: this is also needed after a device lost/reset
// to reset the scene status and ensure the default states are reset.
egl::Error Renderer11::initializeDevice()
{
    SCOPED_ANGLE_HISTOGRAM_TIMER("GPU.ANGLE.Renderer11InitializeDeviceMS");
    TRACE_EVENT0("gpu.angle", "Renderer11::initializeDevice");

    populateRenderer11DeviceCaps();

    mStateCache.clear();

    ASSERT(!mBlit);
    mBlit = new Blit11(this);

    ASSERT(!mClear);
    mClear = new Clear11(this);

    const auto &attributes = mDisplay->getAttributeMap();
    // If automatic trim is enabled, DXGIDevice3::Trim( ) is called for the application
    // automatically when an application is suspended by the OS. This feature is currently
    // only supported for Windows Store applications.
    EGLint enableAutoTrim = static_cast<EGLint>(
        attributes.get(EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_FALSE));

    if (enableAutoTrim == EGL_TRUE)
    {
        ASSERT(!mTrim);
        mTrim = new Trim11(this);
    }

    ASSERT(!mPixelTransfer);
    mPixelTransfer = new PixelTransfer11(this);

    // Gather stats on DXGI and D3D feature level
    ANGLE_HISTOGRAM_BOOLEAN("GPU.ANGLE.SupportsDXGI1_2", mRenderer11DeviceCaps.supportsDXGI1_2);

    ANGLEFeatureLevel angleFeatureLevel = GetANGLEFeatureLevel(mRenderer11DeviceCaps.featureLevel);

    // We don't actually request a 11_1 device, because of complications with the platform
    // update. Instead we check if the mDeviceContext1 pointer cast succeeded.
    // Note: we should support D3D11_0 always, but we aren't guaranteed to be at FL11_0
    // because the app can specify a lower version (such as 9_3) on Display creation.
    /*if (mDeviceContext1 != nullptr)
    {
        angleFeatureLevel = ANGLE_FEATURE_LEVEL_11_1;
    }*/

    ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.D3D11FeatureLevel", angleFeatureLevel,
                                NUM_ANGLE_FEATURE_LEVELS);

    return egl::NoError();
}

void Renderer11::populateRenderer11DeviceCaps()
{
    HRESULT hr = S_OK;

    LARGE_INTEGER version;
    hr = mDxgiAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version);
    if (FAILED(hr))
    {
        mRenderer11DeviceCaps.driverVersion.reset();
        ERR() << "Error querying driver version from DXGI Adapter.";
    }
    else
    {
        mRenderer11DeviceCaps.driverVersion = version;
    }

    /*if (mDeviceContext1)
    {
        D3D11_FEATURE_DATA_D3D11_OPTIONS d3d11Options;
        HRESULT result = mDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &d3d11Options,
                                                      sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS));
        if (SUCCEEDED(result))
        {
            mRenderer11DeviceCaps.supportsClearView = (d3d11Options.ClearView != FALSE);
            mRenderer11DeviceCaps.supportsConstantBufferOffsets =
                (d3d11Options.ConstantBufferOffsetting != FALSE);
        }
    }

    if (mDeviceContext3)
    {
        D3D11_FEATURE_DATA_D3D11_OPTIONS3 d3d11Options3;
        HRESULT result = mDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &d3d11Options3,
                                                      sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS3));
        if (SUCCEEDED(result))
        {
            mRenderer11DeviceCaps.supportsVpRtIndexWriteFromVertexShader =
                (d3d11Options3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer == TRUE);
        }
    }*/

    mRenderer11DeviceCaps.supportsMultisampledDepthStencilSRVs =
        mRenderer11DeviceCaps.featureLevel > D3D_FEATURE_LEVEL_10_0;

    if (getWorkarounds().disableB5G6R5Support)
    {
        mRenderer11DeviceCaps.B5G6R5support    = 0;
        mRenderer11DeviceCaps.B5G6R5maxSamples = 0;
    }
    else
    {
        PopulateFormatDeviceCaps(mDevice, DXGI_FORMAT_B5G6R5_UNORM,
                                 &mRenderer11DeviceCaps.B5G6R5support,
                                 &mRenderer11DeviceCaps.B5G6R5maxSamples);
    }

    PopulateFormatDeviceCaps(mDevice, DXGI_FORMAT_B4G4R4A4_UNORM,
                             &mRenderer11DeviceCaps.B4G4R4A4support,
                             &mRenderer11DeviceCaps.B4G4R4A4maxSamples);
    PopulateFormatDeviceCaps(mDevice, DXGI_FORMAT_B5G5R5A1_UNORM,
                             &mRenderer11DeviceCaps.B5G5R5A1support,
                             &mRenderer11DeviceCaps.B5G5R5A1maxSamples);

    IDXGIAdapter2 *dxgiAdapter2 = d3d11::DynamicCastComObject<IDXGIAdapter2>(mDxgiAdapter);
    mRenderer11DeviceCaps.supportsDXGI1_2 = (dxgiAdapter2 != nullptr);
    SafeRelease(dxgiAdapter2);
}

gl::SupportedSampleSet Renderer11::generateSampleSetForEGLConfig(
    const gl::TextureCaps &colorBufferFormatCaps,
    const gl::TextureCaps &depthStencilBufferFormatCaps) const
{
    gl::SupportedSampleSet sampleCounts;

    // Generate a new set from the set intersection of sample counts between the color and depth
    // format caps.
    std::set_intersection(colorBufferFormatCaps.sampleCounts.begin(),
                          colorBufferFormatCaps.sampleCounts.end(),
                          depthStencilBufferFormatCaps.sampleCounts.begin(),
                          depthStencilBufferFormatCaps.sampleCounts.end(),
                          std::inserter(sampleCounts, sampleCounts.begin()));

    // Format of GL_NONE results in no supported sample counts.
    // Add back the color sample counts to the supported sample set.
    if (depthStencilBufferFormatCaps.sampleCounts.empty())
    {
        sampleCounts = colorBufferFormatCaps.sampleCounts;
    }
    else if (colorBufferFormatCaps.sampleCounts.empty())
    {
        // Likewise, add back the depth sample counts to the supported sample set.
        sampleCounts = depthStencilBufferFormatCaps.sampleCounts;
    }

    // Always support 0 samples
    sampleCounts.insert(0);

    return sampleCounts;
}

egl::ConfigSet Renderer11::generateConfigs()
{
    std::vector<GLenum> colorBufferFormats;

    // 32-bit supported formats
    colorBufferFormats.push_back(GL_BGRA8_EXT);
    colorBufferFormats.push_back(GL_RGBA8_OES);

    // 24-bit supported formats
    colorBufferFormats.push_back(GL_RGB8_OES);

    if (mRenderer11DeviceCaps.featureLevel >= D3D_FEATURE_LEVEL_10_0)
    {
        // Additional high bit depth formats added in D3D 10.0
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb173064.aspx
        colorBufferFormats.push_back(GL_RGBA16F);
        colorBufferFormats.push_back(GL_RGB10_A2);
    }

    if (!mPresentPathFastEnabled)
    {
        // 16-bit supported formats
        // These aren't valid D3D11 swapchain formats, so don't expose them as configs
        // if present path fast is active
        colorBufferFormats.push_back(GL_RGBA4);
        colorBufferFormats.push_back(GL_RGB5_A1);
        colorBufferFormats.push_back(GL_RGB565);
    }

    static const GLenum depthStencilBufferFormats[] = {
        GL_NONE,           GL_DEPTH24_STENCIL8_OES, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16,
        GL_STENCIL_INDEX8,
    };

    const gl::Caps &rendererCaps                  = getNativeCaps();
    const gl::TextureCapsMap &rendererTextureCaps = getNativeTextureCaps();

    const EGLint optimalSurfaceOrientation =
        mPresentPathFastEnabled ? 0 : EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE;

    egl::ConfigSet configs;
    for (GLenum colorBufferInternalFormat : colorBufferFormats)
    {
        const gl::TextureCaps &colorBufferFormatCaps =
            rendererTextureCaps.get(colorBufferInternalFormat);
        if (!colorBufferFormatCaps.renderbuffer)
        {
            ASSERT(!colorBufferFormatCaps.textureAttachment);
            continue;
        }

        for (GLenum depthStencilBufferInternalFormat : depthStencilBufferFormats)
        {
            const gl::TextureCaps &depthStencilBufferFormatCaps =
                rendererTextureCaps.get(depthStencilBufferInternalFormat);
            if (!depthStencilBufferFormatCaps.renderbuffer &&
                depthStencilBufferInternalFormat != GL_NONE)
            {
                ASSERT(!depthStencilBufferFormatCaps.textureAttachment);
                continue;
            }

            const gl::InternalFormat &colorBufferFormatInfo =
                gl::GetSizedInternalFormatInfo(colorBufferInternalFormat);
            const gl::InternalFormat &depthStencilBufferFormatInfo =
                gl::GetSizedInternalFormatInfo(depthStencilBufferInternalFormat);
            const gl::Version &maxVersion = getMaxSupportedESVersion();

            const gl::SupportedSampleSet sampleCounts =
                generateSampleSetForEGLConfig(colorBufferFormatCaps, depthStencilBufferFormatCaps);

            for (GLuint sampleCount : sampleCounts)
            {
                egl::Config config;
                config.renderTargetFormat = colorBufferInternalFormat;
                config.depthStencilFormat = depthStencilBufferInternalFormat;
                config.bufferSize         = colorBufferFormatInfo.pixelBytes * 8;
                config.redSize            = colorBufferFormatInfo.redBits;
                config.greenSize          = colorBufferFormatInfo.greenBits;
                config.blueSize           = colorBufferFormatInfo.blueBits;
                config.luminanceSize      = colorBufferFormatInfo.luminanceBits;
                config.alphaSize          = colorBufferFormatInfo.alphaBits;
                config.alphaMaskSize      = 0;
                config.bindToTextureRGB =
                    ((colorBufferFormatInfo.format == GL_RGB) && (sampleCount <= 1));
                config.bindToTextureRGBA = (((colorBufferFormatInfo.format == GL_RGBA) ||
                                             (colorBufferFormatInfo.format == GL_BGRA_EXT)) &&
                                            (sampleCount <= 1));
                config.colorBufferType   = EGL_RGB_BUFFER;
                config.configCaveat      = EGL_NONE;
                config.configID          = static_cast<EGLint>(configs.size() + 1);

                // PresentPathFast may not be conformant
                config.conformant = 0;
                if (!mPresentPathFastEnabled)
                {
                    // Can only support a conformant ES2 with feature level greater than 10.0.
                    if (mRenderer11DeviceCaps.featureLevel >= D3D_FEATURE_LEVEL_10_0)
                    {
                        config.conformant |= EGL_OPENGL_ES2_BIT;
                    }

                    // We can only support conformant ES3 on FL 10.1+
                    if (maxVersion.major >= 3)
                    {
                        config.conformant |= EGL_OPENGL_ES3_BIT_KHR;
                    }
                }

                config.depthSize         = depthStencilBufferFormatInfo.depthBits;
                config.level             = 0;
                config.matchNativePixmap = EGL_NONE;
                config.maxPBufferWidth   = rendererCaps.max2DTextureSize;
                config.maxPBufferHeight  = rendererCaps.max2DTextureSize;
                config.maxPBufferPixels =
                    rendererCaps.max2DTextureSize * rendererCaps.max2DTextureSize;
                config.maxSwapInterval  = 4;
                config.minSwapInterval  = 0;
                config.nativeRenderable = EGL_FALSE;
                config.nativeVisualID   = 0;
                config.nativeVisualType = EGL_NONE;

                // Can't support ES3 at all without feature level 10.1
                config.renderableType = EGL_OPENGL_ES2_BIT;
                if (maxVersion.major >= 3)
                {
                    config.renderableType |= EGL_OPENGL_ES3_BIT_KHR;
                }

                config.sampleBuffers = (sampleCount == 0) ? 0 : 1;
                config.samples       = sampleCount;
                config.stencilSize   = depthStencilBufferFormatInfo.stencilBits;
                config.surfaceType =
                    EGL_PBUFFER_BIT | EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
                config.transparentType       = EGL_NONE;
                config.transparentRedValue   = 0;
                config.transparentGreenValue = 0;
                config.transparentBlueValue  = 0;
                config.optimalOrientation    = optimalSurfaceOrientation;
                config.colorComponentType    = gl_egl::GLComponentTypeToEGLColorComponentType(
                    colorBufferFormatInfo.componentType);

                configs.add(config);
            }
        }
    }

    ASSERT(configs.size() > 0);
    return configs;
}

void Renderer11::generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const
{
    outExtensions->createContextRobustness = true;

    if (getShareHandleSupport())
    {
        outExtensions->d3dShareHandleClientBuffer     = true;
        outExtensions->surfaceD3DTexture2DShareHandle = true;
    }
    outExtensions->d3dTextureClientBuffer = true;

    outExtensions->keyedMutex          = true;
    outExtensions->querySurfacePointer = true;
    outExtensions->windowFixedSize     = true;

    // If present path fast is active then the surface orientation extension isn't supported
    outExtensions->surfaceOrientation = !mPresentPathFastEnabled;

    // D3D11 does not support present with dirty rectangles until DXGI 1.2.
    outExtensions->postSubBuffer = mRenderer11DeviceCaps.supportsDXGI1_2;

    outExtensions->deviceQuery = true;

    outExtensions->image                 = true;
    outExtensions->imageBase             = true;
    outExtensions->glTexture2DImage      = true;
    outExtensions->glTextureCubemapImage = true;
    outExtensions->glRenderbufferImage   = true;

    outExtensions->stream                     = true;
    outExtensions->streamConsumerGLTexture    = true;
    outExtensions->streamConsumerGLTextureYUV = true;
    outExtensions->streamProducerD3DTexture   = true;

    outExtensions->flexibleSurfaceCompatibility = true;
    outExtensions->directComposition            = !!mDCompModule;

    // Contexts are virtualized so textures can be shared globally
    outExtensions->displayTextureShareGroup = true;

    // getSyncValues requires direct composition.
    outExtensions->getSyncValues = outExtensions->directComposition;

    // D3D11 can be used without a swap chain
    outExtensions->surfacelessContext = true;

    // All D3D feature levels support robust resource init
    outExtensions->robustResourceInitialization = true;

    // Compositor Native Window capabilies require WinVer >= 1803
    /*if (CompositorNativeWindow11::IsSupportedWinRelease())
    {
        outExtensions->windowsUIComposition = true;
    }*/
}

angle::Result Renderer11::flush(Context11 *context11)
{
    mDeviceContext->Flush();
    return angle::Result::Continue;
}

angle::Result Renderer11::finish(Context11 *context11)
{
    if (!mSyncQuery.valid())
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query     = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;

        ANGLE_TRY(allocateResource(context11, queryDesc, &mSyncQuery));
    }

    mDeviceContext->End(mSyncQuery.get());

    HRESULT result       = S_OK;
    unsigned int attempt = 0;
    do
    {
        unsigned int flushFrequency = 100;
        UINT flags = (attempt % flushFrequency == 0) ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH;
        attempt++;

        result = mDeviceContext->GetData(mSyncQuery.get(), nullptr, 0, flags);
        ANGLE_TRY_HR(context11, result, "Failed to get event query data");

        if (result == S_FALSE)
        {
            // Keep polling, but allow other threads to do something useful first
            ScheduleYield();
        }

        // Attempt is incremented before checking if we should test for device loss so that device
        // loss is not checked on the first iteration
        bool checkDeviceLost = (attempt % kPollingD3DDeviceLostCheckFrequency) == 0;
        if (checkDeviceLost && testDeviceLost())
        {
            mDisplay->notifyDeviceLost();
            ANGLE_CHECK(context11, false, "Device was lost while waiting for sync.",
                        GL_OUT_OF_MEMORY);
        }
    } while (result == S_FALSE);

    return angle::Result::Continue;
}

bool Renderer11::isValidNativeWindow(EGLNativeWindowType window) const
{
    /*static_assert(sizeof(ABI::Windows::UI::Composition::SpriteVisual *) == sizeof(HWND),
                  "Pointer size must match Window Handle size");*/

#ifdef ANGLE_ENABLE_WINDOWS_STORE
    return NativeWindow11WinRT::IsValidNativeWindow(window);
#else
    if (NativeWindow11Win32::IsValidNativeWindow(window))
    {
        return true;
    }

    return false;//CompositorNativeWindow11::IsValidNativeWindow(window);
#endif
}

NativeWindowD3D *Renderer11::createNativeWindow(EGLNativeWindowType window,
                                                const egl::Config *config,
                                                const egl::AttributeMap &attribs) const
{
    auto useWinUiComp = window != nullptr && !NativeWindow11Win32::IsValidNativeWindow(window);

    /*if (useWinUiComp)
    {
        return new CompositorNativeWindow11(window, config->alphaSize > 0);
    }
    else*/
    {
#ifdef ANGLE_ENABLE_WINDOWS_STORE
        UNUSED_VARIABLE(attribs);
        return new NativeWindow11WinRT(window, config->alphaSize > 0);
#else
        return new NativeWindow11Win32(
            window, config->alphaSize > 0,
            attribs.get(EGL_DIRECT_COMPOSITION_ANGLE, EGL_FALSE) == EGL_TRUE);
#endif
    }
}

egl::Error Renderer11::getD3DTextureInfo(const egl::Config *configuration,
                                         IUnknown *d3dTexture,
                                         EGLint *width,
                                         EGLint *height,
                                         const angle::Format **angleFormat) const
{
    ID3D11Texture2D *texture = d3d11::DynamicCastComObject<ID3D11Texture2D>(d3dTexture);
    if (texture == nullptr)
    {
        return egl::EglBadParameter() << "client buffer is not a ID3D11Texture2D";
    }

    ID3D11Device *textureDevice = nullptr;
    texture->GetDevice(&textureDevice);
    if (textureDevice != mDevice)
    {
        SafeRelease(texture);
        return egl::EglBadParameter() << "Texture's device does not match.";
    }
    SafeRelease(textureDevice);

    D3D11_TEXTURE2D_DESC desc = {0};
    texture->GetDesc(&desc);
    SafeRelease(texture);

    if (width)
    {
        *width = static_cast<EGLint>(desc.Width);
    }
    if (height)
    {
        *height = static_cast<EGLint>(desc.Height);
    }
    if (static_cast<EGLint>(desc.SampleDesc.Count) != configuration->samples)
    {
        // Both the texture and EGL config sample count may not be the same when multi-sampling
        // is disabled. The EGL sample count can be 0 but a D3D texture is always 1. Therefore,
        // we must only check for a invalid match when the EGL config is non-zero or the texture is
        // not one.
        if (configuration->samples != 0 || desc.SampleDesc.Count != 1)
        {
            return egl::EglBadParameter() << "Texture's sample count does not match.";
        }
    }
    // From table egl.restrictions in EGL_ANGLE_d3d_texture_client_buffer.
    switch (desc.Format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            break;

        default:
            return egl::EglBadParameter()
                   << "Unknown client buffer texture format: " << desc.Format;
    }

    if (angleFormat)
    {
        *angleFormat = &d3d11_angle::GetFormat(desc.Format);
    }

    return egl::NoError();
}

egl::Error Renderer11::validateShareHandle(const egl::Config *config,
                                           HANDLE shareHandle,
                                           const egl::AttributeMap &attribs) const
{
    if (shareHandle == nullptr)
    {
        return egl::EglBadParameter() << "NULL share handle.";
    }

    ID3D11Resource *tempResource11 = nullptr;
    HRESULT result = mDevice->OpenSharedResource(shareHandle, __uuidof(ID3D11Resource),
                                                 (void **)&tempResource11);
    if (FAILED(result))
    {
        return egl::EglBadParameter() << "Failed to open share handle, " << gl::FmtHR(result);
    }

    ID3D11Texture2D *texture2D = d3d11::DynamicCastComObject<ID3D11Texture2D>(tempResource11);
    SafeRelease(tempResource11);

    if (texture2D == nullptr)
    {
        return egl::EglBadParameter()
               << "Failed to query ID3D11Texture2D object from share handle.";
    }

    D3D11_TEXTURE2D_DESC desc = {0};
    texture2D->GetDesc(&desc);
    SafeRelease(texture2D);

    EGLint width  = attribs.getAsInt(EGL_WIDTH, 0);
    EGLint height = attribs.getAsInt(EGL_HEIGHT, 0);
    ASSERT(width != 0 && height != 0);

    const d3d11::Format &backbufferFormatInfo =
        d3d11::Format::Get(config->renderTargetFormat, getRenderer11DeviceCaps());

    if (desc.Width != static_cast<UINT>(width) || desc.Height != static_cast<UINT>(height) ||
        desc.Format != backbufferFormatInfo.texFormat || desc.MipLevels != 1 || desc.ArraySize != 1)
    {
        return egl::EglBadParameter() << "Invalid texture parameters in share handle texture.";
    }

    return egl::NoError();
}

SwapChainD3D *Renderer11::createSwapChain(NativeWindowD3D *nativeWindow,
                                          HANDLE shareHandle,
                                          IUnknown *d3dTexture,
                                          GLenum backBufferFormat,
                                          GLenum depthBufferFormat,
                                          EGLint orientation,
                                          EGLint samples)
{
    return new SwapChain11(this, GetAs<NativeWindow11>(nativeWindow), shareHandle, d3dTexture,
                           backBufferFormat, depthBufferFormat, orientation, samples);
}

void *Renderer11::getD3DDevice()
{
    return mDevice;
}

angle::Result Renderer11::drawWithGeometryShaderAndTransformFeedback(Context11 *context11,
                                                                     gl::PrimitiveMode mode,
                                                                     UINT instanceCount,
                                                                     UINT vertexCount)
{
    const gl::State &glState = context11->getState();
    ProgramD3D *programD3D   = mStateManager.getProgramD3D();

    // Since we use a geometry if-and-only-if we rewrite vertex streams, transform feedback
    // won't get the correct output. To work around this, draw with *only* the stream out
    // first (no pixel shader) to feed the stream out buffers and then draw again with the
    // geometry shader + pixel shader to rasterize the primitives.
    mStateManager.setPixelShader(nullptr);

    if (instanceCount > 0)
    {
        mDeviceContext->DrawInstanced(vertexCount, instanceCount, 0, 0);
    }
    else
    {
        mDeviceContext->Draw(vertexCount, 0);
    }

    rx::ShaderExecutableD3D *pixelExe = nullptr;
    ANGLE_TRY(programD3D->getPixelExecutableForCachedOutputLayout(context11, &pixelExe, nullptr));

    // Skip the draw call if rasterizer discard is enabled (or no fragment shader).
    if (!pixelExe || glState.getRasterizerState().rasterizerDiscard)
    {
        return angle::Result::Continue;
    }

    mStateManager.setPixelShader(&GetAs<ShaderExecutable11>(pixelExe)->getPixelShader());

    // Retrieve the geometry shader.
    rx::ShaderExecutableD3D *geometryExe = nullptr;
    ANGLE_TRY(programD3D->getGeometryExecutableForPrimitiveType(context11, glState, mode,
                                                                &geometryExe, nullptr));

    mStateManager.setGeometryShader(&GetAs<ShaderExecutable11>(geometryExe)->getGeometryShader());

    if (instanceCount > 0)
    {
        mDeviceContext->DrawInstanced(vertexCount, instanceCount, 0, 0);
    }
    else
    {
        mDeviceContext->Draw(vertexCount, 0);
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::drawArrays(const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     GLint firstVertex,
                                     GLsizei vertexCount,
                                     GLsizei instanceCount)
{
    if (mStateManager.getCullEverything())
    {
        return angle::Result::Continue;
    }

    ProgramD3D *programD3D        = mStateManager.getProgramD3D();
    GLsizei adjustedInstanceCount = GetAdjustedInstanceCount(programD3D, instanceCount);

    // Note: vertex indexes can be arbitrarily large.
    UINT clampedVertexCount = gl::GetClampedVertexCount<UINT>(vertexCount);

    const auto &glState = context->getState();
    if (glState.getCurrentTransformFeedback() && glState.isTransformFeedbackActiveUnpaused())
    {
        ANGLE_TRY(markTransformFeedbackUsage(context));

        if (programD3D->usesGeometryShader(glState, mode))
        {
            return drawWithGeometryShaderAndTransformFeedback(
                GetImplAs<Context11>(context), mode, adjustedInstanceCount, clampedVertexCount);
        }
    }

    switch (mode)
    {
        case gl::PrimitiveMode::LineLoop:
            return drawLineLoop(context, clampedVertexCount, gl::DrawElementsType::InvalidEnum,
                                nullptr, 0, adjustedInstanceCount);
        case gl::PrimitiveMode::TriangleFan:
            return drawTriangleFan(context, clampedVertexCount, gl::DrawElementsType::InvalidEnum,
                                   nullptr, 0, adjustedInstanceCount);
        case gl::PrimitiveMode::Points:
            if (getWorkarounds().useInstancedPointSpriteEmulation)
            {
                // This code should not be reachable by multi-view programs.
                ASSERT(programD3D->getState().usesMultiview() == false);

                // If the shader is writing to gl_PointSize, then pointsprites are being rendered.
                // Emulating instanced point sprites for FL9_3 requires the topology to be
                // D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST and DrawIndexedInstanced is called instead.
                if (adjustedInstanceCount == 0)
                {
                    mDeviceContext->DrawIndexedInstanced(6, clampedVertexCount, 0, 0, 0);
                    return angle::Result::Continue;
                }

                // If pointsprite emulation is used with glDrawArraysInstanced then we need to take
                // a less efficent code path. Instanced rendering of emulated pointsprites requires
                // a loop to draw each batch of points. An offset into the instanced data buffer is
                // calculated and applied on each iteration to ensure all instances are rendered
                // correctly. Each instance being rendered requires the inputlayout cache to reapply
                // buffers and offsets.
                for (GLsizei i = 0; i < instanceCount; i++)
                {
                    ANGLE_TRY(mStateManager.updateVertexOffsetsForPointSpritesEmulation(
                        context, firstVertex, i));
                    mDeviceContext->DrawIndexedInstanced(6, clampedVertexCount, 0, 0, 0);
                }

                // This required by updateVertexOffsets... above but is outside of the loop for
                // speed.
                mStateManager.invalidateVertexBuffer();
                return angle::Result::Continue;
            }
            break;
        default:
            break;
    }

    // "Normal" draw case.
    if (adjustedInstanceCount == 0)
    {
        mDeviceContext->Draw(clampedVertexCount, 0);
    }
    else
    {
        mDeviceContext->DrawInstanced(clampedVertexCount, adjustedInstanceCount, 0, 0);
    }
    return angle::Result::Continue;
}

angle::Result Renderer11::drawElements(const gl::Context *context,
                                       gl::PrimitiveMode mode,
                                       GLint startVertex,
                                       GLsizei indexCount,
                                       gl::DrawElementsType indexType,
                                       const void *indices,
                                       GLsizei instanceCount)
{
    if (mStateManager.getCullEverything())
    {
        return angle::Result::Continue;
    }

    // Transform feedback is not allowed for DrawElements, this error should have been caught at the
    // API validation layer.
    const gl::State &glState = context->getState();
    ASSERT(!glState.isTransformFeedbackActiveUnpaused());

    // If this draw call is coming from an indirect call, offset by the indirect call's base vertex.
    // No base vertex parameter exists for a normal drawElements, so params.baseVertex will be zero.
    int baseVertex = -startVertex;

    const ProgramD3D *programD3D  = mStateManager.getProgramD3D();
    GLsizei adjustedInstanceCount = GetAdjustedInstanceCount(programD3D, instanceCount);

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        return drawLineLoop(context, indexCount, indexType, indices, baseVertex,
                            adjustedInstanceCount);
    }

    if (mode == gl::PrimitiveMode::TriangleFan)
    {
        return drawTriangleFan(context, indexCount, indexType, indices, baseVertex,
                               adjustedInstanceCount);
    }

    if (mode != gl::PrimitiveMode::Points || !programD3D->usesInstancedPointSpriteEmulation())
    {
        if (adjustedInstanceCount == 0)
        {
            mDeviceContext->DrawIndexed(indexCount, 0, baseVertex);
        }
        else
        {
            mDeviceContext->DrawIndexedInstanced(indexCount, adjustedInstanceCount, 0, baseVertex,
                                                 0);
        }
        return angle::Result::Continue;
    }

    // This code should not be reachable by multi-view programs.
    ASSERT(programD3D->getState().usesMultiview() == false);

    // If the shader is writing to gl_PointSize, then pointsprites are being rendered.
    // Emulating instanced point sprites for FL9_3 requires the topology to be
    // D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST and DrawIndexedInstanced is called instead.
    //
    // The count parameter passed to drawElements represents the total number of instances to be
    // rendered. Each instance is referenced by the bound index buffer from the the caller.
    //
    // Indexed pointsprite emulation replicates data for duplicate entries found in the index
    // buffer. This is not an efficent rendering mechanism and is only used on downlevel renderers
    // that do not support geometry shaders.
    if (instanceCount == 0)
    {
        mDeviceContext->DrawIndexedInstanced(6, indexCount, 0, 0, 0);
        return angle::Result::Continue;
    }

    // If pointsprite emulation is used with glDrawElementsInstanced then we need to take a less
    // efficent code path. Instanced rendering of emulated pointsprites requires a loop to draw each
    // batch of points. An offset into the instanced data buffer is calculated and applied on each
    // iteration to ensure all instances are rendered correctly.
    gl::IndexRange indexRange;
    ANGLE_TRY(glState.getVertexArray()->getIndexRange(context, indexType, indexCount, indices,
                                                      &indexRange));

    UINT clampedVertexCount = gl::clampCast<UINT>(indexRange.vertexCount());

    // Each instance being rendered requires the inputlayout cache to reapply buffers and offsets.
    for (GLsizei i = 0; i < instanceCount; i++)
    {
        ANGLE_TRY(
            mStateManager.updateVertexOffsetsForPointSpritesEmulation(context, startVertex, i));
        mDeviceContext->DrawIndexedInstanced(6, clampedVertexCount, 0, 0, 0);
    }
    mStateManager.invalidateVertexBuffer();
    return angle::Result::Continue;
}

angle::Result Renderer11::drawArraysIndirect(const gl::Context *context, const void *indirect)
{
    if (mStateManager.getCullEverything())
    {
        return angle::Result::Continue;
    }

    const gl::State &glState = context->getState();
    ASSERT(!glState.isTransformFeedbackActiveUnpaused());

    gl::Buffer *drawIndirectBuffer = glState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(drawIndirectBuffer);
    Buffer11 *storage = GetImplAs<Buffer11>(drawIndirectBuffer);

    uintptr_t offset = reinterpret_cast<uintptr_t>(indirect);

    ID3D11Buffer *buffer = nullptr;
    ANGLE_TRY(storage->getBuffer(context, BUFFER_USAGE_INDIRECT, &buffer));
    mDeviceContext->DrawInstancedIndirect(buffer, static_cast<unsigned int>(offset));
    return angle::Result::Continue;
}

angle::Result Renderer11::drawElementsIndirect(const gl::Context *context, const void *indirect)
{
    if (mStateManager.getCullEverything())
    {
        return angle::Result::Continue;
    }

    const gl::State &glState = context->getState();
    ASSERT(!glState.isTransformFeedbackActiveUnpaused());

    gl::Buffer *drawIndirectBuffer = glState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(drawIndirectBuffer);
    Buffer11 *storage = GetImplAs<Buffer11>(drawIndirectBuffer);
    uintptr_t offset  = reinterpret_cast<uintptr_t>(indirect);

    ID3D11Buffer *buffer = nullptr;
    ANGLE_TRY(storage->getBuffer(context, BUFFER_USAGE_INDIRECT, &buffer));
    mDeviceContext->DrawIndexedInstancedIndirect(buffer, static_cast<unsigned int>(offset));
    return angle::Result::Continue;
}

angle::Result Renderer11::drawLineLoop(const gl::Context *context,
                                       GLuint count,
                                       gl::DrawElementsType type,
                                       const void *indexPointer,
                                       int baseVertex,
                                       int instances)
{
    const gl::State &glState       = context->getState();
    gl::VertexArray *vao           = glState.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer();

    const void *indices = indexPointer;

    // Get the raw indices for an indexed draw
    if (type != gl::DrawElementsType::InvalidEnum && elementArrayBuffer)
    {
        BufferD3D *storage = GetImplAs<BufferD3D>(elementArrayBuffer);
        intptr_t offset    = reinterpret_cast<intptr_t>(indices);

        const uint8_t *bufferData = nullptr;
        ANGLE_TRY(storage->getData(context, &bufferData));

        indices = bufferData + offset;
    }

    if (!mLineLoopIB)
    {
        mLineLoopIB = new StreamingIndexBufferInterface(this);
        ANGLE_TRY(mLineLoopIB->reserveBufferSpace(context, INITIAL_INDEX_BUFFER_SIZE,
                                                  gl::DrawElementsType::UnsignedInt));
    }

    // Checked by Renderer11::applyPrimitiveType
    bool indexCheck = static_cast<unsigned int>(count) + 1 >
                      (std::numeric_limits<unsigned int>::max() / sizeof(unsigned int));
    ANGLE_CHECK(GetImplAs<Context11>(context), !indexCheck,
                "Failed to create a 32-bit looping index buffer for "
                "GL_LINE_LOOP, too many indices required.",
                GL_OUT_OF_MEMORY);

    GetLineLoopIndices(indices, type, static_cast<GLuint>(count),
                       glState.isPrimitiveRestartEnabled(), &mScratchIndexDataBuffer);

    unsigned int spaceNeeded =
        static_cast<unsigned int>(sizeof(GLuint) * mScratchIndexDataBuffer.size());
    ANGLE_TRY(
        mLineLoopIB->reserveBufferSpace(context, spaceNeeded, gl::DrawElementsType::UnsignedInt));

    void *mappedMemory = nullptr;
    unsigned int offset;
    ANGLE_TRY(mLineLoopIB->mapBuffer(context, spaceNeeded, &mappedMemory, &offset));

    // Copy over the converted index data.
    memcpy(mappedMemory, &mScratchIndexDataBuffer[0],
           sizeof(GLuint) * mScratchIndexDataBuffer.size());

    ANGLE_TRY(mLineLoopIB->unmapBuffer(context));

    IndexBuffer11 *indexBuffer          = GetAs<IndexBuffer11>(mLineLoopIB->getIndexBuffer());
    const d3d11::Buffer &d3dIndexBuffer = indexBuffer->getBuffer();
    DXGI_FORMAT indexFormat             = indexBuffer->getIndexFormat();

    mStateManager.setIndexBuffer(d3dIndexBuffer.get(), indexFormat, offset);

    UINT indexCount = static_cast<UINT>(mScratchIndexDataBuffer.size());

    if (instances > 0)
    {
        mDeviceContext->DrawIndexedInstanced(indexCount, instances, 0, baseVertex, 0);
    }
    else
    {
        mDeviceContext->DrawIndexed(indexCount, 0, baseVertex);
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::drawTriangleFan(const gl::Context *context,
                                          GLuint count,
                                          gl::DrawElementsType type,
                                          const void *indices,
                                          int baseVertex,
                                          int instances)
{
    const gl::State &glState       = context->getState();
    gl::VertexArray *vao           = glState.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer();

    const void *indexPointer = indices;

    // Get the raw indices for an indexed draw
    if (type != gl::DrawElementsType::InvalidEnum && elementArrayBuffer)
    {
        BufferD3D *storage = GetImplAs<BufferD3D>(elementArrayBuffer);
        intptr_t offset    = reinterpret_cast<intptr_t>(indices);

        const uint8_t *bufferData = nullptr;
        ANGLE_TRY(storage->getData(context, &bufferData));

        indexPointer = bufferData + offset;
    }

    if (!mTriangleFanIB)
    {
        mTriangleFanIB = new StreamingIndexBufferInterface(this);
        ANGLE_TRY(mTriangleFanIB->reserveBufferSpace(context, INITIAL_INDEX_BUFFER_SIZE,
                                                     gl::DrawElementsType::UnsignedInt));
    }

    // Checked by Renderer11::applyPrimitiveType
    ASSERT(count >= 3);

    const GLuint numTris = count - 2;

    bool indexCheck =
        (numTris > std::numeric_limits<unsigned int>::max() / (sizeof(unsigned int) * 3));
    ANGLE_CHECK(GetImplAs<Context11>(context), !indexCheck,
                "Failed to create a scratch index buffer for GL_TRIANGLE_FAN, "
                "too many indices required.",
                GL_OUT_OF_MEMORY);

    GetTriFanIndices(indexPointer, type, count, glState.isPrimitiveRestartEnabled(),
                     &mScratchIndexDataBuffer);

    const unsigned int spaceNeeded =
        static_cast<unsigned int>(mScratchIndexDataBuffer.size() * sizeof(unsigned int));
    ANGLE_TRY(mTriangleFanIB->reserveBufferSpace(context, spaceNeeded,
                                                 gl::DrawElementsType::UnsignedInt));

    void *mappedMemory = nullptr;
    unsigned int offset;
    ANGLE_TRY(mTriangleFanIB->mapBuffer(context, spaceNeeded, &mappedMemory, &offset));

    memcpy(mappedMemory, &mScratchIndexDataBuffer[0], spaceNeeded);

    ANGLE_TRY(mTriangleFanIB->unmapBuffer(context));

    IndexBuffer11 *indexBuffer          = GetAs<IndexBuffer11>(mTriangleFanIB->getIndexBuffer());
    const d3d11::Buffer &d3dIndexBuffer = indexBuffer->getBuffer();
    DXGI_FORMAT indexFormat             = indexBuffer->getIndexFormat();

    mStateManager.setIndexBuffer(d3dIndexBuffer.get(), indexFormat, offset);

    UINT indexCount = static_cast<UINT>(mScratchIndexDataBuffer.size());

    if (instances > 0)
    {
        mDeviceContext->DrawIndexedInstanced(indexCount, instances, 0, baseVertex, 0);
    }
    else
    {
        mDeviceContext->DrawIndexed(indexCount, 0, baseVertex);
    }

    return angle::Result::Continue;
}

void Renderer11::releaseDeviceResources()
{
    mStateManager.deinitialize();
    mStateCache.clear();

    SafeDelete(mLineLoopIB);
    SafeDelete(mTriangleFanIB);
    SafeDelete(mBlit);
    SafeDelete(mClear);
    SafeDelete(mTrim);
    SafeDelete(mPixelTransfer);

    mSyncQuery.reset();

    mCachedResolveTexture.reset();
}

// set notify to true to broadcast a message to all contexts of the device loss
bool Renderer11::testDeviceLost()
{
    bool isLost = false;

    if (!mDevice)
    {
        return true;
    }

    // GetRemovedReason is used to test if the device is removed
    HRESULT result = mDevice->GetDeviceRemovedReason();
    isLost         = d3d11::isDeviceLostError(result);

    if (isLost)
    {
        ERR() << "The D3D11 device was removed, " << gl::FmtHR(result);
    }

    return isLost;
}

bool Renderer11::testDeviceResettable()
{
    // determine if the device is resettable by creating a dummy device
    PFN_D3D11_CREATE_DEVICE D3D11CreateDevice =
        (PFN_D3D11_CREATE_DEVICE)GetProcAddress(mD3d11Module, "D3D11CreateDevice");

    if (D3D11CreateDevice == nullptr)
    {
        return false;
    }

    ID3D11Device *dummyDevice;
    D3D_FEATURE_LEVEL dummyFeatureLevel;
    ID3D11DeviceContext *dummyContext;
    UINT flags = (mCreateDebugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0);

    ASSERT(mRequestedDriverType != D3D_DRIVER_TYPE_UNKNOWN);
    HRESULT result = D3D11CreateDevice(
        nullptr, mRequestedDriverType, nullptr, flags, mAvailableFeatureLevels.data(),
        static_cast<unsigned int>(mAvailableFeatureLevels.size()), D3D11_SDK_VERSION, &dummyDevice,
        &dummyFeatureLevel, &dummyContext);

    if (!mDevice || FAILED(result))
    {
        return false;
    }

    SafeRelease(dummyContext);
    SafeRelease(dummyDevice);

    return true;
}

void Renderer11::release()
{
    mScratchMemoryBuffer.clear();

    if (mAnnotator != nullptr)
    {
        gl::UninitializeDebugAnnotations();
        SafeDelete(mAnnotator);
    }

    releaseDeviceResources();

    SafeRelease(mDxgiFactory);
    SafeRelease(mDxgiAdapter);

    //SafeRelease(mDeviceContext3);
    //SafeRelease(mDeviceContext1);

    if (mDeviceContext)
    {
        mDeviceContext->ClearState();
        mDeviceContext->Flush();
        SafeRelease(mDeviceContext);
    }

    SafeRelease(mDevice);
    SafeRelease(mDebug);

    if (mD3d11Module)
    {
        FreeLibrary(mD3d11Module);
        mD3d11Module = nullptr;
    }

    if (mDxgiModule)
    {
        FreeLibrary(mDxgiModule);
        mDxgiModule = nullptr;
    }

    if (mDCompModule)
    {
        FreeLibrary(mDCompModule);
        mDCompModule = nullptr;
    }

    mCompiler.release();

    mSupportsShareHandles.reset();
}

bool Renderer11::resetDevice()
{
    // recreate everything
    release();
    egl::Error result = initialize();

    if (result.isError())
    {
        ERR() << "Could not reinitialize D3D11 device: " << result;
        return false;
    }

    return true;
}

std::string Renderer11::getRendererDescription() const
{
    std::ostringstream rendererString;

    rendererString << mDescription;
    rendererString << " Direct3D11";

    rendererString << " vs_" << getMajorShaderModel() << "_" << getMinorShaderModel()
                   << getShaderModelSuffix();
    rendererString << " ps_" << getMajorShaderModel() << "_" << getMinorShaderModel()
                   << getShaderModelSuffix();

    return rendererString.str();
}

DeviceIdentifier Renderer11::getAdapterIdentifier() const
{
    // Don't use the AdapterLuid here, since that doesn't persist across reboot.
    DeviceIdentifier deviceIdentifier = {0};
    deviceIdentifier.VendorId         = mAdapterDescription.VendorId;
    deviceIdentifier.DeviceId         = mAdapterDescription.DeviceId;
    deviceIdentifier.SubSysId         = mAdapterDescription.SubSysId;
    deviceIdentifier.Revision         = mAdapterDescription.Revision;
    deviceIdentifier.FeatureLevel     = static_cast<UINT>(mRenderer11DeviceCaps.featureLevel);

    return deviceIdentifier;
}

unsigned int Renderer11::getReservedVertexUniformVectors() const
{
    // Driver uniforms are stored in a separate constant buffer
    return d3d11_gl::GetReservedVertexUniformVectors(mRenderer11DeviceCaps.featureLevel);
}

unsigned int Renderer11::getReservedFragmentUniformVectors() const
{
    // Driver uniforms are stored in a separate constant buffer
    return d3d11_gl::GetReservedFragmentUniformVectors(mRenderer11DeviceCaps.featureLevel);
}

gl::ShaderMap<unsigned int> Renderer11::getReservedShaderUniformBuffers() const
{
    gl::ShaderMap<unsigned int> shaderReservedUniformBuffers = {};

    // we reserve one buffer for the application uniforms, and one for driver uniforms
    shaderReservedUniformBuffers[gl::ShaderType::Vertex]   = 2;
    shaderReservedUniformBuffers[gl::ShaderType::Fragment] = 2;

    return shaderReservedUniformBuffers;
}

d3d11::ANGLED3D11DeviceType Renderer11::getDeviceType() const
{
    if (mCreatedWithDeviceEXT)
    {
        return d3d11::GetDeviceType(mDevice);
    }

    if ((mRequestedDriverType == D3D_DRIVER_TYPE_SOFTWARE) ||
        (mRequestedDriverType == D3D_DRIVER_TYPE_REFERENCE) ||
        (mRequestedDriverType == D3D_DRIVER_TYPE_NULL))
    {
        return d3d11::ANGLE_D3D11_DEVICE_TYPE_SOFTWARE_REF_OR_NULL;
    }

    if (mRequestedDriverType == D3D_DRIVER_TYPE_WARP)
    {
        return d3d11::ANGLE_D3D11_DEVICE_TYPE_WARP;
    }

    return d3d11::ANGLE_D3D11_DEVICE_TYPE_HARDWARE;
}

bool Renderer11::getShareHandleSupport() const
{
    if (mSupportsShareHandles.valid())
    {
        return mSupportsShareHandles.value();
    }

    // We only currently support share handles with BGRA surfaces, because
    // chrome needs BGRA. Once chrome fixes this, we should always support them.
    if (!getNativeExtensions().textureFormatBGRA8888)
    {
        mSupportsShareHandles = false;
        return false;
    }

    // PIX doesn't seem to support using share handles, so disable them.
    if (gl::DebugAnnotationsActive())
    {
        mSupportsShareHandles = false;
        return false;
    }

    // Also disable share handles on Feature Level 9_3, since it doesn't support share handles on
    // RGBA8 textures/swapchains.
    if (mRenderer11DeviceCaps.featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        mSupportsShareHandles = false;
        return false;
    }

    // Find out which type of D3D11 device the Renderer11 is using
    d3d11::ANGLED3D11DeviceType deviceType = getDeviceType();
    if (deviceType == d3d11::ANGLE_D3D11_DEVICE_TYPE_UNKNOWN)
    {
        mSupportsShareHandles = false;
        return false;
    }

    if (deviceType == d3d11::ANGLE_D3D11_DEVICE_TYPE_SOFTWARE_REF_OR_NULL)
    {
        // Software/Reference/NULL devices don't support share handles
        mSupportsShareHandles = false;
        return false;
    }

    if (deviceType == d3d11::ANGLE_D3D11_DEVICE_TYPE_WARP)
    {
#ifndef ANGLE_ENABLE_WINDOWS_STORE
        if (!IsWindows8OrGreater())
        {
            // WARP on Windows 7 doesn't support shared handles
            mSupportsShareHandles = false;
            return false;
        }
#endif  // ANGLE_ENABLE_WINDOWS_STORE

        // WARP on Windows 8.0+ supports shared handles when shared with another WARP device
        // TODO: allow applications to query for HARDWARE or WARP-specific share handles,
        //       to prevent them trying to use a WARP share handle with an a HW device (or
        //       vice-versa)
        //       e.g. by creating EGL_D3D11_[HARDWARE/WARP]_DEVICE_SHARE_HANDLE_ANGLE
        mSupportsShareHandles = true;
        return true;
    }

    ASSERT(mCreatedWithDeviceEXT || mRequestedDriverType == D3D_DRIVER_TYPE_HARDWARE);
    mSupportsShareHandles = true;
    return true;
}

int Renderer11::getMajorShaderModel() const
{
    switch (mRenderer11DeviceCaps.featureLevel)
    {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return D3D11_SHADER_MAJOR_VERSION;  // 5
        case D3D_FEATURE_LEVEL_10_1:
            return D3D10_1_SHADER_MAJOR_VERSION;  // 4
        case D3D_FEATURE_LEVEL_10_0:
            return D3D10_SHADER_MAJOR_VERSION;  // 4
        case D3D_FEATURE_LEVEL_9_3:
            return D3D10_SHADER_MAJOR_VERSION;  // 4
        default:
            UNREACHABLE();
            return 0;
    }
}

int Renderer11::getMinorShaderModel() const
{
    switch (mRenderer11DeviceCaps.featureLevel)
    {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return D3D11_SHADER_MINOR_VERSION;  // 0
        case D3D_FEATURE_LEVEL_10_1:
            return D3D10_1_SHADER_MINOR_VERSION;  // 1
        case D3D_FEATURE_LEVEL_10_0:
            return D3D10_SHADER_MINOR_VERSION;  // 0
        case D3D_FEATURE_LEVEL_9_3:
            return D3D10_SHADER_MINOR_VERSION;  // 0
        default:
            UNREACHABLE();
            return 0;
    }
}

std::string Renderer11::getShaderModelSuffix() const
{
    switch (mRenderer11DeviceCaps.featureLevel)
    {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            return "";
        case D3D_FEATURE_LEVEL_10_1:
            return "";
        case D3D_FEATURE_LEVEL_10_0:
            return "";
        case D3D_FEATURE_LEVEL_9_3:
            return "_level_9_3";
        default:
            UNREACHABLE();
            return "";
    }
}

const angle::WorkaroundsD3D &RendererD3D::getWorkarounds() const
{
    if (!mWorkaroundsInitialized)
    {
        mWorkarounds            = generateWorkarounds();
        mWorkaroundsInitialized = true;
    }

    return mWorkarounds;
}

angle::Result Renderer11::copyImageInternal(const gl::Context *context,
                                            const gl::Framebuffer *framebuffer,
                                            const gl::Rectangle &sourceRect,
                                            GLenum destFormat,
                                            const gl::Offset &destOffset,
                                            RenderTargetD3D *destRenderTarget)
{
    const gl::FramebufferAttachment *colorAttachment = framebuffer->getReadColorbuffer();
    ASSERT(colorAttachment);

    RenderTarget11 *sourceRenderTarget = nullptr;
    ANGLE_TRY(colorAttachment->getRenderTarget(context, &sourceRenderTarget));
    ASSERT(sourceRenderTarget);

    const d3d11::RenderTargetView &dest =
        GetAs<RenderTarget11>(destRenderTarget)->getRenderTargetView();
    ASSERT(dest.valid());

    gl::Box sourceArea(sourceRect.x, sourceRect.y, 0, sourceRect.width, sourceRect.height, 1);
    gl::Extents sourceSize(sourceRenderTarget->getWidth(), sourceRenderTarget->getHeight(), 1);

    const bool invertSource = UsePresentPathFast(this, colorAttachment);
    if (invertSource)
    {
        sourceArea.y      = sourceSize.height - sourceRect.y;
        sourceArea.height = -sourceArea.height;
    }

    gl::Box destArea(destOffset.x, destOffset.y, 0, sourceRect.width, sourceRect.height, 1);
    gl::Extents destSize(destRenderTarget->getWidth(), destRenderTarget->getHeight(), 1);

    // Use nearest filtering because source and destination are the same size for the direct copy.
    // Convert to the unsized format before calling copyTexture.
    GLenum sourceFormat = colorAttachment->getFormat().info->format;
    if (sourceRenderTarget->getTexture().is2D() && sourceRenderTarget->isMultisampled())
    {
        TextureHelper11 tex;
        ANGLE_TRY(resolveMultisampledTexture(context, sourceRenderTarget,
                                             colorAttachment->getDepthSize(),
                                             colorAttachment->getStencilSize(), &tex));

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
        viewDesc.Format                    = sourceRenderTarget->getFormatSet().srvFormat;
        viewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipLevels       = 1;
        viewDesc.Texture2D.MostDetailedMip = 0;

        d3d11::SharedSRV readSRV;
        ANGLE_TRY(allocateResource(GetImplAs<Context11>(context), viewDesc, tex.get(), &readSRV));
        ASSERT(readSRV.valid());

        ANGLE_TRY(mBlit->copyTexture(context, readSRV, sourceArea, sourceSize, sourceFormat, dest,
                                     destArea, destSize, nullptr, gl::GetUnsizedFormat(destFormat),
                                     GL_NONE, GL_NEAREST, false, false, false));

        return angle::Result::Continue;
    }

    ASSERT(!sourceRenderTarget->isMultisampled());

    const d3d11::SharedSRV &source = sourceRenderTarget->getBlitShaderResourceView(context);
    ASSERT(source.valid());

    ANGLE_TRY(mBlit->copyTexture(context, source, sourceArea, sourceSize, sourceFormat, dest,
                                 destArea, destSize, nullptr, gl::GetUnsizedFormat(destFormat),
                                 GL_NONE, GL_NEAREST, false, false, false));

    return angle::Result::Continue;
}

angle::Result Renderer11::copyImage2D(const gl::Context *context,
                                      const gl::Framebuffer *framebuffer,
                                      const gl::Rectangle &sourceRect,
                                      GLenum destFormat,
                                      const gl::Offset &destOffset,
                                      TextureStorage *storage,
                                      GLint level)
{
    TextureStorage11_2D *storage11 = GetAs<TextureStorage11_2D>(storage);
    ASSERT(storage11);

    gl::ImageIndex index              = gl::ImageIndex::Make2D(level);
    RenderTargetD3D *destRenderTarget = nullptr;
    ANGLE_TRY(storage11->getRenderTarget(context, index, &destRenderTarget));
    ASSERT(destRenderTarget);

    ANGLE_TRY(copyImageInternal(context, framebuffer, sourceRect, destFormat, destOffset,
                                destRenderTarget));

    storage11->markLevelDirty(level);

    return angle::Result::Continue;
}

angle::Result Renderer11::copyImageCube(const gl::Context *context,
                                        const gl::Framebuffer *framebuffer,
                                        const gl::Rectangle &sourceRect,
                                        GLenum destFormat,
                                        const gl::Offset &destOffset,
                                        TextureStorage *storage,
                                        gl::TextureTarget target,
                                        GLint level)
{
    TextureStorage11_Cube *storage11 = GetAs<TextureStorage11_Cube>(storage);
    ASSERT(storage11);

    gl::ImageIndex index              = gl::ImageIndex::MakeCubeMapFace(target, level);
    RenderTargetD3D *destRenderTarget = nullptr;
    ANGLE_TRY(storage11->getRenderTarget(context, index, &destRenderTarget));
    ASSERT(destRenderTarget);

    ANGLE_TRY(copyImageInternal(context, framebuffer, sourceRect, destFormat, destOffset,
                                destRenderTarget));

    storage11->markLevelDirty(level);

    return angle::Result::Continue;
}

angle::Result Renderer11::copyImage3D(const gl::Context *context,
                                      const gl::Framebuffer *framebuffer,
                                      const gl::Rectangle &sourceRect,
                                      GLenum destFormat,
                                      const gl::Offset &destOffset,
                                      TextureStorage *storage,
                                      GLint level)
{
    TextureStorage11_3D *storage11 = GetAs<TextureStorage11_3D>(storage);
    ASSERT(storage11);

    gl::ImageIndex index              = gl::ImageIndex::Make3D(level, destOffset.z);
    RenderTargetD3D *destRenderTarget = nullptr;
    ANGLE_TRY(storage11->getRenderTarget(context, index, &destRenderTarget));
    ASSERT(destRenderTarget);

    ANGLE_TRY(copyImageInternal(context, framebuffer, sourceRect, destFormat, destOffset,
                                destRenderTarget));

    storage11->markLevelDirty(level);

    return angle::Result::Continue;
}

angle::Result Renderer11::copyImage2DArray(const gl::Context *context,
                                           const gl::Framebuffer *framebuffer,
                                           const gl::Rectangle &sourceRect,
                                           GLenum destFormat,
                                           const gl::Offset &destOffset,
                                           TextureStorage *storage,
                                           GLint level)
{
    TextureStorage11_2DArray *storage11 = GetAs<TextureStorage11_2DArray>(storage);
    ASSERT(storage11);

    gl::ImageIndex index              = gl::ImageIndex::Make2DArray(level, destOffset.z);
    RenderTargetD3D *destRenderTarget = nullptr;
    ANGLE_TRY(storage11->getRenderTarget(context, index, &destRenderTarget));
    ASSERT(destRenderTarget);

    ANGLE_TRY(copyImageInternal(context, framebuffer, sourceRect, destFormat, destOffset,
                                destRenderTarget));
    storage11->markLevelDirty(level);

    return angle::Result::Continue;
}

angle::Result Renderer11::copyTexture(const gl::Context *context,
                                      const gl::Texture *source,
                                      GLint sourceLevel,
                                      gl::TextureTarget srcTarget,
                                      const gl::Box &sourceBox,
                                      GLenum destFormat,
                                      GLenum destType,
                                      const gl::Offset &destOffset,
                                      TextureStorage *storage,
                                      gl::TextureTarget destTarget,
                                      GLint destLevel,
                                      bool unpackFlipY,
                                      bool unpackPremultiplyAlpha,
                                      bool unpackUnmultiplyAlpha)
{
    TextureD3D *sourceD3D = GetImplAs<TextureD3D>(source);

    TextureStorage *sourceStorage = nullptr;
    ANGLE_TRY(sourceD3D->getNativeTexture(context, &sourceStorage));

    TextureStorage11 *sourceStorage11 = GetAs<TextureStorage11>(sourceStorage);
    ASSERT(sourceStorage11);

    TextureStorage11 *destStorage11 = GetAs<TextureStorage11>(storage);
    ASSERT(destStorage11);

    // Check for fast path where a CopySubresourceRegion can be used.
    if (unpackPremultiplyAlpha == unpackUnmultiplyAlpha && !unpackFlipY &&
        source->getFormat(srcTarget, sourceLevel).info->format == destFormat &&
        sourceStorage11->getFormatSet().internalFormat ==
            destStorage11->getFormatSet().internalFormat)
    {
        const TextureHelper11 *sourceResource = nullptr;
        ANGLE_TRY(sourceStorage11->getResource(context, &sourceResource));

        const TextureHelper11 *destResource = nullptr;
        ANGLE_TRY(destStorage11->getResource(context, &destResource));

        if (srcTarget == gl::TextureTarget::_2D || srcTarget == gl::TextureTarget::_3D)
        {
            gl::ImageIndex sourceIndex = gl::ImageIndex::MakeFromTarget(srcTarget, sourceLevel);

            UINT sourceSubresource = 0;
            ANGLE_TRY(
                sourceStorage11->getSubresourceIndex(context, sourceIndex, &sourceSubresource));

            gl::ImageIndex destIndex = gl::ImageIndex::MakeFromTarget(destTarget, destLevel);

            UINT destSubresource = 0;
            ANGLE_TRY(destStorage11->getSubresourceIndex(context, destIndex, &destSubresource));

            D3D11_BOX d3dBox{static_cast<UINT>(sourceBox.x),
                             static_cast<UINT>(sourceBox.y),
                             static_cast<UINT>(sourceBox.z),
                             static_cast<UINT>(sourceBox.x + sourceBox.width),
                             static_cast<UINT>(sourceBox.y + sourceBox.height),
                             static_cast<UINT>(sourceBox.z + sourceBox.depth)};

            mDeviceContext->CopySubresourceRegion(
                destResource->get(), destSubresource, destOffset.x, destOffset.y, destOffset.z,
                sourceResource->get(), sourceSubresource, &d3dBox);
        }
        else if (srcTarget == gl::TextureTarget::_2DArray)
        {

            D3D11_BOX d3dBox{static_cast<UINT>(sourceBox.x),
                             static_cast<UINT>(sourceBox.y),
                             0,
                             static_cast<UINT>(sourceBox.x + sourceBox.width),
                             static_cast<UINT>(sourceBox.y + sourceBox.height),
                             1u};

            for (int i = 0; i < sourceBox.depth; i++)
            {
                gl::ImageIndex srcIndex = gl::ImageIndex::Make2DArray(sourceLevel, i + sourceBox.z);
                UINT sourceSubresource  = 0;
                ANGLE_TRY(
                    sourceStorage11->getSubresourceIndex(context, srcIndex, &sourceSubresource));

                gl::ImageIndex dIndex = gl::ImageIndex::Make2DArray(destLevel, i + destOffset.z);
                UINT destSubresource  = 0;
                ANGLE_TRY(destStorage11->getSubresourceIndex(context, dIndex, &destSubresource));

                mDeviceContext->CopySubresourceRegion(
                    destResource->get(), destSubresource, destOffset.x, destOffset.y, 0,
                    sourceResource->get(), sourceSubresource, &d3dBox);
            }
        }
        else
        {
            UNREACHABLE();
        }
    }
    else
    {
        const d3d11::SharedSRV *sourceSRV = nullptr;
        ANGLE_TRY(sourceStorage11->getSRVLevels(context, sourceLevel, sourceLevel, &sourceSRV));

        gl::Extents sourceSize(static_cast<int>(source->getWidth(
                                   NonCubeTextureTypeToTarget(source->getType()), sourceLevel)),
                               static_cast<int>(source->getHeight(
                                   NonCubeTextureTypeToTarget(source->getType()), sourceLevel)),
                               static_cast<int>(source->getDepth(
                                   NonCubeTextureTypeToTarget(source->getType()), sourceLevel)));

        gl::ImageIndex destIndex;
        if (destTarget == gl::TextureTarget::_2D || destTarget == gl::TextureTarget::_3D ||
            gl::IsCubeMapFaceTarget(destTarget))
        {
            destIndex = gl::ImageIndex::MakeFromTarget(destTarget, destLevel);
        }
        else if (destTarget == gl::TextureTarget::_2DArray)
        {
            destIndex = gl::ImageIndex::Make2DArrayRange(destLevel, 0, sourceSize.depth);
        }
        else
        {
            UNREACHABLE();
        }

        RenderTargetD3D *destRenderTargetD3D = nullptr;
        ANGLE_TRY(destStorage11->getRenderTarget(context, destIndex, &destRenderTargetD3D));

        RenderTarget11 *destRenderTarget11 = GetAs<RenderTarget11>(destRenderTargetD3D);

        const d3d11::RenderTargetView &destRTV = destRenderTarget11->getRenderTargetView();
        ASSERT(destRTV.valid());

        gl::Box sourceArea(sourceBox.x, sourceBox.y, sourceBox.z, sourceBox.width, sourceBox.height,
                           sourceBox.depth);

        if (unpackFlipY)
        {
            sourceArea.y += sourceArea.height;
            sourceArea.height = -sourceArea.height;
        }

        gl::Box destArea(destOffset.x, destOffset.y, destOffset.z, sourceBox.width,
                         sourceBox.height, sourceBox.depth);

        gl::Extents destSize(destRenderTarget11->getWidth(), destRenderTarget11->getHeight(),
                             sourceBox.depth);

        // Use nearest filtering because source and destination are the same size for the direct
        // copy
        GLenum sourceFormat = source->getFormat(srcTarget, sourceLevel).info->format;
        ANGLE_TRY(mBlit->copyTexture(context, *sourceSRV, sourceArea, sourceSize, sourceFormat,
                                     destRTV, destArea, destSize, nullptr, destFormat, destType,
                                     GL_NEAREST, false, unpackPremultiplyAlpha,
                                     unpackUnmultiplyAlpha));
    }

    destStorage11->markLevelDirty(destLevel);

    return angle::Result::Continue;
}

angle::Result Renderer11::copyCompressedTexture(const gl::Context *context,
                                                const gl::Texture *source,
                                                GLint sourceLevel,
                                                TextureStorage *storage,
                                                GLint destLevel)
{
    TextureStorage11_2D *destStorage11 = GetAs<TextureStorage11_2D>(storage);
    ASSERT(destStorage11);

    const TextureHelper11 *destResource = nullptr;
    ANGLE_TRY(destStorage11->getResource(context, &destResource));

    gl::ImageIndex destIndex = gl::ImageIndex::Make2D(destLevel);
    UINT destSubresource     = 0;
    ANGLE_TRY(destStorage11->getSubresourceIndex(context, destIndex, &destSubresource));

    TextureD3D *sourceD3D = GetImplAs<TextureD3D>(source);
    ASSERT(sourceD3D);

    TextureStorage *sourceStorage = nullptr;
    ANGLE_TRY(sourceD3D->getNativeTexture(context, &sourceStorage));

    TextureStorage11_2D *sourceStorage11 = GetAs<TextureStorage11_2D>(sourceStorage);
    ASSERT(sourceStorage11);

    const TextureHelper11 *sourceResource = nullptr;
    ANGLE_TRY(sourceStorage11->getResource(context, &sourceResource));

    gl::ImageIndex sourceIndex = gl::ImageIndex::Make2D(sourceLevel);
    UINT sourceSubresource     = 0;
    ANGLE_TRY(sourceStorage11->getSubresourceIndex(context, sourceIndex, &sourceSubresource));

    mDeviceContext->CopySubresourceRegion(destResource->get(), destSubresource, 0, 0, 0,
                                          sourceResource->get(), sourceSubresource, nullptr);

    return angle::Result::Continue;
}

angle::Result Renderer11::createRenderTarget(const gl::Context *context,
                                             int width,
                                             int height,
                                             GLenum format,
                                             GLsizei samples,
                                             RenderTargetD3D **outRT)
{
    const d3d11::Format &formatInfo = d3d11::Format::Get(format, mRenderer11DeviceCaps);

    const gl::TextureCaps &textureCaps = getNativeTextureCaps().get(format);
    GLuint supportedSamples            = textureCaps.getNearestSamples(samples);

    Context11 *context11 = GetImplAs<Context11>(context);

    if (width > 0 && height > 0)
    {
        // Create texture resource
        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = width;
        desc.Height             = height;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.Format             = formatInfo.texFormat;
        desc.SampleDesc.Count   = (supportedSamples == 0) ? 1 : supportedSamples;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        // If a rendertarget or depthstencil format exists for this texture format,
        // we'll flag it to allow binding that way. Shader resource views are a little
        // more complicated.
        bool bindRTV = false, bindDSV = false, bindSRV = false;
        bindRTV = (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN);
        bindDSV = (formatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN);
        bindSRV = (formatInfo.srvFormat != DXGI_FORMAT_UNKNOWN);

        bool isMultisampledDepthStencil = bindDSV && desc.SampleDesc.Count > 1;
        if (isMultisampledDepthStencil &&
            !mRenderer11DeviceCaps.supportsMultisampledDepthStencilSRVs)
        {
            bindSRV = false;
        }

        desc.BindFlags = (bindRTV ? D3D11_BIND_RENDER_TARGET : 0) |
                         (bindDSV ? D3D11_BIND_DEPTH_STENCIL : 0) |
                         (bindSRV ? D3D11_BIND_SHADER_RESOURCE : 0);

        // The format must be either an RTV or a DSV
        ASSERT(bindRTV != bindDSV);

        TextureHelper11 texture;
        ANGLE_TRY(allocateTexture(context11, desc, formatInfo, &texture));
        texture.setDebugName("createRenderTarget.Texture");

        d3d11::SharedSRV srv;
        d3d11::SharedSRV blitSRV;
        if (bindSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format        = formatInfo.srvFormat;
            srvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_SRV_DIMENSION_TEXTURE2D
                                                            : D3D11_SRV_DIMENSION_TEXTURE2DMS;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels       = 1;

            ANGLE_TRY(allocateResource(context11, srvDesc, texture.get(), &srv));
            srv.setDebugName("createRenderTarget.SRV");

            if (formatInfo.blitSRVFormat != formatInfo.srvFormat)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC blitSRVDesc;
                blitSRVDesc.Format        = formatInfo.blitSRVFormat;
                blitSRVDesc.ViewDimension = (supportedSamples == 0)
                                                ? D3D11_SRV_DIMENSION_TEXTURE2D
                                                : D3D11_SRV_DIMENSION_TEXTURE2DMS;
                blitSRVDesc.Texture2D.MostDetailedMip = 0;
                blitSRVDesc.Texture2D.MipLevels       = 1;

                ANGLE_TRY(allocateResource(context11, blitSRVDesc, texture.get(), &blitSRV));
                blitSRV.setDebugName("createRenderTarget.BlitSRV");
            }
            else
            {
                blitSRV = srv.makeCopy();
            }
        }

        if (bindDSV)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Format        = formatInfo.dsvFormat;
            dsvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_DSV_DIMENSION_TEXTURE2D
                                                            : D3D11_DSV_DIMENSION_TEXTURE2DMS;
            dsvDesc.Texture2D.MipSlice = 0;
            dsvDesc.Flags              = 0;

            d3d11::DepthStencilView dsv;
            ANGLE_TRY(allocateResource(context11, dsvDesc, texture.get(), &dsv));
            dsv.setDebugName("createRenderTarget.DSV");

            *outRT = new TextureRenderTarget11(std::move(dsv), texture, srv, format, formatInfo,
                                               width, height, 1, supportedSamples);
        }
        else if (bindRTV)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format        = formatInfo.rtvFormat;
            rtvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_RTV_DIMENSION_TEXTURE2D
                                                            : D3D11_RTV_DIMENSION_TEXTURE2DMS;
            rtvDesc.Texture2D.MipSlice = 0;

            d3d11::RenderTargetView rtv;
            ANGLE_TRY(allocateResource(context11, rtvDesc, texture.get(), &rtv));
            rtv.setDebugName("createRenderTarget.RTV");

            if (formatInfo.dataInitializerFunction != nullptr)
            {
                const float clearValues[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                mDeviceContext->ClearRenderTargetView(rtv.get(), clearValues);
            }

            *outRT = new TextureRenderTarget11(std::move(rtv), texture, srv, blitSRV, format,
                                               formatInfo, width, height, 1, supportedSamples);
        }
        else
        {
            UNREACHABLE();
        }
    }
    else
    {
        *outRT = new TextureRenderTarget11(d3d11::RenderTargetView(), TextureHelper11(),
                                           d3d11::SharedSRV(), d3d11::SharedSRV(), format,
                                           d3d11::Format::Get(GL_NONE, mRenderer11DeviceCaps),
                                           width, height, 1, supportedSamples);
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::createRenderTargetCopy(const gl::Context *context,
                                                 RenderTargetD3D *source,
                                                 RenderTargetD3D **outRT)
{
    ASSERT(source != nullptr);

    RenderTargetD3D *newRT = nullptr;
    ANGLE_TRY(createRenderTarget(context, source->getWidth(), source->getHeight(),
                                 source->getInternalFormat(), source->getSamples(), &newRT));

    RenderTarget11 *source11 = GetAs<RenderTarget11>(source);
    RenderTarget11 *dest11   = GetAs<RenderTarget11>(newRT);

    mDeviceContext->CopySubresourceRegion(dest11->getTexture().get(), dest11->getSubresourceIndex(),
                                          0, 0, 0, source11->getTexture().get(),
                                          source11->getSubresourceIndex(), nullptr);
    *outRT = newRT;
    return angle::Result::Continue;
}

angle::Result Renderer11::loadExecutable(d3d::Context *context,
                                         const uint8_t *function,
                                         size_t length,
                                         gl::ShaderType type,
                                         const std::vector<D3DVarying> &streamOutVaryings,
                                         bool separatedOutputBuffers,
                                         ShaderExecutableD3D **outExecutable)
{
    ShaderData shaderData(function, length);

    Context11 *context11 = static_cast<Context11 *>(context);

    switch (type)
    {
        case gl::ShaderType::Vertex:
        {
            d3d11::VertexShader vertexShader;
            d3d11::GeometryShader streamOutShader;
            ANGLE_TRY(allocateResource(context11, shaderData, &vertexShader));

            if (!streamOutVaryings.empty())
            {
                std::vector<D3D11_SO_DECLARATION_ENTRY> soDeclaration;
                soDeclaration.reserve(streamOutVaryings.size());

                for (const auto &streamOutVarying : streamOutVaryings)
                {
                    D3D11_SO_DECLARATION_ENTRY entry = {0};
                    entry.Stream                     = 0;
                    entry.SemanticName               = streamOutVarying.semanticName.c_str();
                    entry.SemanticIndex              = streamOutVarying.semanticIndex;
                    entry.StartComponent             = 0;
                    entry.ComponentCount = static_cast<BYTE>(streamOutVarying.componentCount);
                    entry.OutputSlot     = static_cast<BYTE>(
                        (separatedOutputBuffers ? streamOutVarying.outputSlot : 0));
                    soDeclaration.push_back(entry);
                }

                ANGLE_TRY(
                    allocateResource(context11, shaderData, &soDeclaration, &streamOutShader));
            }

            *outExecutable = new ShaderExecutable11(function, length, std::move(vertexShader),
                                                    std::move(streamOutShader));
        }
        break;
        case gl::ShaderType::Fragment:
        {
            d3d11::PixelShader pixelShader;
            ANGLE_TRY(allocateResource(context11, shaderData, &pixelShader));
            *outExecutable = new ShaderExecutable11(function, length, std::move(pixelShader));
        }
        break;
        case gl::ShaderType::Geometry:
        {
            d3d11::GeometryShader geometryShader;
            ANGLE_TRY(allocateResource(context11, shaderData, &geometryShader));
            *outExecutable = new ShaderExecutable11(function, length, std::move(geometryShader));
        }
        break;
        case gl::ShaderType::Compute:
        {
            d3d11::ComputeShader computeShader;
            ANGLE_TRY(allocateResource(context11, shaderData, &computeShader));
            *outExecutable = new ShaderExecutable11(function, length, std::move(computeShader));
        }
        break;
        default:
            ANGLE_HR_UNREACHABLE(context11);
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::compileToExecutable(d3d::Context *context,
                                              gl::InfoLog &infoLog,
                                              const std::string &shaderHLSL,
                                              gl::ShaderType type,
                                              const std::vector<D3DVarying> &streamOutVaryings,
                                              bool separatedOutputBuffers,
                                              const angle::CompilerWorkaroundsD3D &workarounds,
                                              ShaderExecutableD3D **outExectuable)
{
    std::stringstream profileStream;

    switch (type)
    {
        case gl::ShaderType::Vertex:
            profileStream << "vs";
            break;
        case gl::ShaderType::Fragment:
            profileStream << "ps";
            break;
        case gl::ShaderType::Geometry:
            profileStream << "gs";
            break;
        case gl::ShaderType::Compute:
            profileStream << "cs";
            break;
        default:
            ANGLE_HR_UNREACHABLE(context);
    }

    profileStream << "_" << getMajorShaderModel() << "_" << getMinorShaderModel()
                  << getShaderModelSuffix();
    std::string profile = profileStream.str();

    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;

    if (gl::DebugAnnotationsActive())
    {
#ifndef NDEBUG
        flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        flags |= D3DCOMPILE_DEBUG;
    }

    if (workarounds.enableIEEEStrictness)
        flags |= D3DCOMPILE_IEEE_STRICTNESS;

    // Sometimes D3DCompile will fail with the default compilation flags for complicated shaders
    // when it would otherwise pass with alternative options.
    // Try the default flags first and if compilation fails, try some alternatives.
    std::vector<CompileConfig> configs;
    configs.push_back(CompileConfig(flags, "default"));
    configs.push_back(CompileConfig(flags | D3DCOMPILE_SKIP_VALIDATION, "skip validation"));
    configs.push_back(CompileConfig(flags | D3DCOMPILE_SKIP_OPTIMIZATION, "skip optimization"));

    if (getMajorShaderModel() == 4 && getShaderModelSuffix() != "")
    {
        // Some shaders might cause a "blob content mismatch between level9 and d3d10 shader".
        // e.g. dEQP-GLES2.functional.shaders.struct.local.loop_nested_struct_array_*.
        // Using the [unroll] directive works around this, as does this D3DCompile flag.
        configs.push_back(
            CompileConfig(flags | D3DCOMPILE_AVOID_FLOW_CONTROL, "avoid flow control"));
    }

    D3D_SHADER_MACRO loopMacros[] = {{"ANGLE_ENABLE_LOOP_FLATTEN", "1"}, {0, 0}};

    // TODO(jmadill): Use ComPtr?
    ID3DBlob *binary = nullptr;
    std::string debugInfo;
    ANGLE_TRY(mCompiler.compileToBinary(context, infoLog, shaderHLSL, profile, configs, loopMacros,
                                        &binary, &debugInfo));

    // It's possible that binary is NULL if the compiler failed in all configurations.  Set the
    // executable to NULL and return GL_NO_ERROR to signify that there was a link error but the
    // internal state is still OK.
    if (!binary)
    {
        *outExectuable = nullptr;
        return angle::Result::Continue;
    }

    angle::Result error = loadExecutable(
        context, static_cast<const uint8_t *>(binary->GetBufferPointer()), binary->GetBufferSize(),
        type, streamOutVaryings, separatedOutputBuffers, outExectuable);

    SafeRelease(binary);
    if (error == angle::Result::Stop)
    {
        return error;
    }

    if (!debugInfo.empty())
    {
        (*outExectuable)->appendDebugInfo(debugInfo);
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::ensureHLSLCompilerInitialized(d3d::Context *context)
{
    return mCompiler.ensureInitialized(context);
}

UniformStorageD3D *Renderer11::createUniformStorage(size_t storageSize)
{
    return new UniformStorage11(storageSize);
}

VertexBuffer *Renderer11::createVertexBuffer()
{
    return new VertexBuffer11(this);
}

IndexBuffer *Renderer11::createIndexBuffer()
{
    return new IndexBuffer11(this);
}

StreamProducerImpl *Renderer11::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    return new StreamProducerD3DTexture(this);
}

bool Renderer11::supportsFastCopyBufferToTexture(GLenum internalFormat) const
{
    ASSERT(getNativeExtensions().pixelBufferObject);

    const gl::InternalFormat &internalFormatInfo = gl::GetSizedInternalFormatInfo(internalFormat);
    const d3d11::Format &d3d11FormatInfo =
        d3d11::Format::Get(internalFormat, mRenderer11DeviceCaps);

    // sRGB formats do not work with D3D11 buffer SRVs
    if (internalFormatInfo.colorEncoding == GL_SRGB)
    {
        return false;
    }

    // We cannot support direct copies to non-color-renderable formats
    if (d3d11FormatInfo.rtvFormat == DXGI_FORMAT_UNKNOWN)
    {
        return false;
    }

    // We skip all 3-channel formats since sometimes format support is missing
    if (internalFormatInfo.componentCount == 3)
    {
        return false;
    }

    // We don't support formats which we can't represent without conversion
    if (d3d11FormatInfo.format().glInternalFormat != internalFormat)
    {
        return false;
    }

    // Buffer SRV creation for this format was not working on Windows 10.
    if (d3d11FormatInfo.texFormat == DXGI_FORMAT_B5G5R5A1_UNORM)
    {
        return false;
    }

    // This format is not supported as a buffer SRV.
    if (d3d11FormatInfo.texFormat == DXGI_FORMAT_A8_UNORM)
    {
        return false;
    }

    return true;
}

angle::Result Renderer11::fastCopyBufferToTexture(const gl::Context *context,
                                                  const gl::PixelUnpackState &unpack,
                                                  unsigned int offset,
                                                  RenderTargetD3D *destRenderTarget,
                                                  GLenum destinationFormat,
                                                  GLenum sourcePixelsType,
                                                  const gl::Box &destArea)
{
    ASSERT(supportsFastCopyBufferToTexture(destinationFormat));
    return mPixelTransfer->copyBufferToTexture(context, unpack, offset, destRenderTarget,
                                               destinationFormat, sourcePixelsType, destArea);
}

ImageD3D *Renderer11::createImage()
{
    return new Image11(this);
}

angle::Result Renderer11::generateMipmap(const gl::Context *context, ImageD3D *dest, ImageD3D *src)
{
    Image11 *dest11 = GetAs<Image11>(dest);
    Image11 *src11  = GetAs<Image11>(src);
    return Image11::GenerateMipmap(context, dest11, src11, mRenderer11DeviceCaps);
}

angle::Result Renderer11::generateMipmapUsingD3D(const gl::Context *context,
                                                 TextureStorage *storage,
                                                 const gl::TextureState &textureState)
{
    TextureStorage11 *storage11 = GetAs<TextureStorage11>(storage);

    ASSERT(storage11->isRenderTarget());
    ASSERT(storage11->supportsNativeMipmapFunction());

    const d3d11::SharedSRV *srv = nullptr;
    ANGLE_TRY(storage11->getSRVLevels(context, textureState.getEffectiveBaseLevel(),
                                      textureState.getEffectiveMaxLevel(), &srv));

    mDeviceContext->GenerateMips(srv->get());

    return angle::Result::Continue;
}

angle::Result Renderer11::copyImage(const gl::Context *context,
                                    ImageD3D *dest,
                                    ImageD3D *source,
                                    const gl::Box &sourceBox,
                                    const gl::Offset &destOffset,
                                    bool unpackFlipY,
                                    bool unpackPremultiplyAlpha,
                                    bool unpackUnmultiplyAlpha)
{
    Image11 *dest11 = GetAs<Image11>(dest);
    Image11 *src11  = GetAs<Image11>(source);
    return Image11::CopyImage(context, dest11, src11, sourceBox, destOffset, unpackFlipY,
                              unpackPremultiplyAlpha, unpackUnmultiplyAlpha, mRenderer11DeviceCaps);
}

TextureStorage *Renderer11::createTextureStorage2D(SwapChainD3D *swapChain)
{
    SwapChain11 *swapChain11 = GetAs<SwapChain11>(swapChain);
    return new TextureStorage11_2D(this, swapChain11);
}

TextureStorage *Renderer11::createTextureStorageEGLImage(EGLImageD3D *eglImage,
                                                         RenderTargetD3D *renderTargetD3D)
{
    return new TextureStorage11_EGLImage(this, eglImage, GetAs<RenderTarget11>(renderTargetD3D));
}

TextureStorage *Renderer11::createTextureStorageExternal(
    egl::Stream *stream,
    const egl::Stream::GLTextureDescription &desc)
{
    return new TextureStorage11_External(this, stream, desc);
}

TextureStorage *Renderer11::createTextureStorage2D(GLenum internalformat,
                                                   bool renderTarget,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   int levels,
                                                   bool hintLevelZeroOnly)
{
    return new TextureStorage11_2D(this, internalformat, renderTarget, width, height, levels,
                                   hintLevelZeroOnly);
}

TextureStorage *Renderer11::createTextureStorageCube(GLenum internalformat,
                                                     bool renderTarget,
                                                     int size,
                                                     int levels,
                                                     bool hintLevelZeroOnly)
{
    return new TextureStorage11_Cube(this, internalformat, renderTarget, size, levels,
                                     hintLevelZeroOnly);
}

TextureStorage *Renderer11::createTextureStorage3D(GLenum internalformat,
                                                   bool renderTarget,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   int levels)
{
    return new TextureStorage11_3D(this, internalformat, renderTarget, width, height, depth,
                                   levels);
}

TextureStorage *Renderer11::createTextureStorage2DArray(GLenum internalformat,
                                                        bool renderTarget,
                                                        GLsizei width,
                                                        GLsizei height,
                                                        GLsizei depth,
                                                        int levels)
{
    return new TextureStorage11_2DArray(this, internalformat, renderTarget, width, height, depth,
                                        levels);
}

TextureStorage *Renderer11::createTextureStorage2DMultisample(GLenum internalformat,
                                                              GLsizei width,
                                                              GLsizei height,
                                                              int levels,
                                                              int samples,
                                                              bool fixedSampleLocations)
{
    return new TextureStorage11_2DMultisample(this, internalformat, width, height, levels, samples,
                                              fixedSampleLocations);
}

TextureStorage *Renderer11::createTextureStorage2DMultisampleArray(GLenum internalformat,
                                                                   GLsizei width,
                                                                   GLsizei height,
                                                                   GLsizei depth,
                                                                   int levels,
                                                                   int samples,
                                                                   bool fixedSampleLocations)
{
    return new TextureStorage11_2DMultisampleArray(this, internalformat, width, height, depth,
                                                   levels, samples, fixedSampleLocations);
}

angle::Result Renderer11::readFromAttachment(const gl::Context *context,
                                             const gl::FramebufferAttachment &srcAttachment,
                                             const gl::Rectangle &sourceArea,
                                             GLenum format,
                                             GLenum type,
                                             GLuint outputPitch,
                                             const gl::PixelPackState &pack,
                                             uint8_t *pixelsOut)
{
    ASSERT(sourceArea.width >= 0);
    ASSERT(sourceArea.height >= 0);

    const bool invertTexture = UsePresentPathFast(this, &srcAttachment);

    RenderTarget11 *rt11 = nullptr;
    ANGLE_TRY(srcAttachment.getRenderTarget(context, &rt11));
    ASSERT(rt11->getTexture().valid());

    const TextureHelper11 &textureHelper = rt11->getTexture();
    unsigned int sourceSubResource       = rt11->getSubresourceIndex();

    const gl::Extents &texSize = textureHelper.getExtents();

    gl::Rectangle actualArea = sourceArea;
    bool reverseRowOrder     = pack.reverseRowOrder;
    if (invertTexture)
    {
        actualArea.y    = texSize.height - actualArea.y - actualArea.height;
        reverseRowOrder = !reverseRowOrder;
    }

    // Clamp read region to the defined texture boundaries, preventing out of bounds reads
    // and reads of uninitialized data.
    gl::Rectangle safeArea;
    safeArea.x = gl::clamp(actualArea.x, 0, texSize.width);
    safeArea.y = gl::clamp(actualArea.y, 0, texSize.height);
    safeArea.width =
        gl::clamp(actualArea.width + std::min(actualArea.x, 0), 0, texSize.width - safeArea.x);
    safeArea.height =
        gl::clamp(actualArea.height + std::min(actualArea.y, 0), 0, texSize.height - safeArea.y);

    ASSERT(safeArea.x >= 0 && safeArea.y >= 0);
    ASSERT(safeArea.x + safeArea.width <= texSize.width);
    ASSERT(safeArea.y + safeArea.height <= texSize.height);

    if (safeArea.width == 0 || safeArea.height == 0)
    {
        // no work to do
        return angle::Result::Continue;
    }

    gl::Extents safeSize(safeArea.width, safeArea.height, 1);
    TextureHelper11 stagingHelper;
    ANGLE_TRY(createStagingTexture(context, textureHelper.getTextureType(),
                                   textureHelper.getFormatSet(), safeSize, StagingAccess::READ,
                                   &stagingHelper));
    stagingHelper.setDebugName("readFromAttachment::stagingHelper");

    TextureHelper11 resolvedTextureHelper;

    // "srcTexture" usually points to the source texture.
    // For 2D multisampled textures, it points to the multisampled resolve texture.
    const TextureHelper11 *srcTexture = &textureHelper;

    if (textureHelper.is2D() && textureHelper.getSampleCount() > 1)
    {
        D3D11_TEXTURE2D_DESC resolveDesc;
        resolveDesc.Width              = static_cast<UINT>(texSize.width);
        resolveDesc.Height             = static_cast<UINT>(texSize.height);
        resolveDesc.MipLevels          = 1;
        resolveDesc.ArraySize          = 1;
        resolveDesc.Format             = textureHelper.getFormat();
        resolveDesc.SampleDesc.Count   = 1;
        resolveDesc.SampleDesc.Quality = 0;
        resolveDesc.Usage              = D3D11_USAGE_DEFAULT;
        resolveDesc.BindFlags          = 0;
        resolveDesc.CPUAccessFlags     = 0;
        resolveDesc.MiscFlags          = 0;

        ANGLE_TRY(allocateTexture(GetImplAs<Context11>(context), resolveDesc,
                                  textureHelper.getFormatSet(), &resolvedTextureHelper));
        resolvedTextureHelper.setDebugName("readFromAttachment::resolvedTextureHelper");

        mDeviceContext->ResolveSubresource(resolvedTextureHelper.get(), 0, textureHelper.get(),
                                           sourceSubResource, textureHelper.getFormat());

        sourceSubResource = 0;
        srcTexture        = &resolvedTextureHelper;
    }

    D3D11_BOX srcBox;
    srcBox.left   = static_cast<UINT>(safeArea.x);
    srcBox.right  = static_cast<UINT>(safeArea.x + safeArea.width);
    srcBox.top    = static_cast<UINT>(safeArea.y);
    srcBox.bottom = static_cast<UINT>(safeArea.y + safeArea.height);

    // Select the correct layer from a 3D attachment
    srcBox.front = 0;
    if (textureHelper.is3D())
    {
        srcBox.front = static_cast<UINT>(srcAttachment.layer());
    }
    srcBox.back = srcBox.front + 1;

    mDeviceContext->CopySubresourceRegion(stagingHelper.get(), 0, 0, 0, 0, srcTexture->get(),
                                          sourceSubResource, &srcBox);

    const angle::Format &angleFormat = GetFormatFromFormatType(format, type);
    gl::Buffer *packBuffer = context->getState().getTargetBuffer(gl::BufferBinding::PixelPack);

    PackPixelsParams packParams(safeArea, angleFormat, outputPitch, reverseRowOrder, packBuffer, 0);
    return packPixels(context, stagingHelper, packParams, pixelsOut);
}

angle::Result Renderer11::packPixels(const gl::Context *context,
                                     const TextureHelper11 &textureHelper,
                                     const PackPixelsParams &params,
                                     uint8_t *pixelsOut)
{
    ID3D11Resource *readResource = textureHelper.get();

    D3D11_MAPPED_SUBRESOURCE mapping;
    ANGLE_TRY(mapResource(context, readResource, 0, D3D11_MAP_READ, 0, &mapping));

    uint8_t *source = static_cast<uint8_t *>(mapping.pData);
    int inputPitch  = static_cast<int>(mapping.RowPitch);

    const auto &formatInfo = textureHelper.getFormatSet();
    ASSERT(formatInfo.format().glInternalFormat != GL_NONE);

    PackPixels(params, formatInfo.format(), inputPitch, source, pixelsOut);

    mDeviceContext->Unmap(readResource, 0);

    return angle::Result::Continue;
}

angle::Result Renderer11::blitRenderbufferRect(const gl::Context *context,
                                               const gl::Rectangle &readRectIn,
                                               const gl::Rectangle &drawRectIn,
                                               RenderTargetD3D *readRenderTarget,
                                               RenderTargetD3D *drawRenderTarget,
                                               GLenum filter,
                                               const gl::Rectangle *scissor,
                                               bool colorBlit,
                                               bool depthBlit,
                                               bool stencilBlit)
{
    // Since blitRenderbufferRect is called for each render buffer that needs to be blitted,
    // it should never be the case that both color and depth/stencil need to be blitted at
    // at the same time.
    ASSERT(colorBlit != (depthBlit || stencilBlit));

    RenderTarget11 *drawRenderTarget11 = GetAs<RenderTarget11>(drawRenderTarget);
    ASSERT(drawRenderTarget11);

    const TextureHelper11 &drawTexture = drawRenderTarget11->getTexture();
    unsigned int drawSubresource       = drawRenderTarget11->getSubresourceIndex();

    RenderTarget11 *readRenderTarget11 = GetAs<RenderTarget11>(readRenderTarget);
    ASSERT(readRenderTarget11);

    TextureHelper11 readTexture;
    unsigned int readSubresource = 0;
    d3d11::SharedSRV readSRV;

    if (readRenderTarget->isMultisampled())
    {
        ANGLE_TRY(resolveMultisampledTexture(context, readRenderTarget11, depthBlit, stencilBlit,
                                             &readTexture));

        if (!stencilBlit)
        {
            const auto &readFormatSet = readTexture.getFormatSet();

            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
            viewDesc.Format                    = readFormatSet.srvFormat;
            viewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipLevels       = 1;
            viewDesc.Texture2D.MostDetailedMip = 0;

            ANGLE_TRY(allocateResource(GetImplAs<Context11>(context), viewDesc, readTexture.get(),
                                       &readSRV));
        }
    }
    else
    {
        ASSERT(readRenderTarget11);
        readTexture     = readRenderTarget11->getTexture();
        readSubresource = readRenderTarget11->getSubresourceIndex();
        readSRV         = readRenderTarget11->getBlitShaderResourceView(context).makeCopy();
        if (!readSRV.valid())
        {
            ASSERT(depthBlit || stencilBlit);
            readSRV = readRenderTarget11->getShaderResourceView(context).makeCopy();
        }
        ASSERT(readSRV.valid());
    }

    // Stencil blits don't use shaders.
    ASSERT(readSRV.valid() || stencilBlit);

    const gl::Extents readSize(readRenderTarget->getWidth(), readRenderTarget->getHeight(), 1);
    const gl::Extents drawSize(drawRenderTarget->getWidth(), drawRenderTarget->getHeight(), 1);

    // From the spec:
    // "The actual region taken from the read framebuffer is limited to the intersection of the
    // source buffers being transferred, which may include the color buffer selected by the read
    // buffer, the depth buffer, and / or the stencil buffer depending on mask."
    // This means negative x and y are out of bounds, and not to be read from. We handle this here
    // by internally scaling the read and draw rectangles.

    // Remove reversal from readRect to simplify further operations.
    gl::Rectangle readRect = readRectIn;
    gl::Rectangle drawRect = drawRectIn;
    if (readRect.isReversedX())
    {
        readRect.x     = readRect.x + readRect.width;
        readRect.width = -readRect.width;
        drawRect.x     = drawRect.x + drawRect.width;
        drawRect.width = -drawRect.width;
    }
    if (readRect.isReversedY())
    {
        readRect.y      = readRect.y + readRect.height;
        readRect.height = -readRect.height;
        drawRect.y      = drawRect.y + drawRect.height;
        drawRect.height = -drawRect.height;
    }

    gl::Rectangle readBounds(0, 0, readSize.width, readSize.height);
    gl::Rectangle inBoundsReadRect;
    if (!gl::ClipRectangle(readRect, readBounds, &inBoundsReadRect))
    {
        return angle::Result::Continue;
    }

    {
        // Calculate the drawRect that corresponds to inBoundsReadRect.
        auto readToDrawX = [&drawRect, &readRect](int readOffset) {
            double readToDrawScale =
                static_cast<double>(drawRect.width) / static_cast<double>(readRect.width);
            return static_cast<int>(
                round(static_cast<double>(readOffset - readRect.x) * readToDrawScale) + drawRect.x);
        };
        auto readToDrawY = [&drawRect, &readRect](int readOffset) {
            double readToDrawScale =
                static_cast<double>(drawRect.height) / static_cast<double>(readRect.height);
            return static_cast<int>(
                round(static_cast<double>(readOffset - readRect.y) * readToDrawScale) + drawRect.y);
        };

        gl::Rectangle drawRectMatchingInBoundsReadRect;
        drawRectMatchingInBoundsReadRect.x = readToDrawX(inBoundsReadRect.x);
        drawRectMatchingInBoundsReadRect.y = readToDrawY(inBoundsReadRect.y);
        drawRectMatchingInBoundsReadRect.width =
            readToDrawX(inBoundsReadRect.x1()) - drawRectMatchingInBoundsReadRect.x;
        drawRectMatchingInBoundsReadRect.height =
            readToDrawY(inBoundsReadRect.y1()) - drawRectMatchingInBoundsReadRect.y;
        drawRect = drawRectMatchingInBoundsReadRect;
        readRect = inBoundsReadRect;
    }

    bool scissorNeeded = false;
    if (scissor)
    {
        gl::Rectangle scissoredDrawRect;
        if (!gl::ClipRectangle(drawRect, *scissor, &scissoredDrawRect))
        {
            return angle::Result::Continue;
        }
        scissorNeeded = scissoredDrawRect != drawRect;
    }

    const auto &destFormatInfo =
        gl::GetSizedInternalFormatInfo(drawRenderTarget->getInternalFormat());
    const auto &srcFormatInfo =
        gl::GetSizedInternalFormatInfo(readRenderTarget->getInternalFormat());
    const auto &formatSet    = drawRenderTarget11->getFormatSet();
    const auto &nativeFormat = formatSet.format();

    // Some blits require masking off emulated texture channels. eg: from RGBA8 to RGB8, we
    // emulate RGB8 with RGBA8, so we need to mask off the alpha channel when we copy.

    gl::Color<bool> colorMask;
    colorMask.red =
        (srcFormatInfo.redBits > 0) && (destFormatInfo.redBits == 0) && (nativeFormat.redBits > 0);
    colorMask.green = (srcFormatInfo.greenBits > 0) && (destFormatInfo.greenBits == 0) &&
                      (nativeFormat.greenBits > 0);
    colorMask.blue = (srcFormatInfo.blueBits > 0) && (destFormatInfo.blueBits == 0) &&
                     (nativeFormat.blueBits > 0);
    colorMask.alpha = (srcFormatInfo.alphaBits > 0) && (destFormatInfo.alphaBits == 0) &&
                      (nativeFormat.alphaBits > 0);

    // We only currently support masking off the alpha channel.
    bool colorMaskingNeeded = colorMask.alpha;
    ASSERT(!colorMask.red && !colorMask.green && !colorMask.blue);

    bool wholeBufferCopy = !scissorNeeded && !colorMaskingNeeded && readRect.x == 0 &&
                           readRect.width == readSize.width && readRect.y == 0 &&
                           readRect.height == readSize.height && drawRect.x == 0 &&
                           drawRect.width == drawSize.width && drawRect.y == 0 &&
                           drawRect.height == drawSize.height;

    bool stretchRequired = readRect.width != drawRect.width || readRect.height != drawRect.height;

    ASSERT(!readRect.isReversedX() && !readRect.isReversedY());
    bool reversalRequired = drawRect.isReversedX() || drawRect.isReversedY();

    bool outOfBounds = readRect.x < 0 || readRect.x + readRect.width > readSize.width ||
                       readRect.y < 0 || readRect.y + readRect.height > readSize.height ||
                       drawRect.x < 0 || drawRect.x + drawRect.width > drawSize.width ||
                       drawRect.y < 0 || drawRect.y + drawRect.height > drawSize.height;

    bool partialDSBlit =
        (nativeFormat.depthBits > 0 && depthBlit) != (nativeFormat.stencilBits > 0 && stencilBlit);

    if (readRenderTarget11->getFormatSet().formatID ==
            drawRenderTarget11->getFormatSet().formatID &&
        !stretchRequired && !outOfBounds && !reversalRequired && !partialDSBlit &&
        !colorMaskingNeeded && (!(depthBlit || stencilBlit) || wholeBufferCopy))
    {
        UINT dstX = drawRect.x;
        UINT dstY = drawRect.y;

        D3D11_BOX readBox;
        readBox.left   = readRect.x;
        readBox.right  = readRect.x + readRect.width;
        readBox.top    = readRect.y;
        readBox.bottom = readRect.y + readRect.height;
        readBox.front  = 0;
        readBox.back   = 1;

        if (scissorNeeded)
        {
            // drawRect is guaranteed to have positive width and height because stretchRequired is
            // false.
            ASSERT(drawRect.width >= 0 || drawRect.height >= 0);

            if (drawRect.x < scissor->x)
            {
                dstX = scissor->x;
                readBox.left += (scissor->x - drawRect.x);
            }
            if (drawRect.y < scissor->y)
            {
                dstY = scissor->y;
                readBox.top += (scissor->y - drawRect.y);
            }
            if (drawRect.x + drawRect.width > scissor->x + scissor->width)
            {
                readBox.right -= ((drawRect.x + drawRect.width) - (scissor->x + scissor->width));
            }
            if (drawRect.y + drawRect.height > scissor->y + scissor->height)
            {
                readBox.bottom -= ((drawRect.y + drawRect.height) - (scissor->y + scissor->height));
            }
        }

        // D3D11 needs depth-stencil CopySubresourceRegions to have a NULL pSrcBox
        // We also require complete framebuffer copies for depth-stencil blit.
        D3D11_BOX *pSrcBox = wholeBufferCopy ? nullptr : &readBox;

        mDeviceContext->CopySubresourceRegion(drawTexture.get(), drawSubresource, dstX, dstY, 0,
                                              readTexture.get(), readSubresource, pSrcBox);
    }
    else
    {
        gl::Box readArea(readRect.x, readRect.y, 0, readRect.width, readRect.height, 1);
        gl::Box drawArea(drawRect.x, drawRect.y, 0, drawRect.width, drawRect.height, 1);

        if (depthBlit && stencilBlit)
        {
            ANGLE_TRY(mBlit->copyDepthStencil(context, readTexture, readSubresource, readArea,
                                              readSize, drawTexture, drawSubresource, drawArea,
                                              drawSize, scissor));
        }
        else if (depthBlit)
        {
            const d3d11::DepthStencilView &drawDSV = drawRenderTarget11->getDepthStencilView();
            ASSERT(readSRV.valid());
            ANGLE_TRY(mBlit->copyDepth(context, readSRV, readArea, readSize, drawDSV, drawArea,
                                       drawSize, scissor));
        }
        else if (stencilBlit)
        {
            ANGLE_TRY(mBlit->copyStencil(context, readTexture, readSubresource, readArea, readSize,
                                         drawTexture, drawSubresource, drawArea, drawSize,
                                         scissor));
        }
        else
        {
            const d3d11::RenderTargetView &drawRTV = drawRenderTarget11->getRenderTargetView();

            // We don't currently support masking off any other channel than alpha
            bool maskOffAlpha = colorMaskingNeeded && colorMask.alpha;
            ASSERT(readSRV.valid());
            ANGLE_TRY(mBlit->copyTexture(context, readSRV, readArea, readSize, srcFormatInfo.format,
                                         drawRTV, drawArea, drawSize, scissor,
                                         destFormatInfo.format, GL_NONE, filter, maskOffAlpha,
                                         false, false));
        }
    }

    return angle::Result::Continue;
}

bool Renderer11::isES3Capable() const
{
    return (d3d11_gl::GetMaximumClientVersion(mRenderer11DeviceCaps.featureLevel).major > 2);
}

RendererClass Renderer11::getRendererClass() const
{
    return RENDERER_D3D11;
}

void Renderer11::onSwap()
{
    // Send histogram updates every half hour
    const double kHistogramUpdateInterval = 30 * 60;

    auto *platform                   = ANGLEPlatformCurrent();
    const double currentTime         = platform->monotonicallyIncreasingTime(platform);
    const double timeSinceLastUpdate = currentTime - mLastHistogramUpdateTime;

    if (timeSinceLastUpdate > kHistogramUpdateInterval)
    {
        updateHistograms();
        mLastHistogramUpdateTime = currentTime;
    }
}

void Renderer11::updateHistograms()
{
    // Update the buffer CPU memory histogram
    {
        size_t sizeSum = 0;
        for (const Buffer11 *buffer : mAliveBuffers)
        {
            sizeSum += buffer->getTotalCPUBufferMemoryBytes();
        }
        const int kOneMegaByte = 1024 * 1024;
        ANGLE_HISTOGRAM_MEMORY_MB("GPU.ANGLE.Buffer11CPUMemoryMB",
                                  static_cast<int>(sizeSum) / kOneMegaByte);
    }
}

void Renderer11::onBufferCreate(const Buffer11 *created)
{
    mAliveBuffers.insert(created);
}

void Renderer11::onBufferDelete(const Buffer11 *deleted)
{
    mAliveBuffers.erase(deleted);
}

angle::Result Renderer11::resolveMultisampledTexture(const gl::Context *context,
                                                     RenderTarget11 *renderTarget,
                                                     bool depth,
                                                     bool stencil,
                                                     TextureHelper11 *textureOut)
{
    if (depth && !stencil)
    {
        return mBlit->resolveDepth(context, renderTarget, textureOut);
    }

    if (stencil)
    {
        return mBlit->resolveStencil(context, renderTarget, depth, textureOut);
    }

    const auto &formatSet = renderTarget->getFormatSet();

    ASSERT(renderTarget->isMultisampled());
    const d3d11::SharedSRV &sourceSRV = renderTarget->getShaderResourceView(context);
    D3D11_SHADER_RESOURCE_VIEW_DESC sourceSRVDesc;
    sourceSRV.get()->GetDesc(&sourceSRVDesc);
    ASSERT(sourceSRVDesc.ViewDimension == D3D_SRV_DIMENSION_TEXTURE2DMS ||
           sourceSRVDesc.ViewDimension == D3D_SRV_DIMENSION_TEXTURE2DMSARRAY);

    if (!mCachedResolveTexture.valid() ||
        mCachedResolveTexture.getExtents().width != renderTarget->getWidth() ||
        mCachedResolveTexture.getExtents().height != renderTarget->getHeight() ||
        mCachedResolveTexture.getFormat() != formatSet.texFormat)
    {
        D3D11_TEXTURE2D_DESC resolveDesc;
        resolveDesc.Width              = renderTarget->getWidth();
        resolveDesc.Height             = renderTarget->getHeight();
        resolveDesc.MipLevels          = 1;
        resolveDesc.ArraySize          = 1;
        resolveDesc.Format             = formatSet.texFormat;
        resolveDesc.SampleDesc.Count   = 1;
        resolveDesc.SampleDesc.Quality = 0;
        resolveDesc.Usage              = D3D11_USAGE_DEFAULT;
        resolveDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        resolveDesc.CPUAccessFlags     = 0;
        resolveDesc.MiscFlags          = 0;

        ANGLE_TRY(allocateTexture(GetImplAs<Context11>(context), resolveDesc, formatSet,
                                  &mCachedResolveTexture));
    }

    mDeviceContext->ResolveSubresource(mCachedResolveTexture.get(), 0,
                                       renderTarget->getTexture().get(),
                                       renderTarget->getSubresourceIndex(), formatSet.texFormat);
    *textureOut = mCachedResolveTexture;
    return angle::Result::Continue;
}

bool Renderer11::getLUID(LUID *adapterLuid) const
{
    adapterLuid->HighPart = 0;
    adapterLuid->LowPart  = 0;

    if (!mDxgiAdapter)
    {
        return false;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    if (FAILED(mDxgiAdapter->GetDesc(&adapterDesc)))
    {
        return false;
    }

    *adapterLuid = adapterDesc.AdapterLuid;
    return true;
}

VertexConversionType Renderer11::getVertexConversionType(angle::FormatID vertexFormatID) const
{
    return d3d11::GetVertexFormatInfo(vertexFormatID, mRenderer11DeviceCaps.featureLevel)
        .conversionType;
}

GLenum Renderer11::getVertexComponentType(angle::FormatID vertexFormatID) const
{
    const auto &format =
        d3d11::GetVertexFormatInfo(vertexFormatID, mRenderer11DeviceCaps.featureLevel);
    return d3d11::GetComponentType(format.nativeFormat);
}

angle::Result Renderer11::getVertexSpaceRequired(const gl::Context *context,
                                                 const gl::VertexAttribute &attrib,
                                                 const gl::VertexBinding &binding,
                                                 size_t count,
                                                 GLsizei instances,
                                                 unsigned int *bytesRequiredOut) const
{
    if (!attrib.enabled)
    {
        *bytesRequiredOut = 16u;
        return angle::Result::Continue;
    }

    unsigned int elementCount  = 0;
    const unsigned int divisor = binding.getDivisor();
    if (instances == 0 || divisor == 0)
    {
        // This could be a clipped cast.
        elementCount = gl::clampCast<unsigned int>(count);
    }
    else
    {
        // Round up to divisor, if possible
        elementCount = UnsignedCeilDivide(static_cast<unsigned int>(instances), divisor);
    }

    ASSERT(elementCount > 0);

    angle::FormatID formatID             = gl::GetVertexFormatID(attrib);
    const D3D_FEATURE_LEVEL featureLevel = mRenderer11DeviceCaps.featureLevel;
    const d3d11::VertexFormat &vertexFormatInfo =
        d3d11::GetVertexFormatInfo(formatID, featureLevel);
    const d3d11::DXGIFormatSize &dxgiFormatInfo =
        d3d11::GetDXGIFormatSizeInfo(vertexFormatInfo.nativeFormat);
    unsigned int elementSize = dxgiFormatInfo.pixelBytes;
    bool check = (elementSize > std::numeric_limits<unsigned int>::max() / elementCount);
    ANGLE_CHECK(GetImplAs<Context11>(context), !check,
                "New vertex buffer size would result in an overflow.", GL_OUT_OF_MEMORY);

    *bytesRequiredOut = elementSize * elementCount;
    return angle::Result::Continue;
}

void Renderer11::generateCaps(gl::Caps *outCaps,
                              gl::TextureCapsMap *outTextureCaps,
                              gl::Extensions *outExtensions,
                              gl::Limitations *outLimitations) const
{
    d3d11_gl::GenerateCaps(mDevice, mDeviceContext, mRenderer11DeviceCaps, getWorkarounds(),
                           outCaps, outTextureCaps, outExtensions, outLimitations);
}

angle::WorkaroundsD3D Renderer11::generateWorkarounds() const
{
    return d3d11::GenerateWorkarounds(mRenderer11DeviceCaps, mAdapterDescription);
}

DeviceImpl *Renderer11::createEGLDevice()
{
    return new DeviceD3D(EGL_D3D11_DEVICE_ANGLE, mDevice);
}

ContextImpl *Renderer11::createContext(const gl::State &state, gl::ErrorSet *errorSet)
{
    return new Context11(state, errorSet, this);
}

FramebufferImpl *Renderer11::createDefaultFramebuffer(const gl::FramebufferState &state)
{
    return new Framebuffer11(state, this);
}

angle::Result Renderer11::getScratchMemoryBuffer(Context11 *context11,
                                                 size_t requestedSize,
                                                 angle::MemoryBuffer **bufferOut)
{
    ANGLE_CHECK_GL_ALLOC(context11, mScratchMemoryBuffer.get(requestedSize, bufferOut));
    return angle::Result::Continue;
}

gl::Version Renderer11::getMaxSupportedESVersion() const
{
    return d3d11_gl::GetMaximumClientVersion(mRenderer11DeviceCaps.featureLevel);
}

gl::DebugAnnotator *Renderer11::getAnnotator()
{
    return mAnnotator;
}

angle::Result Renderer11::dispatchCompute(const gl::Context *context,
                                          GLuint numGroupsX,
                                          GLuint numGroupsY,
                                          GLuint numGroupsZ)
{
    ANGLE_TRY(mStateManager.updateStateForCompute(context, numGroupsX, numGroupsY, numGroupsZ));
    mDeviceContext->Dispatch(numGroupsX, numGroupsY, numGroupsZ);

    return angle::Result::Continue;
}
angle::Result Renderer11::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    const auto &glState          = context->getState();
    auto *dispatchIndirectBuffer = glState.getTargetBuffer(gl::BufferBinding::DispatchIndirect);
    ASSERT(dispatchIndirectBuffer);

    Buffer11 *storage         = GetImplAs<Buffer11>(dispatchIndirectBuffer);
    const uint8_t *bufferData = nullptr;
    // TODO(jie.a.chen@intel.com): num_groups_x,y,z have to be written into the driver constant
    // buffer for the built-in variable gl_NumWorkGroups. There is an opportunity for optimization
    // to use GPU->GPU copy instead.
    // http://anglebug.com/2807
    ANGLE_TRY(storage->getData(context, &bufferData));
    const GLuint *groups = reinterpret_cast<const GLuint *>(bufferData + indirect);
    ANGLE_TRY(mStateManager.updateStateForCompute(context, groups[0], groups[1], groups[2]));

    ID3D11Buffer *buffer = nullptr;
    ANGLE_TRY(storage->getBuffer(context, BUFFER_USAGE_INDIRECT, &buffer));

    mDeviceContext->DispatchIndirect(buffer, static_cast<UINT>(indirect));
    return angle::Result::Continue;
}

angle::Result Renderer11::createStagingTexture(const gl::Context *context,
                                               ResourceType textureType,
                                               const d3d11::Format &formatSet,
                                               const gl::Extents &size,
                                               StagingAccess readAndWriteAccess,
                                               TextureHelper11 *textureOut)
{
    Context11 *context11 = GetImplAs<Context11>(context);

    if (textureType == ResourceType::Texture2D)
    {
        D3D11_TEXTURE2D_DESC stagingDesc;
        stagingDesc.Width              = size.width;
        stagingDesc.Height             = size.height;
        stagingDesc.MipLevels          = 1;
        stagingDesc.ArraySize          = 1;
        stagingDesc.Format             = formatSet.texFormat;
        stagingDesc.SampleDesc.Count   = 1;
        stagingDesc.SampleDesc.Quality = 0;
        stagingDesc.Usage              = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags          = 0;
        stagingDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags          = 0;

        if (readAndWriteAccess == StagingAccess::READ_WRITE)
        {
            stagingDesc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }

        ANGLE_TRY(allocateTexture(context11, stagingDesc, formatSet, textureOut));
        return angle::Result::Continue;
    }
    ASSERT(textureType == ResourceType::Texture3D);

    D3D11_TEXTURE3D_DESC stagingDesc;
    stagingDesc.Width          = size.width;
    stagingDesc.Height         = size.height;
    stagingDesc.Depth          = 1;
    stagingDesc.MipLevels      = 1;
    stagingDesc.Format         = formatSet.texFormat;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags      = 0;

    ANGLE_TRY(allocateTexture(context11, stagingDesc, formatSet, textureOut));
    return angle::Result::Continue;
}

angle::Result Renderer11::allocateTexture(d3d::Context *context,
                                          const D3D11_TEXTURE2D_DESC &desc,
                                          const d3d11::Format &format,
                                          const D3D11_SUBRESOURCE_DATA *initData,
                                          TextureHelper11 *textureOut)
{
    d3d11::Texture2D texture;
    ANGLE_TRY(mResourceManager11.allocate(context, this, &desc, initData, &texture));
    textureOut->init(std::move(texture), desc, format);
    return angle::Result::Continue;
}

angle::Result Renderer11::allocateTexture(d3d::Context *context,
                                          const D3D11_TEXTURE3D_DESC &desc,
                                          const d3d11::Format &format,
                                          const D3D11_SUBRESOURCE_DATA *initData,
                                          TextureHelper11 *textureOut)
{
    d3d11::Texture3D texture;
    ANGLE_TRY(mResourceManager11.allocate(context, this, &desc, initData, &texture));
    textureOut->init(std::move(texture), desc, format);
    return angle::Result::Continue;
}

angle::Result Renderer11::getBlendState(const gl::Context *context,
                                        const d3d11::BlendStateKey &key,
                                        const d3d11::BlendState **outBlendState)
{
    return mStateCache.getBlendState(context, this, key, outBlendState);
}

angle::Result Renderer11::getRasterizerState(const gl::Context *context,
                                             const gl::RasterizerState &rasterState,
                                             bool scissorEnabled,
                                             ID3D11RasterizerState **outRasterizerState)
{
    return mStateCache.getRasterizerState(context, this, rasterState, scissorEnabled,
                                          outRasterizerState);
}

angle::Result Renderer11::getDepthStencilState(const gl::Context *context,
                                               const gl::DepthStencilState &dsState,
                                               const d3d11::DepthStencilState **outDSState)
{
    return mStateCache.getDepthStencilState(context, this, dsState, outDSState);
}

angle::Result Renderer11::getSamplerState(const gl::Context *context,
                                          const gl::SamplerState &samplerState,
                                          ID3D11SamplerState **outSamplerState)
{
    return mStateCache.getSamplerState(context, this, samplerState, outSamplerState);
}

angle::Result Renderer11::clearRenderTarget(const gl::Context *context,
                                            RenderTargetD3D *renderTarget,
                                            const gl::ColorF &clearColorValue,
                                            const float clearDepthValue,
                                            const unsigned int clearStencilValue)
{
    RenderTarget11 *rt11 = GetAs<RenderTarget11>(renderTarget);

    if (rt11->getFormatSet().dsvFormat != DXGI_FORMAT_UNKNOWN)
    {
        ASSERT(rt11->getDepthStencilView().valid());

        const auto &format    = rt11->getFormatSet();
        const UINT clearFlags = (format.format().depthBits > 0 ? D3D11_CLEAR_DEPTH : 0) |
                                (format.format().stencilBits ? D3D11_CLEAR_STENCIL : 0);
        mDeviceContext->ClearDepthStencilView(rt11->getDepthStencilView().get(), clearFlags,
                                              clearDepthValue,
                                              static_cast<UINT8>(clearStencilValue));
        return angle::Result::Continue;
    }

    ASSERT(rt11->getRenderTargetView().valid());
    ID3D11RenderTargetView *rtv = rt11->getRenderTargetView().get();

    // There are complications with some types of RTV and FL 9_3 with ClearRenderTargetView.
    // See https://msdn.microsoft.com/en-us/library/windows/desktop/ff476388(v=vs.85).aspx
    ASSERT(mRenderer11DeviceCaps.featureLevel > D3D_FEATURE_LEVEL_9_3 || !IsArrayRTV(rtv));

    const auto &d3d11Format = rt11->getFormatSet();
    const auto &glFormat    = gl::GetSizedInternalFormatInfo(renderTarget->getInternalFormat());

    gl::ColorF safeClearColor = clearColorValue;

    if (d3d11Format.format().alphaBits > 0 && glFormat.alphaBits == 0)
    {
        safeClearColor.alpha = 1.0f;
    }

    mDeviceContext->ClearRenderTargetView(rtv, &safeClearColor.red);
    return angle::Result::Continue;
}

bool Renderer11::canSelectViewInVertexShader() const
{
    return !getWorkarounds().selectViewInGeometryShader &&
           getRenderer11DeviceCaps().supportsVpRtIndexWriteFromVertexShader;
}

angle::Result Renderer11::mapResource(const gl::Context *context,
                                      ID3D11Resource *resource,
                                      UINT subResource,
                                      D3D11_MAP mapType,
                                      UINT mapFlags,
                                      D3D11_MAPPED_SUBRESOURCE *mappedResource)
{
    HRESULT hr = mDeviceContext->Map(resource, subResource, mapType, mapFlags, mappedResource);
    ANGLE_TRY_HR(GetImplAs<Context11>(context), hr, "Failed to map D3D11 resource.");
    return angle::Result::Continue;
}

angle::Result Renderer11::markTransformFeedbackUsage(const gl::Context *context)
{
    const gl::State &glState                       = context->getState();
    const gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();
    for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &binding =
            transformFeedback->getIndexedBuffer(i);
        if (binding.get() != nullptr)
        {
            BufferD3D *bufferD3D = GetImplAs<BufferD3D>(binding.get());
            ANGLE_TRY(bufferD3D->markTransformFeedbackUsage(context));
        }
    }

    return angle::Result::Continue;
}

angle::Result Renderer11::getIncompleteTexture(const gl::Context *context,
                                               gl::TextureType type,
                                               gl::Texture **textureOut)
{
    return GetImplAs<Context11>(context)->getIncompleteTexture(context, type, textureOut);
}
}  // namespace rx
