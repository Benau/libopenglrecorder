/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_MJPEG_WRITER_HPP
#define HEADER_MJPEG_WRITER_HPP

class CaptureLibrary;

namespace Recorder
{
    void mjpegWriter(CaptureLibrary* cl);
};

#endif
