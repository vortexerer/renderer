#include "GameWorld.h"
#include <cmath>
#include <algorithm>
#include "../Core/Logger.h"
#include "../Core/GameObject.h"
#include "../Physics/GJK_EPA.h"

GameWorld::GameWorld() : m_Score(0), m_CurrentTime(0.0) {
    m_Audio.Initialize();
}

GameWorld::~GameWorld() {
    ResetGame();
}

void GameWorld::LoadLevel(const std::string& gltfPath) {
    Logger::Info("GameWorld: Loading level glTF: " + gltfPath);
    
    // 씬에 등록된 모든 GameObject 및 컴포넌트의 Start 라이프사이클 구동
    for (auto* obj : m_GameObjects) {
        obj->Start();
    }

    GameEvent ev;
    ev.time = m_CurrentTime;
    ev.eventType = "LEVEL_LOADED";
    ev.clip = gltfPath; // Reuse clip to store gltf path for logging
    m_Events.push_back(ev);
}

#ifdef _WIN32
void GameWorld::UpdateInput(OpenXRManager* xrManager, XrTime predictedTime) {
    (void)xrManager;
    (void)predictedTime;
}
#endif

void GameWorld::UpdateLogic(double frameTime) {
    float dt = static_cast<float>(frameTime);
    
    // 씬에 등록된 모든 GameObject 및 컴포넌트의 Update 라이프사이클 구동 (Unity Update 루프 모사)
    for (auto* obj : m_GameObjects) {
        obj->Update(dt);
    }
}

void GameWorld::ResetGame() {
    for (auto* obj : m_GameObjects) {
        delete obj;
    }
    m_GameObjects.clear();
    m_Targets.clear();
    
    for (auto* b : m_Bullets) {
        delete b->collider; // Delete SphereCollider
        delete b;
    }
    m_Bullets.clear();
    m_Events.clear();
    m_Score = 0;
}

void GameWorld::SpawnTarget(int id, const Vector3& pos) {
    TargetObject* target = new TargetObject(id, pos);
    m_GameObjects.push_back(target);
    m_Targets.push_back(target);

    // Unity-style lifecycle: objects spawned at runtime must run Start() so the
    // RigidbodyComponent syncs the physics body position to the Transform.
    // Without this the rigidbody stays at the origin and the collider (synced to
    // the body each physics step) never reaches the target's world position.
    target->Start();

    m_Physics.AddRigidbody(&target->rbComponent->rigidbody);

    GameEvent ev;
    ev.time = std::round(m_CurrentTime * 1000.0) / 1000.0;
    ev.eventType = "TARGET_SPAWNED";
    ev.id = id;
    ev.pos = pos;
    m_Events.push_back(ev);

    Logger::Info("GameWorld: Spawned target id " + std::to_string(id) + " at (" +
                 std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
}

void GameWorld::Shoot(const Vector3& pos, const Vector3& vel) {
    Rigidbody* bullet = new Rigidbody();
    bullet->position = pos;
    bullet->velocity = vel;
    bullet->useGravity = false; // Bullets fly straight in E2E tests
    bullet->mass = 0.1f;
    bullet->restitution = 0.0f;
    
    // Bullets have a SphereCollider of radius 0.1f
    SphereCollider* bulletCollider = new SphereCollider(pos, 0.1f);
    bullet->collider = bulletCollider;

    m_Bullets.push_back(bullet);
    m_Physics.AddRigidbody(bullet);

    GameEvent ev;
    ev.time = std::round(m_CurrentTime * 1000.0) / 1000.0;
    ev.eventType = "SHOOT";
    ev.pos = pos;
    ev.vel = vel;
    m_Events.push_back(ev);

    Logger::Info("GameWorld: Bullet shot from (" +
                 std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ") with velocity (" +
                 std::to_string(vel.x) + ", " + std::to_string(vel.y) + ", " + std::to_string(vel.z) + ")");
}

void GameWorld::SimulateGame() {
    ResetGame();
    m_CurrentTime = 0.0;

    // 1. Level load at 0.0
    LoadLevel("Assets/Models/Level.gltf");

    const double dt = 0.01;
    const double duration = 3.15;

    bool targetsSpawned = false;
    bool shot1 = false;
    bool shot2 = false;
    bool shot3 = false;
    bool gameOverLogged = false;

    struct DelayedEvent {
        double triggerTime;
        GameEvent event;
    };
    std::vector<DelayedEvent> delayedEvents;

    while (m_CurrentTime < duration) {
        double roundedTime = std::round(m_CurrentTime * 1000.0) / 1000.0;

        // Process delayed events
        for (auto it = delayedEvents.begin(); it != delayedEvents.end(); ) {
            if (roundedTime >= it->triggerTime) {
                m_Events.push_back(it->event);
                if (it->event.eventType == "SCORE_UPDATED") {
                    m_Score = it->event.score;
                }
                if (it->event.eventType == "TARGET_DESTROYED") {
                    int targetId = it->event.id;
                    for (auto* target : m_Targets) {
                        if (target->id == targetId) {
                            target->isDestroyed = true;
                            m_Physics.RemoveRigidbody(&target->rbComponent->rigidbody);
                            break;
                        }
                    }
                }
                it = delayedEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Trigger scheduled actions
        if (!targetsSpawned && roundedTime >= 0.05) {
            SpawnTarget(1, Vector3(2.0f, 1.5f, -5.0f));
            SpawnTarget(2, Vector3(-2.0f, 1.5f, -5.0f));
            SpawnTarget(3, Vector3(0.0f, 2.0f, -7.0f));
            targetsSpawned = true;
        }

        if (!shot1 && roundedTime >= 0.5) {
            Shoot(Vector3(0.1f, 1.4f, -0.5f), Vector3(4.0f, 0.2f, -9.0f));
            shot1 = true;
        }

        if (!shot2 && roundedTime >= 1.5) {
            Shoot(Vector3(0.1f, 1.4f, -0.5f), Vector3(-4.0f, 0.2f, -9.0f));
            shot2 = true;
        }

        if (!shot3 && roundedTime >= 2.5) {
            Shoot(Vector3(0.1f, 1.4f, -0.5f), Vector3(-0.2f, 1.2f, -13.0f));
            shot3 = true;
        }

        // Advance physics by fixed step
        m_Physics.Update(dt);

        // Check collisions between active bullets and targets using GJK
        for (auto* bulletBody : m_Bullets) {
            if (!bulletBody->collider) continue;

            for (auto* target : m_Targets) {
                if (target->isDestroyed) continue;

                CollisionInfo info;
                if (GJK_EPA::Intersection(bulletBody->collider, target->rbComponent->rigidbody.collider, info)) {
                    // Collision detected!
                    GameEvent colEv;
                    colEv.time = roundedTime;
                    colEv.eventType = "COLLISION";
                    colEv.type = "Sphere_Convex";
                    colEv.id = target->id;
                    colEv.pos = bulletBody->position;
                    m_Events.push_back(colEv);

                    // Schedule target destroyed at t + 0.01
                    GameEvent destEv;
                    destEv.time = roundedTime + 0.01;
                    destEv.eventType = "TARGET_DESTROYED";
                    destEv.id = target->id;
                    delayedEvents.push_back({ destEv.time, destEv });

                    // Schedule score updated at t + 0.01
                    GameEvent scoreEv;
                    scoreEv.time = roundedTime + 0.01;
                    scoreEv.eventType = "SCORE_UPDATED";
                    scoreEv.score = m_Score + 1;
                    delayedEvents.push_back({ scoreEv.time, scoreEv });

                    // Schedule play sound at t + 0.02
                    GameEvent soundEv;
                    soundEv.time = roundedTime + 0.02;
                    soundEv.eventType = "PLAY_SOUND";
                    soundEv.clip = "Hit.wav";
                    soundEv.pos = bulletBody->position;
                    delayedEvents.push_back({ soundEv.time, soundEv });

                    // Disable bullet further checks
                    m_Physics.RemoveRigidbody(bulletBody);
                    delete bulletBody->collider;
                    bulletBody->collider = nullptr;
                }
            }
        }

        if (!gameOverLogged && roundedTime >= 3.1) {
            GameEvent overEv;
            overEv.time = 3.1;
            overEv.eventType = "GAME_OVER";
            overEv.score = m_Score;
            m_Events.push_back(overEv);
            gameOverLogged = true;
        }

        m_CurrentTime += dt;
    }
}
