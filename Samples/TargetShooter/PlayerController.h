#pragma once
#include "Core/Component.h"
#include "Platform/OpenXRManager.h"

// Forward declarations
class GameWorld;

// [유니티 스타일 아키텍처 - PlayerController 스크립트 컴포넌트]
// MonoBehaviour를 모방하여 Component를 상속받으며,
// 게임 월드 및 VR 런타임 센서 입력을 실시간 중계하여 총기 발사 스크립트 로직을 제어합니다.
class PlayerController : public Component {
public:
    PlayerController(OpenXRManager* xrManager, GameWorld* world);
    virtual ~PlayerController();

    virtual void Start() override;
    virtual void Update(float deltaTime) override;

private:
    OpenXRManager* m_XRManager;
    GameWorld* m_GameWorld;
};
