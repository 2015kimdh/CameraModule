#pragma once

#include "Model.h"
#include "TripleBuffer.h"
#include <thread>
#include <atomic>
#include <string>
#include <dshow.h>
#pragma comment(lib, "strmiids")
#pragma comment(lib, "ole32")

class CameraManager
{
private:
	static const int MAX_CAMERAS = 2;
	const std::string UNKNOWN_DEVICE = "Unknown Device";

	std::thread* _workers[MAX_CAMERAS];
	std::atomic<bool> _isRunning[MAX_CAMERAS];
	// Mutex와 BallData 배열을 TripleBuffer로 대체
	TripleBuffer tripleBuffers[MAX_CAMERAS];

	DebugCallback debugLog;

	CameraManager();
	~CameraManager();

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

	/*/// <summary>
	/// 인덱스에 해당하는 카메라의 프레임 가져오는 함수
	/// <summary>
	bool GetFrameFromCamera(int camIndex, unsigned char* outBuffer, int width, int height);*/

	/// <summary>
	/// 인덱스에 해당하는 카메라에 대한 실제 작업을 실행하는 루프 함수
	/// <summary>
	void WorkerLoop(int camIndex, int deviceIndex, int reqW, int reqH, int fps);
};

