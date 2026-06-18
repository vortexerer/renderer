#pragma once
#include "../Core/Component.h"
#include "../Math/Quaternion.h"

// [사용자 직접 개발용 C++ 스크립트 컴포넌트 예제 - TargetRotator]
// 유니티의 MonoBehaviour 스크립트 작성법과 100% 매치되는 구조입니다.
// 매 프레임마다 사원수(Quaternion)와 삼각함수를 기반으로 게임 오브젝트를 실시간 회전시킵니다.
class TargetRotator : public Component {
public:
    float rotationSpeed = 1.0f;
    
    virtual void Start() override;
    virtual void Update(float deltaTime) override;

private:
    float m_Angle = 0.0f;
};
