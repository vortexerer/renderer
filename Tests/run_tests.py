#!/usr/bin/env python3
import sys
import subprocess
import json
import math
import os
import argparse

# Colors for nice console output
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RESET = "\033[0m"

# Default paths
DEFAULT_GLTF_PATH = os.path.join(os.path.dirname(__file__), "Assets", "Level.gltf")

class TestRunner:
    def __init__(self, engine_cmd, verbose=False):
        self.engine_cmd = engine_cmd
        self.verbose = verbose

    def run_engine(self, extra_args):
        cmd = list(self.engine_cmd) + extra_args
        if self.verbose:
            print(f"{CYAN}[RUNNING]{RESET} {' '.join(cmd)}")
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode != 0:
            if self.verbose:
                print(f"{RED}[ERROR]{RESET} Exit code: {result.returncode}")
                print(f"Stdout:\n{result.stdout}")
                print(f"Stderr:\n{result.stderr}")
            raise RuntimeError(f"Engine crashed or failed with exit code {result.returncode}\nStderr: {result.stderr}")
        return result.stdout

    # ==========================================
    # TIER 1 TESTS
    # ==========================================

    def test_t1_f1_math(self):
        """Tier 1 - F1: Math Operations and Formulas"""
        out = self.run_engine(["--test-math"])
        data = json.loads(out)

        # 1. Vector addition: (1,2,3) + (4,5,6) = (5,7,9)
        assert math.isclose(data["v_add"][0], 5.0), f"Expected 5.0, got {data['v_add'][0]}"
        assert math.isclose(data["v_add"][1], 7.0), f"Expected 7.0, got {data['v_add'][1]}"
        assert math.isclose(data["v_add"][2], 9.0), f"Expected 9.0, got {data['v_add'][2]}"

        # 2. Vector dot product: (1,2,3) . (4,5,6) = 1*4 + 2*5 + 3*6 = 32
        assert math.isclose(data["v_dot"], 32.0), f"Expected 32.0, got {data['v_dot']}"

        # 3. Vector cross product: (1,2,3) x (4,5,6) = (-3, 6, -3)
        assert math.isclose(data["v_cross"][0], -3.0), f"Expected -3.0, got {data['v_cross'][0]}"
        assert math.isclose(data["v_cross"][1], 6.0), f"Expected 6.0, got {data['v_cross'][1]}"
        assert math.isclose(data["v_cross"][2], -3.0), f"Expected -3.0, got {data['v_cross'][2]}"

        # 4. Euler View Transformation matching E(h, p, r) = Rz(r) * Rx(p) * Ry(h)
        # Verify the matrix output matches formula 4.21
        h, p, r = 0.5, 0.3, 0.2
        cos_h, sin_h = math.cos(h), math.sin(h)
        cos_p, sin_p = math.cos(p), math.sin(p)
        cos_r, sin_r = math.cos(r), math.sin(r)

        # Row 0
        r00 = cos_r * cos_h - sin_r * sin_p * sin_h
        r01 = -sin_r * cos_p
        r02 = cos_r * sin_h + sin_r * sin_p * cos_h
        # Row 1
        r10 = sin_r * cos_h + cos_r * sin_p * sin_h
        r11 = cos_r * cos_p
        r12 = sin_r * sin_h - cos_r * sin_p * cos_h
        # Row 2
        r20 = -cos_p * sin_h
        r21 = sin_p
        r22 = cos_p * cos_h

        e_mat = data["euler_mat"]
        assert math.isclose(e_mat[0][0], r00, abs_tol=1e-5), f"Euler matrix R00 incorrect: got {e_mat[0][0]}, expected {r00}"
        assert math.isclose(e_mat[0][1], r01, abs_tol=1e-5), f"Euler matrix R01 incorrect"
        assert math.isclose(e_mat[0][2], r02, abs_tol=1e-5), f"Euler matrix R02 incorrect"
        assert math.isclose(e_mat[1][0], r10, abs_tol=1e-5), f"Euler matrix R10 incorrect"
        assert math.isclose(e_mat[1][1], r11, abs_tol=1e-5), f"Euler matrix R11 incorrect"
        assert math.isclose(e_mat[1][2], r12, abs_tol=1e-5), f"Euler matrix R12 incorrect"
        assert math.isclose(e_mat[2][0], r20, abs_tol=1e-5), f"Euler matrix R20 incorrect"
        assert math.isclose(e_mat[2][1], r21, abs_tol=1e-5), f"Euler matrix R21 incorrect"
        assert math.isclose(e_mat[2][2], r22, abs_tol=1e-5), f"Euler matrix R22 incorrect"

        # 5. Quaternion SLERP at t=0.5
        # Perpendicular axes: rotate 90 around X vs rotate 90 around Y
        qa = (math.cos(math.pi/4), math.sin(math.pi/4), 0.0, 0.0)
        qb = (math.cos(math.pi/4), 0.0, math.sin(math.pi/4), 0.0)
        # Hand-calculated SLERP at t=0.5
        # dot = qa.qb = cos(pi/4)*cos(pi/4) = 0.5
        # theta = acos(0.5) = pi/3 (60 degrees)
        # s0 = sin(pi/6)/sin(pi/3) = 0.5 / (sqrt(3)/2) = 1/sqrt(3)
        # s1 = sin(pi/6)/sin(pi/3) = 1/sqrt(3)
        # slerp = 1/sqrt(3) * qa + 1/sqrt(3) * qb
        # w = 2 * cos(pi/4) / sqrt(3) = sqrt(2)/sqrt(3) = sqrt(2/3) = 0.816496
        # x = sin(pi/4)/sqrt(3) = 1/sqrt(6) = 0.408248
        # y = sin(pi/4)/sqrt(3) = 1/sqrt(6) = 0.408248
        # z = 0.0
        expected_slerp = (math.sqrt(2/3), 1.0/math.sqrt(6), 1.0/math.sqrt(6), 0.0)
        res_slerp = data["slerp_half"]
        assert math.isclose(res_slerp[0], expected_slerp[0], abs_tol=1e-5), f"SLERP W incorrect: got {res_slerp[0]}, expected {expected_slerp[0]}"
        assert math.isclose(res_slerp[1], expected_slerp[1], abs_tol=1e-5), f"SLERP X incorrect: got {res_slerp[1]}, expected {expected_slerp[1]}"
        assert math.isclose(res_slerp[2], expected_slerp[2], abs_tol=1e-5), f"SLERP Y incorrect: got {res_slerp[2]}, expected {expected_slerp[2]}"
        assert math.isclose(res_slerp[3], expected_slerp[3], abs_tol=1e-5), f"SLERP Z incorrect: got {res_slerp[3]}, expected {expected_slerp[3]}"

    def test_t1_f4_physics(self):
        """Tier 1 - F4: Physics Timestep and Integration"""
        out = self.run_engine(["--test-physics", "60", "0.5"])
        steps = json.loads(out)

        assert len(steps) > 0, "No physics steps simulated."

        # Verify 100Hz timesteps (exactly 0.01s steps)
        for i, step in enumerate(steps):
            expected_time = round(i * 0.01, 3)
            assert math.isclose(step["time"], expected_time, abs_tol=1e-5), f"Step {i} time is {step['time']}, expected {expected_time}"

        # Verify Semi-implicit Euler integration:
        # v_{n+1} = v_n + a_n * dt
        # r_{n+1} = r_n + v_{n+1} * dt
        # If it matches Semi-implicit Euler, pos_y[n+1] = pos_y[n] + vel_y[n+1] * 0.01
        for i in range(len(steps) - 1):
            s_curr = steps[i]
            s_next = steps[i+1]
            # Skip collision step since velocity changes discontinuously due to impulse
            if s_curr["pos_y"] - 0.5 <= 1e-4 or s_next["pos_y"] - 0.5 <= 1e-4:
                continue

            expected_pos_next = s_curr["pos_y"] + s_next["vel_y"] * 0.01
            assert math.isclose(s_next["pos_y"], expected_pos_next, abs_tol=1e-5), \
                f"Step {i+1} position does not match Semi-implicit Euler update: got {s_next['pos_y']}, expected {expected_pos_next} (using new velocity {s_next['vel_y']})"

    def test_t1_f7_audio(self):
        """Tier 1 - F7: 3D Spatial Audio and Doppler Calculations"""
        # Source at (1,2,3), moving at (10,0,0) towards listener at (4,2,3), who is stationary
        out = self.run_engine(["--test-audio", "1,2,3", "10,0,0", "4,2,3", "0,0,0"])
        data = json.loads(out)

        # Distance is 3.0
        assert math.isclose(data["distance"], 3.0), f"Expected distance 3.0, got {data['distance']}"

        # Source is moving in direction of relative vector (listener_pos - source_pos) = (3, 0, 0)
        # Velocity vector is (10, 0, 0), so full speed is along relative vector
        # Doppler Factor should be f' = f * (c - v_l) / (c - v_s) = f * (343 - 0) / (343 - 10) = 343 / 333 = 1.03003
        expected_doppler = 343.0 / 333.0
        assert math.isclose(data["doppler_factor"], expected_doppler, abs_tol=1e-5), \
            f"Expected Doppler factor {expected_doppler}, got {data['doppler_factor']}"

        # Source is directly along X axis relative to listener, so azimuth is approximately -90 degrees
        # (source is on listener's left since listener is at (4,2,3) looking at (0,0,-1) or default orientation)
        # Let's verify azimuth is around -90 degrees
        assert math.isclose(data["azimuth_degrees"], -90.0, abs_tol=1.0), f"Expected azimuth ~ -90.0, got {data['azimuth_degrees']}"

    def test_t1_f8_gltf(self):
        """Tier 1 - F8: glTF Parser"""
        out = self.run_engine(["--test-gltf", DEFAULT_GLTF_PATH])
        data = json.loads(out)

        assert "nodes" in data, "No nodes returned in parsed glTF output"
        assert len(data["nodes"]) == 3, f"Expected 3 nodes, got {len(data['nodes'])}"

        # Check node names
        names = [n["name"] for n in data["nodes"]]
        assert "Static_Floor" in names, "Missing node Static_Floor"
        assert "Static_Wall" in names, "Missing node Static_Wall"
        assert "Spawn_Target_A" in names, "Missing node Spawn_Target_A"

        # Check static flags
        for node in data["nodes"]:
            if node["name"].startswith("Static_"):
                assert node["is_static"] is True, f"Node {node['name']} should be static"
            else:
                assert node["is_static"] is False, f"Node {node['name']} should not be static"

    def test_t1_f8_ui(self):
        """Tier 1 - F8: UI Raycast Interaction"""
        # Ray starting at (0, 1.5, 0) pointing directly along -Z (0, 0, -1)
        # The plane is at Z = -2.0, centered at (0, 1.5, -2.0)
        # Should intersect at (0, 1.5, -2.0) with normalized coords (0.5, 0.5)
        out = self.run_engine(["--test-ui", "0,1.5,0", "0,0,-1"])
        data = json.loads(out)

        assert data["intersected"] is True, "Expected ray to intersect UI Quad plane"
        assert math.isclose(data["t"], 2.0), f"Expected t=2.0, got {data['t']}"
        assert math.isclose(data["intersection_point"][0], 0.0), f"Expected X=0.0, got {data['intersection_point'][0]}"
        assert math.isclose(data["intersection_point"][1], 1.5), f"Expected Y=1.5, got {data['intersection_point'][1]}"
        assert math.isclose(data["intersection_point"][2], -2.0), f"Expected Z=-2.0, got {data['intersection_point'][2]}"
        assert math.isclose(data["u"], 0.5), f"Expected U=0.5, got {data['u']}"
        assert math.isclose(data["v"], 0.5), f"Expected V=0.5, got {data['v']}"
        assert data["triggered_action"] == "Restart Game", f"Expected Restart Game, got {data['triggered_action']}"

    # ==========================================
    # TIER 2 TESTS: BOUNDARY CASES
    # ==========================================

    def test_t2_f1_math_boundaries(self):
        """Tier 2 - F1: Math Boundary Conditions (Gimbal Lock, Singular Matrix, Quats)"""
        out = self.run_engine(["--test-math"])
        data = json.loads(out)

        # 1. Gimbal Lock pitch = 90 degrees
        # E(h, 90, r) should have R00 = cos(r+h), R02 = sin(r+h)
        h, r = 0.5, 0.2
        expected_r00 = math.cos(r + h)
        expected_r02 = math.sin(r + h)
        g_mat = data["gimbal_mat"]
        assert math.isclose(g_mat[0][0], expected_r00, abs_tol=1e-5), f"Gimbal Lock R00: expected {expected_r00}, got {g_mat[0][0]}"
        assert math.isclose(g_mat[0][2], expected_r02, abs_tol=1e-5), f"Gimbal Lock R02: expected {expected_r02}, got {g_mat[0][2]}"

        # 2. Singular Matrix Inversion: should fail or handle gracefully (return None / null)
        assert data["singular_invert"] is None, "Expected singular matrix inversion to return None"

        # 3. Zero length quaternion normalization: should handle gracefully without crash
        assert data["zero_q_norm"] == [0.0, 0.0, 0.0, 0.0], f"Expected zero quat normalization to return zero quat, got {data['zero_q_norm']}"

        # 4. Normal transform matrix under non-uniform scaling: A = (M^-1)^T
        # For M_nu = Translate(1,2,3) * RotateY(0.5) * Scale(1,2,3)
        # The top-left 3x3 normal matrix is transpose(invert(R * S)) = transpose(S^-1 * R^-1) = R * S^-1
        # Let's verify normal matrix matches analytical inverse transpose
        norm_mat = data["normal_mat"]
        # Analytical check:
        # M33 = RotateY(0.5) * Scale(1,2,3)
        # RotateY(0.5) = [[cos(0.5), 0, sin(0.5)], [0, 1, 0], [-sin(0.5), 0, cos(0.5)]]
        # Scale(1,2,3) = [[1, 0, 0], [0, 2, 0], [0, 0, 3]]
        # M33 = [[cos(0.5), 0, 3*sin(0.5)], [0, 2, 0], [-sin(0.5), 0, 3*cos(0.5)]]
        # inv(M33) = Scale(1, 0.5, 1/3) * RotateY(-0.5)
        # transpose(inv(M33)) = transpose(Scale(1, 0.5, 1/3) * RotateY(-0.5))
        # = transpose(RotateY(-0.5)) * Scale(1, 0.5, 1/3) = RotateY(0.5) * Scale(1, 0.5, 1/3)
        # = [[cos(0.5), 0, sin(0.5)/3], [0, 0.5, 0], [-sin(0.5), 0, cos(0.5)/3]]
        cos_05, sin_05 = math.cos(0.5), math.sin(0.5)
        expected_norm = [
            [cos_05, 0.0, sin_05/3.0],
            [0.0, 0.5, 0.0],
            [-sin_05, 0.0, cos_05/3.0]
        ]
        for i in range(3):
            for j in range(3):
                assert math.isclose(norm_mat[i][j], expected_norm[i][j], abs_tol=1e-5), \
                    f"Normal matrix component [{i}][{j}] incorrect: got {norm_mat[i][j]}, expected {expected_norm[i][j]}"

    def test_t2_f4_physics_boundaries(self):
        """Tier 2 - F4: Physics stability under extreme frame time (1.0s)"""
        out = self.run_engine(["--test-physics", "1", "2.0"])
        steps = json.loads(out)
        # At 1 FPS for 2 seconds, frame time is 1.0s.
        # Physics engine accumulator should step 100 times per frame, totaling 200 steps.
        assert len(steps) == 200, f"Expected 200 physics steps, got {len(steps)}"

    def test_t2_f7_audio_boundaries(self):
        """Tier 2 - F7: Audio Boundary Conditions (Zero Distance, Supersonic)"""
        # 1. Zero distance (source at listener)
        out = self.run_engine(["--test-audio", "1,2,3", "0,0,0", "1,2,3", "0,0,0"])
        data = json.loads(out)
        assert math.isclose(data["distance"], 0.0), f"Expected distance 0.0, got {data['distance']}"
        # Volume/ILD calculations should cap or handle gracefully without infinity/NaN
        assert not math.isinf(data["ild_left"]) and not math.isnan(data["ild_left"])

        # 2. Supersonic source velocity (400 m/s > speed of sound 343 m/s)
        out = self.run_engine(["--test-audio", "1,2,3", "400,0,0", "4,2,3", "0,0,0"])
        data = json.loads(out)
        # Doppler factor should be capped, not negative or infinite
        assert data["doppler_factor"] > 0.0, f"Doppler factor should be positive, got {data['doppler_factor']}"
        assert not math.isinf(data["doppler_factor"])

    def test_t2_f8_ui_boundaries(self):
        """Tier 2 - F8: Ray parallel to UI Quad plane (No intersection)"""
        # Ray starting at (0, 1.5, 0) pointing along +X (1, 0, 0), parallel to plane at Z=-2
        out = self.run_engine(["--test-ui", "0,1.5,0", "1,0,0"])
        data = json.loads(out)
        assert data["intersected"] is False, "Expected no intersection for parallel ray"

    # ==========================================
    # TIER 3 TESTS: CROSS-FEATURE COMBINATIONS
    # ==========================================

    def test_t3_physics_collision_audio(self):
        """Tier 3: Collision triggers audio event"""
        out = self.run_engine(["--test-game"])
        events = json.loads(out)

        # Verify that for every COLLISION event, a PLAY_SOUND event is triggered close in time and position
        for i, event in enumerate(events):
            if event["event"] == "COLLISION":
                col_pos = event["details"]["pos"]
                col_time = event["time"]

                # Scan forward for PLAY_SOUND event
                sound_triggered = False
                for j in range(i + 1, min(i + 5, len(events))):
                    ev_next = events[j]
                    if ev_next["event"] == "PLAY_SOUND" and ev_next["details"]["clip"] == "Hit.wav":
                        sound_pos = ev_next["details"]["pos"]
                        # Match position (within tolerance)
                        dist = math.sqrt(sum((col_pos[k] - sound_pos[k])**2 for k in range(3)))
                        if dist < 0.1 and abs(ev_next["time"] - col_time) < 0.1:
                            sound_triggered = True
                            break
                assert sound_triggered, f"Collision at time {col_time} did not trigger Hit.wav spatial sound event at same position."

    def test_t3_bullet_collision_score(self):
        """Tier 3: Bullet collision updates score"""
        out = self.run_engine(["--test-game"])
        events = json.loads(out)

        # Verify that a target destruction/collision increments the score
        score_updated = False
        last_score = 0
        for event in events:
            if event["event"] == "SCORE_UPDATED":
                new_score = event["details"]["score"]
                assert new_score > last_score, f"Score should increment, but went from {last_score} to {new_score}"
                last_score = new_score
                score_updated = True

        assert score_updated, "Score was never updated during game simulation"

    def test_t3_doppler_dynamic(self):
        """Tier 3: Doppler factor updates dynamically with velocity"""
        # Run audio calculations at two different approaching velocities
        out_slow = self.run_engine(["--test-audio", "1,2,3", "10,0,0", "4,2,3", "0,0,0"])
        out_fast = self.run_engine(["--test-audio", "1,2,3", "20,0,0", "4,2,3", "0,0,0"])

        data_slow = json.loads(out_slow)
        data_fast = json.loads(out_fast)

        # Faster approaching velocity should lead to higher frequency shift (higher Doppler factor)
        assert data_fast["doppler_factor"] > data_slow["doppler_factor"], \
            f"Doppler factor should increase with velocity: fast ({data_fast['doppler_factor']}) vs slow ({data_slow['doppler_factor']})"

    # ==========================================
    # TIER 4 TESTS: REAL-WORLD SCENARIOS
    # ==========================================

    def test_t4_s1_falling_bounce(self):
        """Tier 4 - Scenario 1: Falling target with bounce trajectory"""
        out = self.run_engine(["--test-physics", "60", "2.0"])
        steps = json.loads(out)

        # 1. Sphere should fall (y position decreases from 10.0 to 0.5)
        # 2. Sphere should bounce (velocity flips from negative to positive)
        # 3. Sphere should fall again under gravity
        bounced = False
        has_fallen = False
        peak_after_bounce = False
        prev_pos_y = 10.0
        prev_vel_y = 0.0

        for step in steps:
            pos_y = step["pos_y"]
            vel_y = step["vel_y"]

            if not bounced and pos_y < prev_pos_y:
                has_fallen = True

            # Detect bounce
            if has_fallen and not bounced and prev_vel_y < 0.0 and vel_y > 0.0:
                bounced = True
                assert math.isclose(pos_y, 0.5, abs_tol=0.05), f"Expected bounce around floor y=0.5, got y={pos_y}"

            if bounced and vel_y < 0.0 and prev_vel_y > 0.0:
                # Reached peak height after bounce and started falling again
                peak_after_bounce = True

            prev_pos_y = pos_y
            prev_vel_y = vel_y

        assert has_fallen, "Sphere did not fall."
        assert bounced, "Sphere did not bounce."
        assert peak_after_bounce, "Sphere did not reach a secondary peak and fall again."

    def test_t4_s2_fps_stability(self):
        """Tier 4 - Scenario 2: Variable Frame Rate Physics Stability"""
        # Run at 30, 60, and 90 FPS rendering rates
        out_30 = self.run_engine(["--test-physics", "30", "1.5"])
        out_60 = self.run_engine(["--test-physics", "60", "1.5"])
        out_90 = self.run_engine(["--test-physics", "90", "1.5"])

        steps_30 = json.loads(out_30)
        steps_60 = json.loads(out_60)
        steps_90 = json.loads(out_90)

        # All runs should execute exactly identical 150 physics steps (1.5s duration * 100Hz = 150 steps)
        assert len(steps_30) == 150, f"Expected 150 steps at 30 FPS, got {len(steps_30)}"
        assert len(steps_60) == 150, f"Expected 150 steps at 60 FPS, got {len(steps_60)}"
        assert len(steps_90) == 150, f"Expected 150 steps at 90 FPS, got {len(steps_90)}"

        # Assert step-by-step equivalence (trajectories are identical regardless of rendering frame rate)
        for i in range(150):
            s30 = steps_30[i]
            s60 = steps_60[i]
            s90 = steps_90[i]

            assert math.isclose(s30["pos_y"], s60["pos_y"], abs_tol=1e-5), f"Step {i} mismatch: 30 FPS y={s30['pos_y']} vs 60 FPS y={s60['pos_y']}"
            assert math.isclose(s60["pos_y"], s90["pos_y"], abs_tol=1e-5), f"Step {i} mismatch: 60 FPS y={s60['pos_y']} vs 90 FPS y={s90['pos_y']}"
            assert math.isclose(s30["vel_y"], s60["vel_y"], abs_tol=1e-5), f"Step {i} mismatch: 30 FPS v={s30['vel_y']} vs 60 FPS v={s60['vel_y']}"
            assert math.isclose(s60["vel_y"], s90["vel_y"], abs_tol=1e-5), f"Step {i} mismatch: 60 FPS v={s60['vel_y']} vs 90 FPS v={s90['vel_y']}"

    def test_t4_s3_moving_audio_doppler(self):
        """Tier 4 - Scenario 3: Doppler effect for moving source"""
        # Sound source moves past stationary listener at (0, 0, 0)
        # Position X goes from negative to positive: X = -50 (approach) -> X = 0 (closest) -> X = 50 (departure)
        # Velocity is constant (+20, 0, 0)
        out_approach = self.run_engine(["--test-audio", "-50,0,0", "20,0,0", "0,0,0", "0,0,0"])
        out_closest  = self.run_engine(["--test-audio", "0,0.01,0", "20,0,0", "0,0,0", "0,0,0"])
        out_depart   = self.run_engine(["--test-audio", "50,0,0", "20,0,0", "0,0,0", "0,0,0"])

        data_app = json.loads(out_approach)
        data_cls = json.loads(out_closest)
        data_dep = json.loads(out_depart)

        # Verify Doppler frequency shift transitions:
        # Approach: frequency increases (doppler_factor > 1.0)
        # Departure: frequency decreases (doppler_factor < 1.0)
        assert data_app["doppler_factor"] > 1.0, f"Expected approach Doppler factor > 1.0, got {data_app['doppler_factor']}"
        assert data_dep["doppler_factor"] < 1.0, f"Expected departure Doppler factor < 1.0, got {data_dep['doppler_factor']}"
        assert data_app["doppler_factor"] > data_cls["doppler_factor"] > data_dep["doppler_factor"], \
            f"Expected Doppler shift to decrease monotonically: {data_app['doppler_factor']} -> {data_cls['doppler_factor']} -> {data_dep['doppler_factor']}"

    def test_t4_s4_ui_button_interaction(self):
        """Tier 4 - Scenario 4: UI Interaction resets game state"""
        # Ray cast hits the Restart Game button
        out = self.run_engine(["--test-ui", "0,1.5,0", "0,0,-1"])
        data = json.loads(out)
        assert data["intersected"] is True
        assert data["triggered_action"] == "Restart Game", "Expected 'Restart Game' button trigger click"

    def test_t4_s5_complete_game_shoot_loop(self):
        """Tier 4 - Scenario 5: Complete Target Shooter Loop"""
        out = self.run_engine(["--test-game"])
        events = json.loads(out)

        # Assert full loop stages occur in order:
        # 1. Level load
        # 2. Target spawn (3 times)
        # 3. Shoot -> Collision -> Destroy -> Score Update (3 times)
        # 4. Game Over (score 3)
        stages = [e["event"] for e in events]

        assert "LEVEL_LOADED" in stages, "Game did not load level"
        assert stages.count("TARGET_SPAWNED") == 3, f"Expected 3 target spawns, got {stages.count('TARGET_SPAWNED')}"
        assert stages.count("SHOOT") == 3, f"Expected 3 shots fired, got {stages.count('SHOOT')}"
        assert stages.count("COLLISION") == 3, f"Expected 3 collisions, got {stages.count('COLLISION')}"
        assert stages.count("TARGET_DESTROYED") == 3, f"Expected 3 target destructions, got {stages.count('TARGET_DESTROYED')}"
        assert stages.count("SCORE_UPDATED") == 3, f"Expected 3 score updates, got {stages.count('SCORE_UPDATED')}"
        assert "GAME_OVER" in stages, "Game did not complete with GAME_OVER"

        # Final score check
        final_game_over = [e for e in events if e["event"] == "GAME_OVER"][0]
        assert final_game_over["details"]["final_score"] == 3, f"Expected final score 3, got {final_game_over['details']['final_score']}"

    # ==========================================
    # RUNNER METHOD
    # ==========================================

    def run_all_tests(self):
        tests = [
            (self.test_t1_f1_math, "TC1: Math Operations and View Formulas"),
            (self.test_t1_f4_physics, "TC2: Physics fixed timestep and Euler integration"),
            (self.test_t1_f7_audio, "TC3: 3D Spatial Audio & Doppler Factor calculations"),
            (self.test_t1_f8_gltf, "TC4: glTF JSON Parser Node verification"),
            (self.test_t1_f8_ui, "TC5: Immediate 3D UI raycast intersection"),
            (self.test_t2_f1_math_boundaries, "TC6: Math boundary/corner cases (Gimbal, Singular, scale)"),
            (self.test_t2_f4_physics_boundaries, "TC7: Physics extreme frametime stability"),
            (self.test_t2_f7_audio_boundaries, "TC8: Audio boundaries (Zero distance, Supersonic)"),
            (self.test_t2_f8_ui_boundaries, "TC9: UI Raycast parallel bounding check"),
            (self.test_t3_physics_collision_audio, "TC10: Integration: Physics collision triggering spatial Hit.wav"),
            (self.test_t3_bullet_collision_score, "TC11: Integration: Bullet hitting target triggering scoring"),
            (self.test_t3_doppler_dynamic, "TC12: Integration: Dynamic Doppler modulation with velocity"),
            (self.test_t4_s1_falling_bounce, "TC13: Scenario 1: Sphere fall and floor bounce trajectory"),
            (self.test_t4_s2_fps_stability, "TC14: Scenario 2: Trajectory invariance across 30/60/90 FPS"),
            (self.test_t4_s3_moving_audio_doppler, "TC15: Scenario 3: Audio Doppler transition (approach to depart)"),
            (self.test_t4_s4_ui_button_interaction, "TC16: Scenario 4: Raycast button hover and score reset link"),
            (self.test_t4_s5_complete_game_shoot_loop, "TC17: Scenario 5: End-to-end game shoot and destroy flow")
        ]

        passed_count = 0
        failed_tests = []

        print(f"\n{YELLOW}=================================================={RESET}")
        print(f"{YELLOW}          VR GAME ENGINE E2E TEST SUITE            {RESET}")
        print(f"{YELLOW}=================================================={RESET}\n")

        for test_func, test_name in tests:
            try:
                test_func()
                print(f"[{GREEN}PASS{RESET}] {test_name}")
                passed_count += 1
            except Exception as e:
                print(f"[{RED}FAIL{RESET}] {test_name}")
                print(f"       -> Details: {e}")
                failed_tests.append((test_name, str(e)))

        total_tests = len(tests)
        success_rate = (passed_count / total_tests) * 100

        print(f"\n{YELLOW}=================================================={RESET}")
        print(f"Summary: {passed_count}/{total_tests} passed ({success_rate:.1f}%)")
        print(f"{YELLOW}=================================================={RESET}\n")

        return len(failed_tests) == 0

def self_verify(mock_script_path, verbose=False):
    """
    Verify the E2E test suite's validation accuracy.
    Runs the tests twice:
    1. Against the healthy mock engine: expects all tests to PASS.
    2. Against the faulty mock engine (--faulty): expects tests to FAIL.
    """
    print(f"\n{CYAN}>>> Meta-Verification: Verifying the E2E Test Suite itself...{RESET}\n")

    # 1. Test against healthy mock engine
    healthy_cmd = [sys.executable, mock_script_path]
    print(f"Running against HEALTHY mock engine: {healthy_cmd}")
    runner_healthy = TestRunner(healthy_cmd, verbose=verbose)
    healthy_ok = runner_healthy.run_all_tests()
    if not healthy_ok:
        print(f"{RED}Meta-Verification Failed: E2E tests failed against the healthy mock engine!{RESET}")
        return False
    print(f"{GREEN}E2E test suite successfully passed all tests against the healthy mock engine.{RESET}\n")

    # 2. Test against faulty mock engine
    faulty_cmd = [sys.executable, mock_script_path, "--faulty"]
    print(f"Running against FAULTY mock engine: {faulty_cmd}")
    runner_faulty = TestRunner(faulty_cmd, verbose=verbose)
    faulty_ok = runner_faulty.run_all_tests()
    if faulty_ok:
        print(f"{RED}Meta-Verification Failed: E2E tests passed against the faulty mock engine! The suite is not sensitive to errors.{RESET}")
        return False
    print(f"{GREEN}E2E test suite successfully detected errors and failed against the faulty mock engine as expected.{RESET}\n")

    print(f"{GREEN}>>> Meta-Verification Successful! The E2E test infrastructure works perfectly.{RESET}\n")
    return True

def main():
    parser = argparse.ArgumentParser(description="VR Game Engine E2E Test Runner")
    parser.add_argument("-e", "--engine-path", help="Path to the VREngine executable")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print debug/verbose outputs")
    parser.add_argument("--verify-suite", action="store_true", help="Run self-verification checks on healthy vs faulty mock configurations")
    args = parser.parse_args()

    mock_engine = os.path.join(os.path.dirname(__file__), "mock_engine.py")

    # Decide engine command
    if args.engine_path:
        engine_cmd = [args.engine_path]
    else:
        # Check standard build locations
        proj_root = os.path.dirname(os.path.dirname(__file__))
        path_options = [
            os.path.join(proj_root, "Build", "VREngine.exe"),
            os.path.join(proj_root, "VREngine.exe"),
            # Cross-platform (Linux/macOS) CMake build output
            os.path.join(proj_root, "Build", "VREngine"),
            os.path.join(proj_root, "VREngine"),
        ]
        found_path = None
        for p in path_options:
            if os.path.exists(p):
                found_path = p
                break

        if found_path:
            engine_cmd = [found_path]
            print(f"Found compiled VREngine executable at: {found_path}")
        else:
            print("VREngine executable not found. Falling back to python mock engine.")
            engine_cmd = [sys.executable, mock_engine]

    if args.verify_suite:
        # Meta-verification mode
        ok = self_verify(mock_engine, verbose=args.verbose)
        sys.exit(0 if ok else 1)
    else:
        runner = TestRunner(engine_cmd, verbose=args.verbose)
        ok = runner.run_all_tests()
        sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
