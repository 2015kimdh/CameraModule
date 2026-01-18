#pragma once

#include <opencv2/opencv.hpp>
#include "Model.h"
#include "TripleBuffer.h"
#include <thread>
#include <atomic>
#include <string>
#pragma comment(lib, "strmiids")
#pragma comment(lib, "ole32")

class CameraManager
{
private:
	static const int MAX_CAMERAS = 2;
	static const int TOP_CAMERA = 0;
	static const int SIDE_CAMERA = 1;
	const std::string UNKNOWN_DEVICE = "Unknown Device";

	std::thread* _workers[MAX_CAMERAS];
	std::atomic<bool> _isRunning[MAX_CAMERAS];
	// Mutex와 BallData 배열을 TripleBuffer로 대체
	TripleBuffer tripleBuffers[MAX_CAMERAS];

	DebugCallback debugLog;

	CameraManager();
	~CameraManager();

	void SetVideoCapture(cv::VideoCapture* capture, int width, int height, int fps);

	void ResizeFrame(cv::Mat* frame, cv::Mat* smallFrame, int reqW, int reqH);

	double GetActualFrameAspect(const cv::VideoCapture* capture);

	bool IsAspectZero(const cv::VideoCapture* capture, int camIndex);

public:
	static CameraManager& Instance();
	/// <summary>
	/// 사용 가능한 카메라 장치 검색
	/// </summary>
	int ScanCameras(int* outDevices);

	/// <summary>
	/// 인덱스에 해당하는 카메라의 이름을 가져오는 함수
	/// <summary>
	std::string GetCameraName(int deviceIndex) const;

	// 카메라 시작
	// deviceIndex: ScanCameras로 찾은 그 번호를 여기에 넣습니다.
	void StartCamera(int camIndex, int deviceIndex, int w, int h, int fps);

	// 인덱스의 카메라를 정지
	void StopCamera(int camIndex);

	// 인덱스(0, 1...)를 주면 고유 ID(Device Path)를 반환
	std::string GetPathByIndex(int index);

	// 고유 ID(Device Path)를 주면 현재 인덱스(0, 1...)를 반환 (못 찾으면 -1)
	int GetIndexByPath(const std::string& targetPath);

	/// <summary>
	/// 인덱스에 해당하는 카메라에 대한 실제 작업을 실행하는 루프 함수
	/// <summary>
	void WorkerLoop(int camIndex, int deviceIndex, int reqW, int reqH, int fps);
};

