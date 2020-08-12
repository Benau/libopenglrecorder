# libopenglrecorder

`libopenglrecorder` is a library allowing optional async readback OpenGL
frame buffer with optional audio recording. It will do video and audio
encoding together. The user of this library has to setup OpenGL context
himself and load suitable callbacks. All functions exposed by
`libopenglrecorder` should be called by the same thread which created the
OpenGL context. Currently, Linux and Windows are supported.

Audio recording is done via PulseAudio on Linux, and via Wasapi on Windows,
so for Linux make sure that your app is playing sound through PulseAudio, while
on Windows make sure Wasapi is present which means Windows Vista or later.
You can optionally link dynamically against PulseAudio to perform lazy loading of libpulse.

One potential use case for `libopenglrecorder` is integrating video recording into a game.

## License

This software is released under BSD 3-clause license, see [`LICENSE`](/LICENSE).

## Dependencies

  * TurboJPEG (required)
  * LibVPX (optional, for VP8 / VP9 encoding)
  * OpenH264 (optional, for H264 encoding)
  * Vorbis (optional if built without audio recording)
  * PulseAudio (optional if built without audio recording in Linux)

## Building on Linux

Install the following packages, Ubuntu command:

```
sudo apt-get install build-essential cmake libturbojpeg \
libvpx-dev libogg-dev libvorbisenc2 libvorbis-dev \
libpulse-dev pkg-config
```

### Compiling

Make sure you have a C++11 capable compiler, GCC 4.8 or later recommended. After
checking out the repo, `cd libopenglrecorder`, and ensure that you have all of
the above dependencies installed:

```
mkdir cmake_build
cd cmake_build
cmake ..
make -j4
sudo make install
```

## Windows

Prebuilt binaries are avaliable [here](https://github.com/supertuxkart/dependencies).
Copy the required files in windows(_64bit)/dependencies/{dll,lib} for MSVC
(or MinGW if you are using Cygwin), you will need
**openglrecorder.dll**, **libvorbisenc.dll**, **libvorbis.dll**, **libogg.dll**,
**libvpx.dll** and **libturbojpeg.dll** at runtime, and **openglrecorder.lib**
at link time. To compile it yourself, just copy the whole dependencies directory
inside here, and run CMake to configure, make sure that no prebuilt `openglrecorder`
binaries or headers file are copied.

## Usage of this library

See [`USAGE.md`](/USAGE.md) for details.
