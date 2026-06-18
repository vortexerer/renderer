# Original User Request

## Initial Request — 2026-06-12T02:00:06Z

C++20, DirectX 12, OpenXR을 사용하여 외부 물리/오디오 미들웨어 없이 바닥부터 자체 작동하는 VR 게임 엔진 및 '스페셜 타깃 슈터' VR 게임의 핵심 컴포넌트를 설계하고 완벽하게 빌드 가능한 코드로 구현합니다.

Working directory: c:/CloudRing/DSHS/2026/활동/GameEngineDEV

Integrity mode: development

## Requirements

### R1. 핵심 VR 렌더링 파이프라인 (DX12 & OpenXR)
- [4.1 기본 변환.pdf] 및 [4.2 특별한 행렬 변환과 연산.pdf]의 식 4.21 오일러 뷰 변환($E(h, p, r) = R_z(r)R_x(p)R_y(h)$)을 연동하는 OpenXR 세션 및 트래킹 시스템 구축.
- DirectX 12의 Texture Array 및 `SV_RenderTargetArrayIndex`/`SV_ViewID`를 사용하는 **Instanced Stereo Rendering** 파이프라인 구현.

### R2. Cartesian 물리 엔진 및 GJK/EPA 충돌 해결기
- 직각좌표계 기준 변위($\vec{r}$), 속도($\vec{v}$), 가속도($\vec{a}$) 물리 관계를 다루는 Rigidbody 모듈 구현.
- 가변 프레임률 보정을 위한 **Fixed Timestep Accumulator** 루프 및 에너지 보존 성능이 우수한 **Semi-implicit Euler(반암시적 오일러)** 적분 구현.
- 볼록 다면체 간 정밀 충돌 검출을 위한 **GJK(Minkowski Difference Simplex) 및 EPA(Minkowski Polytope Expansion)** 충돌 해결 알고리즘과 충격량(Impulse) 물리 반응 구현.

### R3. WASAPI & HRTF 공간 음향 엔진
- WASAPI 배타적 모드 버퍼 스트리밍을 통한 최저지연 오디오 재생 루프 구현.
- 플레이어와 소리원의 상대 위치를 기반으로 시간 지연(ITD) 및 로우패스 필터 주파수 감쇠(ILD)를 적용하는 자체 입체음향 믹싱 및 HRTF 엔진 구현.

### R4. glTF 에셋 로더 & 3D 공간 UI (Immediate UI)
- 오픈소스 라이브러리 없이 glTF 2.0 JSON 및 이진 버퍼 디코딩을 수동으로 파싱하는 로더 구현.
- UI 화면을 오프스크린 텍스처로 렌더링한 후, 3D 공간 상의 사각형 쿼드(Quad)에 대입하여 상호작용하는 Immediate Mode UI 모듈 설계.

## Acceptance Criteria

### 빌드 및 파이프라인 연동
- [ ] CMake를 통해 Windows MSVC 환경에서 모든 C++ 코드 컴파일이 성공해야 함.
- [ ] DirectX 12 디버그 레이어 활성화 시 런타임 리소스 배리어, Descriptor Heap 바인딩 오류가 발생하지 않아야 함.

### 물리 및 기하 변환 검증
- [ ] 가변 프레임률(예: 30FPS vs 90FPS) 상황에서도 고정 시간 격자(Fixed Timestep)에 의해 강체의 낙하 속도 및 충돌 반응이 물리적으로 완전히 일관성을 유지해야 함.
- [ ] 사원수 회전(식 4.39, 식 4.46) 및 구면 선형 보간(SLERP, 식 4.53) 연산이 오일러 회전 변환의 짐벌락 현상 없이 부드러운 회전을 수행해야 함.
- [ ] GJK/EPA 검출을 통한 AABB, Sphere, Convex Hull 간 충돌 시 관통되지 않고 Impulse 반응을 보여야 함.

### 공간 오디오 검증
- [ ] 소리원 위치 변화에 따라 좌우 오디오 버퍼의 도플러 주파수 변조 및 ITD 시간 차이, ILD 음량 차이가 필터링을 통해 올바르게 반영되어 헤드폰으로 출력되어야 함.
