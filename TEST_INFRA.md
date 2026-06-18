# VR Game Engine E2E Test Infrastructure

## 1. Test Philosophy
The E2E (End-to-End) testing framework for the VR Game Engine and "Special Target Shooter" game acts as an opaque-box validator. Because VR hardware is not present or active during automated testing environments (such as CI/CD pipelines), the engine supports dedicated Command-Line Interface (CLI) entry points. These flags bypass the physical HMD (Head-Mounted Display) and controller interfaces, running automated simulation sequences, logging, and mathematical validations that can be parsed and validated programmatically.

## 2. Feature Inventory
The E2E test suite validates the following 8 core engine features:
*   **F1: Core Math & Transformations**: Vector3, Matrix4x4, Quaternions, SLERP, and Euler view transformations ($E(h, p, r) = R_z(r)R_x(p)R_y(h)$).
*   **F2: OpenXR Session & Tracking**: Simulation of OpenXR session state lifecycles, projection matrices, eye tracking, and controller poses.
*   **F3: DirectX 12 Instanced Stereo Renderer**: GPU adapter selection, swapchain backbuffer setups, PSO compilation, resource barriers, and instanced stereo draw calls.
*   **F4: Fixed Timestep Rigidbody Physics**: Rigidbody integration using Semi-implicit Euler updates at a fixed 100Hz frequency.
*   **F5: GJK/EPA Collision & Impulse Resolution**: Collision detection between Sphere-Sphere, Sphere-AABB, and Convex Hulls, and impulse-based collision response.
*   **F6: WASAPI Exclusive Mode Audio Streaming**: Low-latency exclusive mode device initialization, PCM buffer alignment, and WAV parsing.
*   **F7: HRTF 3D Spatial Audio & Doppler**: Spatial panning using Interaural Time Difference (ITD), Interaural Level Difference (ILD), head shadow filtering, and Doppler frequency shifting.
*   **F8: glTF 2.0 Loader & 3D Spatial UI**: Manual decoding of glTF JSON, binary buffer unpacking, BMP/TGA loaders, 3D Quad UI projection, and raycast intersections.

---

## 3. Test Architecture (4-Tier Design)
The test runner executes tests organized into a 4-tier hierarchy:

### Tier 1: Feature Coverage (F1 to F8)
Verifies the basic correctness of each feature individually under standard conditions:
*   **F1**: Core vector, matrix, quaternion calculations, SLERP at $t=0.5$, and Euler View transformation.
*   **F2**: OpenXR lifecycles, projection view matrix retrieval, and controller binding.
*   **F3**: Core DX12 creation, PSO creation, and draw call execution.
*   **F4**: Rigidbody creation and physics integration (Semi-implicit Euler).
*   **F5**: GJK sphere overlapping and EPA penetration/normal calculations.
*   **F6**: WASAPI device initialization and WAV parsing.
*   **F7**: ITD delay, ILD attenuation (inverse square law), and Doppler shift calculations.
*   **F8**: glTF scene parsing, texture decoding, and Ray-Quad UI intersection.

### Tier 2: Boundary & Corner Cases
Tests edge cases, extreme inputs, and robustness:
*   **F1**: Non-uniform scaling (Adjugate transpose), Gimbal Lock pitch ($90^\circ$ and $-90^\circ$), SLERP with collinear/perpendicular quaternions, matrix inversion failure, and zero-length quaternions.
*   **F2**: Session lost state recovery, tracking loss, extreme frame prediction times, and controller disconnection mid-session.
*   **F3**: Swapchain resizing, GPU fallback adapters, descriptor heap overflow, and VRAM exhaustion.
*   **F4**: Physics stability under extreme frame times (1s), infinite mass (static bodies), near-zero mass, and velocity clamping.
*   **F5**: EPA coplanar/degenerate simplexes, touching contact detection, point/line segment collisions, and restitution limits ($e=0, e=1$).
*   **F6**: Unsupported audio sample rates, buffer starvation/underruns, unaligned writes, and corrupt WAV parsing.
*   **F7**: Division-by-zero distance (source at listener), supersonic speeds (Doppler capping), and azimuth at $180^\circ$ (rear-head sound).
*   **F8**: Malformed glTF JSON, mismatched buffers, corrupted texture headers, and ray parallel to UI plane.

### Tier 3: Cross-Feature Combinations
Validates integrations and interactions between two or more modules:
*   **TC3.1**: Rigidbody orientation integrated via Quaternions and updated in DX12 Renderer constant buffers.
*   **TC3.2**: OpenXR viewports driving DX12 stereo projection.
*   **TC3.3**: Fixed timestep physics loop driving GJK/EPA detection and Impulse solver updates.
*   **TC3.4**: Collision events triggering spatial sound effects (Hit.wav).
*   **TC3.5**: Controller tracking raycast intersecting with 3D Quad UI.
*   **TC3.6**: Bullet sphere colliding with target convex hull, updating score, and triggering sound.
*   **TC3.7**: Doppler calculations updating dynamically as sound source velocity changes.
*   **TC3.8**: glTF weapon model drawn in DX12 stereo buffer at controller tracked pose.

### Tier 4: Real-World Scenarios
Tests end-to-end integration and stability under realistic game workloads:
*   **Scenario 1: Falling Target with Bounce**: Dynamic sphere falling from 10m height, hitting a floor, GJK/EPA detecting collision, and Impulse solver bouncing it.
*   **Scenario 2: Variable Frame Rate Physics Stability**: Runs Scenario 1 at 30, 60, and 90 FPS rendering rates, proving physics runs at 100Hz fixed timestep and bounces at the exact same simulation time and height.
*   **Scenario 3: Moving Audio Source & Doppler Effect**: Sound source moving past listener at 20 m/s, asserting frequency increases during approach and decreases during departure.
*   **Scenario 4: Immediate UI Button Click**: Raycast hitting "Restart Game" button on 3D UI, verifying state and score resets.
*   **Scenario 5: Complete Game Shoot Loop**: Level loaded from glTF, target spawned, bullet fired, hit detected by GJK/EPA, target destroyed, score updated, and sound played.

---

## 4. CLI Command Mappings
The test runner invokes the engine binary (`VREngine.exe` or `Build/VREngine.exe`) using the following CLI entry points:

1.  **`--test-math`**
    *   *Purpose*: Validates linear algebra, quaternion SLERP, Euler View transformations.
    *   *Output*: stdout JSON/key-value output of results.
2.  **`--test-physics <fps> <duration>`**
    *   *Purpose*: Runs simulated physics of falling sphere hitting floor under a given frame rate (e.g. 30, 60, 90) and duration.
    *   *Output*: Log records of positions, velocities, and collision times.
3.  **`--test-audio <source_pos> <source_vel> <listener_pos> <listener_vel>`**
    *   *Purpose*: Runs Doppler, ITD, ILD calculations with source and listener state vectors.
    *   *Output*: Log records of calculated coefficients, delay times, and volume levels.
4.  **`--test-gltf <gltf_file_path>`**
    *   *Purpose*: Parses glTF 2.0 file, listing nodes, static colliders, and bounds.
    *   *Output*: Log/stdout listing parsed entities.
5.  **`--test-ui <ray_origin> <ray_dir>`**
    *   *Purpose*: Runs raycast intersection testing against the 3D Quad UI plane.
    *   *Output*: Interactive event logs (e.g., button hover/click trigger).
6.  **`--test-game`**
    *   *Purpose*: Simulates a full game loop sequence (spawning, shooting, bullet flight, collision, and scoring).
    *   *Output*: Complete game event timeline log.
