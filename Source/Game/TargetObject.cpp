#include "TargetObject.h"
#include "TargetRotator.h"

TargetObject::TargetObject(int id, const Vector3& position)
    : GameObject(id), isDestroyed(false) {
    
    // 1. Transform 위치 설정
    transform->position = position;

    // 2. RigidbodyComponent 컴포넌트 동적 추가 (유니티 스타일)
    rbComponent = AddComponent<RigidbodyComponent>();
    rbComponent->rigidbody.mass = 1.0f;
    rbComponent->rigidbody.restitution = 0.8f;
    rbComponent->rigidbody.isStatic = true; // 타깃 갤러리 내 과녁들은 정적(static)임

    // 3. Collider 셋업
    collider = new SphereCollider(position, 0.5f);
    rbComponent->rigidbody.collider = collider;

    // 4. [C++ 스크립트 컴포넌트 장착]
    // 사용자가 개발한 TargetRotator 컴포넌트를 이 오브젝트에 결합하여
    // 런타임에 삼차원 사원수 기반 자전(rotation) 운동을 동적으로 수행하도록 바인딩합니다!
    TargetRotator* rotator = AddComponent<TargetRotator>();
    rotator->rotationSpeed = 1.5f; // 회전 속도 설정
}

TargetObject::~TargetObject() {
    delete collider;
}
