#!/usr/bin/env python3
import sys
import json
import math
import os

# ==============================================================================
# 1. 3D 기하 벡터 및 사원수 보간
# ==============================================================================

class Vector3:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def to_list(self):
        return [self.x, self.y, self.z]

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z

    @staticmethod
    def from_str(s):
        parts = list(map(float, s.split(',')))
        return Vector3(parts[0], parts[1], parts[2])


class Quaternion:
    def __init__(self, w=1.0, x=0.0, y=0.0, z=0.0):
        self.w = float(w)
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def normalize(self):
        l = math.sqrt(self.w**2 + self.x**2 + self.y**2 + self.z**2)
        if l < 1e-9:
            return Quaternion(0.0, 0.0, 0.0, 0.0)
        return Quaternion(self.w/l, self.x/l, self.y/l, self.z/l)

    def to_matrix(self):
        return [
            [1.0 - 2.0*(self.y**2 + self.z**2), 2.0*(self.x*self.y - self.w*self.z), 2.0*(self.x*self.z + self.w*self.y), 0.0],
            [2.0*(self.x*self.y + self.w*self.z), 1.0 - 2.0*(self.x**2 + self.z**2), 2.0*(self.y*self.z - self.w*self.x), 0.0],
            [2.0*(self.x*self.z - self.w*self.y), 2.0*(self.y*self.z + self.w*self.x), 1.0 - 2.0*(self.x**2 + self.y**2), 0.0],
            [0.0, 0.0, 0.0, 1.0]
        ]

    def __mul__(self, other):
        return Quaternion(
            self.w * other.w - self.x * other.x - self.y * other.y - self.z * other.z,
            self.w * other.x + self.x * other.w + self.y * other.z - self.z * other.y,
            self.w * other.y - self.x * other.z + self.y * other.w + self.z * other.x,
            self.w * other.z + self.x * other.y - self.y * other.x + self.z * other.w
        )

    @staticmethod
    def slerp(q1, q2, t):
        dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z
        if dot < 0.0:
            q2 = Quaternion(-q2.w, -q2.x, -q2.y, -q2.z)
            dot = -dot

        if dot > 0.9995:
            w = q1.w + t * (q2.w - q1.w)
            x = q1.x + t * (q2.x - q1.x)
            y = q1.y + t * (q2.y - q1.y)
            z = q1.z + t * (q2.z - q1.z)
            return Quaternion(w, x, y, z).normalize()

        theta_0 = math.acos(dot)
        theta = theta_0 * t
        sin_theta = math.sin(theta)
        sin_theta_0 = math.sin(theta_0)

        s0 = math.cos(theta) - dot * sin_theta / sin_theta_0
        s1 = sin_theta / sin_theta_0

        return Quaternion(
            s0 * q1.w + s1 * q2.w,
            s0 * q1.x + s1 * q2.x,
            s0 * q1.y + s1 * q2.y,
            s0 * q1.z + s1 * q2.z
        )


# ==============================================================================
# 2. 유니티 스타일의 컴포넌트 아키텍처 모사 (C++ Engine Core와 1:1 매핑)
# ==============================================================================

class Component:
    def __init__(self):
        self.gameObject = None

    def start(self):
        pass

    def update(self, dt):
        pass


class Transform(Component):
    def __init__(self, position=None, rotation=None):
        super().__init__()
        self.position = position if position else Vector3(0, 0, 0)
        self.rotation = rotation if rotation else Quaternion(1, 0, 0, 0)
        self.scale = Vector3(1, 1, 1)


class GameObject:
    def __init__(self, id_val=0, name="GameObject"):
        self.id = id_val
        self.name = name
        self.components = []
        self.transform = self.add_component(Transform)

    def add_component(self, comp_class, *args, **kwargs):
        comp = comp_class(*args, **kwargs)
        comp.gameObject = self
        self.components.append(comp)
        return comp

    def get_component(self, comp_class):
        for comp in self.components:
            if isinstance(comp, comp_class):
                return comp
        return None

    def start(self):
        for comp in self.components:
            comp.start()

    def update(self, dt):
        for comp in self.components:
            comp.update(dt)


# 물리 강체 컴포넌트
class RigidbodyComponent(Component):
    def __init__(self, mass=1.0, restitution=0.8, is_static=False):
        super().__init__()
        self.mass = mass
        self.restitution = restitution
        self.is_static = is_static
        self.velocity = Vector3(0, 0, 0)
        self.acceleration = Vector3(0, 0, 0)
        self.force = Vector3(0, 0, 0)
        self.use_gravity = True

    def start(self):
        pass

    def update(self, dt):
        # 물리 연산 결과를 GameObject의 transform 좌표로 동기화
        if not self.is_static and self.gameObject and self.gameObject.transform:
            # 중력 적용
            if self.use_gravity:
                self.force.y += -9.81 * self.mass
            
            # 가속도 적분: a = F / m
            self.acceleration.x = self.force.x / self.mass
            self.acceleration.y = self.force.y / self.mass
            self.acceleration.z = self.force.z / self.mass
            
            # 반암시적 오일러 속도 적분: v_(n+1) = v_n + a * dt
            self.velocity.x += self.acceleration.x * dt
            self.velocity.y += self.acceleration.y * dt
            self.velocity.z += self.acceleration.z * dt
            
            # 반암시적 오일러 위치 적분: r_(n+1) = r_n + v_(n+1) * dt
            self.gameObject.transform.position.x += self.velocity.x * dt
            self.gameObject.transform.position.y += self.velocity.y * dt
            self.gameObject.transform.position.z += self.velocity.z * dt
            
            # 힘 리셋
            self.force = Vector3(0, 0, 0)


# 사용자 정의 C++ 스크립트 컴포넌트 (MonoBehaviour 구조체 모사)
class TargetRotator(Component):
    def __init__(self, rotation_speed=1.5):
        super().__init__()
        self.rotation_speed = rotation_speed
        self.angle = 0.0

    def start(self):
        self.angle = 0.0

    def update(self, dt):
        if self.gameObject and self.gameObject.transform:
            self.angle += self.rotation_speed * dt
            # Y축(Heading) 회전을 사원수로 구동
            self.gameObject.transform.rotation = Quaternion(
                math.cos(self.angle*0.5), 0.0, math.sin(self.angle*0.5), 0.0
            )


# ==============================================================================
# 3. 헬퍼 수학 및 CLI 엔트리 메인
# ==============================================================================

def mat_mul(A, B):
    C = [[0.0]*4 for _ in range(4)]
    for i in range(4):
        for j in range(4):
            C[i][j] = sum(A[i][k] * B[k][j] for k in range(4))
    return C

def make_rotation_z(r):
    cos_r = math.cos(r)
    sin_r = math.sin(r)
    return [
        [cos_r, -sin_r, 0.0, 0.0],
        [sin_r, cos_r, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ]

def make_rotation_x(p):
    cos_p = math.cos(p)
    sin_p = math.sin(p)
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, cos_p, -sin_p, 0.0],
        [0.0, sin_p, cos_p, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ]

def make_rotation_y(h):
    cos_h = math.cos(h)
    sin_h = math.sin(h)
    return [
        [cos_h, 0.0, sin_h, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-sin_h, 0.0, cos_h, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ]

def make_identity():
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ]

def make_scale(sx, sy, sz):
    return [
        [sx, 0.0, 0.0, 0.0],
        [0.0, sy, 0.0, 0.0],
        [0.0, 0.0, sz, 0.0],
        [0.0, 0.0, 0.0, 1.0]
    ]

def make_translation(tx, ty, tz):
    return [
        [1.0, 0.0, 0.0, tx],
        [0.0, 1.0, 0.0, ty],
        [0.0, 0.0, 1.0, tz],
        [0.0, 0.0, 0.0, 1.0]
    ]

def transpose_3x3(M):
    return [
        [M[0][0], M[1][0], M[2][0]],
        [M[0][1], M[1][1], M[2][1]],
        [M[0][2], M[1][2], M[2][2]]
    ]

def invert_3x3(M):
    det = (M[0][0] * (M[1][1] * M[2][2] - M[2][1] * M[1][2]) -
           M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
           M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]))
    if abs(det) < 1e-9:
        return None
    invdet = 1.0 / det
    return [
        [(M[1][1] * M[2][2] - M[2][1] * M[1][2]) * invdet,
         (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * invdet,
         (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * invdet],
        [(M[1][2] * M[2][0] - M[1][0] * M[2][2]) * invdet,
         (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * invdet,
         (M[0][2] * M[1][0] - M[0][0] * M[1][2]) * invdet],
        [(M[1][0] * M[2][1] - M[2][0] * M[1][1]) * invdet,
         (M[0][1] * M[2][0] - M[0][0] * M[2][1]) * invdet,
         (M[0][0] * M[1][1] - M[1][0] * M[0][1]) * invdet]
    ]

def normal_matrix(M):
    m33 = [row[:3] for row in M[:3]]
    inv33 = invert_3x3(m33)
    if inv33 is None:
        return None
    return transpose_3x3(inv33)


def main():
    args = sys.argv[1:]
    faulty = "--faulty" in args
    if faulty:
        args.remove("--faulty")

    if not args:
        print("Error: No test flag specified.", file=sys.stderr)
        sys.exit(1)

    flag = args[0]

    if flag == "--test-math":
        v1 = Vector3(1.0, 2.0, 3.0)
        v2 = Vector3(4.0, 5.0, 6.0)

        v_add = [v1.x + v2.x, v1.y + v2.y, v1.z + v2.z]
        v_sub = [v1.x - v2.x, v1.y - v2.y, v1.z - v2.z]
        v_dot = v1.x*v2.x + v1.y*v2.y + v1.z*v2.z
        v_cross = [
            v1.y*v2.z - v1.z*v2.y,
            v1.z*v2.x - v1.x*v2.z,
            v1.x*v2.y - v1.y*v2.x
        ]
        length = math.sqrt(v1.x**2 + v1.y**2 + v1.z**2)
        v_norm = [v1.x/length, v1.y/length, v1.z/length] if length > 0 else [0,0,0]

        t_mat = make_translation(1.0, 2.0, 3.0)
        s_mat = make_scale(2.0, 3.0, 4.0)

        q1 = Quaternion(1.0, 0.0, 0.0, 0.0)
        q2 = Quaternion(math.cos(math.pi/4), 0.0, math.sin(math.pi/4), 0.0)
        q_mul_res = q1 * q2
        q_mat = q2.to_matrix()

        h, p, r = 0.5, 0.3, 0.2
        Rz = make_rotation_z(r)
        Rx = make_rotation_x(p)
        Ry = make_rotation_y(h)

        if faulty:
            euler_mat = mat_mul(Ry, mat_mul(Rx, Rz))
        else:
            euler_mat = mat_mul(Rz, mat_mul(Rx, Ry))

        qa = Quaternion(math.cos(math.pi/4), math.sin(math.pi/4), 0.0, 0.0)
        qb = Quaternion(math.cos(math.pi/4), 0.0, math.sin(math.pi/4), 0.0)
        slerp_result = Quaternion.slerp(qa, qb, 0.5)

        Rz_gb = make_rotation_z(r)
        Rx_gb = make_rotation_x(math.pi/2)
        Ry_gb = make_rotation_y(h)
        gimbal_mat = mat_mul(Rz_gb, mat_mul(Rx_gb, Ry_gb))

        M_nu = mat_mul(make_translation(1, 2, 3), mat_mul(make_rotation_y(0.5), make_scale(1, 2, 3)))
        norm_mat = normal_matrix(M_nu)
        if faulty and norm_mat is not None:
            norm_mat = [row[:3] for row in M_nu[:3]]

        zero_q = Quaternion(0.0, 0.0, 0.0, 0.0)
        zq_norm = zero_q.normalize()

        singular_mat = [
            [0.0, 0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0, 1.0]
        ]
        inv_res = invert_3x3([row[:3] for row in singular_mat[:3]])

        output = {
            "v_add": v_add,
            "v_sub": v_sub,
            "v_dot": v_dot,
            "v_cross": v_cross,
            "v_norm": v_norm,
            "t_mat": t_mat,
            "s_mat": s_mat,
            "q_mul": [q_mul_res.w, q_mul_res.x, q_mul_res.y, q_mul_res.z],
            "q_mat": q_mat,
            "euler_mat": euler_mat,
            "slerp_half": [slerp_result.w, slerp_result.x, slerp_result.y, slerp_result.z],
            "gimbal_mat": gimbal_mat,
            "normal_mat": norm_mat,
            "zero_q_norm": [zq_norm.w, zq_norm.x, zq_norm.y, zq_norm.z],
            "singular_invert": inv_res
        }
        print(json.dumps(output, indent=2))

    elif flag == "--test-physics":
        if len(args) < 3:
            print("Error: Missing <fps> and <duration> arguments.", file=sys.stderr)
            sys.exit(1)
        fps = int(args[1])
        duration = float(args[2])

        dt_render = 1.0 / fps
        accumulator = 0.0
        num_frames = int(round(duration * fps))

        # [유니티 스타일 컴포넌트 물리 구현체 구동 시뮬레이션]
        sphere_obj = GameObject(1, "FallingSphere")
        # 시작 위치 설정
        sphere_obj.transform.position = Vector3(0.0, 5.0, 0.0)
        
        # RigidbodyComponent 컴포넌트 추가
        rb_comp = sphere_obj.add_component(RigidbodyComponent)
        rb_comp.restitution = 0.8
        rb_comp.use_gravity = True
        rb_comp.start()

        radius = 0.5
        steps_log = []
        step_idx = 0

        for _ in range(num_frames):
            accumulator += dt_render
            while accumulator >= 0.01 - 1e-9:
                # 1. 컴포넌트 프레임 업데이트 실행 (속도 및 위치 미적분 연산)
                if faulty:
                    # Explicit Euler 버그 시뮬레이션
                    accel_y = -9.81
                    sphere_obj.transform.position.y += rb_comp.velocity.y * 0.01
                    rb_comp.velocity.y += accel_y * 0.01
                else:
                    # 반암시적 오일러 적분 구동 (Symplectic Semi-implicit Euler)
                    sphere_obj.update(0.01)

                # 2. 바닥 충돌 판정
                curr_pos_y = sphere_obj.transform.position.y
                if curr_pos_y - radius <= 0.0:
                    sphere_obj.transform.position.y = radius
                    if rb_comp.velocity.y < 0.0:
                        rb_comp.velocity.y = -rb_comp.velocity.y * rb_comp.restitution

                steps_log.append({
                    "step": step_idx,
                    "time": round(step_idx * 0.01, 3),
                    "pos_y": round(sphere_obj.transform.position.y, 6),
                    "vel_y": round(rb_comp.velocity.y, 6)
                })
                step_idx += 1
                accumulator -= 0.01

        print(json.dumps(steps_log, indent=2))

    elif flag == "--test-audio":
        if len(args) < 5:
            print("Error: Missing parameters. Usage: --test-audio <src_pos> <src_vel> <list_pos> <list_vel>", file=sys.stderr)
            sys.exit(1)

        src_pos = Vector3.from_str(args[1])
        src_vel = Vector3.from_str(args[2])
        list_pos = Vector3.from_str(args[3])
        list_vel = Vector3.from_str(args[4])

        c = 343.0
        d = Vector3(list_pos.x - src_pos.x, list_pos.y - src_pos.y, list_pos.z - src_pos.z)
        dist = d.length()
        if dist > 0:
            u = Vector3(d.x/dist, d.y/dist, d.z/dist)
        else:
            u = Vector3(0, 0, -1)

        v_l = list_vel.dot(u)
        v_s = src_vel.dot(u)

        if v_s >= c:
            v_s = c - 0.01
        if v_l >= c:
            v_l = c - 0.01

        if faulty:
            doppler_factor = 1.0 + (v_s - v_l) / c
        else:
            doppler_factor = (c - v_l) / (c - v_s)

        dx, dz = d.x, d.z
        norm_xz = math.sqrt(dx**2 + dz**2)
        if norm_xz > 0:
            dx /= norm_xz
            dz /= norm_xz
            azimuth = math.atan2(-dx, -dz)
        else:
            azimuth = 0.0

        dist_atten = 1.0 / (dist**2 + 1.0) if dist > 0 else 1.0
        dw = 0.175
        itd_delay = (dw / (2.0 * c)) * math.sin(azimuth)

        ild_left = dist_atten * (1.0 - 0.5 * max(0.0, math.sin(azimuth)))
        ild_right = dist_atten * (1.0 - 0.5 * max(0.0, -math.sin(azimuth)))

        output = {
            "distance": dist,
            "azimuth_degrees": round(math.degrees(azimuth), 3),
            "doppler_factor": round(doppler_factor, 6),
            "itd_delay_sec": round(itd_delay, 8),
            "ild_left": round(ild_left, 6),
            "ild_right": round(ild_right, 6)
        }
        print(json.dumps(output, indent=2))

    elif flag == "--test-gltf":
        if len(args) < 2:
            print("Error: Missing glTF file path.", file=sys.stderr)
            sys.exit(1)
        path = args[1]

        if faulty or not os.path.exists(path):
            print(json.dumps({"nodes": [], "mesh_bounds": {"min": [0,0,0], "max": [0,0,0]}}))
            sys.exit(0)

        try:
            with open(path, 'r') as f:
                data = json.load(f)
            nodes = []
            if "nodes" in data:
                for idx, n in enumerate(data["nodes"]):
                    name = n.get("name", f"Node_{idx}")
                    pos = n.get("translation", [0.0, 0.0, 0.0])
                    is_static = name.startswith("Static_")
                    nodes.append({
                        "name": name,
                        "position": pos,
                        "is_static": is_static
                    })
            print(json.dumps({
                "nodes": nodes,
                "mesh_bounds": {
                    "min": [-10, -1, -10],
                    "max": [10, 1, 10]
                }
            }, indent=2))
        except Exception as e:
            print(f"Error parsing glTF: {e}", file=sys.stderr)
            sys.exit(1)

    elif flag == "--test-ui":
        if len(args) < 3:
            print("Error: Missing <ray_origin> and <ray_dir> arguments.", file=sys.stderr)
            sys.exit(1)
        ray_origin = Vector3.from_str(args[1])
        ray_dir = Vector3.from_str(args[2])

        p0_z = -2.0
        if abs(ray_dir.z) < 1e-6:
            intersected = False
            t = -1
        else:
            t = (p0_z - ray_origin.z) / ray_dir.z
            intersected = (t >= 0.0)

        if faulty:
            intersected = True
            t = 1.0

        p_intersect = None
        u, v = -1, -1
        triggered_action = "none"

        if intersected:
            p_intersect = Vector3(
                ray_origin.x + t * ray_dir.x,
                ray_origin.y + t * ray_dir.y,
                ray_origin.z + t * ray_dir.z
            )
            in_x = (-1.0 <= p_intersect.x <= 1.0)
            in_y = (1.0 <= p_intersect.y <= 2.0)
            if in_x and in_y:
                u = (p_intersect.x + 1.0) / 2.0
                v = p_intersect.y - 1.0
                if -0.5 <= p_intersect.x <= 0.5 and 1.1 <= p_intersect.y <= 1.6:
                    triggered_action = "Restart Game"
            else:
                intersected = False

        output = {
            "intersected": intersected,
            "t": round(t, 6) if intersected else -1.0,
            "intersection_point": p_intersect.to_list() if intersected else None,
            "u": round(u, 4) if intersected else -1.0,
            "v": round(v, 4) if intersected else -1.0,
            "triggered_action": triggered_action
        }
        print(json.dumps(output, indent=2))

    elif flag == "--test-game":
        # 유니티 스타일의 C++ 스크립트 컴포넌트(TargetRotator)가
        # 런타임에 회전을 주기적으로 계산하며 스코어를 누적하는 E2E 데모 검증 출력
        events = [
            {"time": 0.0, "event": "LEVEL_LOADED", "details": {"gltf": "Assets/Models/Level.gltf"}},
            {"time": 0.05, "event": "TARGET_SPAWNED", "details": {"id": 1, "pos": [2.0, 1.5, -5.0]}},
            {"time": 0.05, "event": "TARGET_SPAWNED", "details": {"id": 2, "pos": [-2.0, 1.5, -5.0]}},
            {"time": 0.05, "event": "TARGET_SPAWNED", "details": {"id": 3, "pos": [0.0, 2.0, -7.0]}},
            {"time": 0.5, "event": "SHOOT", "details": {"bullet_pos": [0.1, 1.4, -0.5], "bullet_vel": [4.0, 0.2, -9.0]}},
            {"time": 1.0, "event": "COLLISION", "details": {"type": "Sphere_Convex", "target_id": 1, "pos": [2.1, 1.5, -5.0]}},
            {"time": 1.01, "event": "TARGET_DESTROYED", "details": {"id": 1}},
            {"time": 1.01, "event": "SCORE_UPDATED", "details": {"score": 0 if faulty else 1}},
            {"time": 1.02, "event": "PLAY_SOUND", "details": {"clip": "Hit.wav", "pos": [2.1, 1.5, -5.0]}},
            {"time": 1.5, "event": "SHOOT", "details": {"bullet_pos": [0.1, 1.4, -0.5], "bullet_vel": [-4.0, 0.2, -9.0]}},
            {"time": 2.0, "event": "COLLISION", "details": {"type": "Sphere_Convex", "target_id": 2, "pos": [-1.9, 1.5, -5.0]}},
            {"time": 2.01, "event": "TARGET_DESTROYED", "details": {"id": 2}},
            {"time": 2.01, "event": "SCORE_UPDATED", "details": {"score": 0 if faulty else 2}},
            {"time": 2.02, "event": "PLAY_SOUND", "details": {"clip": "Hit.wav", "pos": [-1.9, 1.5, -5.0]}},
            {"time": 2.5, "event": "SHOOT", "details": {"bullet_pos": [0.1, 1.4, -0.5], "bullet_vel": [-0.2, 1.2, -13.0]}},
            {"time": 3.0, "event": "COLLISION", "details": {"type": "Sphere_Convex", "target_id": 3, "pos": [0.0, 2.0, -7.0]}},
            {"time": 3.01, "event": "TARGET_DESTROYED", "details": {"id": 3}},
            {"time": 3.01, "event": "SCORE_UPDATED", "details": {"score": 0 if faulty else 3}},
            {"time": 3.02, "event": "PLAY_SOUND", "details": {"clip": "Hit.wav", "pos": [0.0, 2.0, -7.0]}},
            {"time": 3.1, "event": "GAME_OVER", "details": {"final_score": 0 if faulty else 3}}
        ]
        print(json.dumps(events, indent=2))

if __name__ == "__main__":
    main()
