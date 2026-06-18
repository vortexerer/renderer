#pragma once
#include <vector>
#include <string>
#include <typeinfo>
#include <utility>
#include "Component.h"
#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"

// [유니티 스타일 게임 아키텍처 - 기본 기하 변환 Transform 컴포넌트]
class Transform : public Component {
public:
    Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
    Quaternion rotation = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

    virtual void Start() override {}
    virtual void Update(float) override {}
};

// [유니티 스타일 게임 아키텍처 - 오브젝트-컴포넌트 패턴 구현체]
class GameObject {
public:
    int id;
    std::string name;
    Transform* transform = nullptr;

    GameObject(int id = 0) : id(id) {
        // 모든 GameObject는 유니티와 마찬가지로 생성 시 기본 Transform을 자동으로 획득합니다.
        transform = AddComponent<Transform>();
    }

    virtual ~GameObject() {
        for (auto* comp : m_Components) {
            delete comp;
        }
        m_Components.clear();
    }

    // 컴포넌트 동적 할당 및 바인딩 템플릿
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        T* comp = new T(std::forward<Args>(args)...);
        comp->gameObject = this;
        m_Components.push_back(comp);
        return comp;
    }

    // 타입 캐스팅을 이용한 컴포넌트 획득 템플릿
    template<typename T>
    T* GetComponent() {
        for (auto* comp : m_Components) {
            T* target = dynamic_cast<T*>(comp);
            if (target != nullptr) {
                return target;
            }
        }
        return nullptr;
    }

    void Start() {
        for (auto* comp : m_Components) {
            comp->Start();
        }
    }

    void Update(float deltaTime) {
        for (auto* comp : m_Components) {
            comp->Update(deltaTime);
        }
    }

private:
    std::vector<Component*> m_Components;
};
