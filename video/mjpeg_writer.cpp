/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#include "core/capture_library.hpp"
#include "core/recorder_private.hpp"

namespace Recorder
{
    // ------------------------------------------------------------------------
    void mjpegWriter(CaptureLibrary* cl)
    {
        setThreadName("mjpegWriter");
        FILE* mjpeg_writer = fopen((getSavedName() + ".video").c_str(), "wb");
        if (mjpeg_writer == NULL)
        {
            runCallback(OGR_CBT_ERROR_RECORDING, "Failed to open file for"
                " writing mjpeg.\n");
            return;
        }
        int64_t frames_encoded = 0;
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
                break;
            }
            cl->getJPGList()->pop_front();
            ul.unlock();
            while (frame_count != 0)
            {
                fwrite(&jpg_size, 1, sizeof(uint32_t), mjpeg_writer);
                fwrite(&frames_encoded, 1, sizeof(int64_t), mjpeg_writer);
                fwrite(&jpg_size, 1, sizeof(uint32_t), mjpeg_writer);
                fwrite(jpg, 1, jpg_size, mjpeg_writer);
                frame_count--;
                frames_encoded++;
            }
            tjFree(jpg);
        }
        fclose(mjpeg_writer);
    }   // mjpegWriter
};
