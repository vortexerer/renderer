# DSHS VR Game Engine (C++26 & DirectX 12 & OpenXR)

이 프로젝트는 VR 플랫폼을 타겟으로 한 고성능 커스텀 게임 엔진을 **C++26** 표준 사양과 **DirectX 12 API**, **OpenXR 표준**을 활용하여 외부 미들웨어 없이 바닥부터 자체 개발하는 게임 개발 엔진 프로젝트입니다. 

대학 입시 수시전형 및 고등학교 학교생활기록부(생기부)의 **진로활동, 고급수학Ⅰ, 고급물리학** 교과 세부능력 및 특기사항(세특) 연계를 위한 학술적 수학·물리 구현 탐구를 담고 있습니다.

---

## ❖ 프로젝트 아키텍처 개요

본 엔진은 게임 콘텐츠 제작에 국한되지 않고, 고성능 가상 현실(VR) 구동을 위한 **8대 서브시스템 아키텍처**를 독자적으로 설계하는 것을 본질적인 목적으로 둡니다.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          DSHS Game Engine Pro                           │
├─────────────────┬─────────────────┬──────────────────┬──────────────────┤
│ VR Render Pipe  │ Cartesian Physics│ Spatial Audio    │ glTF Asset Loader│
│  (DirectX 12)   │   (GJK / EPA)   │ (WASAPI & HRTF)  │  (Custom Parser) │
├─────────────────┼─────────────────┼──────────────────┼──────────────────┤
│  OpenXR Session │ Component System│ Immediate 3D UI  │ Script Pipeline  │
│  (Input/Pose)   │  (GameObject)   │  (RenderToTex)   │  (TargetRotator) │
└─────────────────┴─────────────────┴──────────────────┴──────────────────┘
```

1. **핵심 VR 렌더링 파이프라인 (DX12 & OpenXR)**: Instanced Stereo Rendering 기법을 적용하여 한 번의 드로우 콜로 양안 텍스처 배열을 동시에 그려내며, 오일러 뷰 변환($E(h, p, r) = R_z(r)R_x(p)R_y(h)$)을 통한 HMD 시야각 변환을 연동합니다.
2. **자체 3D Cartesian 물리 엔진**: Semi-implicit Euler(반암시적 오일러) 적분을 이용해 가변 프레임률에서도 에너지 보존 법칙을 만족하는 강체 물리 루프를 구성하며, Convex Hull 간 충돌 정밀 검출을 위한 **GJK & EPA 알고리즘**을 구현합니다.
3. **공간 음향 믹서 및 HRTF**: WASAPI 배타적 모드 스트리밍과 도플러 효과, 시간지연(ITD) 및 로우패스 주파수 감쇄(ILD) 필터를 결합한 HRTF 공간 오디오 엔진을 탑재합니다.
4. **유니티 클론 에디터 시뮬레이터**: 로컬 C++ 컴파일 에뮬레이션 및 엔진 코어 동작의 무결성을 입증하기 위해, 비주얼 GUI 환경에서 오브젝트들을 계층적으로 다룰 수 있는 편집 도구를 포함합니다.

---

## 🎮 Unity-like 클론 에디터 조작법

프로젝트 루트의 `run_simulator.bat` 파일을 더블클릭하면 유니티 에디터를 모사한 전문적인 다크 톤 GUI 편집 환경이 실행됩니다.

| 조작 영역 | 마우스/키보드 입력 | 에디터 기능 설명 |
| :--- | :--- | :--- |
| **Scene View (3D)** | **마우스 좌클릭** | 마우스 광선(Ray)이 가리키는 3D 오브젝트 선택 |
| **Scene View (3D)** | **좌클릭 + 드래그** | 선택된 오브젝트를 3D 공간 상에서 끌고 다니며 자유롭게 이동 (Transform Translation) |
| **Scene View (3D)** | **우클릭 + 드래그** | 카메라 시야 회전 (Yaw / Pitch) |
| **Scene View (3D)** | **우클릭 한 채로 WASD / EQ** | 카메라 시선 방향 기준 공중 비행 이동 (수직 위/아래는 E/Q) |
| **Scene View (3D)** | **마우스 휠 스크롤** | 카메라 전후 줌인 / 줌아웃 |
| **Scene View (3D)** | **휠(가운데 클릭) + 드래그** | 카메라 평행 이동 (Panning) |
| **Scene View (3D)** | **우클릭 짧게 클릭 (탭)** | 우클릭 지점 레이캐스트 3D 스폰 메뉴 출력 |
| **Hierarchy** | **F2 단축키 / 더블클릭** | 선택된 오브젝트의 이름 변경 팝업 창 호출 (양방향 동기화) |
| **Hierarchy** | **드래그 앤 드롭** | 드래그한 오브젝트를 다른 오브젝트의 자식으로 드롭하여 계층 구조 설정 |
| **Inspector** | **값 변경 + Enter/FocusOut** | 위치(Pos), 회전(Euler), 크기(Scale), 물리(탄성/반경/속도), 색상 즉각 동기화 |
| **Toolbar** | **Fire Bullet 버튼 클릭** | 플레이어 위치에서 시선 방향으로 탄환 발사 (과녁 타격 물리 검증용) |

---

## 📂 주요 소스 코드 구조

- `Source/`: C++26 엔진 핵심 소스 코드
  - `Math/`: 사원수(Quaternion), 뷰 행렬(Matrix4x4) 등 3D 기하 수학
  - `Physics/`: Semi-implicit Euler 물리 적분 및 강체 물리 계산 엔진
- `Tests/`: 엔진 무결성 입증용 E2E 테스트 및 비주얼 에디터 시뮬레이터
  - `visual_engine_simulator.py`: 유니티 클론 에디터 GUI 프로그램
  - `run_tests.py`: 17개의 시나리오별 물리/수학 수치해석 자동화 검증 테스트
- `run_simulator.bat`: 로컬 환경 원클릭 에디터 런처 배치 파일
- `CMakeLists.txt`: C++26 표준 빌드 구성 프로파일

---

## 🛠️ 빌드 및 테스트

### Windows (전체 VR 엔진)
DirectX 12 / OpenXR / WASAPI를 사용하는 전체 엔진은 Windows + Visual Studio 2022 + Windows SDK 환경에서 빌드합니다.

```bat
cmake -B Build -S .
cmake --build Build --config Release
```

### Linux / macOS (크로스플랫폼 코어 검증)
플랫폼 종속 코드는 `#ifdef _WIN32`로 격리되어 있어, **렌더러/HMD가 없는 환경에서도 엔진 코어(수학·물리·공간오디오 DSP·에셋·UI 레이캐스트)를 실제 C++로 컴파일하여 E2E 수치 검증**을 수행할 수 있습니다. 빌드는 엔진 정적 라이브러리(`VREngineCore`, `Source/`)와 이를 링크하는 샘플 게임(`Samples/TargetShooter/`)으로 나뉘며, 샘플의 CLI 테스트 하니스(`TestMain.cpp`)가 `VREngine` 테스트 러너 실행파일이 됩니다.

```bash
cmake -B Build -S .
cmake --build Build -j

# 컴파일된 실제 엔진으로 17개 E2E 시나리오 검증 (미발견 시 Python mock으로 폴백)
python3 Tests/run_tests.py
```

> 참고: `run_tests.py`는 `Build/VREngine`(또는 Windows의 `Build/VREngine.exe`)를 자동 탐지합니다. 실행 바이너리가 stdout에 JSON 데이터를, stderr에 진단 로그를 출력하므로 테스트 파서와 충돌하지 않습니다.

---

## 📈 학술 탐구 및 생활기록부(세특) 연계 포인트

- **고급수학Ⅰ**: 복소수의 확장계인 사원수(Quaternion)의 대수 구조와 구면 선형 보간(SLERP) 공식을 구현하여, 오일러각의 한계인 짐벌락(Gimbal Lock) 현상을 방지하는 3D 구면 회전을 코드로 증명.
- **고급물리학**: 강체 변위($\vec{r}$), 속도($\vec{v}$), 가속도($\vec{a}$)의 물리 관계를 이산화하고, 에너지 손실이 없는 심플렉틱 반암시적 오일러 적분법(Semi-implicit Euler Integration)을 설계하여 수치적 안정성 분석.
- **진로활동**: 대화형 GUI 파이썬 시뮬레이터를 구축하여 3D 공간 상의 마우스 클릭 Raycast 좌표 변환과 부모-자식 트랜스폼 계층 구조(Hierarchy)의 행렬 합성 연산 구현.
