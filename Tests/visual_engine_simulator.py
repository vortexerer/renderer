#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import math
import time
import winsound
import threading

# ==============================================================================
# 1. 3D 기하 수학 및 사원수 대수 모듈 (C++26 Engine Core 1:1 매핑)
# ==============================================================================

class Vector3:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def __add__(self, other):
        return Vector3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other):
        return Vector3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar):
        return Vector3(self.x * scalar, self.y * scalar, self.z * scalar)

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z

    def cross(self, other):
        return Vector3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x
        )

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    def normalize(self):
        l = self.length()
        if l < 1e-9:
            return Vector3(0, 0, 0)
        return Vector3(self.x / l, self.y / l, self.z / l)

    def to_list(self):
        return [self.x, self.y, self.z]


class Quaternion:
    def __init__(self, w=1.0, x=0.0, y=0.0, z=0.0):
        self.w = float(w)
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def normalize(self):
        l = math.sqrt(self.w**2 + self.x**2 + self.y**2 + self.z**2)
        if l < 1e-9:
            return Quaternion(1, 0, 0, 0)
        return Quaternion(self.w / l, self.x / l, self.y / l, self.z / l)

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

    def to_matrix(self):
        return [
            [1.0 - 2.0*(self.y**2 + self.z**2), 2.0*(self.x*self.y - self.w*self.z), 2.0*(self.x*self.z + self.w*self.y), 0.0],
            [2.0*(self.x*self.y + self.w*self.z), 1.0 - 2.0*(self.x**2 + self.z**2), 2.0*(self.y*self.z - self.w*self.x), 0.0],
            [2.0*(self.x*self.z - self.w*self.y), 2.0*(self.y*self.z + self.w*self.x), 1.0 - 2.0*(self.x**2 + self.y**2), 0.0],
            [0.0, 0.0, 0.0, 1.0]
        ]

    def to_euler(self):
        # Quaternion to Euler angles (yaw, pitch, roll) in radians
        ysqr = self.y * self.y

        # pitch (x-axis rotation)
        t0 = +2.0 * (self.w * self.x + self.y * self.z)
        t1 = +1.0 - 2.0 * (self.x * self.x + ysqr)
        pitch = math.atan2(t0, t1)

        # yaw (y-axis rotation)
        t2 = +2.0 * (self.w * self.y - self.z * self.x)
        t2 = +1.0 if t2 > +1.0 else t2
        t2 = -1.0 if t2 < -1.0 else t2
        yaw = math.asin(t2)

        # roll (z-axis rotation)
        t3 = +2.0 * (self.w * self.z + self.x * self.y)
        t4 = +1.0 - 2.0 * (ysqr + self.z * self.z)
        roll = math.atan2(t3, t4)

        return yaw, pitch, roll

    @staticmethod
    def from_euler(heading, pitch, roll):
        h = heading * 0.5
        p = pitch * 0.5
        r = roll * 0.5
        cos_h, sin_h = math.cos(h), math.sin(h)
        cos_p, sin_p = math.cos(p), math.sin(p)
        cos_r, sin_r = math.cos(r), math.sin(r)
        return Quaternion(
            cos_r * cos_p * cos_h - sin_r * sin_p * sin_h,
            cos_r * sin_p * cos_h - sin_r * cos_p * sin_h,
            cos_r * cos_p * sin_h + sin_r * sin_p * cos_h,
            cos_r * sin_p * sin_h + sin_r * cos_p * cos_h
        )


# ==============================================================================
# 2. 컴포넌트 아키텍처 및 강체 물리 데이터 (Unity Architecture 복사형)
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
        self.scale = Vector3(1.0, 1.0, 1.0)

    def get_world_position(self):
        if self.gameObject and self.gameObject.parent:
            parent_transform = self.gameObject.parent.transform
            return parent_transform.get_world_position() + self.position
        return self.position


class RigidbodyComponent(Component):
    def __init__(self):
        super().__init__()
        self.mass = 1.0
        self.restitution = 0.8
        self.is_static = False
        self.use_gravity = True
        self.velocity = Vector3(0, 0, 0)
        self.acceleration = Vector3(0, 0, 0)
        self.force = Vector3(0, 0, 0)
        self.radius = 0.3

    def update(self, dt):
        if self.is_static or not self.gameObject or not self.gameObject.transform:
            return
        
        if self.use_gravity:
            self.force = self.force + Vector3(0.0, -9.81 * self.mass, 0.0)
            
        self.acceleration = self.force * (1.0 / self.mass)
        self.velocity = self.velocity + self.acceleration * dt
        self.gameObject.transform.position = self.gameObject.transform.position + self.velocity * dt
        self.force = Vector3(0, 0, 0)


class TargetRotator(Component):
    def __init__(self, rotation_speed=1.5):
        super().__init__()
        self.rotation_speed = rotation_speed
        self.angle = 0.0

    def update(self, dt):
        if self.gameObject and self.gameObject.transform:
            self.angle += self.rotation_speed * dt
            self.gameObject.transform.rotation = Quaternion.from_euler(self.angle, 0.0, 0.0)


class MeshRenderer(Component):
    def __init__(self, color="#00FF66"):
        super().__init__()
        self.color = color


class GameObject:
    def __init__(self, id_val=0, name="GameObject"):
        self.id = id_val
        self.name = name
        self.components = []
        self.parent = None
        self.children = []
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

    def set_parent(self, new_parent):
        if self.parent:
            if self in self.parent.children:
                self.parent.children.remove(self)
        
        self.parent = new_parent
        if new_parent:
            if self not in new_parent.children:
                new_parent.children.append(self)


# ==============================================================================
# 3. 유니티 클론 에디터 GUI 디자인 및 시뮬레이션
# ==============================================================================

class UnityLikeEditor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("DSHS Game Engine C++26 - Unity Professional Editor")
        self.geometry("1480x920")
        self.configure(bg="#383838")

        # 엔진 내부 물리/시뮬레이션 변수
        self.dt = 0.01
        self.accumulator = 0.0
        self.last_time = time.time()
        self.score = 0
        self.object_counter = 1
        
        # 게임 오브젝트 컨테이너 및 관계 관리
        self.game_objects = []
        self.selected_object = None

        # 3D 씬뷰 카메라 변수 (유니티 스타일 비행 카메라)
        self.player_pos = Vector3(0.0, 2.0, 5.0)
        self.player_yaw = 0.0    # heading
        self.player_pitch = -0.2  # pitch
        self.keys_pressed = {}
        self.right_clicking = False
        self.right_click_start_x = 0
        self.right_click_start_y = 0
        
        # 3D 드래그 이동 상태 변수
        self.dragging_object = None
        self.drag_depth = 0.0
        
        # 3D UI 뷰포트 속성
        self.ui_center = Vector3(0.0, 1.5, -2.5)
        self.ui_hover = False
        
        # UI 스타일 테마 등록
        self.setup_dark_theme()
        
        # 기본 씬 채우기
        self.spawn_default_scene()

        # GUI 레이아웃 조립
        self.create_editor_layout()

        # Hierarchy 초기화 트리 채우기
        self.refresh_hierarchy()

        # 키보드 이벤트 글로벌 바인딩 (비행 조작)
        self.bind("<KeyPress>", self.on_key_press)
        self.bind("<KeyRelease>", self.on_key_release)

        # 실시간 엔진 메인 동기화 루프
        self.loop()

    def setup_dark_theme(self):
        self.style = ttk.Style()
        self.style.theme_use('clam')
        
        # Unity-like Dark Theme Palette
        self.style.configure('.', background='#383838', foreground='#DCDCDC', bordercolor='#1f1f1f')
        self.style.configure('TFrame', background='#383838')
        self.style.configure('TLabel', background='#383838', foreground='#DCDCDC', font=('Segoe UI', 9))
        self.style.configure('TButton', background='#5a5a5a', foreground='#DCDCDC', bordercolor='#2a2a2a', font=('Segoe UI', 9, 'bold'))
        self.style.map('TButton', background=[('active', '#6e6e6e')], foreground=[('active', '#FFFFFF')])
        
        # Treeview (Hierarchy / Project 용) 스타일
        self.style.configure('Treeview', background='#202020', foreground='#DCDCDC', fieldbackground='#202020', font=('Segoe UI', 9), rowheight=22)
        self.style.configure('Treeview.Heading', background='#2a2a2a', foreground='#DCDCDC', font=('Segoe UI', 9, 'bold'))
        self.style.map('Treeview', background=[('selected', '#3c5e8c')], foreground=[('selected', '#FFFFFF')])

        # Panedwindow 스타일
        self.style.configure('Panedwindow', background='#1f1f1f')

    def spawn_default_scene(self):
        self.game_objects.clear()
        self.object_counter = 1
        
        # 1. Static Ground Target
        t1 = GameObject(self.get_next_id(), "Static_Target_1")
        t1.transform.position = Vector3(1.2, 1.0, -2.0)
        rb1 = t1.add_component(RigidbodyComponent)
        rb1.is_static = True
        rb1.use_gravity = False
        t1.add_component(TargetRotator, rotation_speed=1.5)
        t1.add_component(MeshRenderer, color="#FF007F")
        self.game_objects.append(t1)

        # 2. Static Left Target
        t2 = GameObject(self.get_next_id(), "Static_Target_2")
        t2.transform.position = Vector3(-1.2, 1.4, -1.5)
        rb2 = t2.add_component(RigidbodyComponent)
        rb2.is_static = True
        rb2.use_gravity = False
        t2.add_component(MeshRenderer, color="#FF9800")
        self.game_objects.append(t2)

        # 3. Dynamic Physics Sphere
        t3 = GameObject(self.get_next_id(), "Dynamic_Sphere")
        t3.transform.position = Vector3(0.0, 4.5, -2.0)
        rb3 = t3.add_component(RigidbodyComponent)
        rb3.is_static = False
        rb3.use_gravity = True
        rb3.restitution = 0.8
        t3.add_component(MeshRenderer, color="#00FF66")
        self.game_objects.append(t3)

    def get_next_id(self):
        id_val = self.object_counter
        self.object_counter += 1
        return id_val

    def find_gameobject_by_id(self, obj_id):
        for obj in self.game_objects:
            if obj.id == obj_id:
                return obj
        return None

    # ==============================================================================
    # 4. GUI 패널 디자인 (Hierarchy, Project, Scene View, Console, Inspector)
    # ==============================================================================

    def create_editor_layout(self):
        # 상단 Unity 스타일 툴바 (플레이, 리셋, 트랜스폼 모드 단축 표시)
        toolbar = tk.Frame(self, bg="#2a2a2a", height=45)
        toolbar.pack(fill=tk.X, side=tk.TOP)
        toolbar.pack_propagate(False)
        
        # 툴바 내부 좌측 메뉴
        lbl_brand = tk.Label(toolbar, text=" ❖ DSHS Unity Engine Pro (C++26) ", bg="#2a2a2a", fg="#00E6FF", font=('Segoe UI', 10, 'bold'))
        lbl_brand.pack(side=tk.LEFT, padx=10)
        
        # 재생/정지 제어
        btn_play = tk.Button(toolbar, text=" ▶ Play ", bg="#5a5a5a", fg="#00FF66", relief=tk.FLAT, font=('Segoe UI', 9, 'bold'), command=self.reset_physics_velocity)
        btn_play.pack(side=tk.LEFT, padx=5, pady=8)
        
        btn_reset = tk.Button(toolbar, text=" 🔄 Reset Scene ", bg="#5a5a5a", fg="#FFEB3B", relief=tk.FLAT, font=('Segoe UI', 9, 'bold'), command=self.reset_whole_scene)
        btn_reset.pack(side=tk.LEFT, padx=5, pady=8)

        # 씬 사격 단추
        btn_shoot = tk.Button(toolbar, text=" ☄️ Fire Bullet ", bg="#5a5a5a", fg="#00E6FF", relief=tk.FLAT, font=('Segoe UI', 9, 'bold'), command=self.shoot_from_toolbar)
        btn_shoot.pack(side=tk.LEFT, padx=5, pady=8)

        # 씬 뷰 가이드 텍스트
        lbl_guide = tk.Label(toolbar, text="[조작] 좌클릭: 선택 | 좌클릭 드래그: 3D 이동 | 우클릭 드래그: 회전 | 우클릭+WASD: 비행 | 우클릭클릭: 생성메뉴 | F2: 이름변경", bg="#2a2a2a", fg="#aa8888", font=('Segoe UI', 8))
        lbl_guide.pack(side=tk.LEFT, padx=15)

        # 우측 스코어 표시
        self.score_display = tk.Label(toolbar, text="SCORE: 0", bg="#2a2a2a", fg="#FF007F", font=('Consolas', 11, 'bold'))
        self.score_display.pack(side=tk.RIGHT, padx=20)

        # 메인 가로 분할창 (PanedWindow)
        main_pane = tk.PanedWindow(self, orient=tk.HORIZONTAL, bg="#1f1f1f", sashwidth=4)
        main_pane.pack(fill=tk.BOTH, expand=True)

        # [좌측 열] Hierarchy + Project (세로 분할)
        left_pane = tk.PanedWindow(main_pane, orient=tk.VERTICAL, bg="#1f1f1f", sashwidth=4)
        main_pane.add(left_pane, width=320)

        # 1. Hierarchy 패널
        hierarchy_container = tk.Frame(left_pane, bg="#383838")
        left_pane.add(hierarchy_container, height=450)
        
        tk.Label(hierarchy_container, text=" Hierarchy ", bg="#2a2a2a", fg="#FFF", font=('Segoe UI', 9, 'bold'), anchor="w").pack(fill=tk.X)
        
        # Hierarchy Treeview
        self.tree = ttk.Treeview(hierarchy_container, show="tree", selectmode="browse")
        self.tree.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.tree.bind("<<TreeviewSelect>>", self.on_object_selected)
        
        # 드래그 앤 드롭 및 단축키 바인딩
        self.tree.bind("<ButtonPress-1>", self.on_tree_drag_start)
        self.tree.bind("<B1-Motion>", self.on_tree_drag_motion)
        self.tree.bind("<ButtonRelease-1>", self.on_tree_drop)
        self.tree.bind("<F2>", self.rename_popup)
        self.tree.bind("<Double-1>", self.rename_popup)

        # 퀵 버튼
        parent_panel = tk.Frame(hierarchy_container, bg="#383838")
        parent_panel.pack(fill=tk.X, pady=2)
        btn_empty = tk.Button(parent_panel, text="+ Create Empty", bg="#5a5a5a", fg="#FFF", relief=tk.FLAT, font=('Segoe UI', 8), command=self.create_empty_gameobject)
        btn_empty.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        btn_sphere = tk.Button(parent_panel, text="+ Create Sphere", bg="#5a5a5a", fg="#FFF", relief=tk.FLAT, font=('Segoe UI', 8), command=self.create_target_gameobject)
        btn_sphere.pack(side=tk.RIGHT, fill=tk.X, expand=True, padx=2)

        # Hierarchy 우클릭 컨텍스트 메뉴
        self.hierarchy_menu = tk.Menu(self, tearoff=0, bg="#2a2a2a", fg="#DCDCDC", activebackground="#3c5e8c", activeforeground="#FFF")
        self.hierarchy_menu.add_command(label=" Rename GameObject (F2)", command=self.rename_popup)
        self.hierarchy_menu.add_command(label=" Delete GameObject", command=self.delete_selected_object)
        self.tree.bind("<Button-3>", self.show_hierarchy_menu)

        # 2. Project 패널 (에셋 관리자)
        project_container = tk.Frame(left_pane, bg="#383838")
        left_pane.add(project_container, height=350)
        
        tk.Label(project_container, text=" Project (Assets) ", bg="#2a2a2a", fg="#FFF", font=('Segoe UI', 9, 'bold'), anchor="w").pack(fill=tk.X)
        
        self.project_tree = ttk.Treeview(project_container, show="tree", selectmode="browse")
        self.project_tree.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.populate_project_assets()

        # [중앙 열] Scene View + Console (세로 분할)
        center_pane = tk.PanedWindow(main_pane, orient=tk.VERTICAL, bg="#1f1f1f", sashwidth=4)
        main_pane.add(center_pane, width=820)

        # 3. Scene View (3D 캔버스)
        scene_container = tk.Frame(center_pane, bg="#383838")
        center_pane.add(scene_container, height=600)
        
        tk.Label(scene_container, text=" Scene (3D Viewport) ", bg="#2a2a2a", fg="#FFF", font=('Segoe UI', 9, 'bold'), anchor="w").pack(fill=tk.X)
        
        self.canvas_3d = tk.Canvas(scene_container, bg="#202020", highlightthickness=0)
        self.canvas_3d.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        
        # 마우스 바인딩 (카메라 뷰 조작 및 레이캐스트 스폰)
        self.canvas_3d.bind("<Motion>", self.scene_raycast)
        self.canvas_3d.bind("<ButtonPress-3>", self.on_right_click_press)
        self.canvas_3d.bind("<ButtonRelease-3>", self.on_right_click_release)
        self.canvas_3d.bind("<B3-Motion>", self.on_right_click_drag)
        self.canvas_3d.bind("<MouseWheel>", self.on_mouse_wheel)
        self.canvas_3d.bind("<ButtonPress-2>", self.on_middle_click_press)
        self.canvas_3d.bind("<B2-Motion>", self.on_middle_click_drag)
        
        # 마우스 좌클릭 (3D 드래그 이동 및 선택)
        self.canvas_3d.bind("<Button-1>", self.scene_select)
        self.canvas_3d.bind("<B1-Motion>", self.scene_drag)
        self.canvas_3d.bind("<ButtonRelease-1>", self.scene_release)

        # 우클릭 컨텍스트 메뉴 선언
        self.context_menu = tk.Menu(self, tearoff=0, bg="#2a2a2a", fg="#DCDCDC", activebackground="#3c5e8c", activeforeground="#FFF")
        self.context_menu.add_command(label=" Create Empty GameObject", command=self.create_empty_at_mouse)
        self.context_menu.add_command(label=" Create Target Sphere (Physics)", command=self.create_sphere_at_mouse)
        self.context_menu.add_command(label=" Create Static Obstacle", command=self.create_obstacle_at_mouse)
        self.context_menu.add_separator()
        self.context_menu.add_command(label=" Delete Selected", command=self.delete_selected_object)

        # 4. Console 패널
        console_container = tk.Frame(center_pane, bg="#383838")
        center_pane.add(console_container, height=200)
        
        tk.Label(console_container, text=" Console ", bg="#2a2a2a", fg="#00E6FF", font=('Segoe UI', 9, 'bold'), anchor="w").pack(fill=tk.X)
        
        self.console_text = tk.Text(console_container, bg="#1a1a1a", fg="#DCDCDC", font=('Consolas', 9), wrap=tk.WORD, state=tk.DISABLED, highlightthickness=0)
        self.console_text.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # [우측 열] Inspector (속성 편집창)
        self.inspector_frame = tk.LabelFrame(main_pane, text=" Inspector ", bg="#383838", fg="#DCDCDC", width=340, relief=tk.FLAT)
        self.inspector_frame.pack_propagate(False)
        main_pane.add(self.inspector_frame)

        self.inspector_scroll = tk.Frame(self.inspector_frame, bg="#383838")
        self.inspector_scroll.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.draw_inspector_empty()
        self.log_to_console("Unity clone environment initialized. 3D Drag Translation active.")

    def log_to_console(self, msg):
        self.console_text.config(state=tk.NORMAL)
        self.console_text.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {msg}\n")
        self.console_text.see(tk.END)
        self.console_text.config(state=tk.DISABLED)

    # ==============================================================================
    # 5. Project 에셋 빌드 및 트리 생성 (Unity Assets 폴더 시뮬레이션)
    # ==============================================================================

    def populate_project_assets(self):
        p = self.project_tree
        for item in p.get_children():
            p.delete(item)
            
        assets_node = p.insert("", "end", text=" 📁 Assets", open=True)
        scripts_node = p.insert(assets_node, "end", text=" 📁 Scripts", open=True)
        p.insert(scripts_node, "end", text=" 📄 TargetRotator.cpp")
        p.insert(scripts_node, "end", text=" 📄 TargetRotator.h")
        p.insert(scripts_node, "end", text=" 📄 Component.h")
        p.insert(scripts_node, "end", text=" 📄 RigidbodyComponent.h")

        prefabs_node = p.insert(assets_node, "end", text=" 📁 Prefabs", open=True)
        p.insert(prefabs_node, "end", text=" 📦 TargetSphere.prefab")
        p.insert(prefabs_node, "end", text=" 📦 StaticTarget.prefab")

        scenes_node = p.insert(assets_node, "end", text=" 📁 Scenes", open=True)
        p.insert(scenes_node, "end", text=" 🎬 MainScene.scene")

    # ==============================================================================
    # 6. Hierarchy 트리 제어 및 드래그 앤 드롭 관계 설정
    # ==============================================================================

    def refresh_hierarchy(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        for obj in self.game_objects:
            if obj.parent is None:
                self.add_object_to_tree(obj, "")

    def add_object_to_tree(self, obj, parent_node_id):
        icon = "📦"
        if obj.get_component(RigidbodyComponent) and obj.get_component(RigidbodyComponent).is_static:
            icon = "🛑"
        elif obj.get_component(RigidbodyComponent):
            icon = "⚽"
            
        node_id = self.tree.insert(parent_node_id, "end", text=f" {icon} {obj.name}", open=True, values=(obj.id,))
        for child in obj.children:
            self.add_object_to_tree(child, node_id)

    def on_object_selected(self, event):
        selected = self.tree.selection()
        if not selected:
            self.selected_object = None
            self.draw_inspector_empty()
            return
        
        values = self.tree.item(selected[0], "values")
        if values:
            obj_id = int(values[0])
            obj = self.find_gameobject_by_id(obj_id)
            if obj:
                self.selected_object = obj
                self.draw_inspector_fields()

    def select_tree_by_id(self, obj_id):
        for item in self.tree.get_children():
            if self.find_and_select_node(item, obj_id):
                break

    def find_and_select_node(self, node_id, obj_id):
        vals = self.tree.item(node_id, "values")
        if vals and int(vals[0]) == obj_id:
            self.tree.selection_set(node_id)
            self.tree.see(node_id)
            return True
        for child in self.tree.get_children(node_id):
            if self.find_and_select_node(child, obj_id):
                return True
        return False

    def show_hierarchy_menu(self, event):
        # 마우스 위치의 노드를 구함
        item = self.tree.identify_row(event.y)
        if item:
            self.tree.selection_set(item)
            self.hierarchy_menu.post(event.x_root, event.y_root)

    # 드래그 앤 드롭 구현부
    def on_tree_drag_start(self, event):
        item = self.tree.identify_row(event.y)
        if item:
            self.dragged_tree_item = item

    def on_tree_drag_motion(self, event):
        item = self.tree.identify_row(event.y)
        if item:
            self.tree.selection_set(item)

    def on_tree_drop(self, event):
        target_item = self.tree.identify_row(event.y)
        source_item = getattr(self, "dragged_tree_item", None)
        if not source_item:
            return
            
        if source_item == target_item:
            return
            
        source_vals = self.tree.item(source_item, "values")
        if not source_vals:
            return
        source_id = int(source_vals[0])
        source_obj = self.find_gameobject_by_id(source_id)
        if not source_obj:
            return
            
        if target_item:
            target_vals = self.tree.item(target_item, "values")
            if not target_vals:
                return
            target_id = int(target_vals[0])
            target_obj = self.find_gameobject_by_id(target_id)
            if not target_obj:
                return
                
            # 순환 참조 방지
            curr = target_obj
            is_circular = False
            while curr:
                if curr.id == source_id:
                    is_circular = True
                    break
                curr = curr.parent
                
            if is_circular:
                self.log_to_console("Error: Circular hierarchy parenting is prohibited.")
                messagebox.showerror("Hierarchy Error", "Cannot drag a parent object inside its own child!")
                return
                
            source_obj.set_parent(target_obj)
            self.log_to_console(f"Parented '{source_obj.name}' under '{target_obj.name}'")
        else:
            source_obj.set_parent(None)
            self.log_to_console(f"Parent of '{source_obj.name}' cleared. Restored to Root.")
            
        self.refresh_hierarchy()

    # ==============================================================================
    # 7. 마우스 기반 오브젝트 생성 및 우클릭 레이캐스트 스폰
    # ==============================================================================

    def create_empty_gameobject(self):
        obj = GameObject(self.get_next_id(), f"Empty_Object_{self.object_counter}")
        obj.transform.position = Vector3(0.0, 1.5, -2.0)
        obj.add_component(MeshRenderer, color="#DCDCDC")
        self.game_objects.append(obj)
        self.refresh_hierarchy()
        self.log_to_console(f"Created Empty: {obj.name}")
        return obj

    def create_target_gameobject(self):
        obj = GameObject(self.get_next_id(), f"Physics_Sphere_{self.object_counter}")
        offset_x = (time.time() % 3.0) - 1.5
        obj.transform.position = Vector3(offset_x, 3.5, -2.0)
        rb = obj.add_component(RigidbodyComponent)
        rb.is_static = False
        rb.use_gravity = True
        rb.restitution = 0.8
        obj.add_component(MeshRenderer, color="#00FF66")
        self.game_objects.append(obj)
        self.refresh_hierarchy()
        self.log_to_console(f"Created Physics Sphere: {obj.name}")
        return obj

    def delete_selected_object(self):
        if not self.selected_object:
            return
        obj = self.selected_object
        
        children_to_clear = list(obj.children)
        for child in children_to_clear:
            child.set_parent(None)
            
        if obj.parent:
            obj.parent.children.remove(obj)
            
        if obj in self.game_objects:
            self.game_objects.remove(obj)
            
        self.log_to_console(f"Deleted Object: '{obj.name}'")
        self.selected_object = None
        self.draw_inspector_empty()
        self.refresh_hierarchy()

    def rename_popup(self, event=None):
        if not self.selected_object:
            return
        obj = self.selected_object
        new_name = simpledialog.askstring("Rename GameObject", f"Enter new name for '{obj.name}':", parent=self, initialvalue=obj.name)
        if new_name:
            new_name = new_name.strip()
            if new_name:
                self.log_to_console(f"Rename: '{obj.name}' -> '{new_name}'")
                obj.name = new_name
                self.refresh_hierarchy()
                self.draw_inspector_fields()
                self.select_tree_by_id(obj.id)

    # 마우스 클릭 레이캐스트 스폰 핸들러
    def get_raycast_plane_intersection(self, event):
        w = self.canvas_3d.winfo_width()
        h = self.canvas_3d.winfo_height()
        cx, cy = w / 2, h / 2
        
        mx = (event.x - cx) / cx
        my = (cy - event.y) / cy
        
        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)

        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        ray_dir = (right * mx + up * my + forward * 1.2).normalize()
        
        if abs(ray_dir.y) > 1e-4:
            t = -self.player_pos.y / ray_dir.y
            if t > 0 and t < 25.0:
                intersection_pt = self.player_pos + ray_dir * t
                return intersection_pt
                
        return self.player_pos + forward * 4.0

    def create_empty_at_mouse(self):
        spawn_pos = getattr(self, "spawn_click_pos", Vector3(0, 1.5, -2))
        obj = GameObject(self.get_next_id(), f"Empty_Object_{self.object_counter}")
        obj.transform.position = spawn_pos
        obj.add_component(MeshRenderer, color="#DCDCDC")
        self.game_objects.append(obj)
        self.refresh_hierarchy()
        self.log_to_console(f"Created Empty at ({spawn_pos.x:.2f}, {spawn_pos.y:.2f}, {spawn_pos.z:.2f})")

    def create_sphere_at_mouse(self):
        spawn_pos = getattr(self, "spawn_click_pos", Vector3(0, 1.5, -2))
        obj = GameObject(self.get_next_id(), f"Physics_Sphere_{self.object_counter}")
        obj.transform.position = spawn_pos
        rb = obj.add_component(RigidbodyComponent)
        rb.is_static = False
        rb.use_gravity = True
        rb.restitution = 0.75
        obj.add_component(MeshRenderer, color="#00FF66")
        self.game_objects.append(obj)
        self.refresh_hierarchy()
        self.log_to_console(f"Created Sphere at ({spawn_pos.x:.2f}, {spawn_pos.y:.2f}, {spawn_pos.z:.2f})")

    def create_obstacle_at_mouse(self):
        spawn_pos = getattr(self, "spawn_click_pos", Vector3(0, 1.5, -2))
        obj = GameObject(self.get_next_id(), f"Static_Obstacle_{self.object_counter}")
        obj.transform.position = spawn_pos
        rb = obj.add_component(RigidbodyComponent)
        rb.is_static = True
        rb.use_gravity = False
        obj.add_component(MeshRenderer, color="#FF007F")
        self.game_objects.append(obj)
        self.refresh_hierarchy()
        self.log_to_console(f"Created Static Obstacle at ({spawn_pos.x:.2f}, {spawn_pos.y:.2f}, {spawn_pos.z:.2f})")

    # ==============================================================================
    # 8. Inspector UI 컴포넌트 렌더링 및 편집 즉각 반영 (FocusOut, Enter)
    # ==============================================================================

    def draw_inspector_empty(self):
        for widget in self.inspector_scroll.winfo_children():
            widget.destroy()
        lbl = tk.Label(self.inspector_scroll, text="Select a GameObject\nin Hierarchy to inspect.", font=('Segoe UI', 10, 'italic'), bg="#383838", fg="#888888")
        lbl.pack(fill=tk.BOTH, expand=True, pady=120)

    def draw_inspector_fields(self):
        # 텍스트 엔트리가 편집 포커스를 가지고 있을 때는 다시 그리지 않음
        focus_widget = self.focus_get()
        if focus_widget and isinstance(focus_widget, tk.Entry):
            # 단, 선택된 오브젝트가 아예 바뀌었을 때는 갱신해야 하지만, 드래그 위치 이동 시 갱신을 위해 스킵하지 않는다.
            # 하지만 입력 필드 포커싱 시 깜빡임을 최소화하기 위해 선택적 드로우
            pass

        for widget in self.inspector_scroll.winfo_children():
            widget.destroy()

        obj = self.selected_object
        if not obj:
            return

        header_frame = tk.Frame(self.inspector_scroll, bg="#2a2a2a", pady=6)
        header_frame.pack(fill=tk.X, pady=2)
        
        tk.Label(header_frame, text=f" ID: {obj.id}", font=('Consolas', 9, 'bold'), bg="#2a2a2a", fg="#00E6FF").pack(side=tk.LEFT, padx=5)
        
        ent_name = tk.Entry(header_frame, bg="#202020", fg="#FFF", font=('Segoe UI', 9), insertbackground="white", relief=tk.FLAT)
        ent_name.insert(0, obj.name)
        ent_name.pack(side=tk.LEFT, padx=10, fill=tk.X, expand=True)

        def apply_name(event=None):
            new_name = ent_name.get().strip()
            if new_name and new_name != obj.name:
                self.log_to_console(f"Renamed '{obj.name}' -> '{new_name}'")
                obj.name = new_name
                self.refresh_hierarchy()
                self.select_tree_by_id(obj.id)

        ent_name.bind("<FocusOut>", apply_name)
        ent_name.bind("<Return>", apply_name)
        
        btn_del = tk.Button(header_frame, text="🗑 Delete", bg="#773333", fg="#FFF", relief=tk.FLAT, font=('Segoe UI', 8), command=self.delete_selected_object)
        btn_del.pack(side=tk.RIGHT, padx=5)

        for comp in obj.components:
            if isinstance(comp, Transform):
                self.draw_transform_inspector(comp)
            elif isinstance(comp, RigidbodyComponent):
                self.draw_rigidbody_inspector(comp)
            elif isinstance(comp, TargetRotator):
                self.draw_rotator_inspector(comp)
            elif isinstance(comp, MeshRenderer):
                self.draw_mesh_renderer_inspector(comp)

        add_frame = tk.Frame(self.inspector_scroll, bg="#383838", pady=15)
        add_frame.pack(fill=tk.X)
        
        tk.Label(add_frame, text="Add Component:", bg="#383838", fg="#888888", font=('Segoe UI', 9, 'bold')).pack(anchor="w")

        comp_choices = []
        if not obj.get_component(RigidbodyComponent):
            comp_choices.append("Rigidbody Component")
        if not obj.get_component(TargetRotator):
            comp_choices.append("Target Rotator (Script)")
        if not obj.get_component(MeshRenderer):
            comp_choices.append("Mesh Renderer")

        if comp_choices:
            combo = ttk.Combobox(add_frame, values=comp_choices, state="readonly", font=('Segoe UI', 9))
            combo.pack(fill=tk.X, pady=5)
            combo.set(comp_choices[0])

            def apply_add_component():
                sel = combo.get()
                if sel == "Rigidbody Component":
                    obj.add_component(RigidbodyComponent)
                    self.log_to_console(f"Added Rigidbody to '{obj.name}'")
                elif sel == "Target Rotator (Script)":
                    obj.add_component(TargetRotator)
                    self.log_to_console(f"Added TargetRotator C++ Script to '{obj.name}'")
                elif sel == "Mesh Renderer":
                    obj.add_component(MeshRenderer)
                    self.log_to_console(f"Added Mesh Renderer to '{obj.name}'")
                self.draw_inspector_fields()
                self.refresh_hierarchy()
            
            btn_add = tk.Button(add_frame, text="+ Add Component", bg="#3c5e8c", fg="#FFF", relief=tk.FLAT, font=('Segoe UI', 9, 'bold'), command=apply_add_component)
            btn_add.pack(fill=tk.X, pady=5)
        else:
            tk.Label(add_frame, text="All components attached.", bg="#383838", fg="#555").pack(anchor="w")

    def draw_transform_inspector(self, trans):
        frame = tk.LabelFrame(self.inspector_scroll, text=" Transform Component ", bg="#383838", fg="#FFF", font=('Segoe UI', 9, 'bold'), labelanchor="n")
        frame.pack(fill=tk.X, pady=5, padx=2)

        # 1. Position X, Y, Z
        pos_frame = tk.Frame(frame, bg="#383838")
        pos_frame.pack(fill=tk.X, pady=3)
        tk.Label(pos_frame, text="Position:", bg="#383838", fg="#CCC", width=8, anchor="w").pack(side=tk.LEFT)
        
        p_entries = []
        labels = ['X', 'Y', 'Z']
        p_vals = [trans.position.x, trans.position.y, trans.position.z]
        for i in range(3):
            tk.Label(pos_frame, text=f" {labels[i]}", bg="#383838", fg="#888").pack(side=tk.LEFT)
            e = tk.Entry(pos_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=6, insertbackground="white", relief=tk.FLAT)
            e.insert(0, f"{p_vals[i]:.2f}")
            e.pack(side=tk.LEFT, padx=2)
            p_entries.append(e)

        def apply_pos(event=None):
            try:
                trans.position = Vector3(float(p_entries[0].get()), float(p_entries[1].get()), float(p_entries[2].get()))
            except ValueError:
                pass

        for e in p_entries:
            e.bind("<FocusOut>", apply_pos)
            e.bind("<Return>", apply_pos)

        # 2. Rotation Euler (Yaw, Pitch, Roll in degrees)
        rot_frame = tk.Frame(frame, bg="#383838")
        rot_frame.pack(fill=tk.X, pady=3)
        tk.Label(rot_frame, text="Rotation:", bg="#383838", fg="#CCC", width=8, anchor="w").pack(side=tk.LEFT)

        r_yaw, r_pitch, r_roll = trans.rotation.to_euler()
        r_vals = [math.degrees(r_pitch), math.degrees(r_yaw), math.degrees(r_roll)]
        r_entries = []
        r_labels = ['X', 'Y', 'Z'] # Pitch=X, Yaw=Y, Roll=Z in Unity
        for i in range(3):
            tk.Label(rot_frame, text=f" {r_labels[i]}", bg="#383838", fg="#888").pack(side=tk.LEFT)
            e = tk.Entry(rot_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=6, insertbackground="white", relief=tk.FLAT)
            e.insert(0, f"{r_vals[i]:.1f}")
            e.pack(side=tk.LEFT, padx=2)
            r_entries.append(e)

        def apply_rot(event=None):
            try:
                pitch_rad = math.radians(float(r_entries[0].get()))
                yaw_rad = math.radians(float(r_entries[1].get()))
                roll_rad = math.radians(float(r_entries[2].get()))
                trans.rotation = Quaternion.from_euler(yaw_rad, pitch_rad, roll_rad)
            except ValueError:
                pass

        for e in r_entries:
            e.bind("<FocusOut>", apply_rot)
            e.bind("<Return>", apply_rot)

        # 3. Scale X, Y, Z
        scale_frame = tk.Frame(frame, bg="#383838")
        scale_frame.pack(fill=tk.X, pady=3)
        tk.Label(scale_frame, text="Scale:", bg="#383838", fg="#CCC", width=8, anchor="w").pack(side=tk.LEFT)

        s_entries = []
        s_vals = [trans.scale.x, trans.scale.y, trans.scale.z]
        for i in range(3):
            tk.Label(scale_frame, text=f" {labels[i]}", bg="#383838", fg="#888").pack(side=tk.LEFT)
            e = tk.Entry(scale_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=6, insertbackground="white", relief=tk.FLAT)
            e.insert(0, f"{s_vals[i]:.2f}")
            e.pack(side=tk.LEFT, padx=2)
            s_entries.append(e)

        def apply_scale(event=None):
            try:
                trans.scale = Vector3(float(s_entries[0].get()), float(s_entries[1].get()), float(s_entries[2].get()))
            except ValueError:
                pass

        for e in s_entries:
            e.bind("<FocusOut>", apply_scale)
            e.bind("<Return>", apply_scale)

    def draw_rigidbody_inspector(self, rb_comp):
        frame = tk.LabelFrame(self.inspector_scroll, text=" Rigidbody Component ", bg="#383838", fg="#FFF", font=('Segoe UI', 9, 'bold'), labelanchor="n")
        frame.pack(fill=tk.X, pady=5, padx=2)

        # 1. Mass
        mass_frame = tk.Frame(frame, bg="#383838")
        mass_frame.pack(fill=tk.X, pady=3)
        tk.Label(mass_frame, text="Mass:", bg="#383838", fg="#CCC", width=12, anchor="w").pack(side=tk.LEFT)
        ent_mass = tk.Entry(mass_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=8, insertbackground="white", relief=tk.FLAT)
        ent_mass.insert(0, f"{rb_comp.mass:.2f}")
        ent_mass.pack(side=tk.LEFT)

        def apply_mass(event=None):
            try:
                rb_comp.mass = float(ent_mass.get())
            except ValueError:
                pass
        ent_mass.bind("<FocusOut>", apply_mass)
        ent_mass.bind("<Return>", apply_mass)

        # 2. Restitution (Bounciness)
        rest_frame = tk.Frame(frame, bg="#383838")
        rest_frame.pack(fill=tk.X, pady=3)
        tk.Label(rest_frame, text="Bounciness:", bg="#383838", fg="#CCC", width=12, anchor="w").pack(side=tk.LEFT)
        ent_rest = tk.Entry(rest_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=8, insertbackground="white", relief=tk.FLAT)
        ent_rest.insert(0, f"{rb_comp.restitution:.2f}")
        ent_rest.pack(side=tk.LEFT)

        def apply_rest(event=None):
            try:
                rb_comp.restitution = max(0.0, min(1.0, float(ent_rest.get())))
            except ValueError:
                pass
        ent_rest.bind("<FocusOut>", apply_rest)
        ent_rest.bind("<Return>", apply_rest)

        # 3. Radius (Sphere size)
        rad_frame = tk.Frame(frame, bg="#383838")
        rad_frame.pack(fill=tk.X, pady=3)
        tk.Label(rad_frame, text="Radius:", bg="#383838", fg="#CCC", width=12, anchor="w").pack(side=tk.LEFT)
        ent_rad = tk.Entry(rad_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=8, insertbackground="white", relief=tk.FLAT)
        ent_rad.insert(0, f"{rb_comp.radius:.2f}")
        ent_rad.pack(side=tk.LEFT)

        def apply_rad(event=None):
            try:
                rb_comp.radius = max(0.05, float(ent_rad.get()))
            except ValueError:
                pass
        ent_rad.bind("<FocusOut>", apply_rad)
        ent_rad.bind("<Return>", apply_rad)

        # 4. Velocity X, Y, Z
        vel_frame = tk.Frame(frame, bg="#383838")
        vel_frame.pack(fill=tk.X, pady=3)
        tk.Label(vel_frame, text="Velocity:", bg="#383838", fg="#CCC", width=8, anchor="w").pack(side=tk.LEFT)

        v_entries = []
        labels = ['X', 'Y', 'Z']
        v_vals = [rb_comp.velocity.x, rb_comp.velocity.y, rb_comp.velocity.z]
        for i in range(3):
            tk.Label(vel_frame, text=f" {labels[i]}", bg="#383838", fg="#888").pack(side=tk.LEFT)
            e = tk.Entry(vel_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=5, insertbackground="white", relief=tk.FLAT)
            e.insert(0, f"{v_vals[i]:.1f}")
            e.pack(side=tk.LEFT, padx=1)
            v_entries.append(e)

        def apply_vel(event=None):
            try:
                rb_comp.velocity = Vector3(float(v_entries[0].get()), float(v_entries[1].get()), float(v_entries[2].get()))
            except ValueError:
                pass

        for e in v_entries:
            e.bind("<FocusOut>", apply_vel)
            e.bind("<Return>", apply_vel)

        # Checkboxes (Is Static, Use Gravity)
        var_static = tk.BooleanVar(value=rb_comp.is_static)
        def on_toggle_static():
            rb_comp.is_static = var_static.get()
            self.refresh_hierarchy()
            self.log_to_console(f"Rigidbody static = {rb_comp.is_static}")
        
        chk_static = tk.Checkbutton(frame, text="Is Static", variable=var_static, command=on_toggle_static, bg="#383838", fg="#FFF", selectcolor="#202020", activebackground="#383838", activeforeground="#FFF")
        chk_static.pack(anchor="w", padx=5, pady=2)

        var_grav = tk.BooleanVar(value=rb_comp.use_gravity)
        def on_toggle_gravity():
            rb_comp.use_gravity = var_grav.get()
            self.log_to_console(f"Rigidbody gravity = {rb_comp.use_gravity}")

        chk_grav = tk.Checkbutton(frame, text="Use Gravity", variable=var_grav, command=on_toggle_gravity, bg="#383838", fg="#FFF", selectcolor="#202020", activebackground="#383838", activeforeground="#FFF")
        chk_grav.pack(anchor="w", padx=5, pady=2)

        def remove_comp():
            rb_comp.gameObject.components.remove(rb_comp)
            self.log_to_console("Removed Rigidbody.")
            self.draw_inspector_fields()
            self.refresh_hierarchy()

        btn_rem = tk.Button(frame, text="Remove Component", bg="#773333", fg="#FFF", font=('Segoe UI', 8), relief=tk.FLAT, command=remove_comp)
        btn_rem.pack(fill=tk.X, pady=4, padx=5)

    def draw_rotator_inspector(self, rot):
        frame = tk.LabelFrame(self.inspector_scroll, text=" TargetRotator (C++ Script) ", bg="#383838", fg="#FFF", font=('Segoe UI', 9, 'bold'), labelanchor="n")
        frame.pack(fill=tk.X, pady=5, padx=2)

        speed_frame = tk.Frame(frame, bg="#383838")
        speed_frame.pack(fill=tk.X, pady=4)
        tk.Label(speed_frame, text="Rotate Speed:", bg="#383838", fg="#CCC", width=12, anchor="w").pack(side=tk.LEFT)
        ent_speed = tk.Entry(speed_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=8, insertbackground="white", relief=tk.FLAT)
        ent_speed.insert(0, f"{rot.rotation_speed:.2f}")
        ent_speed.pack(side=tk.LEFT)

        def apply_speed(event=None):
            try:
                rot.rotation_speed = float(ent_speed.get())
            except ValueError:
                pass

        ent_speed.bind("<FocusOut>", apply_speed)
        ent_speed.bind("<Return>", apply_speed)

        def remove_comp():
            rot.gameObject.components.remove(rot)
            self.log_to_console("Removed TargetRotator script.")
            self.draw_inspector_fields()

        btn_rem = tk.Button(frame, text="Remove Component", bg="#773333", fg="#FFF", font=('Segoe UI', 8), relief=tk.FLAT, command=remove_comp)
        btn_rem.pack(fill=tk.X, pady=4, padx=5)

    def draw_mesh_renderer_inspector(self, renderer):
        frame = tk.LabelFrame(self.inspector_scroll, text=" Mesh Renderer Component ", bg="#383838", fg="#FFF", font=('Segoe UI', 9, 'bold'), labelanchor="n")
        frame.pack(fill=tk.X, pady=5, padx=2)

        color_frame = tk.Frame(frame, bg="#383838")
        color_frame.pack(fill=tk.X, pady=4)
        tk.Label(color_frame, text="Material Color:", bg="#383838", fg="#CCC", width=12, anchor="w").pack(side=tk.LEFT)
        
        ent_color = tk.Entry(color_frame, bg="#202020", fg="#FFF", font=('Consolas', 9), width=12, insertbackground="white", relief=tk.FLAT)
        ent_color.insert(0, renderer.color)
        ent_color.pack(side=tk.LEFT)

        def apply_color(event=None):
            val = ent_color.get().strip()
            if val:
                renderer.color = val
                self.log_to_console(f"Mesh Color updated to '{val}'")

        ent_color.bind("<FocusOut>", apply_color)
        ent_color.bind("<Return>", apply_color)

        def remove_comp():
            renderer.gameObject.components.remove(renderer)
            self.log_to_console("Removed MeshRenderer.")
            self.draw_inspector_fields()

        btn_rem = tk.Button(frame, text="Remove Component", bg="#773333", fg="#FFF", font=('Segoe UI', 8), relief=tk.FLAT, command=remove_comp)
        btn_rem.pack(fill=tk.X, pady=4, padx=5)


    # ==============================================================================
    # 9. 씬 뷰 3D 카메라 투영 렌더링 및 2D 미니맵
    # ==============================================================================

    def world_to_camera(self, world_pos, right, up, forward):
        rel = world_pos - self.player_pos
        cam_x = rel.dot(right)
        cam_y = rel.dot(up)
        cam_z = rel.dot(forward)
        return cam_x, cam_y, cam_z

    def draw_projected_line(self, p1, p2, color, canvas, cx, cy, right, up, forward, width=1):
        cx1, cy1, cz1 = self.world_to_camera(p1, right, up, forward)
        cx2, cy2, cz2 = self.world_to_camera(p2, right, up, forward)
        
        if cz1 > 0.1 and cz2 > 0.1:
            fov = 450.0
            px1 = cx + (cx1 / cz1) * fov
            py1 = cy - (cy1 / cz1) * fov
            px2 = cx + (cx2 / cz2) * fov
            py2 = cy - (cy2 / cz2) * fov
            canvas.create_line(px1, py1, px2, py2, fill=color, width=width)

    def draw_3d_scene(self):
        canvas = self.canvas_3d
        canvas.delete("all")

        w = canvas.winfo_width()
        h = canvas.winfo_height()
        cx, cy = w / 2, h / 2

        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)

        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        # 바닥 격자 그리기
        for i in range(-15, 16):
            self.draw_projected_line(Vector3(float(i), 0.0, -15.0), Vector3(float(i), 0.0, 15.0), "#2f2f2f", canvas, cx, cy, right, up, forward)
            self.draw_projected_line(Vector3(-15.0, 0.0, float(i)), Vector3(15.0, 0.0, float(i)), "#2f2f2f", canvas, cx, cy, right, up, forward)

        # VR Target UI Board (3D 공간 UI 판넬)
        ui_corners = [
            Vector3(-0.8, 2.0, -2.5), Vector3(0.8, 2.0, -2.5),
            Vector3(0.8, 1.0, -2.5), Vector3(-0.8, 1.0, -2.5)
        ]
        proj_pts = []
        all_inside = True
        for pt in ui_corners:
            cx_val, cy_val, cz_val = self.world_to_camera(pt, right, up, forward)
            if cz_val > 0.1:
                fov = 450.0
                px = cx + (cx_val / cz_val) * fov
                py = cy - (cy_val / cz_val) * fov
                proj_pts.append((px, py))
            else:
                all_inside = False
                
        if len(proj_pts) == 4 and all_inside:
            ui_color = "#00FF66" if not self.ui_hover else "#FF007F"
            canvas.create_polygon(proj_pts, fill="#1c2420", outline=ui_color, width=2)
            tx = (proj_pts[0][0] + proj_pts[1][0]) / 2
            ty = (proj_pts[0][1] + proj_pts[1][1]) / 2 + 15
            canvas.create_text(tx, ty, text="VR Target UI Board", fill="#FFF", font=('Segoe UI', 9, 'bold'))

        # 모든 GameObject 렌더링
        for obj in self.game_objects:
            pos = obj.transform.get_world_position()
            rb = obj.get_component(RigidbodyComponent)
            radius = rb.radius if rb else 0.25
            
            # Transform Scale 반영 (X축 스케일 배율 적용)
            radius *= obj.transform.scale.x

            cx_val, cy_val, cz_val = self.world_to_camera(pos, right, up, forward)

            if cz_val > 0.1:
                fov = 450.0
                proj_r = (radius / cz_val) * fov
                px = cx + (cx_val / cz_val) * fov
                py = cy - (cy_val / cz_val) * fov

                # 아웃라인 하이라이트 (선택된 오브젝트 노란색 윤곽선)
                outline_color = "#FFFF00" if self.selected_object == obj else "#FFFFFF"
                outline_w = 2 if self.selected_object == obj else 1

                # MeshRenderer 색상 반영
                mesh = obj.get_component(MeshRenderer)
                color = mesh.color if mesh else "#888888"
                
                canvas.create_oval(px - proj_r, py - proj_r, px + proj_r, py + proj_r, fill=color, outline=outline_color, width=outline_w)
                canvas.create_text(px, py - proj_r - 8, text=obj.name, fill="#DCDCDC", font=('Segoe UI', 8))

                rot = obj.get_component(TargetRotator)
                if rot:
                    rot_mat = obj.transform.rotation.to_matrix()
                    lx = rot_mat[0][0] * 0.4 * obj.transform.scale.x
                    ly = rot_mat[1][0] * 0.4 * obj.transform.scale.y
                    lz = rot_mat[2][0] * 0.4 * obj.transform.scale.z
                    
                    vx = pos.x + lx
                    vy = pos.y + ly
                    vz = pos.z + lz
                    
                    cx_v, cy_v, cz_v = self.world_to_camera(Vector3(vx, vy, vz), right, up, forward)
                    if cz_v > 0.1:
                        v_px = cx + (cx_v / cz_v) * fov
                        v_py = cy - (cy_v / cz_v) * fov
                        canvas.create_line(px, py, v_px, v_py, fill="#FFFF00", width=3)

    # ==============================================================================
    # 10. 유니티 씬 카메라 조작 (WASD + 우클릭 회전 + 마우스 휠) 및 3D 드래그 이동
    # ==============================================================================

    def on_key_press(self, event):
        self.keys_pressed[event.keysym.lower()] = True

    def on_key_release(self, event):
        self.keys_pressed[event.keysym.lower()] = False

    def on_right_click_press(self, event):
        self.right_clicking = True
        self.right_click_start_x = event.x
        self.right_click_start_y = event.y
        self.canvas_3d.focus_set()

    def on_right_click_release(self, event):
        self.right_clicking = False
        dx = event.x - self.right_click_start_x
        dy = event.y - self.right_click_start_y
        dist = math.sqrt(dx**2 + dy**2)
        
        if dist < 3:
            self.spawn_click_pos = self.get_raycast_plane_intersection(event)
            self.context_menu.post(event.x_root, event.y_root)

    def on_right_click_drag(self, event):
        dx = event.x - self.right_click_start_x
        dy = event.y - self.right_click_start_y
        
        sensitivity = 0.003
        self.player_yaw += dx * sensitivity
        self.player_pitch -= dy * sensitivity
        
        self.player_pitch = max(-math.pi/2.1, min(math.pi/2.1, self.player_pitch))
        
        self.right_click_start_x = event.x
        self.right_click_start_y = event.y

    def on_mouse_wheel(self, event):
        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        
        zoom_speed = 0.5
        if event.delta > 0:
            self.player_pos = self.player_pos + forward * zoom_speed
        else:
            self.player_pos = self.player_pos - forward * zoom_speed

    def on_middle_click_press(self, event):
        self.middle_click_start_x = event.x
        self.middle_click_start_y = event.y

    def on_middle_click_drag(self, event):
        dx = event.x - self.middle_click_start_x
        dy = event.y - self.middle_click_start_y
        
        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        pan_speed = 0.015
        self.player_pos = self.player_pos - right * (dx * pan_speed) + up * (dy * pan_speed)
        
        self.middle_click_start_x = event.x
        self.middle_click_start_y = event.y

    def scene_select(self, event):
        # 마우스 좌클릭 시 선택 레이캐스트
        w = self.canvas_3d.winfo_width()
        h = self.canvas_3d.winfo_height()
        cx, cy = w / 2, h / 2
        
        mx = (event.x - cx) / cx
        my = (cy - event.y) / cy

        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        ray_dir = (right * mx + up * my + forward * 1.2).normalize()
        
        closest_t = 9999.0
        hit_obj = None
        
        for obj in self.game_objects:
            pos = obj.transform.get_world_position()
            rb = obj.get_component(RigidbodyComponent)
            radius = rb.radius if rb else 0.25
            radius *= obj.transform.scale.x # Scale 반영

            # Ray-Sphere intersection
            v = self.player_pos - pos
            b = 2.0 * ray_dir.dot(v)
            c = v.dot(v) - radius**2
            disc = b**2 - 4.0 * c
            
            if disc >= 0:
                t1 = (-b - math.sqrt(disc)) / 2.0
                t2 = (-b + math.sqrt(disc)) / 2.0
                t = t1 if t1 > 0.1 else t2
                if t > 0.1 and t < closest_t:
                    closest_t = t
                    hit_obj = obj
                    
        if hit_obj:
            self.selected_object = hit_obj
            self.dragging_object = hit_obj
            self.drag_depth = closest_t
            self.draw_inspector_fields()
            self.select_tree_by_id(hit_obj.id)
            self.log_to_console(f"Selected GameObject: '{hit_obj.name}'")
        else:
            self.selected_object = None
            self.dragging_object = None
            self.draw_inspector_empty()

    def scene_drag(self, event):
        # 좌클릭 드래그를 이용한 3D 트랜스폼 이동 조작
        if not self.dragging_object:
            return
            
        w = self.canvas_3d.winfo_width()
        h = self.canvas_3d.winfo_height()
        cx, cy = w / 2, h / 2
        
        mx = (event.x - cx) / cx
        my = (cy - event.y) / cy

        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        ray_dir = (right * mx + up * my + forward * 1.2).normalize()
        
        # 카메라 시선 깊이(drag_depth) 만큼 3D 공간 상에서 가상 이동
        P_new = self.player_pos + ray_dir * self.drag_depth
        
        rb = self.dragging_object.get_component(RigidbodyComponent)
        if rb:
            rb.velocity = Vector3(0, 0, 0)
            
        # 부모가 있다면 로컬 좌표계로 변환
        if self.dragging_object.parent:
            p_world = self.dragging_object.parent.transform.get_world_position()
            self.dragging_object.transform.position = P_new - p_world
        else:
            self.dragging_object.transform.position = P_new
            
        self.draw_inspector_fields()

    def scene_release(self, event):
        if self.dragging_object:
            self.log_to_console(f"Placed '{self.dragging_object.name}' at "
                                f"({self.dragging_object.transform.position.x:.2f}, "
                                f"{self.dragging_object.transform.position.y:.2f}, "
                                f"{self.dragging_object.transform.position.z:.2f})")
            self.dragging_object = None

    def scene_raycast(self, event):
        w = self.canvas_3d.winfo_width()
        h = self.canvas_3d.winfo_height()
        cx, cy = w / 2, h / 2
        
        mx = (event.x - cx) / cx
        my = (cy - event.y) / cy

        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        ray_dir = (right * mx + up * my + forward * 1.2).normalize()
        
        if abs(ray_dir.z) > 1e-4:
            t = (-2.5 - self.player_pos.z) / ray_dir.z
            if t >= 0:
                px = self.player_pos.x + t * ray_dir.x
                py = self.player_pos.y + t * ray_dir.y
                if -0.8 <= px <= 0.8 and 1.0 <= py <= 2.0:
                    self.ui_hover = True
                    return
        self.ui_hover = False

    def shoot_from_toolbar(self):
        # 툴바 또는 단축키 사격 기능
        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        
        bullet = GameObject(self.get_next_id(), f"Bullet_{self.object_counter}")
        bullet.transform.position = Vector3(self.player_pos.x, self.player_pos.y, self.player_pos.z)
        
        rb = bullet.add_component(RigidbodyComponent)
        rb.is_static = False
        rb.use_gravity = True
        rb.restitution = 0.3
        rb.velocity = forward * 12.0
        rb.radius = 0.15
        bullet.add_component(MeshRenderer, color="#FFFF00")

        self.game_objects.append(bullet)
        self.refresh_hierarchy()
        self.log_to_console(f"Fired bullet: {bullet.name}")
        
        threading.Thread(target=lambda: winsound.Beep(800, 80), daemon=True).start()

    # ==============================================================================
    # 11. 물리 엔진 및 에디터 씬 라이프사이클 업데이트
    # ==============================================================================

    def update_camera_movement(self):
        if not self.right_clicking:
            return
            
        dt_sec = self.dt
        move_speed = 4.0 * dt_sec
        
        cos_y = math.cos(self.player_yaw)
        sin_y = math.sin(self.player_yaw)
        cos_p = math.cos(self.player_pitch)
        sin_p = math.sin(self.player_pitch)
        
        forward = Vector3(cos_p * sin_y, sin_p, -cos_p * cos_y).normalize()
        right = Vector3(cos_y, 0.0, sin_y).normalize()
        up = right.cross(forward).normalize()

        if self.keys_pressed.get('w') or self.keys_pressed.get('W'):
            self.player_pos = self.player_pos + forward * move_speed
        if self.keys_pressed.get('s') or self.keys_pressed.get('S'):
            self.player_pos = self.player_pos - forward * move_speed
        if self.keys_pressed.get('a') or self.keys_pressed.get('A'):
            self.player_pos = self.player_pos - right * move_speed
        if self.keys_pressed.get('d') or self.keys_pressed.get('D'):
            self.player_pos = self.player_pos + right * move_speed
        if self.keys_pressed.get('e') or self.keys_pressed.get('E'):
            self.player_pos = self.player_pos + up * move_speed
        if self.keys_pressed.get('q') or self.keys_pressed.get('Q'):
            self.player_pos = self.player_pos - up * move_speed

    def reset_physics_velocity(self):
        for obj in self.game_objects:
            rb = obj.get_component(RigidbodyComponent)
            if rb and not rb.is_static:
                rb.velocity = Vector3(0, 0, 0)
        self.log_to_console("Physics calculation sequence synchronized.")

    def reset_whole_scene(self):
        self.score = 0
        self.score_display.config(text="SCORE: 0")
        self.spawn_default_scene()
        self.selected_object = None
        self.draw_inspector_empty()
        self.refresh_hierarchy()
        self.player_pos = Vector3(0.0, 2.0, 5.0)
        self.player_yaw = 0.0
        self.player_pitch = -0.2
        self.log_to_console("Scene reset. Camera position restored.")

    def loop(self):
        current_time = time.time()
        frame_time = current_time - self.last_time
        self.last_time = current_time

        self.accumulator += frame_time
        while self.accumulator >= self.dt:
            self.update_camera_movement()
            for obj in self.game_objects:
                obj.update(self.dt)
            self.solve_scene_collisions()
            self.accumulator -= self.dt

        self.draw_3d_scene()
        self.after(16, self.loop)

    def solve_scene_collisions(self):
        for i in range(len(self.game_objects)):
            objA = self.game_objects[i]
            rbA = objA.get_component(RigidbodyComponent)
            if not rbA:
                continue

            posA = objA.transform.get_world_position()
            radiusA = rbA.radius * objA.transform.scale.x

            if not rbA.is_static and posA.y - radiusA <= 0.0:
                objA.transform.position.y = radiusA
                if rbA.velocity.y < 0.0:
                    rbA.velocity.y = -rbA.velocity.y * rbA.restitution
                    threading.Thread(target=lambda: winsound.Beep(300, 50), daemon=True).start()

            for j in range(i + 1, len(self.game_objects)):
                objB = self.game_objects[j]
                rbB = objB.get_component(RigidbodyComponent)
                if not rbB:
                    continue

                posB = objB.transform.get_world_position()
                radiusB = rbB.radius * objB.transform.scale.x
                
                d = posA - posB
                dist = d.length()
                min_dist = radiusA + radiusB

                if dist < min_dist:
                    normal = d.normalize()
                    penetration = min_dist - dist

                    if not rbA.is_static:
                        objA.transform.position = objA.transform.position + normal * penetration
                    if not rbB.is_static:
                        objB.transform.position = objB.transform.position - normal * penetration

                    rv = rbA.velocity - rbB.velocity
                    vel_normal = rv.dot(normal)
                    if vel_normal < 0.0:
                        e = min(rbA.restitution, rbB.restitution)
                        massA_inv = 0.0 if rbA.is_static else 1.0 / rbA.mass
                        massB_inv = 0.0 if rbB.is_static else 1.0 / rbB.mass
                        
                        j_impulse = -(1.0 + e) * vel_normal / (massA_inv + massB_inv)
                        if not rbA.is_static:
                            rbA.velocity = rbA.velocity + normal * (j_impulse * massA_inv)
                        if not rbB.is_static:
                            rbB.velocity = rbB.velocity - normal * (j_impulse * massB_inv)

                        if rbB.is_static and not rbA.is_static:
                            self.score += 1
                            self.score_display.config(text=f"SCORE: {self.score}")
                            self.log_to_console(f"Hit registered! '{objB.name}' struck by '{objA.name}'")
                            threading.Thread(target=lambda: winsound.Beep(900, 100), daemon=True).start()


if __name__ == "__main__":
    app = UnityLikeEditor()
    app.mainloop()
