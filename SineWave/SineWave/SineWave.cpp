// SineWave.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <string>

constexpr double PI = 3.14159265358979323846;
constexpr int SAMPLE_RATE = 48000;
constexpr int BUFFER_SIZE = 48000;
constexpr double AMPLITUDE = 0.5;  // Output level

// The GenerateSineWave function generates a sine wave.
void GenerateSineWave(float* buffer, int bufferSize, int frequency) {
    for (int i = 0; i < bufferSize; ++i) {
        double time = static_cast<double>(i) / SAMPLE_RATE;
        double sample = AMPLITUDE * sin(2.0 * PI * frequency * time);
		buffer[i * 2] = static_cast<float>(sample);     // Left channel
		buffer[i * 2 + 1] = static_cast<float>(sample); // Right channel
    }
}

int PlaySineWave(int frequency, int seconds) {
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {  
       std::cerr << "CoInitialize failed: " << std::hex << hr << std::endl;  
       return -1;  
    }
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioRenderClient* pRenderClient = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));  
    if (FAILED(hr)) {  
       std::cerr << "CoCreateInstance failed: " << std::hex << hr << std::endl;  
       return -1;  
    }
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);

    WAVEFORMATEX* pwfx = nullptr;
    pAudioClient->GetMixFormat(&pwfx);

    pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, nullptr);

    pAudioClient->GetService(IID_PPV_ARGS(&pRenderClient));

	// Get the actual size of the allocated buffer.
    UINT32 bufferFrameCount;
    pAudioClient->GetBufferSize(&bufferFrameCount);

	// Prepare a buffer for the generated sine wave.
    float* buffer = new float[bufferFrameCount * pwfx->nChannels];
    GenerateSineWave(buffer, bufferFrameCount, frequency);

    pAudioClient->Start();

	// Play the sine wave for the specified number of seconds.
    UINT32 padding = 0;
    for (int i = 0; i < seconds; ++i) {
        pAudioClient->GetCurrentPadding(&padding);
        UINT32 availableFrames = bufferFrameCount - padding;

        BYTE* pData = nullptr;
        pRenderClient->GetBuffer(availableFrames, &pData);
        memcpy(pData, buffer, availableFrames * pwfx->nBlockAlign);
        pRenderClient->ReleaseBuffer(availableFrames, 0);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    pAudioClient->Stop();

    delete[] buffer;
    pRenderClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

	return 0;
}

//
// PlaySineWave
//
void usage() {
	std::cerr << "Usage: SineWave.exe [time] time 0 - 1000, 0 is forever, default" << std::endl;
}

//
// main
//
int main(int ac, char **av) {
	int time = 0;

    if (ac > 2) {
		usage();
		return -1;
	}
    else if (ac == 2) {
        time = std::stoi(av[1]);
    }

	if (time < 0 || time > 1000) {
		usage();
		return -1;
	}

    std::cout << "Sine Wave!, times = " << time << std::endl;

    for (int i = 0; ! time || i < time; ++i) {
        (void) PlaySineWave(440, 1);
        (void) PlaySineWave(880, 1);
    }

    return 0;
}
