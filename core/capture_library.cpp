/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#include "core/capture_library.hpp"

#include "audio/pulseaudio_recorder.hpp"
#include "audio/wasapi_recorder.hpp"
#include "core/mkv_writer.hpp"
#include "core/recorder_private.hpp"
#include "video/mjpeg_writer.hpp"
#include "video/vpx_encoder.hpp"

const uint32_t E_GL_PIXEL_PACK_BUFFER = 0x88EB;
const uint32_t E_GL_STREAM_READ = 0x88E1;
const uint32_t E_GL_READ_ONLY = 0x88B8;
const uint32_t E_GL_RGBA = 0x1908;
const uint32_t E_GL_UNSIGNED_BYTE = 0x1401;

// ----------------------------------------------------------------------------
CaptureLibrary::CaptureLibrary(RecorderConfig* rc)
{
    m_recorder_cfg = rc;
    m_destroy.store(false);
    m_sound_stop.store(true);
    m_display_progress.store(false);
    m_capturing = false;
    m_compress_handle = tjInitCompress();
    m_decompress_handle = tjInitDecompress();
    m_audio_data = NULL;
    if (m_recorder_cfg->m_triple_buffering > 0)
    {
        ogrGenBuffers(3, m_pbo);
        for (int i = 0; i < 3; i++)
        {
            ogrBindBuffer(E_GL_PIXEL_PACK_BUFFER, m_pbo[i]);
            ogrBufferData(E_GL_PIXEL_PACK_BUFFER, m_recorder_cfg->m_width *
                m_recorder_cfg->m_height * 4, NULL, E_GL_STREAM_READ);
        }
        ogrBindBuffer(E_GL_PIXEL_PACK_BUFFER, 0);
    }
    m_fbi = new uint8_t[m_recorder_cfg->m_width * m_recorder_cfg->m_height
        * 4]();
    m_frame_type = 0;
    m_capture_thread = std::thread(CaptureLibrary::captureConversion, this);
}   // CaptureLibrary

// ----------------------------------------------------------------------------
CaptureLibrary::~CaptureLibrary()
{
    m_destroy.store(true);
    std::unique_lock<std::mutex> ul(m_fbi_mutex);
    m_frame_type = isCapturing() ? -1 : -2;
    m_fbi_ready.notify_one();
    ul.unlock();
    m_capture_thread.join();
    tjDestroy(m_compress_handle);
    tjDestroy(m_decompress_handle);
    delete m_audio_data;
    delete[] m_fbi;
    if (m_recorder_cfg->m_triple_buffering > 0)
    {
        ogrDeleteBuffers(3, m_pbo);
    }
}   // ~CaptureLibrary

// ----------------------------------------------------------------------------
void CaptureLibrary::reset()
{
    std::lock_guard<std::mutex> lock(m_capturing_mutex);
    if (m_capturing)
    {
        return;
    }
    m_capturing = true;
    runCallback(OGR_CBT_START_RECORDING, NULL);
    m_pbo_use = 0;
    m_accumulated_time = 0.;
    if (m_recorder_cfg->m_record_audio > 0)
    {
        m_sound_stop.store(false);
        m_audio_enc_thread = std::thread(Recorder::audioRecorder, this);
    }
    switch (m_recorder_cfg->m_video_format)
    {
    case OGR_VF_VP8:
    case OGR_VF_VP9:
        m_video_enc_thread = std::thread(Recorder::vpxEncoder, this);
        break;
    case OGR_VF_MJPEG:
        m_video_enc_thread = std::thread(Recorder::mjpegWriter, this);
        break;
    case OGR_VF_H264:
        break;
    default:
        break;
    }
}   // reset

// ----------------------------------------------------------------------------
int CaptureLibrary::bmpToJPG(uint8_t* raw, unsigned width, unsigned height,
                             uint8_t** jpeg_buffer, unsigned long* jpeg_size)
{
    int ret = 0;
#ifdef TJFLAG_FASTDCT
    ret = tjCompress2(m_compress_handle, raw, width, 0, height, TJPF_RGBX,
        jpeg_buffer, jpeg_size, TJSAMP_420,
        m_recorder_cfg->m_record_jpg_quality, TJFLAG_FASTDCT);
#else
    ret = tjCompress2(m_compress_handle, raw, width, 0, height, TJPF_RGBX,
        jpeg_buffer, jpeg_size, TJSAMP_420,
        m_recorder_cfg->m_record_jpg_quality, 0);
#endif
    if (ret != 0)
    {
        char* err = tjGetErrorStr();
        std::string msg = "Turbojpeg encode error: ";
        msg = msg + err + "\n";
        runCallback(OGR_CBT_ERROR_RECORDING, msg.c_str());
        return ret;
    }
    return ret;
}   // bmpToJPG

// ----------------------------------------------------------------------------
int CaptureLibrary::yuvConversion(uint8_t* jpeg_buffer, unsigned jpeg_size,
                                  uint8_t* yuv_buffer)
{
    int ret = tjDecompressToYUV(m_decompress_handle, jpeg_buffer, jpeg_size,
        yuv_buffer, 0);
    if (ret != 0)
    {
        char* err = tjGetErrorStr();
        std::string msg = "Turbojpeg YUV conversion error: ";
        msg = msg + err + "\n";
        runCallback(OGR_CBT_ERROR_RECORDING, msg.c_str());
        return ret;
    }
    return ret;
}   // yuvConversion

// ----------------------------------------------------------------------------
int CaptureLibrary::getFrameCount(double rate)
{
    const double frame_rate = 1. / double(m_recorder_cfg->m_record_fps);
    m_accumulated_time += rate;
    if (m_accumulated_time < frame_rate)
    {
        return 0;
    }
    int frame_count = 0;
    while (m_accumulated_time >= frame_rate)
    {
        frame_count++;
        m_accumulated_time = m_accumulated_time - frame_rate;
    }
    return frame_count;
}   // getFrameCount

// ----------------------------------------------------------------------------
void CaptureLibrary::capture()
{
    if (!isCapturing()) return;
    int pbo_read = -1;
    if (m_pbo_use > 3 && m_pbo_use % 3 == 0)
        m_pbo_use = 3;
    auto rate = std::chrono::high_resolution_clock::now() - m_framerate_timer;
    m_framerate_timer = std::chrono::high_resolution_clock::now();
    const unsigned width = m_recorder_cfg->m_width;
    const unsigned height = m_recorder_cfg->m_height;
    const bool use_pbo = m_recorder_cfg->m_triple_buffering > 0;
    if (m_pbo_use >= 3)
    {
        int frame_count = getFrameCount(std::chrono::duration_cast
            <std::chrono::duration<double> >(rate).count());
        if (frame_count != 0)
        {
            const unsigned size = width * height * 4;
            std::lock_guard<std::mutex> lock(m_fbi_mutex);
            if (use_pbo)
            {
                pbo_read = m_pbo_use % 3;
                ogrBindBuffer(E_GL_PIXEL_PACK_BUFFER, m_pbo[pbo_read]);
                void* ptr = ogrMapBuffer(E_GL_PIXEL_PACK_BUFFER,
                    E_GL_READ_ONLY);
                memcpy(m_fbi, ptr, size);
                ogrUnmapBuffer(E_GL_PIXEL_PACK_BUFFER);
            }
            else
            {
                ogrReadPixels(0, 0, width, height, E_GL_RGBA,
                    E_GL_UNSIGNED_BYTE, m_fbi);
            }
            m_frame_type = frame_count;
            m_fbi_ready.notify_one();
        }
    }
    int pbo_use = m_pbo_use++ % 3;
    if (!use_pbo)
        return;

    assert(pbo_read == -1 || pbo_use == pbo_read);
    ogrBindBuffer(E_GL_PIXEL_PACK_BUFFER, m_pbo[pbo_use]);
    ogrReadPixels(0, 0, width, height, E_GL_RGBA, E_GL_UNSIGNED_BYTE, NULL);
    ogrBindBuffer(E_GL_PIXEL_PACK_BUFFER, 0);
}   // capture

// ----------------------------------------------------------------------------
void CaptureLibrary::captureConversion(CaptureLibrary* cl)
{
    setThreadName("captureConvert");
    while (true)
    {
        std::unique_lock<std::mutex> ul(cl->m_fbi_mutex);
        cl->m_fbi_ready.wait(ul, [&cl]
            { return cl->m_frame_type != 0; });
        uint8_t* fbi = cl->m_fbi;
        int frame_count = cl->m_frame_type;
        if (frame_count == -1)
        {
            ul.unlock();
            if (cl->m_recorder_cfg->m_record_audio > 0)
            {
                cl->m_sound_stop.store(true);
                cl->m_audio_enc_thread.join();
            }
            std::unique_lock<std::mutex> ulj(cl->m_jpg_list_mutex);
            if (!cl->m_destroy.load() && cl->m_jpg_list.size() > 100)
            {
                runCallback(OGR_CBT_WAIT_RECORDING, NULL);
            }
            cl->m_jpg_list.emplace_back((uint8_t*)NULL, 0, 0);
            cl->m_jpg_list_ready.notify_one();
            ulj.unlock();
            cl->m_display_progress.store(!cl->m_destroy.load());
            cl->m_video_enc_thread.join();
            cl->m_display_progress.store(false);
            std::string f = Recorder::writeMKV(getSavedName() + ".video",
                getSavedName() + ".audio");
            if (cl->m_destroy.load())
            {
                return;
            }
            if (f.empty())
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to mux a mkv.\n");
            }
            else
            {
                runCallback(OGR_CBT_SAVED_RECORDING, f.c_str());
            }
            std::lock_guard<std::mutex> lc(cl->m_capturing_mutex);
            std::lock_guard<std::mutex> lf(cl->m_fbi_mutex);
            cl->m_capturing = false;
            cl->m_frame_type = 0;
            continue;
        }
        else if (frame_count == -2)
        {
            ul.unlock();
            return;
        }

        const unsigned width = cl->m_recorder_cfg->m_width;
        const unsigned height = cl->m_recorder_cfg->m_height;
        const int pitch = width * 4;
        uint8_t* p2 = fbi + (height - 1) * pitch;
        uint8_t* tmp_buf = new uint8_t[pitch];
        for (unsigned i = 0; i < height; i += 2)
        {
            memcpy(tmp_buf, fbi, pitch);
            memcpy(fbi, p2, pitch);
            memcpy(p2, tmp_buf, pitch);
            fbi += pitch;
            p2 -= pitch;
        }
        delete [] tmp_buf;
        uint8_t* jpg = NULL;
        unsigned long jpg_size = 0;
        cl->bmpToJPG(cl->m_fbi, width, height, &jpg, &jpg_size);
        cl->m_frame_type = 0;
        ul.unlock();

        std::lock_guard<std::mutex> lg(cl->m_jpg_list_mutex);
        cl->m_jpg_list.emplace_back(jpg, jpg_size, frame_count);
        cl->m_jpg_list_ready.notify_one();
    }
}   // captureConversion
