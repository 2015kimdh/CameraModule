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
	int ScanCameras(int* outIndices, int maxCheck);

	/// <summary>
	/// 인덱스에 해당하는 카메라의 이름을 가져오는 함수
	/// <summary>
	std::string GetCameraName(int deviceIndex);
};

