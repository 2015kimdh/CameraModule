#include <opencv2/opencv.hpp>
#include <chrono>
#include "CameraManager.h"
#include <cstdio>
#include <iostream>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib") // 사용 라이브러리 링크

#pragma region Private
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

void CameraManager::SetVideoCapture(cv::VideoCapture* capture, int width, int height, int fps)
{
	// 해상도 변경이 잘 안 될 때, 포맷을 MJPG로 먼저 설정하면 잘 먹히는 경우가 많음
	capture->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

	// 요청 해상도 및 FPS 설정 요청
	capture->set(cv::CAP_PROP_FRAME_WIDTH, width);
	capture->set(cv::CAP_PROP_FRAME_HEIGHT, height);
	capture->set(cv::CAP_PROP_FPS, fps);

	// 실제로 적용된 해상도를 확인하기
	// 하드웨어가 지원하지 않는 해상도를 요청하면 가장 가까운 다른 값으로 설정
	// 비율이 깨지는지 확인하기 위해 실제 값을 로그로 찍어보기
	int actualWidth = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_WIDTH));
	int actualHeight = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_HEIGHT));

	std::cout << R"(    - 요청: )" << width << " x " << height << '\n';
	std::cout << R"(    - 실제: )" << actualWidth << " x " << actualHeight << '\n';
}

void CameraManager::ResizeFrame(cv::Mat* frame, cv::Mat* smallFrame, int reqW, int reqH)
{
	int srcW = frame->cols;
	int srcH = frame->rows;

	double scaleW = static_cast<double>(reqW) / srcW;
	double scaleH = static_cast<double>(reqH) / srcH;

	double scale = min(scaleW, scaleH);

	if (scale < 1.0)
	{
		int newWidth = static_cast<int>(srcW * scale);
		int newHeight = static_cast<int>(srcH * scale);

		// 계산된 크기가 0이 아니어야 함
		if (newWidth > 0 && newHeight > 0)
		{
			cv::resize(*frame, *smallFrame, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);
		}
		else
		{
			frame->copyTo(*smallFrame); // 너무 작아지면 그냥 원본 사용
			// 참조형식으로 얕은 복사를 하면 원본을 그대로 가리키는 효과가 일어나 원본을 다시쓰려고 지워버리면
			// 참조 에러가 발생함
		}
	}
	else
		frame->copyTo(*smallFrame);
}

double CameraManager::GetActualFrameAspect(const cv::VideoCapture* capture)
{
	// 실제로 적용된 해상도를 확인하기
	// 하드웨어가 지원하지 않는 해상도를 요청하면 가장 가까운 다른 값으로 설정
	// 비율이 깨지는지 확인하기 위해 실제 값을 로그로 찍어보기
	int actualWidth = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_WIDTH));
	int actualHeight = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_HEIGHT));

	// 비율 계산 (비율 유지를 위해)
	// 새 프레임의 비율 적용을 위한 aspect
	return static_cast<double>(actualWidth / actualHeight);
}

bool CameraManager::IsAspectZero(const cv::VideoCapture* capture, int camIndex)
{
	// 실제로 적용된 해상도를 확인하기
	// 하드웨어가 지원하지 않는 해상도를 요청하면 가장 가까운 다른 값으로 설정
	// 비율이 깨지는지 확인하기 위해 실제 값을 로그로 찍어보기
	int actualWidth = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_WIDTH));
	int actualHeight = static_cast<int>(capture->get(cv::CAP_PROP_FRAME_HEIGHT));

	// [안전장치 1] 카메라가 해상도를 0으로 반환하면 강제 종료 (나누기 에러 방지)
	if (actualWidth <= 0 || actualHeight <= 0)
		return true;
	else
		return false;
}

#pragma endregion Private

#pragma region Public
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

/// <summary>
/// 카메라의 고유 Path 얻어오기
/// </summary>
/// <param name="index">접근하고자 하는 index</param>
/// <returns></returns>
std::string CameraManager::GetPathByIndex(int index)
{
	std::string resultPath = "";
	int currentIndex = 0;

	HRESULT hr = CoInitialize(nullptr);

	// COM 초기화 실패
	if (FAILED(hr))
		return "";

	ICreateDevEnum* deviceEnum = nullptr;
	hr = CoCreateInstance(
		CLSID_VideoInputDeviceCategory,
		nullptr, CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, reinterpret_cast<void**>(&deviceEnum));

	if (SUCCEEDED(hr))
	{
		IEnumMoniker* pEnum = nullptr;

		hr = deviceEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEnum, 0);

		if (hr == S_OK)
		{
			IMoniker* moniker = nullptr;
			while (pEnum->Next(1, &moniker, nullptr) == S_OK)
			{
				if (currentIndex == index)
				{
					// 고유 path 찾기
					LPOLESTR name = nullptr;
					hr = moniker->GetDisplayName(nullptr, nullptr, &name);

					if (SUCCEEDED(hr))
					{
						char temp[1024];
						WideCharToMultiByte(
							CP_ACP, 0, name, -1,
							temp, sizeof(temp),
							nullptr, nullptr);
						resultPath = std::string(temp);
						CoTaskMemFree(name);
					}
					moniker->Release();
					break;
				}

				moniker->Release();
				currentIndex++;
			}
			pEnum->Release();
		}
		deviceEnum->Release();
	}
	CoUninitialize();

	return resultPath;
}

/// <summary>
/// 카메라의 고유 Path 로 기기 index 가져오기
/// </summary>
/// <param name="targetPath">접근하고자 하는 path</param>
/// <returns></returns>
int CameraManager::GetIndexByPath(const std::string& targetPath)
{
	int resultIndex = -1;
	int currentIndex = 0;

	if (targetPath.empty())
		return -1;

	HRESULT hr = CoInitialize(nullptr);


	// COM 초기화 실패
	if (FAILED(hr))
		return -1;

	ICreateDevEnum* deviceEnum = nullptr;
	hr = CoCreateInstance(
		CLSID_VideoInputDeviceCategory,
		nullptr, CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, reinterpret_cast<void**>(&deviceEnum));

	if (SUCCEEDED(hr))
	{
		IEnumMoniker* pEnum = nullptr;

		hr = deviceEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEnum, 0);

		if (hr == S_OK)
		{
			IMoniker* moniker = nullptr;
			while (pEnum->Next(1, &moniker, nullptr) == S_OK)
			{
				// 고유 path 찾기
				LPOLESTR name = nullptr;
				hr = moniker->GetDisplayName(nullptr, nullptr, &name);

				if (SUCCEEDED(hr))
				{
					char temp[1024];
					WideCharToMultiByte(
						CP_ACP, 0, name, -1,
						temp, sizeof(temp),
						nullptr, nullptr);
					std::string currentPath(temp);
					CoTaskMemFree(name);

					if (currentPath.find(targetPath) != std::string::npos)
					{
						resultIndex = currentIndex;
						moniker->Release();
						break;
					}
				}
				moniker->Release();
				currentIndex++;
			}
			pEnum->Release();
		}
		deviceEnum->Release();
	}
	CoUninitialize();

	return resultPath;
}
// 해당하는 카메라에 대한 작업 반복문
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

	// FPS, width, height 설정
	SetVideoCapture(&capture, reqW, reqH, fps);

	if (IsAspectZero(&capture, camIndex))
	{
		std::cout << "[Fatal Error] 카메라 해상도가 0입니다. 장치를 확인하세요." << '\n';
		_isRunning[camIndex] = false;
		capture.release();
		return;
	}


	//======================여기까지 카메라 기본 세팅=================================

	std::string windowName = "Cam View " + std::to_string(camIndex);
	cv::Mat rawFrame;
	cv::Mat smallFrame;

	while (_isRunning[camIndex])
	{
		try
		{
			if (capture.read(rawFrame))
			{
				if (!rawFrame.empty())
				{
					ResizeFrame(&rawFrame, &smallFrame, reqW, reqH);

					// 추후 이 위치에 이미지 정보 갱신이 들어갈 예정
					cv::imshow(windowName, smallFrame);

					if (cv::waitKey(1) == 27)
						_isRunning[camIndex] = false;
				}
			}
			else
			{
				// 프레임이 비었다면 잠깐 쉬기
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
		catch (const cv::Exception& e)
		{
			// OpenCV 에러가 발생하면 여기서 잡힙니다. (프로그램 안 꺼짐)
			std::cout << "[OpenCV Error Skip] " << e.what() << '\n';
		}
		catch (const std::exception& e)
		{
			// 기타 C++ 에러
			std::cout << "[System Error Skip] " << e.what() << '\n';
		}
	}

	cv::destroyWindow(windowName);
	capture.release();
	std::cout << ">>> [" << camIndex << R"(번 카메라] 종료됨.)" << '\n';
}

// Index를 통해 고유 Path를 얻어오기



#pragma endregion Public
