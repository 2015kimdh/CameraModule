#pragma once

#include <string>

// 2차원 좌표 표현용 구조체
struct Vector2
{
    float x;
    float y;
};

struct BallData
{
    int id;                 // 공 ID
    Vector2 position; // 좌표(0.0 ~ 1.0)
    Vector2 velocity; // 속도
    bool isTracked;         // 추적 성공 여부
    long long timestamp;    // 데이터 생성 시간 (ms)
};

// 디버그 로그를 Unity로 보내기 위한 콜백 함수 포인터 타입
typedef void(*DebugCallback)(const char* message);