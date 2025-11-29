// sine440_waveout.cpp
// Build: cl /EHsc sine440_waveout.cpp /link winmm.lib

#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdio>
#pragma comment(lib, "winmm.lib")

typedef short int16_t;

static volatile bool g_running = true;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT ||
        ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_LOGOFF_EVENT ||
        ctrlType == CTRL_SHUTDOWN_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main() {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Assumptions for simplicity
    const int sampleRate = 44100;       // 44.1 kHz
    const int channels = 2;           // stereo
    const int bitsPerSample = 16;       // signed 16-bit PCM
    const double freq = 440.0;       // A4
    const double amplitude = 0.3;       // 30% of full scale to avoid clipping
    const int framesPerBuffer = 2048;   // buffer size per write

    // Wave format setup
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = channels;
    fmt.nSamplesPerSec = sampleRate;
    fmt.wBitsPerSample = bitsPerSample;
    fmt.nBlockAlign = (fmt.nChannels * fmt.wBitsPerSample) / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    // Open default render device
    HWAVEOUT hwo = nullptr;
    MMRESULT mm = waveOutOpen(&hwo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
    if (mm != MMSYSERR_NOERROR) {
        char msg[256];
        waveOutGetErrorTextA(mm, msg, sizeof(msg));
        std::fprintf(stderr, "waveOutOpen failed: %s\n", msg);
        return 1;
    }

    // Prepare double buffers
    const int bytesPerFrame = fmt.nBlockAlign;
    const int bufferBytes = framesPerBuffer * bytesPerFrame;

    int16_t* bufA = (int16_t*)std::malloc(bufferBytes);
    int16_t* bufB = (int16_t*)std::malloc(bufferBytes);
    if (!bufA || !bufB) {
        std::fprintf(stderr, "Out of memory\n");
        waveOutClose(hwo);
        return 1;
    }

    WAVEHDR hdrA{}, hdrB{};
    hdrA.lpData = (LPSTR)bufA;
    hdrA.dwBufferLength = bufferBytes;
    hdrB.lpData = (LPSTR)bufB;
    hdrB.dwBufferLength = bufferBytes;

    waveOutPrepareHeader(hwo, &hdrA, sizeof(hdrA));
    waveOutPrepareHeader(hwo, &hdrB, sizeof(hdrB));

    // Sine generator state
    const double twoPi = 6.28318530717958647692;
    double phase = 0.0;
    const double phaseInc = twoPi * freq / sampleRate;
    const int16_t amp16 = (int16_t)std::lround(amplitude * 32767.0);

    auto fillBuffer = [&](int16_t* buf) {
        for (int i = 0; i < framesPerBuffer; ++i) {
            double s = std::sin(phase);
            phase += phaseInc;
            if (phase >= twoPi) phase -= twoPi;
            int16_t sample = (int16_t)std::lround(amp16 * s);
            // Stereo: L = R = sample
            buf[i * 2 + 0] = sample;
            buf[i * 2 + 1] = sample;
        }
        };

    // Prime and start
    fillBuffer(bufA);
    fillBuffer(bufB);
    waveOutWrite(hwo, &hdrA, sizeof(hdrA));
    waveOutWrite(hwo, &hdrB, sizeof(hdrB));

    // Loop: reuse buffers when they are done
    while (g_running) {
        if (hdrA.dwFlags & WHDR_DONE) {
            fillBuffer(bufA);
            hdrA.dwFlags &= ~WHDR_DONE;
            waveOutWrite(hwo, &hdrA, sizeof(hdrA));
        }
        if (hdrB.dwFlags & WHDR_DONE) {
            fillBuffer(bufB);
            hdrB.dwFlags &= ~WHDR_DONE;
            waveOutWrite(hwo, &hdrB, sizeof(hdrB));
        }
        Sleep(1); // light polling
    }

    // Stop and clean up
    waveOutReset(hwo);
    waveOutUnprepareHeader(hwo, &hdrA, sizeof(hdrA));
    waveOutUnprepareHeader(hwo, &hdrB, sizeof(hdrB));
    std::free(bufA);
    std::free(bufB);
    waveOutClose(hwo);

    return 0;
}