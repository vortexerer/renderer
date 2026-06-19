#pragma once

class GameObject;

// [유니티 스타일 게임 아키텍처 - 모든 컴포넌트의 가상 기본 클래스]
class Component {
public:
    GameObject* gameObject = nullptr;

    Component() = default;
    virtual ~Component() = default;

    virtual void Start() {}
    virtual void Update(float /*deltaTime*/) {}
};
