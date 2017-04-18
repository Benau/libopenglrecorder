/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_VORBIS_ENCODE_HPP
#define HEADER_VORBIS_ENCODE_HPP

struct AudioEncoderData;
namespace Recorder
{
#ifdef ENABLE_REC_SOUND
    int vorbisEncoder(AudioEncoderData* aed);
#else
    inline int vorbisEncoder(AudioEncoderData* aed) { return 0; }
#endif
};

#endif
