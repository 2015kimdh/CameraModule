#include <opencv2/opencv.hpp>
#include <chrono>
#include "CameraManager.h"
#include <cstdio>
#include <iostream>
#include <dshow.h>

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

int CameraManager::ScanCameras(int* outDevices)
{
	int count = 0;
	std::cout << "Start Scanning Camera..." << '\n';

    // COM 사용 전 초기화
    HRESULT hr = CoInitialize(nullptr);

    // 실패했으니 중단
    if (FAILED(hr)) 
    {
        std::cout << "Error: COM Initialization Failed." << '\n';
        return 0;
    }

    // 2. 장치 관리자 생성
    ICreateDevEnum* deviceEnum = nullptr;
    hr = CoCreateInstance(
        CLSID_SystemDeviceEnum, // 시스템 열거자를 만들겠다
        nullptr, // 상위 객체 - 없으므로 null
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum, 
        reinterpret_cast<void**>(&deviceEnum));

    // 위에 COM 함수가 성공했다면
    if (SUCCEEDED(hr))
    {
        IEnumMoniker* pEnum = nullptr; // 장치 목록을 가리킬 포인터
        hr = deviceEnum->CreateClassEnumerator(
            CLSID_VideoInputDeviceCategory, // 찾을 장치 범주
            &pEnum, // 찾은 목록을 전달할 곳
            0);

        if (hr == S_OK)
        {
            IMoniker* moniker = nullptr;
            int currentDeviceIndex = 0;

            while (pEnum->Next(1, &moniker, nullptr) == S_OK)
            {
                // 추후 혹시라도 카메라 필터 기능을 넣는다면 해당 함수가 들어갈 공간
            	
            	outDevices[count] = currentDeviceIndex;
                count++;
                currentDeviceIndex++; // 다음 번호로

                moniker->Release();
            }
            pEnum->Release();
        }
        else
        {
            std::cout << "No Video Input Device Found" << '\n';
        }
        deviceEnum->Release();
    }
    CoUninitialize();

    std::cout << "Scan Complete. Found Devices : " + std::to_string(count) << '\n';

	return count;
}

std::string CameraManager::GetCameraName(int deviceIndex) {
    std::string resultName = UNKNOWN_DEVICE;

    // 1. COM 라이브러리 초기화
    HRESULT hr = CoInitialize(nullptr);

    // 실패하면 모르는 장치라고 반환
    if (FAILED(hr)) return UNKNOWN_DEVICE;

    // 2. 장치 관리자 생성
    ICreateDevEnum* deviceEnum = nullptr;
    hr = CoCreateInstance(
        CLSID_SystemDeviceEnum, // 시스템 열거자를 만들겠다
        nullptr, // 상위 객체 - 없으므로 null
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        reinterpret_cast<void**>(&deviceEnum));

    // 위에 COM 함수가 성공했다면
    if (SUCCEEDED(hr))
    {
        IEnumMoniker* pEnum = nullptr; // 장치 목록을 가리킬 포인터
        hr = deviceEnum->CreateClassEnumerator(
            CLSID_VideoInputDeviceCategory, // 찾을 장치 범주
            &pEnum, // 찾은 목록을 전달할 곳
            0);

        // 목록이 비어있지 않다면
        if (hr == S_OK) {
            IMoniker* pMoniker = nullptr;
            int currentDeviceIndex = 0;

            // 4. 장치들을 순회하며 찾고자 하는 index까지 이동
            while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
                if (currentDeviceIndex == deviceIndex) {
                    // 해당 인덱스의 장치를 찾음 -> 속성(이름) 가져오기
                    IPropertyBag* pPropBag;
                    hr = pMoniker->BindToStorage(
                        0, 0, 
                        IID_IPropertyBag, 
                        reinterpret_cast<void**>(&pPropBag));

                    if (FAILED(hr)) {
                        pMoniker->Release();
                        continue;
                    }

                    VARIANT var;
                    VariantInit(&var);

                    // "FriendlyName" 속성이 우리가 원하는 장치 이름
                    hr = pPropBag->Read(L"FriendlyName", &var, nullptr);
                    if (SUCCEEDED(hr)) {
                        // BSTR(Wide Char) -> std::string 변환
                        char tmp[256];
                        int len = WideCharToMultiByte(
                            CP_ACP, 0, 
                            var.bstrVal, -1, tmp, 
                            sizeof(tmp), nullptr, nullptr);
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
                currentDeviceIndex++;
            }
            pEnum->Release();
        }
        deviceEnum->Release();
    }

    // COM 해제
    CoUninitialize();

    return resultName;
}
