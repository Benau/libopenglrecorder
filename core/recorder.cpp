/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#include "audio/vorbis_encoder.hpp"
#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"
#include "video/mjpeg_writer.hpp"
#include "video/openh264_encoder.hpp"
#include "video/vpx_encoder.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <cstring>

// ============================================================================
ogrFucReadPixels ogrReadPixels = NULL;
ogrFucGenBuffers ogrGenBuffers = NULL;
ogrFucBindBuffer ogrBindBuffer = NULL;
ogrFucBufferData ogrBufferData = NULL;
ogrFucDeleteBuffers ogrDeleteBuffers = NULL;
ogrFucMapBuffer ogrMapBuffer = NULL;
ogrFucMapBufferRange ogrMapBufferRange = NULL;
ogrFucUnmapBuffer ogrUnmapBuffer = NULL;
// ============================================================================
std::unique_ptr<RecorderConfig> g_recorder_config(nullptr);
// ============================================================================
std::unique_ptr<CaptureLibrary> g_capture_library(nullptr);
// ============================================================================
std::string g_saved_name;
// ============================================================================
StringCallback g_cb_saved_rec = NULL;
// ============================================================================
IntCallback g_cb_progress_rec = NULL;
// ============================================================================
GeneralCallback g_cb_start_rec = NULL;
// ============================================================================
StringCallback g_cb_error_rec = NULL;
// ============================================================================
std::array<void*, OGR_CBT_COUNT> g_all_user_data;
// ============================================================================
bool validateConfig(RecorderConfig* rc)
{
    if (rc == NULL)
        return false;
    if (rc->m_triple_buffering > 1 || rc->m_record_audio > 1)
        return false;
    if (rc->m_width > 16384 || rc->m_height > 16384)
        return false;
    if (rc->m_video_format >= OGR_VF_COUNT ||
        rc->m_audio_format >= OGR_AF_COUNT)
        return false;
    if (rc->m_audio_bitrate == 0 || rc->m_video_bitrate == 0 ||
        rc->m_record_fps == 0)
        return false;
    if (rc->m_record_jpg_quality > 100)
        return false;
    return true;
}   // validateConfig

// ----------------------------------------------------------------------------
int ogrInitConfig(RecorderConfig* rc)
{
    RecorderConfig* new_rc = new RecorderConfig;
    g_recorder_config.reset(new_rc);

    if (!validateConfig(rc))
    {
        new_rc->m_triple_buffering = 1;
        new_rc->m_record_audio = 0;
        new_rc->m_width = 800;
        new_rc->m_height = 600;
        new_rc->m_video_format = OGR_VF_MJPEG;
        new_rc->m_audio_format = OGR_AF_VORBIS;
        new_rc->m_audio_bitrate = 112000;
        new_rc->m_video_bitrate = 100000;
        new_rc->m_record_fps = 30;
        new_rc->m_record_jpg_quality = 90;
        return 0;
    }

    memcpy(new_rc, rc, sizeof(RecorderConfig));
    while (new_rc->m_width % 8 != 0)
    {
        new_rc->m_width--;
    }
    while (new_rc->m_height % 2 != 0)
    {
        new_rc->m_height--;
    }
    if (ogrCheckVideoEncoder(new_rc->m_video_format) == 0)
    {
        runCallback(OGR_CBT_ERROR_RECORDING, "Unsupported video format,"
            " fallback to MJPEG\n");
        new_rc->m_video_format = OGR_VF_MJPEG;
    }
    return 1;
}   // ogrInitConfig

// ----------------------------------------------------------------------------
RecorderConfig* getConfig()
{
    assert(g_recorder_config.get() != nullptr);
    return g_recorder_config.get();
}   // getConfig

// ----------------------------------------------------------------------------
void ogrSetSavedName(const char* name)
{
    if (g_capture_library.get() == nullptr ||
        !g_capture_library.get()->isCapturing())
        g_saved_name = name;
}   // ogrSetSavedName

// ----------------------------------------------------------------------------
const std::string& getSavedName()
{
    return g_saved_name;
}   // getSavedName

// ----------------------------------------------------------------------------
void ogrPrepareCapture(void)
{
    assert(g_recorder_config.get() != nullptr && !g_saved_name.empty() &&
        ogrReadPixels != NULL);
    if (g_capture_library.get() == nullptr)
    {
        assert(g_recorder_config.get() != nullptr);
        g_capture_library.reset(new CaptureLibrary(getConfig()));
    }
    g_capture_library.get()->reset();
}   // ogrPrepareCapture

// ----------------------------------------------------------------------------
void ogrCapture(void)
{
    if (g_capture_library.get() == nullptr)
        return;
    g_capture_library.get()->capture();
}   // ogrCapture

// ----------------------------------------------------------------------------
void ogrStopCapture(void)
{
    if (g_capture_library.get() == nullptr)
        return;
    g_capture_library.get()->stopCapture();
}   // ogrStopCapture

// ----------------------------------------------------------------------------
void ogrDestroy(void)
{
    delete g_capture_library.release();
}   // ogrDestroy

// ----------------------------------------------------------------------------
void ogrRegGeneralCallback(CallBackType cbt, GeneralCallback cb, void* data)
{
    switch (cbt)
    {
    case OGR_CBT_START_RECORDING:
        g_cb_start_rec = cb;
        g_all_user_data[OGR_CBT_START_RECORDING] = data;
        break;
    default:
        assert(false && "Wrong callback enum");
        break;
    }
}   // ogrRegGeneralCallback

// ----------------------------------------------------------------------------
void ogrRegStringCallback(CallBackType cbt, StringCallback cb, void* data)
{
    switch (cbt)
    {
    case OGR_CBT_SAVED_RECORDING:
        g_cb_saved_rec = cb;
        g_all_user_data[OGR_CBT_SAVED_RECORDING] = data;
        break;
    case OGR_CBT_ERROR_RECORDING:
        g_cb_error_rec = cb;
        g_all_user_data[OGR_CBT_ERROR_RECORDING] = data;
        break;
    default:
        assert(false && "Wrong callback enum");
        break;
    }
}   // ogrRegStringCallback

// ----------------------------------------------------------------------------
void ogrRegIntCallback(CallBackType cbt, IntCallback cb, void* data)
{
    switch (cbt)
    {
    case OGR_CBT_PROGRESS_RECORDING:
        g_cb_progress_rec = cb;
        g_all_user_data[OGR_CBT_PROGRESS_RECORDING] = data;
        break;
    default:
        assert(false && "Wrong callback enum");
        break;
    }
}   // ogrRegIntCallback

// ----------------------------------------------------------------------------
void runCallback(CallBackType cbt, const void* arg)
{
    switch (cbt)
    {
    case OGR_CBT_START_RECORDING:
    {
        if (g_cb_start_rec == NULL) return;
        g_cb_start_rec(g_all_user_data[OGR_CBT_START_RECORDING]);
        break;
    }
    case OGR_CBT_SAVED_RECORDING:
    {
        if (g_cb_saved_rec == NULL) return;
        const char* s = (const char*)arg;
        g_cb_saved_rec(s, g_all_user_data[OGR_CBT_SAVED_RECORDING]);
        break;
    }
    case OGR_CBT_ERROR_RECORDING:
    {
        if (g_cb_error_rec == NULL) return;
        const char* s = (const char*)arg;
        g_cb_error_rec(s, g_all_user_data[OGR_CBT_ERROR_RECORDING]);
        break;
    }
    case OGR_CBT_PROGRESS_RECORDING:
    {
        if (g_cb_progress_rec == NULL) return;
        const int* i = (const int*)arg;
        g_cb_progress_rec(*i, g_all_user_data[OGR_CBT_PROGRESS_RECORDING]);
        break;
    }
    default:
        break;
    }
}   // runCallback

// ----------------------------------------------------------------------------
int ogrCapturing(void)
{
    if (g_capture_library.get() == nullptr)
        return 0;
    return g_capture_library.get()->isCapturing() ? 1 : 0;
}   // ogrCapturing

// ----------------------------------------------------------------------------
void ogrRegReadPixelsFunction(ogrFucReadPixels read_pixels)
{
    assert(read_pixels != NULL);
    ogrReadPixels = read_pixels;
}   // ogrRegReadPixelsFunction

// ----------------------------------------------------------------------------
void ogrRegPBOFunctions(ogrFucGenBuffers gen_buffers,
                        ogrFucBindBuffer bind_buffer,
                        ogrFucBufferData buffer_data,
                        ogrFucDeleteBuffers delete_buffers,
                        ogrFucMapBuffer map_buffer,
                        ogrFucUnmapBuffer unmap_buffer)
{
    assert(gen_buffers != NULL);
    ogrGenBuffers = gen_buffers;
    assert(bind_buffer != NULL);
    ogrBindBuffer = bind_buffer;
    assert(buffer_data != NULL);
    ogrBufferData = buffer_data;
    assert(delete_buffers != NULL);
    ogrDeleteBuffers = delete_buffers;
    assert(map_buffer != NULL);
    ogrMapBuffer = map_buffer;
    assert(unmap_buffer != NULL);
    ogrUnmapBuffer = unmap_buffer;
}   // ogrRegPBOFunctions

// ----------------------------------------------------------------------------
void ogrRegPBOFunctionsRange(ogrFucGenBuffers gen_buffers,
                             ogrFucBindBuffer bind_buffer,
                             ogrFucBufferData buffer_data,
                             ogrFucDeleteBuffers delete_buffers,
                             ogrFucMapBufferRange map_buffer_range,
                             ogrFucUnmapBuffer unmap_buffer)
{
    assert(gen_buffers != NULL);
    ogrGenBuffers = gen_buffers;
    assert(bind_buffer != NULL);
    ogrBindBuffer = bind_buffer;
    assert(buffer_data != NULL);
    ogrBufferData = buffer_data;
    assert(delete_buffers != NULL);
    ogrDeleteBuffers = delete_buffers;
    assert(map_buffer_range != NULL);
    ogrMapBufferRange = map_buffer_range;
    assert(unmap_buffer != NULL);
    ogrUnmapBuffer = unmap_buffer;
}   // ogrRegPBOFunctions

// ----------------------------------------------------------------------------
/** This function sets the name of this thread in the debugger.
  *  \param name Name of the thread.
  */
#if defined(_MSC_VER)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

    void setThreadName(const char *name)
    {
        const DWORD MS_VC_EXCEPTION=0x406D1388;
#pragma pack(push,8)
        typedef struct tagTHREADNAME_INFO
        {
            DWORD dwType; // Must be 0x1000.
            LPCSTR szName; // Pointer to name (in user addr space).
            DWORD dwThreadID; // Thread ID (-1=caller thread).
            DWORD dwFlags; // Reserved for future use, must be zero.
        } THREADNAME_INFO;
#pragma pack(pop)

        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = -1;
        info.dwFlags = 0;

        __try
        {
            RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR),
                            (ULONG_PTR*)&info );
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }

    }   // setThreadName
#elif defined(__linux__) && defined(__GLIBC__) && defined(__GLIBC_MINOR__)
    void setThreadName(const char* name)
    {
#if __GLIBC__ > 2 || __GLIBC_MINOR__ > 11
        pthread_setname_np(pthread_self(), name);
#endif
    }   // setThreadName
#else
    void setThreadName(const char* name)
    {
    }   // setThreadName
#endif
// ----------------------------------------------------------------------------
int ogrCheckAudioEncoder(AudioFormat af)
{
    switch (af)
    {
    case OGR_AF_VORBIS:
        return Recorder::vorbisEncoder(NULL);
    default:
        return 0;
    }
}   // ogrCheckAudioEncoder

// ----------------------------------------------------------------------------
int ogrCheckVideoEncoder(VideoFormat vf)
{
    switch (vf)
    {
    case OGR_VF_VP8:
    case OGR_VF_VP9:
        return Recorder::vpxEncoder(NULL);
    case OGR_VF_MJPEG:
        return Recorder::mjpegWriter(NULL);
    case OGR_VF_H264:
        return Recorder::openh264Encoder(NULL);
    default:
        return 0;
    }
}   // ogrCheckVideoEncoder
