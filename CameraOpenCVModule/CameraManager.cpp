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

/// <summary>
/// DirectShow를 통한 기기 목록에 접근하여 index에 맞는 장치의 이름을 가져옴
/// </summary>
/// <param name="deviceIndex">가져오고자 하는 장치의 index</param>
/// <returns></returns>
std::string CameraManager::GetCameraName(int deviceIndex) const
{
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

/// <summary>
/// 카메라 시작 함수
/// </summary>
/// <param name="camIndex">할당할 카메라 인덱스 (deviceIndex와 다름)</param>
/// <param name="deviceIndex">컴퓨터에서 찾을 웹캠 Index (기기 목록에서 사용할 인덱스)</param>
/// <param name="fps">목표 프레임</param>
void CameraManager::StartCamera(int camIndex, int deviceIndex, int w, int h, int fps)
{
    // 유효성 검사 (0번, 1번 슬롯이 맞는지)
    if (camIndex < 0 || camIndex >= MAX_CAMERAS) {
        std::cout << "Error: Invalid Camera Index\n";
        return;
    }

    // 이미 해당 슬롯에 카메라가 돌아가고 있다면 먼저 종료
    if (_workers[camIndex] != nullptr) {
        std::cout << "Stopping existing camera before restart...\n";

        // 관리 중이던 카메라 종료
        StopCamera(camIndex);
    }

    // 작동 camIndex 플래그 수정
    _isRunning[camIndex] = true;

    _workers[camIndex] = new std::thread(&CameraManager::WorkerLoop, this, camIndex, deviceIndex, w, h, fps);
}

void CameraManager::StopCamera(int camIndex)
{
    // 유효성
    if (camIndex < 0 || camIndex >= MAX_CAMERAS)
        return;

    // 루프 종료 신호
    _isRunning[camIndex] = false;

    if (_workers[camIndex] != nullptr)
    {
        if (_workers[camIndex]->joinable())
        {
            _workers[camIndex]->join();
        }
        // 메모리 해제
        delete _workers[camIndex];
        _workers[camIndex] = nullptr;
    }

    std::cout << "Camera Thread Stopped: Slot " + std::to_string(camIndex) + "\n";
}

void CameraManager::WorkerLoop(int camIndex, int deviceIndex, int reqW, int reqH, int fps)
{
    // 카메라 열기
    cv::VideoCapture capture;
    capture.open(deviceIndex, cv::CAP_DSHOW);

    // 못 열었다
    if (!capture.isOpened())
    {
        std::cout << R"([Error] 카메라를 열 수 없습니다. Index: )" << deviceIndex << '\n';
        _isRunning[camIndex] = false;
        return;
    }

    // 요청 해상도 및 FPS 설정 요청
    capture.set(cv::CAP_PROP_FRAME_WIDTH, reqW);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, reqH);
    capture.set(cv::CAP_PROP_FPS, fps);

    // 실제로 적용된 해상도를 확인하기
    // 하드웨어가 지원하지 않는 해상도를 요청하면 가장 가까운 다른 값으로 설정
    // 비율이 깨지는지 확인하기 위해 실제 값을 로그로 찍어보기
    int actualWidth = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    int actualHeight = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));

    std::cout << ">>> [" << camIndex << R"(번 카메라] 설정 완료)" << '\n';
    std::cout << R"(    - 요청: )" << reqW << " x " << reqH << '\n';
    std::cout << R"(    - 실제: )" << actualWidth << " x " << actualHeight << '\n';
    
	//======================여기까지 카메라 기본 세팅=================================

    std::string windowName = "Cam View " + std::to_string(camIndex);
    cv::Mat frame;

    while (_isRunning[camIndex])
    {
	    if (capture.read(frame))
	    {
		    if (!frame.empty())
			    cv::imshow(windowName,frame);

            if (cv::waitKey(1) == 27)
                _isRunning[camIndex] = false;
	    }
        else
        {
            // 프레임이 비었다면 잠깐 쉬기
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    cv::destroyWindow(windowName);
    capture.release();
    std::cout << ">>> [" << camIndex << R"(번 카메라] 종료됨.)" << '\n';
}
