/* Copyright (c) 2017, libopenglrecorder contributors
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree.
 */

#ifndef HEADER_MKV_WRITER_HPP
#define HEADER_MKV_WRITER_HPP
#include <string>

namespace Recorder
{
    std::string writeMKV(const std::string& video, const std::string& audio);
};

#endif
