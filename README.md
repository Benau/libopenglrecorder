# libopenglrecorder

libopenglrecorder is a library allowing optional async readback OpenGL
frame buffer with optional audio recording. It will do video and audio
encoding together. The user of this library has to setup OpenGL context
himself and load suitable callbacks. All functions exposed by
libopenglrecorder should be called by the same thread which created the
OpenGL context. Currently using in Linux and Windows is supported.

One example usage is to create an advanced bundled-recorder for game out of it.

## License
This software is released under BSD 3-clause license, see [`LICENSE`](/LICENSE).

## Dependencies
  * TurboJPEG (required)
  * LibVPX (optional for VP8 / VP9 encoding)
  * OpenH264 (optional for H264 encoding)
  * Vorbis (optional if build without audio recording)
  * PulseAudio (optional if build without audio recording in Linux)

## Building on Linux

Install the following packages, Ubuntu command:

```
sudo apt-get install build-essential cmake libjpeg-turbo-devel \
libvpx-dev libogg-dev libvorbisenc2 libvorbis-dev \
libpulse-dev pkg-config
```

### Compiling

Make sure you have a c++11 capable compiler, gcc 4.8 recommended. After
checking out the repo, `cd libopenglrecorder`, and assume that you have all of
the above dependencies installed:

```
mkdir cmake_build
cd cmake_build
cmake ..
make -j4
sudo make install
```

## Windows

Prebulit binaries are avaliable [here](https://github.com/supertuxkart/dependencies).
Copy the suitable files in windows(_64bit)/dependencies/{dll,lib} for MSVC
(or mingw if you are using cygwin), you will need
**openglrecorder.dll**, **libvorbisenc.dll**, **libvorbis.dll**, **libogg.dll**,
**libvpx.dll** and **libturbojpeg.dll** for runtime, and **openglrecorder.lib**
for linking. For compiling yourself, just copy the whole dependencies directory
inside here, and run CMake to configure, make sure that no openglrecorder
prebuilt binaries or header file are copied.

## Usage of this library

See [`USAGE.md`](/USAGE.md) for details.
