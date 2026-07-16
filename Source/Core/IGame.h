#pragma once
#include <vector>

class GameObject;
class OpenXRManager;

// [엔진-게임 경계 인터페이스]
// 엔진(VREngineCore)은 구체적인 게임을 알지 못하고, 게임 모듈이 이 인터페이스를
// 구현해 Engine::Initialize에 주입합니다. XrTime은 openxr.h의 int64_t typedef이므로
// 헤더 의존을 피하기 위해 long long으로 받습니다.
class IGame {
public:
    virtual ~IGame() = default;

    // 엔진 서브시스템 초기화 직후 1회 호출됩니다 (레벨/에셋 로드 지점).
    virtual bool OnStart() { return true; }

    // XR 세션이 돌고 있을 때 매 프레임, 입력 갱신 시점에 호출됩니다.
    virtual void OnUpdateInput(OpenXRManager* /*xrManager*/, long long /*predictedDisplayTime*/) {}

    // 매 프레임 게임 로직 갱신 시점에 호출됩니다.
    virtual void OnUpdate(double /*frameTime*/) {}

    // 렌더러에 넘길 씬 오브젝트 목록.
    virtual const std::vector<GameObject*>& GetGameObjects() const = 0;
};
