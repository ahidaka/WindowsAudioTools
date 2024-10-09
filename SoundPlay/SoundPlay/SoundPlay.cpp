// SoundPlay.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <Propsys.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

//#define BUFFER_SIZE 9600
#define BUFFER_SIZE 19200  // Process 10ms audio data

#define REFTIME_PER_MILLICEC (10) 

#pragma comment(lib, "Propsys.lib")

// Show device information
void PrintDeviceInfo(IMMDevice* pDevice) {
    if (pDevice == nullptr) {
        std::cerr << "Device is null" << std::endl;
        return;
    }

    IPropertyStore* pProps = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) {
        std::cerr << "Failed to open property store. Error code: " << hr << std::endl;
        return;
    }

    PROPVARIANT varName;
    PropVariantInit(&varName);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    if (FAILED(hr)) {
        std::cerr << "Failed to get device friendly name. Error code: " << hr << std::endl;
        return;
    }

    std::wcout << L"Device Name: " << varName.pwszVal << std::endl;

    PropVariantClear(&varName);
    pProps->Release();
}

// Process audio stream
void HandleAudioStream(IAudioClient* pAudioClientIn, IAudioClient* pAudioClientOut, IAudioCaptureClient* pCaptureClient, IAudioRenderClient* pRenderClient, WAVEFORMATEX* pwfx) {
    BYTE* pData;
    DWORD flags;
    UINT32 packetLength = 0;
    std::vector<BYTE> buffer(BUFFER_SIZE);

    pAudioClientIn->Start();
    pAudioClientOut->Start();

    if (pCaptureClient == nullptr) {
        std::cerr << "Capture client is null." << std::endl;
        return;
    }

    while (true) {
        pCaptureClient->GetNextPacketSize(&packetLength);
        if (packetLength == 0) {
			// If no data is available, sleep for a while
            std::this_thread::sleep_for(std::chrono::milliseconds(REFTIME_PER_MILLICEC));
            // Debug note
            std::wcout << L"Sleep 10 for Capture: " << std::endl;
            continue;
        }

        UINT32 numFramesAvailable;
        pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);

		// Copy data to buffer
        memcpy(buffer.data(), pData, numFramesAvailable * pwfx->nBlockAlign);
        pCaptureClient->ReleaseBuffer(numFramesAvailable);

        while (true) {
            HRESULT hr;
            UINT32 bufferFrameCount = 0;
            BYTE* pRenderData = nullptr;

            hr = pAudioClientOut->GetBufferSize(&bufferFrameCount);
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer size. Error code: " << hr << std::endl;
                return;
            }

            std::wcout << L"numFramesAvailable: " << numFramesAvailable << std::endl;

            hr = pRenderClient->GetBuffer(numFramesAvailable, &pRenderData);
            if (FAILED(hr)) {
                std::cerr << "Failed to get render buffer. Error code: " << hr << std::endl;
                return;
            }
            if (pRenderData == nullptr) {
                std::cerr << "Render data buffer is null." << std::endl;
                return;
            }
			// Write data to render buffer
            memcpy(pRenderData, buffer.data(), numFramesAvailable * pwfx->nBlockAlign);

            hr = pRenderClient->ReleaseBuffer(numFramesAvailable, 0);
            if (FAILED(hr)) {
                std::cerr << "Failed to release render buffer. Error code: " << hr << std::endl;
                return;
            }
			break;
        }
    }

    pAudioClientIn->Stop();
    pAudioClientOut->Stop();
}

HRESULT StartAudioProcessing() {
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDeviceIn = nullptr;
    IMMDevice* pDeviceOut = nullptr;
    IAudioClient* pAudioClientIn = nullptr;
    IAudioClient* pAudioClientOut = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    IAudioRenderClient* pRenderClient = nullptr;

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM library. Error code = 0x"
            << std::hex << hr << std::endl;
        return hr;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        std::cerr << "Failed to create IMMDeviceEnumerator. Error code: " << hr << std::endl;
        return hr;
    }

    // 入力デバイスの取得
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDeviceIn);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio capture device. Error code: " << hr << std::endl;
        return hr;
    }

    PrintDeviceInfo(pDeviceIn);

	// Get input audio client
    hr = pDeviceIn->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientIn);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate input audio client. Error code: " << hr << std::endl;
        return hr;
    }

	// Get output device
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDeviceOut);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio render device. Error code: " << hr << std::endl;
        return hr;
    }

    PrintDeviceInfo(pDeviceOut);

	// Get output audio client
    hr = pDeviceOut->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientOut);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate output audio client. Error code: " << hr << std::endl;
        return hr;
    }

	// Get mix format
    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClientIn->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Failed to get input mix format. Error code: " << hr << std::endl;
        return hr;
    }

	// Check if input format is supported
    WAVEFORMATEX* closestMatch = nullptr;
    hr = pAudioClientIn->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx, &closestMatch);
    if (hr == S_FALSE) {
        pwfx = closestMatch;
        std::cerr << "Input format not supported exactly, using closest match." << std::endl;
    }
    else if (FAILED(hr)) {
        std::cerr << "Input format not supported. Error code: " << hr << std::endl;
        return hr;
    }

	// Initialize audio clients
    hr = pAudioClientIn->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize input audio client. Error code: " << hr << std::endl;
        return hr;
    }

    hr = pAudioClientOut->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize output audio client. Error code: " << hr << std::endl;
        return hr;
    }

    hr = pAudioClientIn->GetService(IID_PPV_ARGS(&pCaptureClient));
    if (FAILED(hr)) {
        std::cerr << "Failed to get input capture client. Error code: " << hr << std::endl;
        return hr;
    }

    hr = pAudioClientOut->GetService(IID_PPV_ARGS(&pRenderClient));
    if (FAILED(hr)) {
        std::cerr << "Failed to get output render client. Error code: " << hr << std::endl;
        return hr;
    }

	// Show audio format
    std::wcout << L"Channels: " << pwfx->nChannels << std::endl;
    std::wcout << L"Sample Rate: " << pwfx->nSamplesPerSec << std::endl;
    std::wcout << L"Bits Per Sample: " << pwfx->wBitsPerSample << std::endl;

    HandleAudioStream(pAudioClientIn, pAudioClientOut, pCaptureClient, pRenderClient, pwfx);

	// Free resources
    pCaptureClient->Release();
    pRenderClient->Release();
    pAudioClientIn->Release();
    pAudioClientOut->Release();
    pDeviceIn->Release();
    pDeviceOut->Release();
    pEnumerator->Release();
    CoUninitialize();

	return S_OK;
}

// Entry point
int main() {
    setlocale(LC_ALL, "Japanese");

    std::cout << "Sound Play!\n";

    std::wcout << L"Starting audio processing..." << std::endl;
    StartAudioProcessing();

    return 0;
}
