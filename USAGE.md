# Usage of libopenglrecorder

In the code where you create your renderer or OpenGL context, add these for
libopenglrecorder initialization after them:
```c++
    RecorderConfig cfg;
    cfg.m_triple_buffering = 1;
    cfg.m_record_audio = 1;
    cfg.m_width = 1920;
    cfg.m_height = 1080;
    cfg.m_video_format = OGR_VF_VP8;
    cfg.m_audio_format = OGR_AF_VORBIS;
    cfg.m_audio_bitrate = 112000;
    cfg.m_video_bitrate = 200000;
    cfg.m_record_fps = 30;
    cfg.m_record_jpg_quality = 90;
    ogrInitConfig(&cfg);
    ogrRegReadPixelsFunction(glReadPixels);
    ogrRegPBOFunctions(glGenBuffers, glBindBuffer, glBufferData,
    glDeleteBuffers, glMapBuffer, glUnmapBuffer);
    ogrSetSavedName("record");
```

This will enable Vorbis and VP8 encoding with triple buffering enabled which saves
a **record.webm** in the current directory. `ogrSetSavedNamed();` or `ogrInitConfig();`
should only be called when there is no capturing happening, see `ogrCapturing();`.

You may adjust the settings above as names imply (see [`openglrecorder.h`](/openglrecorder.h) for details),
use `OGR_VF_MJPEG` will allow a faster saving of recording with better quality,
though the file will be large.

The sounding recording in Linux is done by PulseAudio and Windows by Wasapi,
so for Linux make sure that your app is playing sound through PulseAudio, while
in Windows make sure Wasapi is present which means Windows Vista or later.

Notice: In Windows you may need wrapper for those gl* functions, as some of
them may have a `__stdcall` supplied, this is true for GLEW at least, this is
an example using c++11 lambda:
```c++
    ogrRegReadPixelsFunction([]
        (int x, int y, int w, int h, unsigned int f, unsigned int t, void* d)
        { glReadPixels(x, y, w, h, f, t, d); });

    ogrRegPBOFunctions([](int n, unsigned int* b) { glGenBuffers(n, b); },
        [](unsigned int t, unsigned int b) { glBindBuffer(t, b); },
        [](unsigned int t, ptrdiff_t s, const void* d, unsigned int u)
        { glBufferData(t, s, d, u); },
        [](int n, const unsigned int* b) { glDeleteBuffers(n, b); },
        [](unsigned int t, unsigned int a) { return glMapBuffer(t, a); },
        [](unsigned int t) { return glUnmapBuffer(t); });

```
Then for each recording video do an `ogrPrepareCapture();` first, after that
near the end of your rendering loop do an `ogrCapture();`, libopenglrecorder
will add or discard frames for you, so difference between `m_record_fps` and
frame rate of your app doesn't matter, the synchronization for video and audio
will be handled too. You just need to make sure that this function is called
only once per frame.

Finally do an `ogrStopCapture();` to save the recording video, you may need an
`ogrDestroy();` for a proper clean up when you delete your renderer or OpenGL
context. Notice: If you somehow need to re-create the OpenGL context (changing
resolution for example), make sure that do an `ogrDestroy();` first, as the pbo
buffer is needed to be re-created too.
