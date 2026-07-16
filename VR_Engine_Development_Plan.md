# C++ 기반 고성능 VR 게임 엔진 자체 개발 계획 및 기술 명세서
**프로젝트명**: VR Game Engine from Scratch (C++, DirectX 12, OpenXR)
**학업 연계 및 세특 목표**: 삼차원 기하 대수학(오일러각 & 사원수), 컴퓨터 물리학(Cartesian 강체 역학 & 이산화 적분), 저수준 정보과학(DirectX 12 & OpenXR 시스템 프로그래밍)

본 기술 명세서는 외부 상용 게임 엔진(Unity, Unreal)을 배제하고, C++20과 DirectX 12, OpenXR 표준 규격을 기반으로 실시간 3차원 가상현실 공간을 효율적으로 제어하고 렌더링하는 **VR 게임 개발 엔진(VR Game Engine)**의 핵심 코어를 설계 및 제작하기 위한 계획서입니다. 동봉된 데모 씬(타깃 슈터 루프)은 엔진의 각 서브시스템(렌더러, 물리, 오디오, UI)의 결합성과 작동 정합성을 실증 검증하기 위한 테스트 벤치마크 도구입니다.

---

## 1. 엔진 설계 철학 및 아키텍처 목표

1. **저수준 하드웨어 직접 제어 역량**: 그래픽스 드라이버와 직접 소통하는 **DirectX 12 API** 및 VR 헤드셋 표준 하드웨어 인터페이스인 **OpenXR API**를 직접 다루어 컴퓨터 그래픽스 하드웨어의 파이프라인 작동 메커니즘을 저수준에서 규명합니다.
2. **모듈식 설계와 관심사 분리 (Decoupling)**: 렌더러, 물리 solver, 오디오 믹서, UI 레이아웃 엔진을 각각 독립적인 라이브러리 형태로 모듈화하고, 상호 의존성을 최소화하여 결합도를 극도로 낮춘 클린 엔진 코어를 설계합니다.
3. **수치해석학적 안정성 및 정밀성**: 교과 기하 수학 교안인 **[4.1 기본 변환]**, **[4.2 특별한 행렬 변환과 연산]**, **[4.3 사원수]**의 회전 기하 연산식과 물리학의 뉴턴 역학 미분 방정식을 컴퓨터 언어로 정밀하게 이산화(Discretization)하여 시뮬레이션의 물리적/기하적 정확성을 영속적으로 확보합니다.

---

## 2. 엔진 디렉터리 폴더 구조 (Engine Directory Structure)

```text
GameEngineDEV/
├── CMakeLists.txt          # 전체 빌드 설정 (VREngineCore 라이브러리 + 샘플 앱)
├── Source/                 # 엔진 코어 정적 라이브러리 (VREngineCore, 게임 비종속)
│   ├── Core/               # 엔진의 핵심 기반 시스템 (시간 및 로그)
│   │   ├── Engine.cpp/h    # 전체 서브시스템(물리, 렌더러 등) 초기화 및 동기화 관리
│   │   ├── IGame.h         # 엔진-게임 경계 인터페이스 (게임 모듈 주입점)
│   │   ├── Timer.cpp/h     # QueryPerformanceCounter 기반 정밀 델타 타임 측정
│   │   └── Logger.cpp/h    # std::source_location 기반 비동기 스레드 안전 파일 로깅
│   ├── Math/               # 삼차원 공간 변환 대수학 (교과 기하 수학 연계)
│   │   ├── Vector3.h       # Cartesian 3차원 벡터 구조체 및 연산자 오버로딩
│   │   ├── Matrix4x4.h     # 아핀 변환(T, R, S) 및 법선 변환(역의 전치) 행렬 구현
│   │   └── Quaternion.cpp/h # Hamilton 사원수 수학 및 SLERP 구면 선형 보간 구현
│   ├── Platform/           # OS 및 VR 디바이스 추상화
│   │   ├── Win32Window.cpp/h # Win32 창 생성 및 메시지 핸들링
│   │   └── OpenXRManager.cpp/h # OpenXR 그래픽 바인딩 및 HMD/컨트롤러 예측 Pose 트래킹
│   ├── Renderer/           # DirectX 12 렌더링 엔진
│   │   ├── DX12Renderer.cpp/h # Device, Swapchain, Command List 리소스 배리어 관리
│   │   ├── PipelineState.cpp/h # Graphics PSO 및 Root Signature 캡슐화
│   │   └── ConstantBuffer.h  # 셰이더 상수 버퍼 정의
│   ├── Physics/            # 수치해석 기반 자체 제작 3D 물리 엔진
│   │   ├── PhysicsEngine.cpp/h # Fixed Timestep Accumulator 루프 및 적분기 스케줄러
│   │   ├── Rigidbody.cpp/h     # 강체의 질량, Cartesian 변위(r), 속도(v), 가속도(a) 데이터
│   │   ├── Colliders.cpp/h     # 충돌 형태 정의 (AABB, Sphere, OBB)
│   │   └── GJK_EPA.cpp/h       # Minkowski Difference 기반 GJK & EPA 충돌 해결 알고리즘
│   ├── Audio/              # 자체 제작 3D 공간 음향 엔진
│   │   ├── AudioEngine.cpp/h   # WASAPI Exclusive 버퍼 제어 및 공간 믹서 스레드
│   │   ├── AudioSource.cpp/h   # 입체 음향 주파수 변조 및 방출 데이터
│   │   └── HRTFFilter.cpp/h    # ITD(시간차)/ILD(음량차) 머리전달함수 보간 필터
│   ├── Asset/              # 에셋 파이프라인
│   │   ├── AssetLoader.cpp/h   # glTF 2.0 JSON 파서 및 바이너리 언패킹 수동 디코더
│   │   └── TextureLoader.cpp/h # BMP 수동 파일 디코더
│   └── UI/                 # 자체 Immediate UI 시스템
│       ├── ImmediateUI.cpp/h  # 텍스트, 버튼 드로우 리스트 및 오프스크린 렌더 타깃 매핑
│       └── FontRenderer.cpp/h # SDF 기반 폰트 셰이더 및 텍스처 아틀라스 변환
├── Samples/
│   └── TargetShooter/      # VREngineCore를 링크하는 샘플 게임 (엔진 검증용 데모)
│       ├── Main.cpp           # VR 앱 진입점 (WinMain) — GameWorld를 엔진에 주입
│       ├── TestMain.cpp       # E2E CLI 테스트 하니스 진입점 (main)
│       ├── GameWorld.cpp/h    # IGame 구현체: 레벨 씬 로드 및 노드 파싱 관리
│       ├── PlayerController.cpp/h # OpenXR 조준/발사 입력 매핑 스크립트
│       ├── TargetObject.cpp/h # 타깃 충돌 이벤트 감지 및 스코어 갱신 스크립트
│       └── TargetRotator.cpp/h # 타깃 회전 연출 컴포넌트
└── Shaders/                # HLSL 셰이더 코드
```

---

## 3. 기하 대수학 모듈 설계 (고급수학Ⅰ 교과 연계)

### 3.1. [4.1 기본 변환] & [4.2.1 오일러 변환] 행렬 변환 설계
3차원 아핀 변환은 비가환성($MN \neq NM$)을 가집니다. 따라서 행렬 곱셈 순서가 엄격히 준수되어야 합니다. 엔진의 버텍스 변환은 오른손 좌표계 열 우선 행렬 기준으로 **식 4.17** 강체 변환 형태를 취합니다.

$$
X = T(t)R = \begin{bmatrix}
r_{00} & r_{01} & r_{02} & t_x \\
r_{10} & r_{11} & r_{12} & t_y \\
r_{20} & r_{21} & r_{22} & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
$$

오일러 변환은 헤드/요($h$), 피치($p$), 롤($r$) 회전의 연속 곱으로 계산되며, **식 4.21**을 코드에 대입하여 다음과 같이 3D 오일러 회전 행렬 $E$를 구합니다.

$$
E(h, p, r) = R_z(r)R_x(p)R_y(h)
$$

### 3.2. 짐벌락(Gimbal Lock) 특이점 분석 유도
**[4.2.2 오일러 변환으로부터 매개변수 추출]**에 의거하여, 피치각이 특이 한계인 **$p = 90^\circ$ ($\sin p = 1, \cos p = 0$)**이 되는 물리적 상태가 발생하면, 유도된 복합 회전 행렬 $E$는 다음과 같이 전개됩니다.

$$
E(h, 90^\circ, r) = \begin{bmatrix}
\cos r\cos h - \sin r\sin h & 0 & \cos r\sin h + \sin r\cos h \\
\sin r\cos h + \cos r\sin h & 0 & \sin r\sin h - \cos r\cos h \\
0 & 1 & 0
\end{bmatrix}
$$

삼각함수의 덧셈정리(차 공식)를 적용하여 행렬의 각 성분을 결합각 $(r + h)$에 관한 식으로 정리합니다.

$$
E(h, 90^\circ, r) = \begin{bmatrix}
\cos(r + h) & 0 & \sin(r + h) \\
\sin(r + h) & 0 & -\cos(r + h) \\
0 & 1 & 0
\end{bmatrix}
$$

*   **수학적 짐벌락 규명**: 행렬의 각 성분이 헤드($h$)와 롤($r$) 개별 값에 무관하게 오직 합산각인 $(r+h)$ 에 의해서만 지배당하게 됨으로써, 3차원 회전 공간 상에서 고유한 물리 회전축 하나가 소실(자유도 상실, Rank=2 축소)됩니다. 이 한계를 극복하기 위해 엔진은 내부 회전 단위를 사원수로 설계합니다.

### 3.3. [4.3 사원수] 회전 모듈 설계 및 구면 선형 보간 (SLERP)
짐벌락이 없는 4차원 구면 제어를 위해 실수부 $q_w$와 허수부 $\mathbf{q}_v = (q_x, q_y, q_z)$로 구성된 Hamilton 사원수 $\hat{q}$를 구현합니다.

$$
\hat{q} = (\mathbf{q}_v, q_w) = iq_x + jq_y + kq_z + q_w
$$

회전각의 합성은 **식 4.32 (해밀턴 곱)**에 따라 외적과 내적을 활용해 결합됩니다.

$$
\hat{q}\hat{r} = (\mathbf{q}_v \times \mathbf{r}_v + r_w\mathbf{q}_v + q_w\mathbf{r}_v,\; q_w r_w - \mathbf{q}_v \cdot \mathbf{r}_v)
$$

두 회전 사원수 사이를 등속 회전으로 구면 보간하기 위해 **식 4.53**을 그대로 코드에 구현합니다.

$$
\text{slerp}(\hat{q}, \hat{r}, t) = \frac{\sin(\phi(1-t))}{\sin\phi}\hat{q} + \frac{\sin(\phi t)}{\sin\phi}\hat{r} \quad (\cos\phi = q_x r_x + q_y r_y + q_z r_z + q_w r_w)
$$

---

## 4. 수치해석 기반 물리 엔진 설계 (고급물리학 교과 연계)

### 4.1. Cartesian 직각좌표계 기준 이산 역학 분석
물체(강체)의 가수가 변하는 기하 운동 법칙을 계산하기 위해 Cartesian 직각좌표계를 바탕으로 3차원 위치 벡터 $\vec{r}$, 속도 벡터 $\vec{v}$, 가속도 벡터 $\vec{a}$를 선언합니다.
뉴턴의 제2법칙 $\vec{F} = m\vec{a}$로부터 가속도 미분 방정식을 컴퓨터에서 연산 가능하도록 이산화합니다.

### 4.2. 프레임 독립성 제어를 위한 Fixed Timestep Accumulator
렌더링 성능 편차로 발생하는 프레임 레이트 스터터링(Stuttering) 현상 발생 시, 가변 델타 타임에 의한 물리 오차 누적과 터널링(Tunneling, 장애물 뚫림) 현상을 방어하기 위해 고정 격자 시간 누적기 루프를 엔진에 내장합니다.

```cpp
void PhysicsEngine::Update(double frameTime) {
    m_Accumulator += frameTime;
    while (m_Accumulator >= m_FixedTimeStep) {
        FixedUpdate(m_FixedTimeStep); // 언제나 동일한 보폭(dt = 0.01초)으로 상태 물리 업데이트
        m_Accumulator -= m_FixedTimeStep;
    }
}
```

### 4.3. 반암시적 오일러(Semi-implicit Euler) 적분기 및 에너지 보존
일반 명시적 오일러법 대비 에너지 보존율이 뛰어나 시뮬레이션 붕괴를 억제하는 심플렉틱 수치 적분 공식을 적용합니다.

$$
\vec{v}_{n+1} = \vec{v}_n + \vec{a}_n \Delta t
$$

$$
\vec{r}_{n+1} = \vec{r}_n + \vec{v}_{n+1} \Delta t \quad (\text{새로 갱신된 속도 } \vec{v}_{n+1}\text{을 입력으로 변위 계산})
$$

### 4.4. GJK 및 EPA 충돌 해결 알고리즘
강체의 정밀 3D 충돌 감지를 위해 볼록 다면체의 민코프스키 차($A \ominus B$)가 원점을 포함하는지 판별하는 GJK 알고리즘과, 원점과 충돌체의 최단 평면을 찾아내 충돌 법선 및 침투 깊이를 연산하는 EPA 확장 구조를 순수 C++ 알고리즘으로 빌드합니다.

---

## 5. 공간 음향 엔진 설계 (파동물리학 연계)

### 5.1. WASAPI Exclusive Mode 버퍼 스트리밍
Windows 오디오 믹서를 우회하여 최저지연 버퍼 제어를 위해 WASAPI 배타적 모드 세션을 초기화하고 비동기 오디오 스트리밍 스레드를 구현합니다.

### 5.2. HRTF 입체 음향 믹싱 및 도플러 효과
파동학의 소리 전달 시간차를 컴퓨터 공간에 구현하기 위해 다음 물리식을 믹싱 스레드에서 실시간 계산합니다.
*   **ITD (시간 차)**: 두 귀의 격차 거리 $d_w$에 따른 음원의 시간 도달 지연을 계산해 왼쪽/오른쪽 채널 버퍼를 지연 버퍼 라인에 할당.
*   **ILD (음량 차)**: 소리가 머리에 차단되는 회절 현상을 모사하기 위해 각도에 비례한 Low-Pass IIR 필터를 Contralateral(반대쪽) 버퍼에 적용.
*   **도플러 효과**: 음원과 청취자 간의 상대 속도를 연산하여, 주파수 변조 펙터를 사운드 재생 피치에 곱해 피치 변형을 유도.

---

## 6. 학업 역량 세부능력 및 특기사항 (세특) 기재 가이드

### 🔍 [정보기험 / 컴퓨터공학 세특 작성 방향]
> "상용 엔진(유니티, 언리얼)에 의존하는 개발 방식을 지양하고, 시스템 설계자(System Architect) 관점에서 C++20과 DirectX 12 저수준 API, OpenXR 규격을 수동 제어하는 VR 게임 엔진을 자체 제작함. 모듈 간의 낮은 결합도(Decoupling)를 유지하기 위해 Core, Math, Physics, Audio 서브시스템의 엄격한 상호작용 규칙(Interface Contracts)을 설계하고 E2E 자동 테스트 검증 인프라를 활용하여 엔진의 전체 작동 무결성을 정합성 있게 제어하는 소프트웨어 엔지니어링 역량을 보여줌."

### 📐 [고급수학Ⅰ 세특 작성 방향]
> "공간 변환 학습을 연계하여 3D 오일러 뷰 변환 행렬(식 4.21)을 유도하고, 특정 각도($90^\circ$)에서 yaw와 roll이 하나의 결합각으로 상쇄되어 3차원 회전축 중 1차원 자유도가 무손실 변환되지 않는 짐벌락 현상을 수학적으로 명밀히 논증함. 이를 해결하고자 기하 대수학 자료인 해밀턴 사원수 곱(식 4.32) 및 구면 선형 보간(SLERP, 식 4.53) 연산 구조체를 C++로 직접 정밀 설계함. 나아가 비균등 스케일링이 가해질 때 법선 벡터의 기하학적 수직 관계 보존을 위해 역의 전치 행렬 $A = (M^{-1})^T$ 변환 모듈을 수학적으로 도출하여 기하 엔진을 최적화함."

### 🍎 [고급물리학 세특 작성 방향]
> "고전 역학의 미분방정식을 컴퓨터 공간에 이식하기 위해 Cartesian 좌표계 상의 물리량을 매핑하고 이산화함. 컴퓨터 성능 차이로 인한 가변 프레임률 델타 타임이 물리 시뮬레이션의 터널링 오류와 에너지 폭주를 야기함을 규명하고, 물리와 렌더링 루프를 격리하는 Fixed Timestep Accumulator 루프를 설계함. 수치해석 기법으로 심플렉틱 반암시적 오일러 적분식을 코딩하여 에너지 상한 보존을 실증하고 볼록 다면체의 GJK/EPA 3D 충돌 solver를 직접 제작함. 또한 파동학의 음파 회절 및 전파 특성을 ITD/ILD 및 도플러 효과 수식으로 믹서 스레드 상에 합성하여 3D 공간 음향 기술을 융합하는 등 물리학과 수학, 컴퓨터 과학을 통합하는 우수한 역량을 입증함."
