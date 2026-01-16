#include <iostream>
#include "CameraManager.h"

int main()
{
	CameraManager& camera_manager = CameraManager::Instance();

	// 결과를 담을 배열 준비 (최대 10개까지 검색한다고 가정)
	int foundIndices[10] = { 0, };

	std::cout << "=== 카메라 검색 시작 ===\n" << std::endl;

	// 카메라 스캔 함수 호출 (0번부터 9번 포트까지 검사)
	int count = camera_manager.ScanCameras(foundIndices, 10);

	std::cout << "=== 검색 결과 ===" << std::endl;
	std::cout << "총 발견된 카메라 개수: " << count << "개" << std::endl;

	if (count > 0) {
		for (int i = 0; i < count; i++) {
			int deviceIndex = foundIndices[i];

			// [핵심] 인덱스로 이름 가져오기
			std::string camName = camera_manager.GetCameraName(deviceIndex);

			std::cout << "  [" << i << "] Device Index " << deviceIndex
				<< " : " << camName << std::endl;
		}
	}
	else {
		std::cout << "  - 연결된 카메라가 없습니다." << std::endl;
	}

	// ▼▼▼ 이 코드를 추가하세요 ▼▼▼
	system("pause");
	// ▲▲▲ "계속하려면 아무 키나 누르십시오..." 가 뜨고 멈춥니다.

	return 0;
}
