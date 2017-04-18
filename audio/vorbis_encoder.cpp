/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifdef ENABLE_REC_SOUND

#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

namespace Recorder
{
    int vorbisEncoder(AudioEncoderData* aed)
    {
        // Runtime encoder checking
        if (aed == NULL)
            return 1;
        setThreadName("vorbisEncoder");
        vorbis_info vi;
        vorbis_dsp_state vd;
        vorbis_block vb;
        vorbis_info_init(&vi);
        vorbis_encode_init(&vi, aed->m_channels, aed->m_sample_rate, -1,
            aed->m_audio_bitrate, -1);
        vorbis_analysis_init(&vd, &vi);
        vorbis_block_init(&vd, &vb);
        vorbis_comment vc;
        vorbis_comment_init(&vc);
        vorbis_comment_add_tag(&vc, "Encoder",
            "Vorbis encoder by libopenglrecorder");
        ogg_packet header;
        ogg_packet header_comm;
        ogg_packet header_code;
        vorbis_analysis_headerout(&vd, &vc, &header, &header_comm,
            &header_code);
        if (header.bytes > 255 || header_comm.bytes > 255)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Header is too long for"
                " vorbis.\n");
            return 1;
        }
        FILE* vb_data = fopen((getSavedName() + ".audio").c_str(), "wb");
        if (vb_data == NULL)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to open file for"
                " encoding vorbis.\n");
            return 1;
        }
        fwrite(&aed->m_sample_rate, 1, sizeof(uint32_t), vb_data);
        fwrite(&aed->m_channels, 1, sizeof(uint32_t), vb_data);
        const uint32_t all = header.bytes + header_comm.bytes +
            header_code.bytes + 3;
        fwrite(&all, 1, sizeof(uint32_t), vb_data);
        uint8_t size = 2;
        fwrite(&size, 1, sizeof(uint8_t), vb_data);
        size = (uint8_t)header.bytes;
        fwrite(&size, 1, sizeof(uint8_t), vb_data);
        size = (uint8_t)header_comm.bytes;
        fwrite(&size, 1, sizeof(uint8_t), vb_data);
        fwrite(header.packet, 1, header.bytes, vb_data);
        fwrite(header_comm.packet, 1, header_comm.bytes, vb_data);
        fwrite(header_code.packet, 1, header_code.bytes, vb_data);
        ogg_packet op;
        int64_t last_timestamp = 0;
        bool eos = false;
        while (eos == false)
        {
            std::unique_lock<std::mutex> ul(*aed->m_mutex);
            aed->m_cv->wait(ul, [&aed] { return !aed->m_buf_list->empty(); });
            int8_t* audio_buf = aed->m_buf_list->front();
            aed->m_buf_list->pop_front();
            ul.unlock();
            if (audio_buf == NULL)
            {
                vorbis_analysis_wrote(&vd, 0);
                eos = true;
            }
            else
            {
                float **buffer = vorbis_analysis_buffer(&vd, 1024);
                const unsigned channels = aed->m_channels;
                if (aed->m_audio_type == AudioEncoderData::AT_PCM)
                {
                    for (unsigned j = 0; j < channels; j++)
                    {
                        for (unsigned i = 0; i < 1024; i++)
                        {
                            int8_t* each_channel =
                                &audio_buf[i * channels * 2 + j * 2];
                            buffer[j][i] = float((each_channel[1] << 8) |
                                (0x00ff & (int)each_channel[0])) / 32768.0f;
                        }
                    }
                }
                else
                {
                    for (unsigned j = 0; j < channels; j++)
                    {
                        for (unsigned i = 0; i < 1024; i++)
                        {
                            float* fbuf = reinterpret_cast<float*>(audio_buf);
                            buffer[j][i] = fbuf[i * channels + j];
                        }
                    }
                }
                vorbis_analysis_wrote(&vd, 1024);
            }
            while (vorbis_analysis_blockout(&vd, &vb) == 1)
            {
                vorbis_analysis(&vb, NULL);
                vorbis_bitrate_addblock(&vb);
                while (vorbis_bitrate_flushpacket(&vd, &op))
                {
                    if (op.granulepos > 0)
                    {
                        uint32_t frame_size = (uint32_t)op.bytes;
                        fwrite(&frame_size, 1, sizeof(uint32_t), vb_data);
                        fwrite(&last_timestamp, 1, sizeof(int64_t), vb_data);
                        fwrite(op.packet, 1, frame_size, vb_data);
                        double s = (double)op.granulepos /
                            (double)aed->m_sample_rate * 1000000000.;
                        last_timestamp = (int64_t)s;
                    }
                }
            }
            delete[] audio_buf;
        }
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        fclose(vb_data);
        return 1;
    }   // vorbisEncoder
}

#endif
