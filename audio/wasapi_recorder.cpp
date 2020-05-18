/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#if defined(ENABLE_REC_SOUND) && defined(WIN32)

#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"
#include "audio/vorbis_encoder.hpp"

#include <audioclient.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <windows.h>

#if defined (__MINGW32__) || defined(__CYGWIN__)
    #include <stdint.h>
    inline GUID uuidFromString(const char* s)
    {
        unsigned long p0;
        unsigned int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;
        sscanf(s, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            &p0, &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10);
        GUID g = { p0, (uint16_t)p1, (uint16_t)p2, { (uint8_t)p3, (uint8_t)p4,
            (uint8_t)p5, (uint8_t)p6, (uint8_t)p7, (uint8_t)p8, (uint8_t)p9,
            (uint8_t)p10 }};
        return g;
    }
    #undef KSDATAFORMAT_SUBTYPE_PCM
    #define KSDATAFORMAT_SUBTYPE_PCM \
        uuidFromString("00000001-0000-0010-8000-00aa00389b71")
    #undef KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
    #define KSDATAFORMAT_SUBTYPE_IEEE_FLOAT \
        uuidFromString("00000003-0000-0010-8000-00aa00389b71")
#endif

namespace Recorder
{
    // ========================================================================
    const REFERENCE_TIME REFTIMES_PER_SEC = 10000000;
    // ========================================================================
    class WasapiData : public CommonAudioData
    {
    public:
        bool m_loaded;
        IMMDeviceEnumerator* m_dev_enum;
        IMMDevice* m_dev;
        IAudioClient* m_client;
        IAudioCaptureClient* m_capture_client;
        WAVEFORMATEX* m_wav_format;
        uint32_t m_buffer_size;
        // --------------------------------------------------------------------
        WasapiData()
        {
            m_loaded = false;
            m_dev_enum = NULL;
            m_dev = NULL;
            m_client = NULL;
            m_capture_client = NULL;
            m_wav_format = NULL;
        }   // WasapiData
        // --------------------------------------------------------------------
        bool load()
        {
            HRESULT hr = CoInitialize(NULL);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to CoInitialize.\n");
                return false;
            }

            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                (void**)&m_dev_enum);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to CoCreateInstance.\n");
                return false;
            }

            hr = m_dev_enum->GetDefaultAudioEndpoint(eRender, eConsole,
                &m_dev);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_dev_enum->GetDefaultAudioEndpoint.\n");
                return false;
            }

            hr = m_dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                (void**)&m_client);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_dev->Activate.\n");
                return false;
            }

            hr = m_client->GetMixFormat(&m_wav_format);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_client->GetMixFormat.\n");
                return false;
            }

            hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_LOOPBACK, REFTIMES_PER_SEC, 0,
                m_wav_format, NULL);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_client->Initialize.\n");
                return false;
            }

            hr = m_client->GetBufferSize(&m_buffer_size);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_client->GetBufferSize.\n");
                return false;
            }

            hr = m_client->GetService(__uuidof(IAudioCaptureClient),
                (void**)&m_capture_client);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING,
                    "Failed to m_client->GetService.\n");
                return false;
            }

            m_loaded = true;
            return true;
        }   // load
        // --------------------------------------------------------------------
        ~WasapiData()
        {
            if (m_loaded)
            {
                CoTaskMemFree(m_wav_format);
                if (m_dev_enum)
                    m_dev_enum->Release();
                if (m_dev)
                    m_dev->Release();
                if (m_client)
                    m_client->Release();
                if (m_capture_client)
                    m_capture_client->Release();
            }
        }   // ~WasapiData
    };
    // ========================================================================
    void audioRecorder(CaptureLibrary* cl)
    {
        setThreadName("audioRecorder");
        WasapiData* wasapi_data =
            dynamic_cast<WasapiData*>(cl->getAudioData());
        if (wasapi_data == NULL)
        {
            wasapi_data = new WasapiData();
            cl->setAudioData(wasapi_data);
            if (!wasapi_data->load())
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to load wasapi"
                    " data.\n");
            }
        }
        if (!wasapi_data->m_loaded)
        {
            return;
        }
        AudioEncoderData aed = {};
        if (wasapi_data->m_wav_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            WAVEFORMATEXTENSIBLE* wav_for_ext =
                (WAVEFORMATEXTENSIBLE*)wasapi_data->m_wav_format;
            aed.m_channels = wav_for_ext->Format.nChannels;
            aed.m_sample_rate = wav_for_ext->Format.nSamplesPerSec;
            if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_PCM, wav_for_ext->SubFormat))
            {
                aed.m_audio_type = AudioEncoderData::AT_PCM;
                if (wav_for_ext->Format.wBitsPerSample != 16)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Only 16bit PCM is"
                        " supported.\n");
                    return;
                }
            }
            else if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, wav_for_ext
                ->SubFormat))
            {
                aed.m_audio_type = AudioEncoderData::AT_FLOAT;
                if (wav_for_ext->Format.wBitsPerSample != 32)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Only 32bit float is"
                        " supported.\n");
                    return;
                }
            }
            else
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Unsupported audio input"
                    " format.\n");
                return;
            }
        }
        else if (wasapi_data->m_wav_format->wFormatTag == WAVE_FORMAT_PCM)
        {
            aed.m_channels = wasapi_data->m_wav_format->nChannels;
            aed.m_sample_rate = wasapi_data->m_wav_format->nSamplesPerSec;
            aed.m_audio_type = AudioEncoderData::AT_PCM;
            if (wasapi_data->m_wav_format->wBitsPerSample != 16)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Only 16bit PCM is"
                    " supported.\n");
                return;
            }
        }
        else
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Unsupported audio input"
                " format.\n");
            return;
        }
        if (aed.m_sample_rate > 48000)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Only support maximum 48000hz"
                " sample rate audio.\n");
            return;
        }
        HRESULT hr = wasapi_data->m_client->Reset();
        if (FAILED(hr))
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to reset audio"
                " recorder.\n");
            return;
        }
        hr = wasapi_data->m_client->Start();
        if (FAILED(hr))
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to start audio"
                " recorder.\n");
            return;
        }
        REFERENCE_TIME duration = REFTIMES_PER_SEC *
            wasapi_data->m_buffer_size / wasapi_data->m_wav_format
            ->nSamplesPerSec;

        std::list<int8_t*> audio_data;
        std::mutex audio_mutex;
        std::condition_variable audio_cv;
        std::thread audio_enc_thread;
        aed.m_buf_list = &audio_data;
        aed.m_mutex = &audio_mutex;
        aed.m_cv = &audio_cv;
        aed.m_audio_bitrate = cl->getRecorderConfig().m_audio_bitrate;

        switch (cl->getRecorderConfig().m_audio_format)
        {
        case OGR_AF_VORBIS:
            audio_enc_thread = std::thread(vorbisEncoder, &aed);
            break;
        default:
            break;
        }

        const unsigned frag_size = 1024 * aed.m_channels *
            (wasapi_data->m_wav_format->wBitsPerSample / 8);
        int8_t* each_audio_buf = new int8_t[frag_size]();
        unsigned readed = 0;
        while (true)
        {
            if (cl->getSoundStop())
            {
                std::lock_guard<std::mutex> lock(audio_mutex);
                audio_data.push_back(each_audio_buf);
                audio_data.push_back(NULL);
                audio_cv.notify_one();
                break;
            }
            uint32_t packet_length = 0;
            hr = wasapi_data->m_capture_client->GetNextPacketSize(
                &packet_length);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to get next audio"
                    " packet size.\n");
            }
            if (packet_length == 0)
            {
                REFERENCE_TIME sleep_time = duration / 10000 / 2;
                Sleep((uint32_t)sleep_time);
                continue;
            }
            BYTE* data;
            DWORD flags;
            hr = wasapi_data->m_capture_client->GetBuffer(&data,
                &packet_length, &flags, NULL, NULL);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to get audio"
                    " buffer.\n");
            }
            const unsigned bytes = aed.m_channels * (wasapi_data->m_wav_format
                ->wBitsPerSample / 8) * packet_length;
            unsigned copy_bytes = bytes;
            bool buf_full = readed + copy_bytes > frag_size;
            if (buf_full)
            {
                copy_bytes = frag_size - readed;
                memcpy(each_audio_buf + readed, data, copy_bytes);
                std::unique_lock<std::mutex> ul(audio_mutex);
                audio_data.push_back(each_audio_buf);
                audio_cv.notify_one();
                ul.unlock();
                unsigned remaining_bytes = (unsigned)bytes - copy_bytes;
                unsigned count = 0;
                while (remaining_bytes > frag_size)
                {
                    each_audio_buf = new int8_t[frag_size]();
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                    {
                        memcpy(each_audio_buf,
                            (uint8_t*)data + copy_bytes + frag_size * count,
                            frag_size);
                    }
                    std::unique_lock<std::mutex> ul(audio_mutex);
                    audio_data.push_back(each_audio_buf);
                    audio_cv.notify_one();
                    ul.unlock();
                    remaining_bytes -= frag_size;
                    count++;
                }
                each_audio_buf = new int8_t[frag_size]();
                readed = remaining_bytes;
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                {
                    memcpy(each_audio_buf,
                        (uint8_t*)data + copy_bytes + frag_size * count,
                        remaining_bytes);
                }
            }
            else
            {
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                {
                    memcpy(each_audio_buf + readed, data, copy_bytes);
                }
                readed += copy_bytes;
            }
            hr = wasapi_data->m_capture_client->ReleaseBuffer(packet_length);
            if (FAILED(hr))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to release audio"
                    " buffer.\n");
            }
        }
        hr = wasapi_data->m_client->Stop();
        if (FAILED(hr))
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to stop audio"
                " recorder.\n");
        }
        audio_enc_thread.join();
    }   // audioRecorder
}
#endif
