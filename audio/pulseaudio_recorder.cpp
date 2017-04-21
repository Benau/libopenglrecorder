/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#if defined(ENABLE_REC_SOUND) && !defined(WIN32)

#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"
#include "audio/vorbis_encoder.hpp"

#include <pulse/pulseaudio.h>
#include <string>

#ifndef ENABLE_PULSE_WO_DL
#include <dlfcn.h>
#endif

namespace Recorder
{
    // ========================================================================
    void serverInfoCallBack(pa_context* c, const pa_server_info* i, void* data)
    {
        *(std::string*)data = i->default_sink_name;
    }   // serverInfoCallBack
    // ========================================================================
    class PulseAudioData : public CommonAudioData
    {
    public:
        bool m_loaded;
        pa_mainloop* m_loop;
        pa_context* m_context;
        pa_stream* m_stream;
        pa_sample_spec m_sample_spec;
        std::string m_default_sink;
#ifndef ENABLE_PULSE_WO_DL
        void* m_dl_handle;

        typedef pa_stream* (*pa_stream_new_t)(pa_context*, const char*,
            const pa_sample_spec*, const pa_channel_map*);
        pa_stream_new_t pa_stream_new;

        typedef int (*pa_stream_connect_record_t)(pa_stream*, const char*,
            const pa_buffer_attr*, pa_stream_flags_t);
        pa_stream_connect_record_t pa_stream_connect_record;

        typedef pa_stream_state_t (*pa_stream_get_state_t)(pa_stream*);
        pa_stream_get_state_t pa_stream_get_state;

        typedef size_t (*pa_stream_readable_size_t)(pa_stream*);
        pa_stream_readable_size_t pa_stream_readable_size;

        typedef int (*pa_stream_peek_t)(pa_stream*, const void**, size_t*);
        pa_stream_peek_t pa_stream_peek;

        typedef int (*pa_stream_drop_t)(pa_stream*);
        pa_stream_drop_t pa_stream_drop;

        typedef int (*pa_stream_disconnect_t)(pa_stream*);
        pa_stream_disconnect_t pa_stream_disconnect;

        typedef void (*pa_stream_unref_t)(pa_stream*);
        pa_stream_unref_t pa_stream_unref;

        typedef pa_mainloop* (*pa_mainloop_new_t)(void);
        pa_mainloop_new_t pa_mainloop_new;

        typedef pa_mainloop_api* (*pa_mainloop_get_api_t)(pa_mainloop*);
        pa_mainloop_get_api_t pa_mainloop_get_api;

        typedef pa_context* (*pa_context_new_t)(pa_mainloop_api*, const char*);
        pa_context_new_t pa_context_new;

        typedef int (*pa_context_connect_t)(pa_context*, const char*,
            pa_context_flags_t, const pa_spawn_api*);
        pa_context_connect_t pa_context_connect;

        typedef int (*pa_mainloop_iterate_t)(pa_mainloop*, int, int*);
        pa_mainloop_iterate_t pa_mainloop_iterate;

        typedef pa_context_state_t (*pa_context_get_state_t)(pa_context*);
        pa_context_get_state_t pa_context_get_state;

        typedef pa_operation* (*pa_context_get_server_info_t)(pa_context*,
            pa_server_info_cb_t, void*);
        pa_context_get_server_info_t pa_context_get_server_info;

        typedef pa_operation_state_t (*pa_operation_get_state_t)
            (pa_operation*);
        pa_operation_get_state_t pa_operation_get_state;

        typedef void (*pa_operation_unref_t)(pa_operation*);
        pa_operation_unref_t pa_operation_unref;

        typedef void (*pa_context_disconnect_t)(pa_context*);
        pa_context_disconnect_t pa_context_disconnect;

        typedef void (*pa_context_unref_t)(pa_context*);
        pa_context_unref_t pa_context_unref;

        typedef void (*pa_mainloop_free_t)(pa_mainloop*);
        pa_mainloop_free_t pa_mainloop_free;
#endif
        // --------------------------------------------------------------------
        PulseAudioData()
        {
            m_loaded = false;
            m_loop = NULL;
            m_context = NULL;
            m_stream = NULL;
#ifndef ENABLE_PULSE_WO_DL
            m_dl_handle = NULL;
            pa_stream_new = NULL;
            pa_stream_connect_record = NULL;
            pa_stream_get_state = NULL;
            pa_stream_readable_size = NULL;
            pa_stream_peek = NULL;
            pa_stream_drop = NULL;
            pa_stream_disconnect = NULL;
            pa_stream_unref = NULL;
            pa_mainloop_new = NULL;
            pa_mainloop_get_api = NULL;
            pa_context_new = NULL;
            pa_context_connect = NULL;
            pa_mainloop_iterate = NULL;
            pa_context_get_state = NULL;
            pa_context_get_server_info = NULL;
            pa_operation_get_state = NULL;
            pa_operation_unref = NULL;
            pa_context_disconnect = NULL;
            pa_context_unref = NULL;
            pa_mainloop_free = NULL;
#endif
        }   // PulseAudioData
        // --------------------------------------------------------------------
#ifndef ENABLE_PULSE_WO_DL
        bool loadPulseAudioLibrary()
        {
            m_dl_handle = dlopen("libpulse.so", RTLD_LAZY);
            if (m_dl_handle == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to open"
                " PulseAudio library\n");
                return false;
            }
            pa_stream_new = (pa_stream_new_t)dlsym(m_dl_handle,
                "pa_stream_new");
            if (pa_stream_new == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_new'\n");
                return false;
            }
            pa_stream_connect_record = (pa_stream_connect_record_t)dlsym
                (m_dl_handle, "pa_stream_connect_record");
            if (pa_stream_connect_record == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_connect_record'\n");
                return false;
            }
            pa_stream_get_state = (pa_stream_get_state_t)dlsym(m_dl_handle,
                "pa_stream_get_state");
            if (pa_stream_get_state == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_get_state'\n");
                return false;
            }
            pa_stream_readable_size = (pa_stream_readable_size_t)dlsym
                (m_dl_handle, "pa_stream_readable_size");
            if (pa_stream_readable_size == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_readable_size'\n");
                return false;
            }
            pa_stream_peek = (pa_stream_peek_t)dlsym(m_dl_handle,
                "pa_stream_peek");
            if (pa_stream_peek == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_peek'\n");
                return false;
            }
            pa_stream_drop = (pa_stream_drop_t)dlsym(m_dl_handle,
                "pa_stream_drop");
            if (pa_stream_drop == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_drop'\n");
                return false;
            }
            pa_stream_disconnect = (pa_stream_disconnect_t)dlsym(m_dl_handle,
                "pa_stream_disconnect");
            if (pa_stream_disconnect == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_disconnect'\n");
                return false;
            }
            pa_stream_unref = (pa_stream_unref_t)dlsym(m_dl_handle,
                "pa_stream_unref");
            if (pa_stream_unref == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_stream_unref'\n");
                return false;
            }
            pa_mainloop_new = (pa_mainloop_new_t)dlsym(m_dl_handle,
                "pa_mainloop_new");
            if (pa_mainloop_new == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_mainloop_new'\n");
                return false;
            }
            pa_mainloop_get_api = (pa_mainloop_get_api_t)dlsym(m_dl_handle,
                "pa_mainloop_get_api");
            if (pa_mainloop_get_api == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_mainloop_get_api'\n");
                return false;
            }
            pa_context_new = (pa_context_new_t)dlsym(m_dl_handle,
                "pa_context_new");
            if (pa_context_new == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_new'\n");
                return false;
            }
            pa_context_connect = (pa_context_connect_t)dlsym(m_dl_handle,
                "pa_context_connect");
            if (pa_context_connect == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_connect'\n");
                return false;
            }
            pa_mainloop_iterate = (pa_mainloop_iterate_t)dlsym(m_dl_handle,
                "pa_mainloop_iterate");
            if (pa_mainloop_iterate == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_mainloop_iterate'\n");
                return false;
            }
            pa_context_get_state = (pa_context_get_state_t)dlsym(m_dl_handle,
                "pa_context_get_state");
            if (pa_context_get_state == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_get_state'\n");
                return false;
            }
            pa_context_get_server_info = (pa_context_get_server_info_t)dlsym
                (m_dl_handle, "pa_context_get_server_info");
            if (pa_context_get_server_info == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_get_server_info'\n");
                return false;
            }
            pa_operation_get_state = (pa_operation_get_state_t)dlsym
                (m_dl_handle, "pa_operation_get_state");
            if (pa_operation_get_state == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_operation_get_state'\n");
                return false;
            }
            pa_operation_unref = (pa_operation_unref_t)dlsym(m_dl_handle,
                "pa_operation_unref");
            if (pa_operation_unref == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_operation_unref'\n");
                return false;
            }
            pa_context_disconnect = (pa_context_disconnect_t)dlsym(m_dl_handle,
                "pa_context_disconnect");
            if (pa_context_disconnect == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_disconnect'\n");
                return false;
            }
            pa_context_unref = (pa_context_unref_t)dlsym(m_dl_handle,
                "pa_context_unref");
            if (pa_context_unref == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_context_unref'\n");
                return false;
            }
            pa_mainloop_free = (pa_mainloop_free_t)dlsym(m_dl_handle,
                "pa_mainloop_free");
            if (pa_mainloop_free == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load function"
                    " 'pa_mainloop_free'\n");
                return false;
            }
            return true;
        }   // loadPulseAudioLibrary
#endif
        // --------------------------------------------------------------------
        bool load()
        {
#ifndef ENABLE_PULSE_WO_DL
            if (!loadPulseAudioLibrary())
            {
                if (m_dl_handle != NULL)
                {
                    dlclose(m_dl_handle);
                    m_dl_handle = NULL;
                }
                return false;
            }
#endif
            m_loop = pa_mainloop_new();
            if (m_loop == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING,"Failed to create"
                    " mainloop\n");
                return false;
            }
            m_context = pa_context_new(pa_mainloop_get_api(m_loop),
                "audioRecord");
            if (m_context == NULL)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to create"
                    " context\n");
                return false;
            }
            pa_context_connect(m_context, NULL, PA_CONTEXT_NOAUTOSPAWN , NULL);
            while (true)
            {
                while (pa_mainloop_iterate(m_loop, 0, NULL) > 0);
                pa_context_state_t state = pa_context_get_state(m_context);
                if (state == PA_CONTEXT_READY)
                    break;
                if (!PA_CONTEXT_IS_GOOD(state))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Failed to connect"
                        " to context\n");
                    return false;
                }
            }
            pa_operation* pa_op = pa_context_get_server_info(m_context,
                serverInfoCallBack, &m_default_sink);
            enum pa_operation_state op_state;
            while ((op_state =
                pa_operation_get_state(pa_op)) == PA_OPERATION_RUNNING)
            {
                pa_mainloop_iterate(m_loop, 0, NULL);
            }
            pa_operation_unref(pa_op);
            if (m_default_sink.empty())
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to get default"
                    " sink\n");
                return false;
            }
            m_default_sink += ".monitor";
            m_sample_spec.format = PA_SAMPLE_S16LE;
            m_sample_spec.rate = 44100;
            m_sample_spec.channels = 2;

            m_loaded = true;
            return true;
        }   // load
        // --------------------------------------------------------------------
        void configAudioType(AudioEncoderData* aed)
        {
            aed->m_sample_rate = m_sample_spec.rate;
            aed->m_channels = m_sample_spec.channels;
            aed->m_audio_type = AudioEncoderData::AT_PCM;
        }   // configAudioType
        // --------------------------------------------------------------------
        inline void mainLoopIterate()
        {
            while (pa_mainloop_iterate(m_loop, 0, NULL) > 0);
        }   // mainLoopIterate
        // --------------------------------------------------------------------
        bool createRecordStream()
        {
            assert(m_stream == NULL);
            m_stream = pa_stream_new(m_context, "input", &m_sample_spec, NULL);
            if (m_stream == NULL)
            {
                return false;
            }
            pa_buffer_attr buf_attr;
            const unsigned frag_size = 1024 * m_sample_spec.channels *
                sizeof(int16_t);
            buf_attr.fragsize = frag_size;
            const unsigned max_uint = -1;
            buf_attr.maxlength = max_uint;
            buf_attr.minreq = max_uint;
            buf_attr.prebuf = max_uint;
            buf_attr.tlength = max_uint;
            pa_stream_connect_record(m_stream, m_default_sink.c_str(),
                &buf_attr, (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY));
            while (true)
            {
                mainLoopIterate();
                pa_stream_state_t state = pa_stream_get_state(m_stream);
                if (state == PA_STREAM_READY)
                    break;
                if (!PA_STREAM_IS_GOOD(state))
                {
                    return false;
                }
            }
            return true;
        }   // createRecordStream
        // --------------------------------------------------------------------
        void removeRecordStream()
        {
            assert(m_stream != NULL);
            pa_stream_disconnect(m_stream);
            pa_stream_unref(m_stream);
            m_stream = NULL;
        }   // removeRecordStream
        // --------------------------------------------------------------------
        inline size_t getReadableSize()
        {
            assert(m_stream != NULL);
            return pa_stream_readable_size(m_stream);
        }   // removeRecordStream
        // --------------------------------------------------------------------
        inline void peekStream(const void** data, size_t* bytes)
        {
            assert(m_stream != NULL);
            pa_stream_peek(m_stream, data, bytes);
        }   // peekStream
        // --------------------------------------------------------------------
        inline void dropStream()
        {
            assert(m_stream != NULL);
            pa_stream_drop(m_stream);
        }   // dropStream
        // --------------------------------------------------------------------
        ~PulseAudioData()
        {
            if (m_loaded)
            {
                if (m_context != NULL)
                {
                    pa_context_disconnect(m_context);
                    pa_context_unref(m_context);
                }
                if (m_loop != NULL)
                {
                    pa_mainloop_free(m_loop);
                }
#ifndef ENABLE_PULSE_WO_DL
                if (m_dl_handle != NULL)
                {
                    dlclose(m_dl_handle);
                }
#endif
            }
        }   // ~PulseAudioData
    };
    // ========================================================================
    void audioRecorder(CaptureLibrary* cl)
    {
        setThreadName("audioRecorder");
        PulseAudioData* pa_data =
            dynamic_cast<PulseAudioData*>(cl->getAudioData());
        if (pa_data == NULL)
        {
            pa_data = new PulseAudioData();
            cl->setAudioData(pa_data);
            if (!pa_data->load())
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Cannot load pulseaudio"
                    " data.\n");
            }
        }
        if (!pa_data->m_loaded)
        {
            return;
        }

        if (pa_data->createRecordStream() == false)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to create audio"
                " record stream.\n");
            if (pa_data->m_stream != NULL)
            {
                pa_data->removeRecordStream();
            }
            return;
        }

        std::list<int8_t*> pcm_data;
        std::mutex pcm_mutex;
        std::condition_variable pcm_cv;
        std::thread audio_enc_thread;

        AudioEncoderData aed;
        pa_data->configAudioType(&aed);
        aed.m_buf_list = &pcm_data;
        aed.m_mutex = &pcm_mutex;
        aed.m_cv = &pcm_cv;
        aed.m_audio_bitrate = cl->getRecorderConfig().m_audio_bitrate;
        const unsigned frag_size = 1024 * pa_data->m_sample_spec.channels *
            sizeof(int16_t);

        switch (cl->getRecorderConfig().m_audio_format)
        {
        case OGR_AF_VORBIS:
            audio_enc_thread = std::thread(vorbisEncoder, &aed);
            break;
        default:
            break;
        }

        int8_t* each_pcm_buf = new int8_t[frag_size]();
        unsigned readed = 0;
        while (true)
        {
            if (cl->getSoundStop())
            {
                std::lock_guard<std::mutex> lock(pcm_mutex);
                pcm_data.push_back(each_pcm_buf);
                pcm_data.push_back(NULL);
                pcm_cv.notify_one();
                break;
            }
            pa_data->mainLoopIterate();
            const void* data;
            size_t bytes;
            size_t readable = pa_data->getReadableSize();
            if (readable == 0)
                continue;
            pa_data->peekStream(&data, &bytes);
            if (data == NULL)
            {
                if (bytes > 0)
                    pa_data->dropStream();
                continue;
            }
            unsigned copy_bytes = (unsigned)bytes;
            bool buf_full = readed + copy_bytes > frag_size;
            if (buf_full)
            {
                copy_bytes = frag_size - readed;
                memcpy(each_pcm_buf + readed, data, copy_bytes);
                std::unique_lock<std::mutex> ul(pcm_mutex);
                pcm_data.push_back(each_pcm_buf);
                pcm_cv.notify_one();
                ul.unlock();
                unsigned remaining_bytes = (unsigned)bytes - copy_bytes;
                unsigned count = 0;
                while (remaining_bytes > frag_size)
                {
                    each_pcm_buf = new int8_t[frag_size]();
                    memcpy(each_pcm_buf,
                        (uint8_t*)data + copy_bytes + frag_size * count,
                        frag_size);
                    std::unique_lock<std::mutex> ul(pcm_mutex);
                    pcm_data.push_back(each_pcm_buf);
                    pcm_cv.notify_one();
                    ul.unlock();
                    remaining_bytes -= frag_size;
                    count++;
                }
                each_pcm_buf = new int8_t[frag_size]();
                readed = remaining_bytes;
                memcpy(each_pcm_buf,
                    (uint8_t*)data + copy_bytes + frag_size * count,
                    remaining_bytes);
            }
            else
            {
                memcpy(each_pcm_buf + readed, data, copy_bytes);
                readed += copy_bytes;
            }
            pa_data->dropStream();
        }
        audio_enc_thread.join();
        pa_data->removeRecordStream();
    }   // audioRecorder
}
#endif
