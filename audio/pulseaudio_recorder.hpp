/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef WIN32

#ifndef HEADER_PULSEAUDIO_RECORD_HPP
#define HEADER_PULSEAUDIO_RECORD_HPP

class CaptureLibrary;
namespace Recorder
{
#ifdef ENABLE_REC_SOUND
    void audioRecorder(CaptureLibrary* cl);
#else
    inline void audioRecorder(CaptureLibrary* cl) {}
#endif
};

#endif

#endif
