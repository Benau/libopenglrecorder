/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_OPENH264_ENCODER_HPP
#define HEADER_OPENH264_ENCODER_HPP

class CaptureLibrary;

namespace Recorder
{
#ifdef ENABLE_H264
    int openh264Encoder(CaptureLibrary* cl);
#else
    inline int openh264Encoder(CaptureLibrary* cl) { return 0; }
#endif
};
#endif
