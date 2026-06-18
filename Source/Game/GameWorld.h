#pragma once
#include <vector>
#include <string>
#include "../Math/Vector3.h"
#include "TargetObject.h"
#include "../Physics/PhysicsEngine.h"
#include "../Audio/AudioEngine.h"
#ifdef _WIN32
// Provides OpenXRManager and XrTime for the VR runtime input hook below.
#include "../Platform/OpenXRManager.h"
#endif

struct GameEvent {
    double time;
    std::string eventType;
    int id = 0;
    Vector3 pos = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 vel = Vector3(0.0f, 0.0f, 0.0f);
    int score = 0;
    std::string clip = "";
    std::string type = "";
};

class GameWorld {
public:
    GameWorld();
    ~GameWorld();

    void LoadLevel(const std::string& gltfPath);
#ifdef _WIN32
    // VR runtime input hook (OpenXR). Excluded from the portable/test build.
    void UpdateInput(OpenXRManager* xrManager, XrTime predictedTime);
#endif
    void UpdateLogic(double frameTime);
    const std::vector<GameObject*>& GetGameObjects() const { return m_GameObjects; }
    int GetScore() const { return m_Score; }
    void ResetGame();

    // Simulation interface for --test-game
    void SimulateGame();
    const std::vector<GameEvent>& GetEvents() const { return m_Events; }

    // Spawn and shoot functions
    void SpawnTarget(int id, const Vector3& pos);
    void Shoot(const Vector3& pos, const Vector3& vel);

private:
    std::vector<GameObject*> m_GameObjects;
    std::vector<TargetObject*> m_Targets;
    std::vector<Rigidbody*> m_Bullets;
    
    PhysicsEngine m_Physics;
    AudioEngine m_Audio;
    int m_Score;
    double m_CurrentTime;

    std::vector<GameEvent> m_Events;
};
