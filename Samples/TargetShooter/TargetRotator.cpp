#include "TargetRotator.h"
#include "Core/GameObject.h"
#include <cmath>

void TargetRotator::Start() {
    m_Angle = 0.0f;
}

void TargetRotator::Update(float deltaTime) {
    if (gameObject && gameObject->transform) {
        // 매 프레임마다 각도 증가
        m_Angle += rotationSpeed * deltaTime;
        
        // 고급수학1 삼차원 회전 교안 연계: 오일러 회전 각으로부터 짐벌락 회피용 사원수를 생성
        // Y축(Heading)을 기준으로 회전
        gameObject->transform->rotation = Quaternion::FromEuler(m_Angle, 0.0f, 0.0f);
    }
}
