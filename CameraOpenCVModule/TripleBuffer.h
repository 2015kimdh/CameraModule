#pragma once

#include "Model.h"
#include <atomic>
#include <thread>

class TripleBuffer
{
private:
	BallData buffers[3];	// 데이터 저장소.
	std::atomic<int> sharedIndex; // 가장 최신 데이터가 있는 곳
	int writerIndex; // 카메라가 가진 인덱스
	int readerIndex; // 클라이언트가 가진 인덱스

public:
	TripleBuffer()
	{
		// 0번은 공유, 1번은 쓰기, 2번은 읽기
		sharedIndex.store(0);
		writerIndex = 1;
		readerIndex = 2;

		for (int i = 0; i < 3; i++)
			buffers[i] = {0, };
	}
};

