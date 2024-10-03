// SoundPlay.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <Propsys.h>
#include <Functiondiscoverykeys_devpkey.h>  // これも追加してください

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

//#define BUFFER_SIZE 9600  // バッファサイズ
#define BUFFER_SIZE 19200  // バッファサイズ

#define REFTIME_PER_MILLICEC (10) 

#define DEBUGP 0

#pragma comment(lib, "Propsys.lib")

// デバイス情報を表示
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
    //wprintf(L"Device Name: %ws\n", varName.pwszVal);

    PropVariantClear(&varName);
    pProps->Release();
}

// オーディオストリームの処理
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
            // 入力データがないときは少し待機
            std::this_thread::sleep_for(std::chrono::milliseconds(REFTIME_PER_MILLICEC));
            // Debug note
            std::wcout << L"Sleep 10 for Capture: " << std::endl;
            continue;
        }

        UINT32 numFramesAvailable;
        pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);

        // キャプチャされたオーディオをコピー
        memcpy(buffer.data(), pData, numFramesAvailable * pwfx->nBlockAlign);
#if DEBUGP
        std::wcout << L"Capture Copied: " << numFramesAvailable * pwfx->nBlockAlign << std::endl;
#endif

        pCaptureClient->ReleaseBuffer(numFramesAvailable);

#if DEBUGP
		// Debug note
        std::wcout << L"Captured numFramesAvailable: " << numFramesAvailable
            << L", pwfx->nBlockAlign: " << pwfx->nBlockAlign
            << L", total: " << (numFramesAvailable * pwfx->nBlockAlign)
            << L", packetLength: " << packetLength
            << std::endl;
#endif
        while (true) {
            HRESULT hr;

            //UINT32 padding = 0;
            //hr = pAudioClientOut->GetCurrentPadding(&padding);
            //if (FAILED(hr)) {
            //    std::cerr << "Failed to get padding. Error code: " << hr << std::endl;
            //    return;
            //}

            //バッファサイズが pwfx->nSamplesPerSec とは異なる場合も考えられるため、GetBufferSize() を使ってバッファサイズを管理
            UINT32 bufferFrameCount = 0;
            hr = pAudioClientOut->GetBufferSize(&bufferFrameCount);
            if (FAILED(hr)) {
                std::cerr << "Failed to get buffer size. Error code: " << hr << std::endl;
                return;
            }

            std::wcout << L"numFramesAvailable: " << numFramesAvailable << std::endl;

#if DEBUGP
			std::wcout << L"padding: " << padding
                << L", bufferFrameCount: " << bufferFrameCount
                << L", pwfx->nSamplesPerSec: " << pwfx->nSamplesPerSec
                << std::endl;
#endif
            // UINT32 numFramesPadding = bufferFrameCount - padding;

            if (TRUE /*numFramesPadding > 0*/) {
                BYTE* pRenderData = nullptr;

#if 0
                UINT32 numFramesPadding = numFramesAvailable;
                //hr = pRenderClient->GetBuffer(numFramesPadding, &pRenderData);
                //if (FAILED(hr)) {
                //    std::cerr << "Failed to get render buffer. Error code: " << hr << std::endl;
                //    return;
                //}
#endif
#if 1
                /////////////////////////////////////
                hr = pRenderClient->GetBuffer(numFramesAvailable, &pRenderData);
                if (FAILED(hr)) {
                    std::cerr << "Failed to get render buffer. Error code: " << hr << std::endl;
                    return;
                }
                if (pRenderData == nullptr) {
                    std::cerr << "Render data buffer is null." << std::endl;
                    return;
                }
#endif
                // バッファにデータを書き込む
                //memcpy(pRenderData, buffer.data(), numFramesPadding * pwfx->nBlockAlign);
                memcpy(pRenderData, buffer.data(), numFramesAvailable * pwfx->nBlockAlign);

#if DEBUGP
				// Debug note
                std::wcout << L"Writing buffer: " << numFramesPadding
                    << L", bufferFrameCount: " << bufferFrameCount
                    << L", padding: " << padding
                    << L", total: " << (bufferFrameCount * padding)
                    << std::endl;
#endif
                // バッファを解放する
#if 1
//                hr = pRenderClient->ReleaseBuffer(numFramesPadding, 0);
                hr = pRenderClient->ReleaseBuffer(numFramesAvailable, 0);
                if (FAILED(hr)) {
                    std::cerr << "Failed to release render buffer. Error code: " << hr << std::endl;
                    return;
                }
#endif
				break;
            }
            else {
                // バッファが空くのを待機
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                // Debug note
#if DEBUGP
                std::wcout << L"Sleep 10 for wait: " << std::endl;
#endif
            }
        }
    }

    pAudioClientIn->Stop();
    pAudioClientOut->Stop();
}

HRESULT StartAudioProcessing() {
    //CoInitialize(nullptr);

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

    // 入力オーディオクライアントの取得
    hr = pDeviceIn->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientIn);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate input audio client. Error code: " << hr << std::endl;
        return hr;
    }

    // 出力デバイスの取得
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDeviceOut);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio render device. Error code: " << hr << std::endl;
        return hr;
    }

    PrintDeviceInfo(pDeviceOut);

    // 出力オーディオクライアントの取得
    hr = pDeviceOut->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientOut);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate output audio client. Error code: " << hr << std::endl;
        return hr;
    }

    // ミックスフォーマットの取得
    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClientIn->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Failed to get input mix format. Error code: " << hr << std::endl;
        return hr;
    }

    // フォーマットサポートの確認
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

    // 入力デバイスの初期化
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

    // デバイス情報表示
    std::wcout << L"Channels: " << pwfx->nChannels << std::endl;
    std::wcout << L"Sample Rate: " << pwfx->nSamplesPerSec << std::endl;
    std::wcout << L"Bits Per Sample: " << pwfx->wBitsPerSample << std::endl;

    HandleAudioStream(pAudioClientIn, pAudioClientOut, pCaptureClient, pRenderClient, pwfx);

    // 後処理
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

// エントリーポイント
int main() {
    setlocale(LC_ALL, "Japanese");

    std::cout << "Sound Play!\n";

    std::wcout << L"Starting audio processing..." << std::endl;
    StartAudioProcessing();

    return 0;
}
