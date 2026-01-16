#include <opencv2/opencv.hpp>
#include <chrono>
#include "CameraManager.h"
#include <cstdio>
#include <iostream>

CameraManager::CameraManager()
{
	for (int i = 0; i < MAX_CAMERAS; i++)
	{
		_workers[i] = nullptr;
		_isRunning[i] = false;
	}
}

CameraManager::~CameraManager()
{
	
}

CameraManager& CameraManager::Instance()
{
	static CameraManager instance;

	return instance;
}

int CameraManager::ScanCameras(int* outIndices, int maxCheck)
{
	int count = 0;
	std::cout << "Start Scanning Camera...";

	for (int i = 0; i < maxCheck; i++)
	{
		cv::VideoCapture tempCap;
		tempCap.open(i, cv::CAP_DSHOW);

		if (tempCap.isOpened())
		{
			outIndices[count] = i;
			count += 1;

			std::cout << "Found Camera Device Index: " + std::to_string(i);
			tempCap.release();
		}
	}

	return count;
}

std::string CameraManager::GetCameraName(int deviceIndex) {
    std::string resultName = "Unknown Device";

    // 1. COM 라이브러리 초기화
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return resultName;

    // 2. 시스템 장치 열거자 생성
    ICreateDevEnum* pDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);

    if (SUCCEEDED(hr)) {
        // 3. 비디오 입력 장치 열거자 생성
        IEnumMoniker* pEnum = NULL;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);

        if (hr == S_OK) {
            IMoniker* pMoniker = NULL;
            int currentIdx = 0;

            // 4. 장치들을 순회하며 찾고자 하는 index까지 이동
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
                if (currentIdx == deviceIndex) {
                    // 해당 인덱스의 장치를 찾음 -> 속성(이름) 가져오기
                    IPropertyBag* pPropBag;
                    hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);

                    if (FAILED(hr)) {
                        pMoniker->Release();
                        continue;
                    }

                    VARIANT var;
                    VariantInit(&var);

                    // "FriendlyName" 속성이 우리가 원하는 장치 이름
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    if (SUCCEEDED(hr)) {
                        // BSTR(Wide Char) -> std::string 변환
                        char tmp[256];
                        int len = WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, tmp, sizeof(tmp), NULL, NULL);
                        if (len > 0) {
                            resultName = std::string(tmp);
                        }
                        VariantClear(&var);
                    }
                    pPropBag->Release();
                    pMoniker->Release();
                    break; // 찾았으니 종료
                }

                pMoniker->Release();
                currentIdx++;
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }

    // COM 해제
    CoUninitialize();

    return resultName;
}
