#pragma once
#include "../Core/Component.h"
#include "../Core/GameObject.h"
#include "Rigidbody.h"

// [유니티 스타일 물리 연계 - Rigidbody 컴포넌트]
// 기존 C++ 물리 라이브러리인 Rigidbody를 유니티 스타일의 컴포넌트로 래핑하여
// GameObject에 동적으로 부착 가능하게 하고, 물리 시뮬레이션 좌표와 Transform을 동기화합니다.
class RigidbodyComponent : public Component {
public:
    Rigidbody rigidbody;

    RigidbodyComponent() {
        // 기본값 초기화
        rigidbody.mass = 1.0f;
        rigidbody.restitution = 0.8f;
        rigidbody.isStatic = false;
        rigidbody.useGravity = true;
    }

    virtual void Start() override {
        // 유니티처럼 시작 시 Transform의 위치를 물리 강체의 초기 위치로 씽크합니다.
        if (gameObject && gameObject->transform) {
            rigidbody.position = gameObject->transform->position;
        }
    }

    virtual void Update(float) override {
        // 물리 엔진이 계산한 3D Cartesian 물리 강체 좌표를 GameObject의 Transform 좌표로 피드백합니다.
        if (gameObject && gameObject->transform && !rigidbody.isStatic) {
            gameObject->transform->position = rigidbody.position;
        }
    }
};
