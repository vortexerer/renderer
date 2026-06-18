#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <fstream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
// On non-Windows the secure CRT variant is unavailable; the calls in this file
// only parse floating-point fields, so the classic sscanf is a drop-in.
#define sscanf_s sscanf
#endif

#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Core/Timer.h"
#include "Core/Logger.h"
#include "Physics/PhysicsEngine.h"
#include "Physics/Colliders.h"
#include "Audio/HRTFFilter.h"
#include "Asset/AssetLoader.h"
#include "UI/ImmediateUI.h"
#include "Game/GameWorld.h"

// Set console mode for virtual terminal sequences (colors)
void EnableVirtualTerminalProcessing() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
    // POSIX terminals already interpret ANSI escape sequences; nothing to do.
}

// Helpers for JSON generation
std::string MatrixToJson(const Matrix4x4& m) {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "[[%.6f, %.6f, %.6f, %.6f], "
        "[%.6f, %.6f, %.6f, %.6f], "
        "[%.6f, %.6f, %.6f, %.6f], "
        "[%.6f, %.6f, %.6f, %.6f]]",
        m(0,0), m(0,1), m(0,2), m(0,3),
        m(1,0), m(1,1), m(1,2), m(1,3),
        m(2,0), m(2,1), m(2,2), m(2,3),
        m(3,0), m(3,1), m(3,2), m(3,3));
    return buf;
}

std::string NormalMatrixToJson(float n[3][3]) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "[[%.6f, %.6f, %.6f], "
        "[%.6f, %.6f, %.6f], "
        "[%.6f, %.6f, %.6f]]",
        n[0][0], n[0][1], n[0][2],
        n[1][0], n[1][1], n[1][2],
        n[2][0], n[2][1], n[2][2]);
    return buf;
}

Vector3 ParseVector3(const std::string& str) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    sscanf_s(str.c_str(), "%f,%f,%f", &x, &y, &z);
    return Vector3(x, y, z);
}

// Struct for parsing glTF JSON manually
struct gltfNode {
    std::string name;
    float translation[3] = {0.0f, 0.0f, 0.0f};
    bool is_static = false;
};

std::vector<gltfNode> ParseGltfNodes(const std::string& path) {
    std::vector<gltfNode> nodes;
    std::ifstream file(path);
    if (!file.is_open()) {
        return nodes;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    size_t nodes_pos = content.find("\"nodes\"");
    if (nodes_pos == std::string::npos) {
        return nodes;
    }

    size_t start_bracket = content.find('[', nodes_pos);
    if (start_bracket == std::string::npos) {
        return nodes;
    }

    size_t end_bracket = content.find(']', start_bracket);
    if (end_bracket == std::string::npos) {
        return nodes;
    }

    std::string nodes_sub = content.substr(start_bracket, end_bracket - start_bracket);

    size_t idx = 0;
    while (true) {
        size_t node_start = nodes_sub.find('{', idx);
        if (node_start == std::string::npos) {
            break;
        }
        size_t node_end = nodes_sub.find('}', node_start);
        if (node_end == std::string::npos) {
            break;
        }

        std::string node_str = nodes_sub.substr(node_start, node_end - node_start + 1);
        idx = node_end + 1;

        std::string name;
        size_t name_pos = node_str.find("\"name\"");
        if (name_pos != std::string::npos) {
            size_t colon = node_str.find(':', name_pos);
            size_t first_quote = node_str.find('"', colon);
            size_t second_quote = node_str.find('"', first_quote + 1);
            if (first_quote != std::string::npos && second_quote != std::string::npos) {
                name = node_str.substr(first_quote + 1, second_quote - first_quote - 1);
            }
        }

        float translation[3] = {0.0f, 0.0f, 0.0f};
        size_t trans_pos = node_str.find("\"translation\"");
        if (trans_pos != std::string::npos) {
            size_t colon = node_str.find(':', trans_pos);
            size_t open_b = node_str.find('[', colon);
            size_t close_b = node_str.find(']', open_b);
            if (open_b != std::string::npos && close_b != std::string::npos) {
                std::string trans_sub = node_str.substr(open_b + 1, close_b - open_b - 1);
                sscanf_s(trans_sub.c_str(), "%f,%f,%f", &translation[0], &translation[1], &translation[2]);
            }
        }

        gltfNode node;
        node.name = name;
        node.translation[0] = translation[0];
        node.translation[1] = translation[1];
        node.translation[2] = translation[2];
        node.is_static = (name.rfind("Static_", 0) == 0);
        nodes.push_back(node);
    }

    return nodes;
}

// ----------------------------------------------------
// Handlers for CLI Flags
// ----------------------------------------------------

void TestMath() {
    Vector3 v1(1.0f, 2.0f, 3.0f);
    Vector3 v2(4.0f, 5.0f, 6.0f);

    Vector3 v_add = v1 + v2;
    Vector3 v_sub = v1 - v2;
    float v_dot = v1.Dot(v2);
    Vector3 v_cross = v1.Cross(v2);
    Vector3 v_norm = v1.Normalize();

    Matrix4x4 t_mat = Matrix4x4::Translation(1.0f, 2.0f, 3.0f);
    Matrix4x4 s_mat = Matrix4x4::Scaling(2.0f, 3.0f, 4.0f);

    Quaternion q1(1.0f, 0.0f, 0.0f, 0.0f);
    float angle_90 = 3.141592653589793f / 2.0f;
    Quaternion q2(std::cos(angle_90 * 0.5f), 0.0f, std::sin(angle_90 * 0.5f), 0.0f);
    Quaternion q_mul = q1 * q2;
    Matrix4x4 q_mat = q2.ToRotationMatrix();

    float h = 0.5f, p = 0.3f, r = 0.2f;
    Matrix4x4 euler_mat = Matrix4x4::EulerView(h, p, r);

    Quaternion qa(std::cos(angle_90 * 0.5f), std::sin(angle_90 * 0.5f), 0.0f, 0.0f);
    Quaternion qb(std::cos(angle_90 * 0.5f), 0.0f, std::sin(angle_90 * 0.5f), 0.0f);
    Quaternion slerp_result = Quaternion::Slerp(qa, qb, 0.5f);

    float pi_over_2 = 3.141592653589793f / 2.0f;
    Matrix4x4 gimbal_mat = Matrix4x4::EulerView(h, pi_over_2, r);

    Matrix4x4 M_nu = Matrix4x4::Translation(1.0f, 2.0f, 3.0f) * 
                      Matrix4x4::RotationY(0.5f) * 
                      Matrix4x4::Scaling(1.0f, 2.0f, 3.0f);
    float norm_mat_data[3][3] = {0};
    bool norm_ok = M_nu.GetNormalMatrix(norm_mat_data);

    Quaternion zero_q(0.0f, 0.0f, 0.0f, 0.0f);
    Quaternion zq_norm = zero_q.Normalize();

    Matrix4x4 singular_mat;
    float sing_norm_data[3][3] = {0};
    bool sing_ok = singular_mat.GetNormalMatrix(sing_norm_data);

    printf("{\n");
    printf("  \"v_add\": [%.6f, %.6f, %.6f],\n", v_add.x, v_add.y, v_add.z);
    printf("  \"v_sub\": [%.6f, %.6f, %.6f],\n", v_sub.x, v_sub.y, v_sub.z);
    printf("  \"v_dot\": %.6f,\n", v_dot);
    printf("  \"v_cross\": [%.6f, %.6f, %.6f],\n", v_cross.x, v_cross.y, v_cross.z);
    printf("  \"v_norm\": [%.6f, %.6f, %.6f],\n", v_norm.x, v_norm.y, v_norm.z);

    printf("  \"t_mat\": %s,\n", MatrixToJson(t_mat).c_str());
    printf("  \"s_mat\": %s,\n", MatrixToJson(s_mat).c_str());

    printf("  \"q_mul\": [%.6f, %.6f, %.6f, %.6f],\n", q_mul.w, q_mul.x, q_mul.y, q_mul.z);
    printf("  \"q_mat\": %s,\n", MatrixToJson(q_mat).c_str());

    printf("  \"euler_mat\": %s,\n", MatrixToJson(euler_mat).c_str());
    printf("  \"slerp_half\": [%.6f, %.6f, %.6f, %.6f],\n", slerp_result.w, slerp_result.x, slerp_result.y, slerp_result.z);
    printf("  \"gimbal_mat\": %s,\n", MatrixToJson(gimbal_mat).c_str());

    if (norm_ok) {
        printf("  \"normal_mat\": %s,\n", NormalMatrixToJson(norm_mat_data).c_str());
    } else {
        printf("  \"normal_mat\": null,\n");
    }

    printf("  \"zero_q_norm\": [%.6f, %.6f, %.6f, %.6f],\n", zq_norm.w, zq_norm.x, zq_norm.y, zq_norm.z);

    if (sing_ok) {
        printf("  \"singular_invert\": %s\n", NormalMatrixToJson(sing_norm_data).c_str());
    } else {
        printf("  \"singular_invert\": null\n");
    }
    printf("}\n");
}

void TestPhysics(int fps, float duration) {
    float dt_render = 1.0f / fps;
    float accumulator = 0.0f;
    float time_elapsed = 0.0f;

    PhysicsEngine physicsEngine;
    
    Rigidbody sphere;
    sphere.position = Vector3(0.0f, 5.0f, 0.0f);
    sphere.velocity = Vector3(0.0f, 0.0f, 0.0f);
    sphere.mass = 1.0f;
    sphere.restitution = 0.8f;
    
    SphereCollider sphereCollider(Vector3(0.0f, 5.0f, 0.0f), 0.5f);
    sphere.collider = &sphereCollider;
    physicsEngine.AddRigidbody(&sphere);

    Rigidbody floor;
    floor.position = Vector3(0.0f, -0.5f, 0.0f);
    floor.isStatic = true;
    floor.mass = 1.0f;
    floor.restitution = 0.8f;

    OBBCollider floorCollider(Vector3(0.0f, -0.5f, 0.0f), Vector3(100.0f, 0.5f, 100.0f), Quaternion());
    floor.collider = &floorCollider;
    physicsEngine.AddRigidbody(&floor);

    std::vector<std::string> steps_log;
    int step_idx = 0;

    int total_frames = static_cast<int>(std::round(duration * fps));
    for (int frame = 0; frame < total_frames; ++frame) {
        accumulator += dt_render;
        while (accumulator >= 0.01f - 1e-6f) {
            physicsEngine.FixedUpdate(0.01);

            double step_time = std::round(step_idx * 0.01 * 1000.0) / 1000.0;

            char step_buf[256];
            snprintf(step_buf, sizeof(step_buf),
                "  {\n"
                "    \"step\": %d,\n"
                "    \"time\": %.3f,\n"
                "    \"pos_y\": %.6f,\n"
                "    \"vel_y\": %.6f\n"
                "  }",
                step_idx,
                step_time,
                (double)sphere.position.y,
                (double)sphere.velocity.y);
            steps_log.push_back(step_buf);

            step_idx++;
            accumulator -= 0.01f;
        }
        time_elapsed += dt_render;
    }

    printf("[\n");
    for (size_t i = 0; i < steps_log.size(); ++i) {
        printf("%s", steps_log[i].c_str());
        if (i + 1 < steps_log.size()) {
            printf(",\n");
        } else {
            printf("\n");
        }
    }
    printf("]\n");
}

void TestAudio(const std::string& src_pos_str, const std::string& src_vel_str, 
               const std::string& list_pos_str, const std::string& list_vel_str) {
    Vector3 src_pos = ParseVector3(src_pos_str);
    Vector3 src_vel = ParseVector3(src_vel_str);
    Vector3 list_pos = ParseVector3(list_pos_str);
    Vector3 list_vel = ParseVector3(list_vel_str);

    SpatialParameters params = HRTFFilter::CalculateSpatialParams(src_pos, src_vel, list_pos, list_vel);

    printf("{\n");
    printf("  \"distance\": %.6f,\n", (double)params.distance);
    printf("  \"azimuth_degrees\": %.3f,\n", (double)params.azimuthDegrees);
    printf("  \"doppler_factor\": %.6f,\n", (double)params.dopplerFactor);
    printf("  \"itd_delay_sec\": %.8f,\n", (double)params.itdDelaySec);
    printf("  \"ild_left\": %.6f,\n", (double)params.ildLeft);
    printf("  \"ild_right\": %.6f\n", (double)params.ildRight);
    printf("}\n");
}

void TestGltf(const std::string& path) {
    AssetLoader::ModelData modelData;
    if (!AssetLoader::LoadGLTF(path, modelData)) {
        std::cerr << "Failed to load glTF file: " << path << "\n";
        return;
    }

    printf("{\n");
    printf("  \"nodes\": [\n");
    for (size_t i = 0; i < modelData.nodes.size(); ++i) {
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", modelData.nodes[i].name.c_str());
        printf("      \"position\": [%.6f, %.6f, %.6f],\n",
            (double)modelData.nodes[i].position.x,
            (double)modelData.nodes[i].position.y,
            (double)modelData.nodes[i].position.z);
        printf("      \"is_static\": %s\n", modelData.nodes[i].isStatic ? "true" : "false");
        printf("    }");
        if (i + 1 < modelData.nodes.size()) {
            printf(",\n");
        } else {
            printf("\n");
        }
    }
    printf("  ],\n");
    printf("  \"mesh_bounds\": {\n");
    printf("    \"min\": [%.1f, %.1f, %.1f],\n",
        (double)modelData.minBounds.x, (double)modelData.minBounds.y, (double)modelData.minBounds.z);
    printf("    \"max\": [%.1f, %.1f, %.1f]\n",
        (double)modelData.maxBounds.x, (double)modelData.maxBounds.y, (double)modelData.maxBounds.z);
    printf("  }\n");
    printf("}\n");
}

void TestUi(const std::string& ray_origin_str, const std::string& ray_dir_str) {
    Vector3 ray_origin = ParseVector3(ray_origin_str);
    Vector3 ray_dir = ParseVector3(ray_dir_str);

    RaycastResult result = ImmediateUI::RaycastUI(ray_origin, ray_dir);

    printf("{\n");
    printf("  \"intersected\": %s,\n", result.intersected ? "true" : "false");
    printf("  \"t\": %.6f,\n", (double)result.t);
    if (result.intersected) {
        printf("  \"intersection_point\": [%.6f, %.6f, %.6f],\n",
            (double)result.point.x, (double)result.point.y, (double)result.point.z);
    } else {
        printf("  \"intersection_point\": null,\n");
    }
    printf("  \"u\": %.4f,\n", (double)result.u);
    printf("  \"v\": %.4f,\n", (double)result.v);
    printf("  \"triggered_action\": \"%s\"\n", result.triggeredAction.c_str());
    printf("}\n");
}

void TestGame() {
    GameWorld gameWorld;
    gameWorld.SimulateGame();
    
    const auto& events = gameWorld.GetEvents();

    printf("[\n");
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];
        printf("  {\n");
        printf("    \"time\": %.3f,\n", ev.time);
        printf("    \"event\": \"%s\"", ev.eventType.c_str());
        
        if (ev.eventType == "LEVEL_LOADED") {
            printf(",\n    \"details\": {\n");
            printf("      \"gltf\": \"%s\"\n", ev.clip.c_str());
            printf("    }\n");
        } else if (ev.eventType == "TARGET_SPAWNED") {
            printf(",\n    \"details\": {\n");
            printf("      \"id\": %d,\n", ev.id);
            printf("      \"pos\": [%.1f, %.1f, %.1f]\n", (double)ev.pos.x, (double)ev.pos.y, (double)ev.pos.z);
            printf("    }\n");
        } else if (ev.eventType == "SHOOT") {
            printf(",\n    \"details\": {\n");
            printf("      \"bullet_pos\": [%.1f, %.1f, %.1f],\n", (double)ev.pos.x, (double)ev.pos.y, (double)ev.pos.z);
            printf("      \"bullet_vel\": [%.1f, %.1f, %.1f]\n", (double)ev.vel.x, (double)ev.vel.y, (double)ev.vel.z);
            printf("    }\n");
        } else if (ev.eventType == "COLLISION") {
            printf(",\n    \"details\": {\n");
            printf("      \"type\": \"%s\",\n", ev.type.c_str());
            printf("      \"target_id\": %d,\n", ev.id);
            printf("      \"pos\": [%.1f, %.1f, %.1f]\n", (double)ev.pos.x, (double)ev.pos.y, (double)ev.pos.z);
            printf("    }\n");
        } else if (ev.eventType == "TARGET_DESTROYED") {
            printf(",\n    \"details\": {\n");
            printf("      \"id\": %d\n", ev.id);
            printf("    }\n");
        } else if (ev.eventType == "SCORE_UPDATED") {
            printf(",\n    \"details\": {\n");
            printf("      \"score\": %d\n", ev.score);
            printf("    }\n");
        } else if (ev.eventType == "PLAY_SOUND") {
            printf(",\n    \"details\": {\n");
            printf("      \"clip\": \"%s\",\n", ev.clip.c_str());
            printf("      \"pos\": [%.1f, %.1f, %.1f]\n", (double)ev.pos.x, (double)ev.pos.y, (double)ev.pos.z);
            printf("    }\n");
        } else if (ev.eventType == "GAME_OVER") {
            printf(",\n    \"details\": {\n");
            printf("      \"final_score\": %d\n", ev.score);
            printf("    }\n");
        } else {
            printf("\n");
        }
        
        printf("  }");
        if (i + 1 < events.size()) {
            printf(",\n");
        } else {
            printf("\n");
        }
    }
    printf("]\n");
}

// ----------------------------------------------------
// Internal Math and Utility Unit Tests (No Flags)
// ----------------------------------------------------

void RunUnitTests() {
    Logger::Info("Starting internal engine unit tests...");

    // Vector3 tests
    Vector3 v1(1.0f, 2.0f, 3.0f);
    Vector3 v2(4.0f, 5.0f, 6.0f);

    Vector3 add_res = v1 + v2;
    assert(add_res == Vector3(5.0f, 7.0f, 9.0f));

    float dot_res = v1.Dot(v2);
    assert(std::abs(dot_res - 32.0f) < 1e-5f);

    Vector3 cross_res = v1.Cross(v2);
    assert(cross_res == Vector3(-3.0f, 6.0f, -3.0f));

    Vector3 zero_v(0.0f, 0.0f, 0.0f);
    assert(zero_v.Normalize() == Vector3(0.0f, 0.0f, 0.0f));

    Logger::Info("Vector3 unit tests passed.");

    // Matrix4x4 tests
    Matrix4x4 translation = Matrix4x4::Translation(1.0f, 2.0f, 3.0f);
    Vector3 point(1.0f, 1.0f, 1.0f);
    Vector3 trans_point = translation.TransformPoint(point);
    assert(trans_point == Vector3(2.0f, 3.0f, 4.0f));

    Vector3 dir(1.0f, 1.0f, 1.0f);
    Vector3 trans_dir = translation.TransformVector(dir);
    assert(trans_dir == Vector3(1.0f, 1.0f, 1.0f)); // Point was not translated!

    // normal matrix checks
    Matrix4x4 scale = Matrix4x4::Scaling(1.0f, 2.0f, 3.0f);
    float normal_m[3][3];
    assert(scale.GetNormalMatrix(normal_m));
    assert(std::abs(normal_m[0][0] - 1.0f) < 1e-5f);
    assert(std::abs(normal_m[1][1] - 0.5f) < 1e-5f);
    assert(std::abs(normal_m[2][2] - 0.333333f) < 1e-5f);

    Logger::Info("Matrix4x4 unit tests passed.");

    // Quaternion tests
    Quaternion q_ident;
    assert(q_ident.w == 1.0f && q_ident.x == 0.0f && q_ident.y == 0.0f && q_ident.z == 0.0f);

    float angle = 0.5f;
    Quaternion q_rot_y = Quaternion::FromEuler(angle, 0.0f, 0.0f);
    Matrix4x4 m_rot_y = q_rot_y.ToRotationMatrix();
    Matrix4x4 m_rot_y_euler = Matrix4x4::RotationY(angle);

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            assert(std::abs(m_rot_y(r,c) - m_rot_y_euler(r,c)) < 1e-5f);
        }
    }

    // Quaternion conversion back and forth
    Quaternion q_rot_all = Quaternion::FromEuler(0.5f, -0.3f, 0.2f);
    float h, p, r;
    q_rot_all.ToEuler(h, p, r);
    assert(std::abs(h - 0.5f) < 1e-4f);
    assert(std::abs(p - (-0.3f)) < 1e-4f);
    assert(std::abs(r - 0.2f) < 1e-4f);

    // Gimbal lock test
    float pi_2 = 3.14159265f / 2.0f;
    Quaternion q_lock = Quaternion::FromEuler(0.5f, pi_2, 0.2f);
    q_lock.ToEuler(h, p, r);
    assert(std::abs(h - 0.0f) < 1e-4f); // Heading set to 0 when locked
    assert(std::abs(p - pi_2) < 1e-4f);
    assert(std::abs(r - 0.7f) < 1e-4f); // roll + heading = 0.2 + 0.5 = 0.7

    Logger::Info("Quaternion unit tests passed.");

    // Timer tests
    Timer timer;
    timer.Tick();
    float d1 = timer.GetDeltaTime();
    assert(d1 >= 0.0f);

    timer.Stop();
    timer.Tick();
    assert(timer.GetDeltaTime() == 0.0f);

    Logger::Info("Timer unit tests passed.");

    Logger::Info("All internal unit tests completed successfully!");
}

int main(int argc, char* argv[]) {
    EnableVirtualTerminalProcessing();

    if (argc > 1) {
        std::string flag = argv[1];
        if (flag == "--test-math") {
            TestMath();
            return 0;
        } else if (flag == "--test-physics") {
            if (argc < 4) {
                std::cerr << "Usage: --test-physics <fps> <duration>\n";
                return 1;
            }
            int fps = std::stoi(argv[2]);
            float duration = std::stof(argv[3]);
            TestPhysics(fps, duration);
            return 0;
        } else if (flag == "--test-audio") {
            if (argc < 6) {
                std::cerr << "Usage: --test-audio <src_pos> <src_vel> <list_pos> <list_vel>\n";
                return 1;
            }
            TestAudio(argv[2], argv[3], argv[4], argv[5]);
            return 0;
        } else if (flag == "--test-gltf") {
            if (argc < 3) {
                std::cerr << "Usage: --test-gltf <gltf_file_path>\n";
                return 1;
            }
            TestGltf(argv[2]);
            return 0;
        } else if (flag == "--test-ui") {
            if (argc < 4) {
                std::cerr << "Usage: --test-ui <ray_origin> <ray_dir>\n";
                return 1;
            }
            TestUi(argv[2], argv[3]);
            return 0;
        } else if (flag == "--test-game") {
            TestGame();
            return 0;
        } else {
            std::cerr << "Unknown flag: " << flag << "\n";
            return 1;
        }
    }

    // Run internal math/util checks if run without arguments
    try {
        RunUnitTests();
    } catch (const std::exception& e) {
        Logger::Error(std::string("Test failed with exception: ") + e.what());
        return 1;
    }

    // Make sure logger queues are processed before exiting
    Logger::GetInstance().Shutdown();
    return 0;
}
