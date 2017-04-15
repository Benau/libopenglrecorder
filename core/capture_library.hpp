/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_CAPTURE_LIBRARY_HPP
#define HEADER_CAPTURE_LIBRARY_HPP

#include "openglrecorder.h"

#if defined(_MSC_VER) && _MSC_VER < 1700
    typedef unsigned char    uint8_t;
    typedef unsigned short   uint16_t;
    typedef __int32          int32_t;
    typedef unsigned __int32 uint32_t;
    typedef __int64          int64_t;
    typedef unsigned __int64 uint64_t;
#else
    #include <stdint.h>
#endif

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <turbojpeg.h>

struct AudioEncoderData
{
    enum AudioType { AT_FLOAT, AT_PCM };
    std::mutex* m_mutex;
    std::condition_variable* m_cv;
    std::list<int8_t*>* m_buf_list;
    uint32_t m_sample_rate;
    uint32_t m_channels;
    uint32_t m_audio_bitrate;
    AudioType m_audio_type;
};

class CommonAudioData
{
public:
    CommonAudioData() {}
    virtual ~CommonAudioData() {}
};

typedef std::list<std::tuple<uint8_t*, unsigned, int> > JPGList;

class CaptureLibrary
{
private:
    RecorderConfig* m_recorder_cfg;

    std::atomic_bool m_display_progress, m_sound_stop;

    bool m_destroy;
    std::mutex m_destroy_mutex;

    bool m_capturing;
    mutable std::mutex m_capturing_mutex;

    tjhandle m_compress_handle, m_decompress_handle;

    JPGList m_jpg_list;
    std::mutex m_jpg_list_mutex;
    std::condition_variable m_jpg_list_ready;

    uint8_t* m_fbi;
    int m_frame_type;
    std::mutex m_fbi_mutex;
    std::condition_variable m_fbi_ready;

    std::thread m_capture_thread, m_audio_enc_thread, m_video_enc_thread;

    uint32_t m_pbo[3];

    unsigned m_pbo_use;

    std::chrono::high_resolution_clock::time_point m_framerate_timer;

    double m_accumulated_time;

    CommonAudioData* m_audio_data;

    // ------------------------------------------------------------------------
    int getFrameCount(double rate);

public:
    // ------------------------------------------------------------------------
    CaptureLibrary(RecorderConfig* rc);
    // ------------------------------------------------------------------------
    ~CaptureLibrary();
    // ------------------------------------------------------------------------
    void capture();
    // ------------------------------------------------------------------------
    void reset();
    // ------------------------------------------------------------------------
    int bmpToJPG(uint8_t* raw, unsigned width, unsigned height,
        uint8_t** jpeg_buffer, unsigned long* jpeg_size);
    // ------------------------------------------------------------------------
    int yuvConversion(uint8_t* jpeg_buffer, unsigned jpeg_size,
                      uint8_t* yuv_buffer);
    // ------------------------------------------------------------------------
    JPGList* getJPGList()                               { return &m_jpg_list; }
    // ------------------------------------------------------------------------
    std::mutex* getJPGListMutex()                 { return &m_jpg_list_mutex; }
    // ------------------------------------------------------------------------
    std::condition_variable* getJPGListCV()       { return &m_jpg_list_ready; }
    // ------------------------------------------------------------------------
    bool displayingProgress() const       { return m_display_progress.load(); }
    // ------------------------------------------------------------------------
    bool getSoundStop() const                   { return m_sound_stop.load(); }
    // ------------------------------------------------------------------------
    bool isCapturing() const
    {
        std::lock_guard<std::mutex> lock(m_capturing_mutex);
        return m_capturing;
    }
    // ------------------------------------------------------------------------
    void stopCapture()
    {
        if (!isCapturing()) return;
        std::lock_guard<std::mutex> lock(m_fbi_mutex);
        m_frame_type = -1;
        m_fbi_ready.notify_one();
    }
    // ------------------------------------------------------------------------
    const RecorderConfig& getRecorderConfig() const { return *m_recorder_cfg; }
    // ------------------------------------------------------------------------
    static void captureConversion(CaptureLibrary* cl);
    // ------------------------------------------------------------------------
    CommonAudioData* getAudioData() const              { return m_audio_data; }
    // ------------------------------------------------------------------------
    void setAudioData(CommonAudioData* data)           { m_audio_data = data; }

};

#endif
