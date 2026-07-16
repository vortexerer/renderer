#include "PlayerController.h"
#include "GameWorld.h"
#include "Core/GameObject.h"

PlayerController::PlayerController(OpenXRManager* xrManager, GameWorld* world)
    : m_XRManager(xrManager), m_GameWorld(world) {}

PlayerController::~PlayerController() {}

void PlayerController::Start() {}

void PlayerController::Update(float deltaTime) {
    (void)deltaTime;
    if (!m_XRManager || !m_GameWorld) return;

    // VR 컨트롤러의 사격(Trigger) 입력 이벤트를 감지하여 C++ 스크립트로 동작 제어
    if (m_XRManager->IsTriggerPressed(1)) { // 우측 손 컨트롤러 트리거
        XrPosef pose = m_XRManager->GetControllerPose(1);
        Vector3 bulletPos(pose.position.x, pose.position.y, pose.position.z);
        Vector3 bulletVel(0.0f, 0.0f, -20.0f); // 전방 Z축을 향한 사격
        
        m_GameWorld->Shoot(bulletPos, bulletVel);
    }
}
