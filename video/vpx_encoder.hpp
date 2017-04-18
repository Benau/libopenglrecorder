/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_VPX_ENCODER_HPP
#define HEADER_VPX_ENCODER_HPP

class CaptureLibrary;

namespace Recorder
{
#ifdef ENABLE_VPX
    int vpxEncoder(CaptureLibrary* cl);
#else
    inline int vpxEncoder(CaptureLibrary* cl) { return 0; }
#endif
};
#endif
