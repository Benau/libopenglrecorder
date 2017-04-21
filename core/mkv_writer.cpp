/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#include "core/recorder_private.hpp"

#include <algorithm>
#include <cstring>
#include <list>
#include <memory>
#include <mkvmuxer/mkvmuxer.h>
#include <mkvmuxer/mkvwriter.h>
#include <mkvparser/mkvparser.h>
#include <sys/stat.h>

namespace Recorder
{
    // ------------------------------------------------------------------------
    std::string writeMKV(const std::string& video, const std::string& audio)
    {
        std::string no_ext = video.substr(0, video.find_last_of("."));
        VideoFormat vf = getConfig()->m_video_format;
        std::string file_name = no_ext +
            (vf == OGR_VF_VP8 || vf == OGR_VF_VP9 ? ".webm" : ".mkv");
        mkvmuxer::MkvWriter writer;
        if (!writer.Open(file_name.c_str()))
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Error while opening output"
                " file.\n");
            return "";
        }
        mkvmuxer::Segment muxer_segment;
        if (!muxer_segment.Init(&writer))
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Could not initialize muxer"
                " segment.\n");
            return "";
        }

        std::list<std::unique_ptr<mkvmuxer::Frame> > audio_frames;
        const unsigned max_buf_size = std::max(getConfig()->m_height *
            getConfig()->m_width * 3, unsigned(1024 * 1024));
        std::unique_ptr<uint8_t[]> buf_holder(new uint8_t[max_buf_size]);
        uint8_t* buf = buf_holder.get();

        FILE* input = NULL;
        struct stat st;
        int result = stat(audio.c_str(), &st);
        size_t readed = 0;
        if (result == 0)
        {
            input = fopen(audio.c_str(), "rb");
            uint32_t sample_rate, channels;
            readed = fread(&sample_rate, 1, sizeof(uint32_t), input);
            if (readed != sizeof(uint32_t))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for sample"
                    " rate.\n");
                return "";
            }
            readed = fread(&channels, 1, sizeof(uint32_t), input);
            if (readed != sizeof(uint32_t))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for"
                    " channels.\n");
                return "";
            }
            if (sample_rate > 48000 || channels > 256)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Invalid values for"
                    " sample rate or channels.\n");
                return "";
            }
            uint64_t aud_track = muxer_segment.AddAudioTrack(sample_rate, channels,
                0);
            if (!aud_track)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Could not add audio"
                    " track.\n");
                return "";
            }
            mkvmuxer::AudioTrack* const at = static_cast<mkvmuxer::AudioTrack*>
                (muxer_segment.GetTrackByNumber(aud_track));
            if (!at)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Could not get audio"
                    " track.\n");
                return "";
            }
            uint32_t codec_private_size = 0;
            readed = fread(&codec_private_size, 1, sizeof(uint32_t), input);
            if (readed != sizeof(uint32_t))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for codec"
                    " private.\n");
                return "";
            }
            if (codec_private_size > 0 && codec_private_size < max_buf_size)
            {
                readed = fread(buf, 1, codec_private_size, input);
                if (readed != codec_private_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for"
                        " codec private size.\n");
                    return "";
                }
                if (!at->SetCodecPrivate(buf, codec_private_size))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Could not add audio"
                        " private data.\n");
                    return "";
                }
            }
            while (fread(buf, 1, 12, input) == 12)
            {
                uint32_t frame_size;
                int64_t timestamp;
                memcpy(&frame_size, buf, sizeof(uint32_t));
                memcpy(&timestamp, buf + sizeof(uint32_t), sizeof(int64_t));
                if (frame_size > max_buf_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid frame size"
                        " for audio.\n");
                    return "";
                }
                readed = fread(buf, 1, frame_size, input);
                if (readed != frame_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for"
                        " audio frame size.\n");
                    return "";
                }
                mkvmuxer::Frame* audio_frame = new mkvmuxer::Frame();
                if (!audio_frame->Init(buf, frame_size))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Failed to construct"
                        " a frame.\n");
                    return "";
                }
                audio_frame->set_track_number(aud_track);
                audio_frame->set_timestamp(timestamp);
                audio_frame->set_is_key(true);
                audio_frames.emplace_back(audio_frame);
            }
            fclose(input);
            input = NULL;
            if (remove(audio.c_str()) != 0)
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Failed to remove audio"
                    " data file\n");
            }
        }
        uint64_t vid_track = muxer_segment.AddVideoTrack(getConfig()->m_width,
            getConfig()->m_height, 0);
        if (!vid_track)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Could not add video"
                " track.\n");
            return "";
        }
        mkvmuxer::VideoTrack* const vt = static_cast<mkvmuxer::VideoTrack*>(
            muxer_segment.GetTrackByNumber(vid_track));
        if (!vt)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Could not get video"
                " track.\n");
            return "";
        }
        vt->set_frame_rate(getConfig()->m_record_fps);
        switch (vf)
        {
        case OGR_VF_VP8:
            vt->set_codec_id("V_VP8");
            break;
        case OGR_VF_VP9:
            vt->set_codec_id("V_VP9");
            break;
        case OGR_VF_MJPEG:
            vt->set_codec_id("V_MJPEG");
            break;
        case OGR_VF_H264:
            vt->set_codec_id("V_MPEG4/ISO/AVC");
            break;
        default:
            break;
        }
        result = stat(video.c_str(), &st);
        if (result == 0)
        {
            input = fopen(video.c_str(), "rb");
            uint32_t codec_private_size;
            readed = fread(&codec_private_size, 1, sizeof(uint32_t), input);
            if (readed != sizeof(uint32_t))
            {
                runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for codec"
                    " private.\n");
                return "";
            }
            if (codec_private_size > 0 && codec_private_size < max_buf_size)
            {
                readed = fread(buf, 1, codec_private_size, input);
                if (readed != codec_private_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for"
                        " codec private size.\n");
                    return "";
                }
                if (!vt->SetCodecPrivate(buf, codec_private_size))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Could not add video"
                        " private data.\n");
                    return "";
                }
            }
            while (fread(buf, 1, 13, input) == 13)
            {
                bool key_frame;
                uint32_t frame_size;
                int64_t timestamp;
                memcpy(&frame_size, buf, sizeof(uint32_t));
                if (frame_size > max_buf_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid frame size"
                        " for video.\n");
                    return "";
                }
                memcpy(&timestamp, buf + sizeof(uint32_t), sizeof(int64_t));
                memcpy(&key_frame, buf + sizeof(uint32_t) + sizeof(int64_t),
                    sizeof(bool));
                timestamp *= 1000000000ll / getConfig()->m_record_fps;
                readed = fread(buf, 1, frame_size, input);
                if (readed != frame_size)
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Invalid read for"
                        " video frame size.\n");
                    return "";
                }
                mkvmuxer::Frame muxer_frame;
                if (!muxer_frame.Init(buf, frame_size))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Failed to construct"
                        " a frame.\n");
                    return "";
                }
                muxer_frame.set_track_number(vid_track);
                muxer_frame.set_timestamp(timestamp);
                muxer_frame.set_is_key(key_frame);
                mkvmuxer::Frame* cur_aud_frame =
                    audio_frames.empty() ? NULL : audio_frames.front().get();
                if (cur_aud_frame != NULL)
                {
                    while (cur_aud_frame->timestamp() < (uint64_t)timestamp)
                    {
                        if (!muxer_segment.AddGenericFrame(cur_aud_frame))
                        {
                            runCallback(OGR_CBT_ERROR_RECORDING, "Could not"
                                " add audio frame.\n");
                            return "";
                        }
                        audio_frames.pop_front();
                        if (audio_frames.empty())
                        {
                            cur_aud_frame = NULL;
                            break;
                        }
                        cur_aud_frame = audio_frames.front().get();
                    }
                }
                if (!muxer_segment.AddGenericFrame(&muxer_frame))
                {
                    runCallback(OGR_CBT_ERROR_RECORDING, "Could not add video"
                        " frame.\n");
                    return "";
                }
            }
        }
        if (input)
        {
            fclose(input);
        }
        if (remove(video.c_str()) != 0)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to remove video data"
                " file.\n");
        }
        if (!muxer_segment.Finalize())
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Finalization of segment"
                " failed.\n");
            return "";
        }
        writer.Close();
        return file_name;
    }   // writeMKV
};
