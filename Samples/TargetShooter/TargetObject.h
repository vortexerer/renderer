#pragma once
#include "Core/GameObject.h"
#include "Physics/RigidbodyComponent.h"
#include "Physics/Colliders.h"

// [유니티 스타일 아키텍처 - Prefab / GameObject 상속 구현체]
// 쌩 클래스 멤버로 데이터를 들고 있는 구조에서 벗어나,
// 생성자(Constructor) 내에서 필요한 컴포넌트들을 동적으로 AddComponent하여 
// 오브젝트의 속성과 스크립트를 정의하는 유니티 최신 프레임워크 스타일로 재설계되었습니다.
class TargetObject : public GameObject {
public:
    TargetObject(int id, const Vector3& position);
    virtual ~TargetObject();

    bool isDestroyed;
    SphereCollider* collider;
    RigidbodyComponent* rbComponent;
};
