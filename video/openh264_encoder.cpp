/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifdef ENABLE_H264

#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"

#include <wels/codec_api.h>
#include <wels/codec_ver.h>

namespace Recorder
{
    // ------------------------------------------------------------------------
    int openh264Encoder(CaptureLibrary* cl)
    {
        // Runtime encoder checking
        if (cl == NULL)
            return 1;
        setThreadName("openH264Encoder");
        FILE* h264_data = fopen((getSavedName() + ".video").c_str(), "wb");
        if (h264_data == NULL)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to open file for"
                " writing h264.\n");
            return 1;
        }

        ISVCEncoder* o264_encoder = NULL;
        int ret = WelsCreateSVCEncoder(&o264_encoder);
        if (ret != 0 || o264_encoder == NULL)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to create openh264"
                " encoder.\n");
            return 1;
        }

        const unsigned width = cl->getRecorderConfig().m_width;
        const unsigned height = cl->getRecorderConfig().m_height;

        SEncParamExt param;
        o264_encoder->GetDefaultParams(&param);
        param.iUsageType = CAMERA_VIDEO_REAL_TIME;
        param.fMaxFrameRate = cl->getRecorderConfig().m_record_fps;
        param.iPicWidth = width;
        param.iPicHeight = height;
        param.iTargetBitrate = cl->getRecorderConfig().m_video_bitrate;
        param.iMaxBitrate = cl->getRecorderConfig().m_video_bitrate;
        param.iRCMode = RC_BUFFERBASED_MODE;
        param.iTemporalLayerNum = 1;
        param.iSpatialLayerNum = 1;
        param.bEnableDenoise = 0;
        param.bEnableBackgroundDetection = 1;
        param.bEnableAdaptiveQuant = 1;
        param.bEnableFrameSkip = false;
        param.bEnableLongTermReference = 0;
        param.iLtrMarkPeriod = 30;
#if OPENH264_MAJOR > 1 || (OPENH264_MAJOR == 1 && OPENH264_MINOR >= 4)
        param.eSpsPpsIdStrategy = CONSTANT_ID;
#else
        param.bEnableSpsPpsIdAddition = 0;
#endif
        param.bPrefixNalAddingCtrl = 0;
        param.iLoopFilterDisableIdc = 0;
        param.sSpatialLayers[0].iVideoWidth = param.iPicWidth;
        param.sSpatialLayers[0].iVideoHeight = param.iPicHeight;
        param.sSpatialLayers[0].fFrameRate = param.fMaxFrameRate;
        param.sSpatialLayers[0].iSpatialBitrate = param.iTargetBitrate;
        param.sSpatialLayers[0].iMaxSpatialBitrate = param.iMaxBitrate;
        param.sSpatialLayers[0].uiProfileIdc = PRO_HIGH;
        o264_encoder->InitializeExt(&param);

        SFrameBSInfo fbi = {0};
        memset(&fbi, 0, sizeof(SFrameBSInfo));
        ret = o264_encoder->EncodeParameterSets(&fbi);
        if (ret != cmResultSuccess)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to create openh264"
                " header.\n");
            fclose(h264_data);
            o264_encoder->Uninitialize();
            WelsDestroySVCEncoder(o264_encoder);
            return 1;
        }

        const uint8_t one = 1;
        uint8_t* sps_data = fbi.sLayerInfo[0].pBsBuf + 4;
        uint16_t sps_length = fbi.sLayerInfo[0].pNalLengthInByte[0] - 4;
        uint8_t* pps_data = fbi.sLayerInfo[0].pBsBuf + sps_length + 8;
        uint16_t pps_length = fbi.sLayerInfo[0].pNalLengthInByte[1] - 4;
        const uint32_t header_size = 5 + 3 + sps_length + 3 + pps_length;
        fwrite(&header_size, 1, sizeof(uint32_t), h264_data);

        // Version
        fwrite(&one, 1, sizeof(uint8_t), h264_data);
        // Profile
        fwrite(&sps_data[1], 1, sizeof(uint8_t), h264_data);
        // Profile constraints
        fwrite(&sps_data[2], 1, sizeof(uint8_t), h264_data);
        // Level
        fwrite(&sps_data[3], 1, sizeof(uint8_t), h264_data);

        // 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
        uint8_t tmp = 0xff;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);

        // 3 bits reserved (111) + 5 bits number of sps (00001)
        tmp = 0xe1;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
        tmp = (sps_length >> 8) & 0xff;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
        tmp = sps_length & 0xff;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
        fwrite(sps_data, 1, sps_length, h264_data);

        // PPS size, length and data
        fwrite(&one, 1, sizeof(uint8_t), h264_data);
        tmp = (pps_length >> 8) & 0xff;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
        tmp = pps_length & 0xff;
        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
        fwrite(pps_data, 1, pps_length, h264_data);

        int64_t frames_encoded = 0;
        uint8_t* yuv = new uint8_t[width * height * 3 / 2]();
        float last_size = -1.0f;
        int cur_finished_count = 0;
        while (true)
        {
            std::unique_lock<std::mutex> ul(*cl->getJPGListMutex());
            cl->getJPGListCV()->wait(ul, [&cl]
                { return !cl->getJPGList()->empty(); });
            auto& p = cl->getJPGList()->front();
            uint8_t* jpg = std::get<0>(p);
            uint32_t jpg_size = std::get<1>(p);
            int frame_count = std::get<2>(p);
            if (jpg == NULL)
            {
                cl->getJPGList()->clear();
                ul.unlock();
                if (cl->displayingProgress())
                {
                    int rate = 99;
                    runCallback(OGR_CBT_PROGRESS_RECORDING, &rate);
                }
                break;
            }
            cl->getJPGList()->pop_front();
            ul.unlock();
            if (cl->displayingProgress())
            {
                if (last_size == -1.0f)
                    last_size = (float)(cl->getJPGList()->size());
                cur_finished_count += frame_count;
                int rate = (int)(cur_finished_count / last_size * 100.0f);
                rate = rate > 99 ? 99 : rate;
                runCallback(OGR_CBT_PROGRESS_RECORDING, &rate);
            }
            int ret = cl->yuvConversion(jpg, jpg_size, yuv);
            if (ret < 0)
            {
                tjFree(jpg);
                continue;
            }
            tjFree(jpg);
            memset(&fbi, 0, sizeof(SFrameBSInfo));
            SSourcePicture sp;
            memset(&sp, 0, sizeof(SSourcePicture));
            sp.iPicWidth = width;
            sp.iPicHeight = height;
            sp.iColorFormat = videoFormatI420;
            sp.iStride[0] = sp.iPicWidth;
            sp.iStride[1] = sp.iStride[2] = sp.iPicWidth >> 1;
            sp.pData[0] = yuv;
            sp.pData[1] = sp.pData[0] + width * height;
            sp.pData[2] = sp.pData[1] + (width * height >> 2);
            ret = o264_encoder->EncodeFrame(&sp, &fbi);
            uint32_t layers[MAX_LAYER_NUM_OF_FRAME] = {0};
            if (ret == cmResultSuccess &&
                fbi.eFrameType != videoFrameTypeSkip)
            {
                uint32_t frame_size = 0;
                for (int i = fbi.iLayerNum - 1; i < fbi.iLayerNum; i++)
                {
                    for (int j = 0; j < fbi.sLayerInfo[i].iNalCount; j++)
                    {
                        layers[i] += fbi.sLayerInfo[i].pNalLengthInByte[j];
                    }
                    frame_size += layers[i];
                }
                fwrite(&frame_size, 1, sizeof(uint32_t), h264_data);
                fwrite(&frames_encoded, 1, sizeof(int64_t), h264_data);
                bool key_frame = (fbi.eFrameType == videoFrameTypeIDR);
                fwrite(&key_frame, 1, sizeof(bool), h264_data);
                for (int i = fbi.iLayerNum - 1; i < fbi.iLayerNum; i++)
                {
                    uint32_t total_len = 4;
                    for (int j = 0; j < fbi.sLayerInfo[i].iNalCount; j++)
                    {
                        const uint32_t len = layers[i] - 4;
                        tmp = (len >> 24) & 0xff;
                        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
                        tmp = (len >> 16) & 0xff;
                        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
                        tmp = (len >> 8) & 0xff;
                        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
                        tmp = len & 0xff;
                        fwrite(&tmp, 1, sizeof(uint8_t), h264_data);
                        fwrite(fbi.sLayerInfo[i].pBsBuf + total_len, 1, len,
                            h264_data);
                        total_len += layers[i];
                    }
                }
                frames_encoded += frame_count;
            }
        }
        delete[] yuv;
        o264_encoder->Uninitialize();
        WelsDestroySVCEncoder(o264_encoder);
        fclose(h264_data);
        return 1;
    }   // openh264Encoder
}

#endif
