// OpenBW 완전 플레이 가능 게임 - 모든 기능 구현

#include "bwgame.h"
#include "native_window.h"
#include "native_window_drawing.h"
#include "native_sound.h"
#include "ui_custom.h"  // ★ 스타크래프트 UI 패널
#include "ui/common.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <functional>
#include <exception>
#include <csignal>
#include <map>
#include <SDL2/SDL.h>  // SDL 함수 사용을 위해
#ifndef OPENBW_NO_SDL_IMAGE
#include <SDL2/SDL_image.h>  // PNG 이미지 로드
#endif

#ifdef EMSCRIPTEN
 #include <emscripten/emscripten.h>
 #include <unistd.h>
 #include <errno.h>
#endif

// 프로토스 아이콘 이미지 데이터 (전역)
bwgame::a_vector<uint8_t> g_protoss_icon_indices;  // 8비트 팔레트
bwgame::a_vector<uint32_t> g_protoss_icon_rgba;    // RGBA 원본 데이터
int g_protoss_icon_width = 0;
int g_protoss_icon_height = 0;
bool g_protoss_icons_loaded = false;

// Armor 툴팁 이미지 데이터 (전역)
bwgame::a_vector<uint32_t> g_armor_tooltip_rgba;
int g_armor_tooltip_width = 0;
int g_armor_tooltip_height = 0;
bool g_armor_tooltip_loaded = false;

// Shield 툴팁 이미지 데이터 (전역)
bwgame::a_vector<uint32_t> g_shield_tooltip_rgba;
int g_shield_tooltip_width = 0;
int g_shield_tooltip_height = 0;
bool g_shield_tooltip_loaded = false;

// UI command card category for ui_custom.h dynamic labels
// 0=default(units), 1=Nexus, 2=Gateway, 3=Hatchery/Lair/Hive, 4=Command Center, 5=AttackBuilding, 6=Resource
int g_ui_selected_category = 0;

// UI active button index (현재 활성화된 버튼 인덱스)
// -1 = 활성 버튼 없음, 0~8 = 해당 버튼 활성화
// 유닛: 0=M, 1=S, 2=A, 3=P, 4=H, 5=G, 6=B, 7=V
// 공격 건물: 0=A, 1=S
int g_ui_active_button = 1;  // 기본값: S (정지 상태)

// UI command mode active (명령 모드 활성화 여부)
// false = 일반 모드, true = 명령 모드 (R키, O키 등)
bool g_ui_command_mode_active = false;

// UI mouse coordinates (마우스 UI 좌표)
int g_ui_mouse_x = 0;
int g_ui_mouse_y = 0;

// UI 좌표 표시 여부 (F9 키로 토글)
bool g_ui_show_coordinates = true;

// 타겟 링 시각화 (스타크래프트 스타일)
// g_target_ring_kind: 0=self/ally, 1=enemy, 2=resource
bool g_target_ring_active = false;
int g_target_ring_x = 0;
int g_target_ring_y = 0;
int g_target_ring_radius = 0;
int g_target_ring_frames = 0;
int g_target_ring_kind = 0;

// 명령 링 (우클릭/A공격 등 명령 시 땅에 표시)
int g_command_ring_x = 0;
int g_command_ring_y = 0;
int g_command_ring_radius = 0;
int g_command_ring_frames = 0;
int g_command_ring_kind = 0;
#ifdef _WIN32
#include <windows.h>
#endif

using namespace bwgame;
using ui::log;

FILE* log_file = nullptr;

// 멀티 타겟 링 UI 버퍼 정의 (ui_custom.h의 extern과 매칭)
namespace bwgame {
	a_vector<ui_target_ring_t> g_target_rings;
}

namespace bwgame {
namespace ui {

void log_str(a_string str) {
    fwrite(str.data(), str.size(), 1, stdout);
    fflush(stdout);
    if (!log_file) log_file = fopen("log.txt", "wb");
    if (log_file) {
        fwrite(str.data(), str.size(), 1, log_file);
        fflush(log_file);
    }
}

// (moved on_left_button_up implementation below the class definition)

void fatal_error_str(a_string str) {
    log("fatal error: %s\n", str);
    std::terminate();
}

}
}

// 플레이어 입력 핸들러 - 완전 구현
static void crash_log(const char* tag, const char* extra = nullptr) {
    if (extra) ui::log("%s %s\n", tag, extra);
    else ui::log("%s\n", tag);
    if (log_file) fflush(log_file);
}

static void on_terminate_handler() {
    crash_log("[CRASH_UNHANDLED] std::terminate");
    std::abort();
}

static void on_signal_handler(int sig) {
    switch (sig) {
        case SIGSEGV: crash_log("[CRASH_UNHANDLED] SIGSEGV"); break;
        case SIGABRT: crash_log("[CRASH_UNHANDLED] SIGABRT"); break;
        default: crash_log("[CRASH_UNHANDLED] SIGNAL"); break;
    }
    std::abort();
}

#ifdef _WIN32
static LONG WINAPI seh_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    ui::log("[CRASH_SEH] code=0x%08lx at=%p\n", (unsigned long)code, info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr);
    if (log_file) fflush(log_file);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void install_crash_handlers() {
    std::set_terminate(on_terminate_handler);
    std::signal(SIGABRT, on_signal_handler);
#ifdef SIGSEGV
    std::signal(SIGSEGV, on_signal_handler);
#endif
#ifdef _WIN32
    SetUnhandledExceptionFilter(seh_filter);
#endif
}
struct player_input_handler {
    state& st;
    action_state& action_st;
    action_functions actions;
    
    int player_id = 0;
    
    // 마우스 상태
    xy mouse_pos;
    xy mouse_screen_pos;
    bool left_button_down = false;
    bool right_button_down = false;
    xy drag_start_pos;
    xy drag_start_screen_pos;  // 드래그 시작 화면 좌표 추가
    xy drag_start_camera_pos;  // 드래그 시작 시 카메라 위치 (크래시 방지)
    bool is_dragging = false;
    std::chrono::high_resolution_clock::time_point drag_start_time;
    
    // 키보드 상태
    bool shift_pressed = false;
    bool ctrl_pressed = false;
    bool alt_pressed = false;
    bool allow_enemy_control = false; // 원작 기본: false
    // UI toast callback (set by player_game)
    std::function<void(const char*, int)> toast_cb;
    // Selection suppression (attack/build modes)
    std::function<void(bool)> selection_suppress_cb;
    bool zerg_build_anywhere = false; // F6 toggle (test)
    
    // 명령 모드
    enum class CommandMode {
        Normal,
        Attack,         // A키
        Move,           // M키
        Patrol,         // P키
        Hold,           // H키
        Build,          // B키
        Gather,         // G키
        AdvancedBuild,  // V키
        Rally,          // 렐리 포인트 설정 모드 (좌클릭/우클릭)
        AutoRally       // 자동 렐리 포인트 모드 (우클릭만, 자원만)
    };
    CommandMode command_mode = CommandMode::Normal;
    
    // Gather 모드 종료 후 선택 보호 (프레임 카운터)
    int skip_selection_sync_frames = 0;
    
    // Attack/Patrol 모드에서 Down에서 명령 실행 시 Up에서 선택 무시
    bool command_issued_on_down = false;
    
    // 타겟 링 시각화 (유닛/건물 클릭 시)
    xy target_ring_pos{0, 0};
    int target_ring_radius = 0;
    int target_ring_frames = 0;  // 남은 프레임 수 (단일 링, 하위 호환용)
    int target_ring_total_frames = 0; // 전체 수명 (깜빡임 제어용)

    // 멀티 타겟 링 시각화용 구조체 (포인터 보관 금지)
    struct TargetRing {
        xy pos{0, 0};
        int radius = 0;
        int frames = 0; // 남은 프레임 수
        int kind = 0;   // 0=self/ally, 1=enemy, 2=resource
        unit_t* target_unit = nullptr; // 유닛 타겟인 경우 해당 유닛을 따라가기 위한 포인터 (없으면 nullptr)
    };

    // 최근 타겟 링 여러 개를 유지 (각각 5초 동안 유지 후 자동 제거)
    a_vector<TargetRing> target_rings;
    static constexpr int MAX_TARGET_RINGS = 16;
    
    // 명령 링 시각화 (우클릭/A공격 등 명령 시 땅에 표시)
    xy command_ring_pos{0, 0};
    int command_ring_radius = 0;
    int command_ring_frames = 0;
    int command_ring_kind = 0;  // 0=기본, 1=공격, 2=정찰
    
    // 렐리 포인트 시각화 (지속적 표시 - 다중 건물 지원)
    struct RallyVisualization {
        xy building_pos{0, 0};
        xy rally_pos{0, 0};
        bool is_active = false;
        bool is_auto_rally = false;  // true=노란색(6번), false=초록색(5번)
        int frames_remaining = 0;  // 일시적 표시용 (설정 직후 강조)
        unit_t* last_selected_building = nullptr;  // 마지막 선택된 건물
    };
    RallyVisualization rally_visual;
    
    // 다중 렐리 포인트 시각화 (여러 건물 선택 시)
    struct MultiRallyPoint {
        xy building_pos;
        xy rally_pos;
        bool is_auto_rally;
    };
    a_vector<MultiRallyPoint> multi_rally_points;
    
    // 건물별 추가 렐리 포인트 저장 (5번 일반 렐리 + 6번 오토 렐리 동시 저장)
    struct BuildingRallyPoints {
        unit_t* building = nullptr;
        
        // 5번 일반 렐리 (초록색)
        bool has_normal_rally = false;
        xy normal_rally_pos{0, 0};
        unit_t* normal_rally_target = nullptr;
        
        // 6번 오토 렐리 (노란색)
        bool has_auto_rally = false;
        xy auto_rally_pos{0, 0};
        unit_t* auto_rally_target = nullptr;
    };
    a_vector<BuildingRallyPoints> building_rally_storage;
    
    // 타임스탬프
    std::chrono::high_resolution_clock::time_point start_time;
    
    // 부대지정 더블클릭 감지 (화면 이동) - 메인 10개 + 넘패드 10개 = 20개
    std::array<std::chrono::high_resolution_clock::time_point, 20> last_group_select_time;
    int last_group_index_for_double_click = -1;
    static constexpr int DOUBLE_CLICK_MS = 500;  // 0.5초 이내 더블클릭
    
    // 마지막 알림 위치 (Ctrl+C, Space로 이동)
    xy last_alert_pos{0, 0};
    bool has_alert_pos = false;
    
    player_input_handler(state& st, action_state& action_st) 
        : st(st), action_st(action_st), actions(st, action_st) {
        start_time = std::chrono::high_resolution_clock::now();
        // 부대지정 타이머 초기화
        for (auto& t : last_group_select_time) {
            t = std::chrono::high_resolution_clock::time_point::min();
        }
        last_group_index_for_double_click = -1;
    }
    
    // Build UI state and helpers
    const unit_type_t* building_to_place = nullptr;
    bool build_preview_active = false;
    xy build_preview_pos{};
    bool build_preview_valid = false;
    // 디버그: 프리뷰 변경 사항 로그 최소화용 상태
    xy last_logged_preview_tile{-1, -1};
    bool last_logged_preview_valid = false;
    
    int get_effective_owner() const {
        if (allow_enemy_control) {
            for (int owner = 0; owner < 8; ++owner) {
                if (!actions.action_st.selection.at(owner).empty()) return owner;
            }
        }
        return player_id;
    }
    void clear_all_selections() {
        for (int owner = 0; owner < 8; ++owner) actions.action_st.selection.at(owner).clear();
        // 선택을 전부 해제하면, 빌드/고급 빌드 모드도 함께 종료하여
        // 새로 선택한 일꾼이 자동으로 건설 모드에 남지 않도록 한다.
        if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
            exit_build_mode();
        }
    }

    bool is_selected_worker_protoss() {
        int pid = get_effective_owner();
        unit_t* u = actions.get_single_selected_unit(pid);
        return u && actions.unit_is(u, UnitTypes::Protoss_Probe);
    }
    bool is_selected_worker_zerg() {
        int pid = get_effective_owner();
        unit_t* u = actions.get_single_selected_unit(pid);
        return u && actions.unit_is(u, UnitTypes::Zerg_Drone);
    }
    bool is_selected_single_worker() {
        int pid = get_effective_owner();
        unit_t* u = actions.get_single_selected_unit(pid);
        return u && (actions.unit_is(u, UnitTypes::Protoss_Probe) || 
                     actions.unit_is(u, UnitTypes::Zerg_Drone) ||
                     actions.unit_is(u, UnitTypes::Terran_SCV));
    }
    
    // 선택된 유닛들의 공통 가능 명령 추출
    std::set<CommandMode> get_available_commands() {
        std::set<CommandMode> available;
        
        int pid = get_effective_owner();
        
        // 선택된 유닛 목록 가져오기
        a_vector<unit_t*> selected_units;
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                for (unit_t* u : action_st.selection[p]) {
                    selected_units.push_back(u);
                }
            }
        } else {
            for (unit_t* u : action_st.selection[pid]) {
                selected_units.push_back(u);
            }
        }
        
        // 선택된 유닛이 없으면 빈 세트 반환
        if (selected_units.empty()) return available;
        
        // 건물이 포함되어 있는지 체크
        bool has_building = false;
        bool has_unit = false;
        bool has_worker = false;
        
        for (unit_t* u : selected_units) {
            if (u->unit_type->flags & unit_type_t::flag_building) {
                has_building = true;
            } else {
                has_unit = true;
                // 일꾼 체크
                if (actions.unit_is(u, UnitTypes::Protoss_Probe) || 
                    actions.unit_is(u, UnitTypes::Zerg_Drone) ||
                    actions.unit_is(u, UnitTypes::Terran_SCV)) {
                    has_worker = true;
                }
            }
        }
        
        // 건물과 유닛이 혼합 선택되었으면 명령 없음 (스타크래프트 원작 규칙)
        if (has_building && has_unit) {
            // 카테고리 8: 건물 + 유닛 혼합이지만 유닛 기본 명령만 허용 (M,S,A,P,H)
            if (g_ui_selected_category == 8) {
                available.insert(CommandMode::Move);
                available.insert(CommandMode::Attack);
                available.insert(CommandMode::Patrol);
                available.insert(CommandMode::Hold);
                return available;
            }
            // 카테고리 9: 건물/자원 + 일꾼 유닛만 선택 - M,S,A,P,H,G 허용
            if (g_ui_selected_category == 9) {
                available.insert(CommandMode::Move);
                available.insert(CommandMode::Attack);
                available.insert(CommandMode::Patrol);
                available.insert(CommandMode::Hold);
                available.insert(CommandMode::Gather);
                return available;
            }
            return available;
        }
        
        // 건물만 선택된 경우
        if (has_building && !has_unit) {
            // 건물 1개만 선택되었을 때만 명령 가능
            if (selected_units.size() == 1) {
                unit_t* building = selected_units[0];
                
                // 넥서스: 프로브 생산, 렐리 포인트
                if (actions.unit_is(building, UnitTypes::Protoss_Nexus)) {
                    available.insert(CommandMode::Rally);
                }
                // 해처리/레어/하이브: 드론 생산, 렐리 포인트
                else if (actions.unit_is(building, UnitTypes::Zerg_Hatchery) ||
                         actions.unit_is(building, UnitTypes::Zerg_Lair) ||
                         actions.unit_is(building, UnitTypes::Zerg_Hive)) {
                    available.insert(CommandMode::Rally);
                }
                // 커맨드 센터: SCV 생산, 렐리 포인트
                else if (actions.unit_is(building, UnitTypes::Terran_Command_Center)) {
                    available.insert(CommandMode::Rally);
                }
                // 게이트웨이: 질럿/드라군 생산, 렐리 포인트
                else if (actions.unit_is(building, UnitTypes::Protoss_Gateway)) {
                    available.insert(CommandMode::Rally);
                }
                // 다른 생산 건물들도 여기에 추가 가능
            }
            return available;
        }
        
        // 유닛만 선택된 경우
        if (has_unit && !has_building) {
            // 기본 명령 (모든 유닛 가능)
            available.insert(CommandMode::Move);
            available.insert(CommandMode::Attack);
            available.insert(CommandMode::Patrol);
            available.insert(CommandMode::Hold);
            
            // Gather: 일꾼만 선택되었거나, 일꾼 2기 이상만 선택된 경우에만 가능
            // 일꾼 + 다른 유닛 혼합 시에는 Gather 불가
            int worker_count = 0;
            int non_worker_count = 0;
            for (unit_t* u : selected_units) {
                bool is_worker = (actions.unit_is(u, UnitTypes::Protoss_Probe) || 
                                 actions.unit_is(u, UnitTypes::Zerg_Drone) ||
                                 actions.unit_is(u, UnitTypes::Terran_SCV));
                if (is_worker) worker_count++;
                else non_worker_count++;
            }
            
            // 일꾼만 선택되었을 때만 Gather 가능
            if (worker_count > 0 && non_worker_count == 0) {
                available.insert(CommandMode::Gather);
            }
            
            // 일꾼 1기만 선택된 경우 건설 명령 추가
            if (has_worker && selected_units.size() == 1) {
                available.insert(CommandMode::Build);
                available.insert(CommandMode::AdvancedBuild);
            }
        }
        
        return available;
    }
    
    // 현재 선택된 유닛이 특정 명령을 사용할 수 있는지 확인
    bool can_use_command(CommandMode mode) {
        auto available = get_available_commands();
        return available.count(mode) > 0;
    }

    // 실제 선택된 일꾼과 소유자 탐색 (F5 ON 시 모든 소유자에서 탐색, 다중 선택에서도 탐색)
    std::pair<int, unit_t*> find_selected_worker_any_owner() {
        auto is_worker = [&](unit_t* u) {
            return u && (actions.unit_is(u, UnitTypes::Protoss_Probe) || actions.unit_is(u, UnitTypes::Zerg_Drone));
        };
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) {
                for (unit_t* u : action_st.selection[pid]) if (is_worker(u)) return {pid, u};
            }
            return {-1, nullptr};
        }
        for (unit_t* u : action_st.selection[player_id]) if (is_worker(u)) return {player_id, u};
        return {-1, nullptr};
    }
    void enter_build_mode() {
        command_mode = CommandMode::Build;
        if (selection_suppress_cb) selection_suppress_cb(true);
    }
    void exit_build_mode() {
        command_mode = CommandMode::Normal;
        building_to_place = nullptr;
        build_preview_active = false;
        build_preview_valid = false;
        g_ui_active_button = -1;  // 빌드 모드 종료 시에는 아무 버튼도 하이라이트하지 않음
        if (selection_suppress_cb) selection_suppress_cb(false);
    }
    
    // 명령 모드 취소 시 하이라이트 초기화
    void reset_command_mode() {
        command_mode = CommandMode::Normal;
        g_ui_active_button = -1;          // 버튼 하이라이트 해제
        g_ui_command_mode_active = false; // UI 명령 모드 비활성화
        if (selection_suppress_cb) selection_suppress_cb(false);
    }

    void set_building_to_place(const unit_type_t* ut) {
        building_to_place = ut;
        build_preview_active = (ut != nullptr);  // 건물 선택 시 즉시 프리뷰 활성화
        log("[SET_BUILDING] building_to_place=%p, type_id=%d, preview_active=%d\n", 
            (void*)ut, ut ? (int)ut->id : -1, (int)build_preview_active);
        if (selection_suppress_cb) selection_suppress_cb(true);
    }
    
    // 자동 렐리 포인트 설정
    void set_auto_rally_point(unit_t* building, int pid);
    
    // 건물의 렐리 포인트 저장/조회
    BuildingRallyPoints* get_building_rally_storage(unit_t* building) {
        for (auto& brp : building_rally_storage) {
            if (brp.building == building) return &brp;
        }
        return nullptr;
    }
    
    BuildingRallyPoints* get_or_create_building_rally_storage(unit_t* building) {
        auto* existing = get_building_rally_storage(building);
        if (existing) return existing;
        
        BuildingRallyPoints brp;
        brp.building = building;
        building_rally_storage.push_back(brp);
        return &building_rally_storage.back();
    }
    
    void set_normal_rally(unit_t* building, xy pos, unit_t* target) {
        auto* brp = get_or_create_building_rally_storage(building);
        brp->has_normal_rally = true;
        brp->normal_rally_pos = pos;
        brp->normal_rally_target = target;
        log("[RALLY_STORAGE] 일반 렐리 저장: building=%p, pos=(%d,%d), target=%p\n", 
            (void*)building, pos.x, pos.y, (void*)target);
    }
    
    void set_auto_rally(unit_t* building, xy pos, unit_t* target) {
        auto* brp = get_or_create_building_rally_storage(building);
        brp->has_auto_rally = true;
        brp->auto_rally_pos = pos;
        brp->auto_rally_target = target;
        log("[RALLY_STORAGE] 오토 렐리 저장: building=%p, pos=(%d,%d), target=%p\n", 
            (void*)building, pos.x, pos.y, (void*)target);
    }
    
    // 이벤트 처리
    void handle_event(const native_window::event_t& e, xy screen_pos);
    
    // 마우스 이벤트
    void on_mouse_move(xy pos, xy screen_mouse_pos);
    void on_left_button_down(xy pos, xy screen_mouse_pos);
    void on_left_button_up(xy pos);
    void on_right_click(xy pos);  // 구현 필요
    void on_command_button_click(int btn_index);
    
    // 화면 이동 함수 (카메라 콜백 필요)
    std::function<void(xy)> move_camera_cb;
    
    // 키보드 이벤트
    void on_key_down(int scancode);
    void on_key_up(int scancode);
    
    // 유닛 찾기
    unit_t* find_unit_at(xy pos);
    void select_units_in_rect(rect area);
    
    // 좌표 변환
    xy screen_to_game(xy screen_pos, xy screen_offset) const {
        return screen_pos + screen_offset;
    }
    
    // 타임스탬프 가져오기
    long long get_timestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    }
    
    // 타겟 링 업데이트 (매 프레임 호출)
    void update_target_ring() {
        if (target_ring_frames > 0) {
            target_ring_frames--;
        }
        // 멀티 타겟 링 수명 감소 및 만료된 링 제거
        for (size_t i = 0; i < target_rings.size();) {
            auto& r = target_rings[i];

            // 유닛 타겟인 경우, 해당 유닛의 현재 위치를 따라가도록 갱신
            if (r.target_unit && r.target_unit->sprite) {
                r.pos = r.target_unit->sprite->position;
            }

            if (r.frames > 0) {
                r.frames--;
            }
            if (r.frames <= 0) {
                target_rings.erase(target_rings.begin() + i);
            } else {
                ++i;
            }
        }
        // 멀티 타겟 링이 하나라도 존재하면 지상 커맨드 링은 완전히 비활성화
        if (!target_rings.empty()) {
            if (command_ring_frames > 0) {
                log("[RING_SYNC] target_rings active → clear command_ring (frames=%d, pos=(%d,%d))\n",
                    command_ring_frames, command_ring_pos.x, command_ring_pos.y);
            }
            command_ring_frames = 0;
        } else if (command_ring_frames > 0) {
            // 멀티 링이 없을 때만 커맨드 링 수명 감소
            command_ring_frames--;
        }
        if (rally_visual.frames_remaining > 0) {
            rally_visual.frames_remaining--;
            if (rally_visual.frames_remaining == 0) {
                rally_visual.is_active = false;
            }
        }
    }
};

void player_input_handler::handle_event(const native_window::event_t& e, xy screen_pos) {
    long long ts = get_timestamp();
    
    // 맵 경계 (픽셀 단위)
    int mw = (int)st.game->map_width - 1;
    int mh = (int)st.game->map_height - 1;
    
    // UI 영역 체크 (미니맵 및 하단 패널)
    bool is_ui_area = (e.mouse_y > 550);
    
    switch (e.type) {
        case native_window::event_t::type_mouse_motion:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            mouse_pos = screen_to_game(mouse_screen_pos, screen_pos);
            // 맵 경계 내로 제한
            mouse_pos.x = std::max(0, std::min(mouse_pos.x, mw));
            mouse_pos.y = std::max(0, std::min(mouse_pos.y, mh));
            // UI 영역이 아닐 때만 전달
            if (!is_ui_area) {
                on_mouse_move(mouse_pos, mouse_screen_pos);
            }
            break;
            
        case native_window::event_t::type_mouse_button_down:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            log("[SDL_DOWN] btn=%d, screen=(%d,%d), is_ui=%d\n", e.button, e.mouse_x, e.mouse_y, (int)is_ui_area);
            
            if (is_ui_area && e.button == 1) {
                // UI 영역에서는 드래그 상태 강제 초기화만 하고 폴링 루프에서 처리
                log("[SDL_DOWN] UI 영역, 드래그 초기화\n");
                this->left_button_down = false;
                this->is_dragging = false;
                // break 제거: 폴링 루프에서 버튼 클릭 처리하도록 허용
            } else {
                // 게임 영역에서만 게임 로직 처리
                mouse_pos = screen_to_game(mouse_screen_pos, screen_pos);
                // 맵 경계 내로 제한
                mouse_pos.x = std::max(0, std::min(mouse_pos.x, mw));
                mouse_pos.y = std::max(0, std::min(mouse_pos.y, mh));
                log("[SDL_DOWN] 게임 영역, game_pos=(%d,%d)\n", mouse_pos.x, mouse_pos.y);
                
                if (e.button == 1) {
                    log("[SDL_DOWN] on_left_button_down 호출\n");
                    on_left_button_down(mouse_pos, mouse_screen_pos);
                }
            }
            break;
            
        case native_window::event_t::type_mouse_button_up:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            log("[SDL_UP] btn=%d, screen=(%d,%d), is_ui=%d\n", e.button, e.mouse_x, e.mouse_y, (int)is_ui_area);
            
            if (is_ui_area) {
                // UI 영역에서는 드래그 상태 초기화만 하고 폴링 루프에서 처리
                mouse_pos = xy(0, 0);
                this->left_button_down = false;
                this->is_dragging = false;
                log("[SDL_UP] UI 영역, 드래그 초기화, 폴링 루프에서 버튼 처리\n");
                // break 제거: 폴링 루프에서 버튼 클릭 처리하도록 허용
            } else {
                // 게임 영역: 폴링 루프에서 처리하도록 드래그 상태만 초기화
                this->left_button_down = false;
                this->is_dragging = false;
                log("[SDL_UP] 게임 영역, 폴링 루프에서 처리\n");
                // on_left_button_up 호출 제거: 폴링 루프에서만 처리
            }
            break;
            
        case native_window::event_t::type_key_down:
            log("[%lld ms] 키 DOWN: scancode=%d\n", ts, e.scancode);
            on_key_down(e.scancode);
            break;
            
        case native_window::event_t::type_key_up:
            log("[%lld ms] 키 UP: scancode=%d\n", ts, e.scancode);
            on_key_up(e.scancode);
            break;
            
        default:
            break;
    }
}

void player_input_handler::on_mouse_move(xy pos, xy screen_mouse_pos) {
    // Do not start or maintain drag while in Build mode
    if (command_mode == CommandMode::Build) {
        is_dragging = false;
        return;
    }
    
    // UI 영역에서는 드래그 방지
    bool is_ui_area = (screen_mouse_pos.y > 550);
    if (is_ui_area) {
        is_dragging = false;
        left_button_down = false;  // UI 영역에서는 드래그 상태 초기화
        return;
    }
    
    // 맵 경계 내로 좌표 제한 (픽셀 단위)
    int mw = (int)st.game->map_width - 1;
    int mh = (int)st.game->map_height - 1;
    pos.x = std::max(0, std::min(pos.x, mw));
    pos.y = std::max(0, std::min(pos.y, mh));
    
    if (left_button_down) {
        if (!is_dragging) {
            // 드래그 시작 위치와 현재 위치의 차이 계산 (화면 좌표 기준)
            int dx = screen_mouse_pos.x - drag_start_screen_pos.x;
            int dy = screen_mouse_pos.y - drag_start_screen_pos.y;
            int dist_sq = dx * dx + dy * dy;
            
            // 카메라 이동 감지: 게임 좌표 차이가 화면 좌표 차이와 크게 다르면 카메라가 이동한 것
            int game_dx = pos.x - drag_start_pos.x;
            int game_dy = pos.y - drag_start_pos.y;
            int game_dist_sq = game_dx * game_dx + game_dy * game_dy;
            
            // 화면 좌표 이동은 작은데 게임 좌표 이동이 크면 카메라 이동
            bool camera_moved = (dist_sq < 100 && game_dist_sq > 10000);
            
            if (camera_moved) {
                log("[CAMERA_MOVED] 카메라 이동 감지, 드래그 취소: 화면거리^2=%d, 게임거리^2=%d\n", 
                    dist_sq, game_dist_sq);
                left_button_down = false;
                is_dragging = false;
                return;
            }
            
            // 드래그 임계값을 높여서 안정성 향상 (50픽셀 이상 이동 필요)
            if (dist_sq > 2500) {  // 50픽셀 이상 이동 (2500 = 50^2)
                is_dragging = true;
                log(">>> 드래그 시작 확정: 시작=(%d,%d), 현재=(%d,%d)\n", 
                    drag_start_pos.x, drag_start_pos.y, pos.x, pos.y);
            }
        }
    }
}

void player_input_handler::on_left_button_down(xy pos, xy screen_mouse_pos) {
    // 미니맵 및 UI 영역 클릭 시 드래그 방지 (크래시 방지)
    // 미니맵은 왼쪽 하단 (x < 200, y > 550)
    // UI 패널은 화면 하단 전체 (y > 550)
    bool is_ui_area = (screen_mouse_pos.y > 550);
    
    left_button_down = !is_ui_area;  // UI 영역 클릭 시 드래그 시작 안 함
    drag_start_pos = pos;
    drag_start_screen_pos = screen_mouse_pos;
    is_dragging = false;
    drag_start_time = std::chrono::high_resolution_clock::now();
    
    if (is_ui_area) {
        log("[UI] UI 영역 클릭 감지, 드래그 방지: 화면=(%d,%d)\n", 
            screen_mouse_pos.x, screen_mouse_pos.y);
    }

    // 공격/정찰 모드는 좌클릭 다운 즉시 확정 처리하여 반응속도 향상
    if (!is_ui_area) {
        // Attack confirm on down
        if (command_mode == CommandMode::Attack) {
            left_button_down = false;
            command_issued_on_down = true;  // Up에서 선택 무시
            log("[공격확정] 좌클릭(Down) at (%d,%d)\n", pos.x, pos.y);
            unit_t* target = find_unit_at(pos);
            
            // 타겟 링 시각화 (유닛/건물/자원 클릭 시)
            if (target) {
                // 자기 자신에게 우클릭/스킬 사용하는 경우에는 링 시각화 표시하지 않음
                bool is_self_target = false;
                int eff_owner = get_effective_owner();
                if (!allow_enemy_control) {
                    auto& sel = action_st.selection[eff_owner];
                    if (sel.size() == 1 && sel.front() == target) is_self_target = true;
                } else {
                    for (int owner = 0; owner < 8 && !is_self_target; ++owner) {
                        auto& sel = action_st.selection[owner];
                        if (sel.size() == 1 && sel.front() == target) is_self_target = true;
                    }
                }
                if (!is_self_target) {
                    xy local_pos = target->sprite->position;
                    int unit_width = target->unit_type->dimensions.to.x - target->unit_type->dimensions.from.x;
                    int unit_height = target->unit_type->dimensions.to.y - target->unit_type->dimensions.from.y;
                    int local_radius = std::max(unit_width, unit_height) / 2 + 15;  // 크기 증가
                    
                    // 타겟 종류에 따라 링 색상 구분: 0=아군/자신, 1=적군, 2=자원
                    bool is_resource = actions.unit_is_mineral_field(target) || 
                                       actions.unit_is(target, UnitTypes::Resource_Vespene_Geyser) ||
                                       actions.unit_is_refinery(target);
                    int local_kind = is_resource ? 2 : (target->owner == player_id ? 0 : 1);

                    // 멀티 타겟 링에도 추가
                    TargetRing tr;
                    tr.pos = local_pos;
                    tr.radius = local_radius;
                    tr.frames = 120; // 5초
                    tr.kind = local_kind;
                    tr.target_unit = target; // 유닛/건물/자원 타겟일 때 해당 유닛을 따라가도록 설정
                    if (target_rings.size() >= MAX_TARGET_RINGS) {
                        target_rings.erase(target_rings.begin());
                    }
                    target_rings.push_back(tr);

                    log("[TARGET_RING] 공격 타겟 링: pos=(%d,%d), radius=%d (멀티링 size=%d)\n", 
                        target_ring_pos.x, target_ring_pos.y, target_ring_radius, (int)target_rings.size());
                }
            }
            auto is_attack_building = [&](unit_t* u) -> bool {
                if (!u) return false;
                return (
                    actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
                    actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
                    actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
                    actions.unit_is(u, UnitTypes::Terran_Bunker) ||
                    actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
                );
            };
            auto issue_attack_for = [&](int pid) {
                bool has_attack_building = false;
                for (unit_t* u : action_st.selection[pid]) {
                    if (is_attack_building(u)) { has_attack_building = true; break; }
                }

                if (target) {
                    // 1) 공격 건물: 적군 타겟이면 TowerAttack 발행
                    if (has_attack_building) {
                        actions.action_order(pid, actions.get_order_type(Orders::TowerAttack), pos, target, nullptr, shift_pressed);
                        log("[ATTACK_BUILDING] pid=%d tower-attack target owner=%d type=%d at (%d,%d)\n", 
                            pid, target->owner, (int)target->unit_type->id, target->sprite->position.x, target->sprite->position.y);
                    }

                    // 2) 유닛들: 항상 AttackDefault 발행 (공격 가능한 유닛은 공격, 나머지는 무시)
                    actions.action_order(pid, actions.get_order_type(Orders::AttackDefault), pos, target, nullptr, shift_pressed);
                    log("[ATTACK] pid=%d unit-target owner=%d type=%d at (%d,%d)\n", 
                        pid, target->owner, (int)target->unit_type->id, target->sprite->position.x, target->sprite->position.y);
                } else {
                    // 타겟이 없으면 유닛들에 대해 AttackMove 발행, 건물은 이동 불가라 무시됨
                    actions.action_order(pid, actions.get_order_type(Orders::AttackMove), pos, nullptr, nullptr, shift_pressed);
                    if (has_attack_building) {
                        log("[ATTACK_BUILDING] pid=%d no target - buildings stay, units attack-move to (%d,%d)\n", pid, pos.x, pos.y);
                    } else {
                        log("[ATTACKMOVE] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
                    }
                }
            };
            if (allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_attack_for(pid);
            } else issue_attack_for(player_id);
            reset_command_mode();
            return;
        }

        // Patrol confirm on down
        if (command_mode == CommandMode::Patrol) {
            left_button_down = false;
            command_issued_on_down = true;  // Up에서 선택 무시
            log("[정찰확정] 좌클릭(Down) at (%d,%d)\n", pos.x, pos.y);

            // Patrol도 공격과 동일한 링 시각화 사용 (유닛/건물/자원 공통)
            unit_t* target = find_unit_at(pos);
            if (target) {
                // 자기 자신에게 정찰 명령을 내리는 경우에는 링 시각화 표시하지 않음
                bool is_self_target = false;
                int eff_owner = get_effective_owner();
                if (!allow_enemy_control) {
                    auto& sel = action_st.selection[eff_owner];
                    if (sel.size() == 1 && sel.front() == target) is_self_target = true;
                } else {
                    for (int owner = 0; owner < 8 && !is_self_target; ++owner) {
                        auto& sel = action_st.selection[owner];
                        if (sel.size() == 1 && sel.front() == target) is_self_target = true;
                    }
                }
                if (!is_self_target) {
                    xy local_pos = target->sprite->position;
                    int unit_width = target->unit_type->dimensions.to.x - target->unit_type->dimensions.from.x;
                    int unit_height = target->unit_type->dimensions.to.y - target->unit_type->dimensions.from.y;
                    int local_radius = std::max(unit_width, unit_height) / 2 + 15;

                    bool is_resource = actions.unit_is_mineral_field(target) ||
                                       actions.unit_is(target, UnitTypes::Resource_Vespene_Geyser) ||
                                       actions.unit_is_refinery(target);
                    int local_kind = is_resource ? 2 : (target->owner == player_id ? 0 : 1);

                    // 멀티 타겟 링에도 추가
                    TargetRing tr;
                    tr.pos = local_pos;
                    tr.radius = local_radius;
                    tr.frames = 120; // 5초
                    tr.kind = local_kind;
                    tr.target_unit = target; // 유닛/건물/자원 타겟일 때 해당 유닛을 따라가도록 설정
                    if (target_rings.size() >= MAX_TARGET_RINGS) {
                        target_rings.erase(target_rings.begin());
                    }
                    target_rings.push_back(tr);

                    log("[TARGET_RING] 정찰 타겟 링: pos=(%d,%d), radius=%d (멀티링 size=%d)\n",
                        target_ring_pos.x, target_ring_pos.y, target_ring_radius, (int)target_rings.size());
                }
            }

            auto issue_patrol_for = [&](int pid) {
                actions.action_order(pid, actions.get_order_type(Orders::Patrol), pos, nullptr, nullptr, shift_pressed);
                log("[PATROL] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
            };
            if (allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_patrol_for(pid);
            } else issue_patrol_for(player_id);
            reset_command_mode();
            return;
        }
    }
}

void player_input_handler::on_left_button_up(xy pos) {
    int sel_count = 0;
    for (int i = 0; i < 8; ++i) sel_count += action_st.selection[i].size();
    log("[BTN_UP] 시작: pos=(%d,%d), is_drag=%d, cmd_mode=%d, selected=%d\n", 
        pos.x, pos.y, (int)is_dragging, (int)command_mode, sel_count);
    
    left_button_down = false;

    // Attack confirm takes precedence
    // Attack 모드: 좌클릭으로 타겟 선택
    if (command_mode == CommandMode::Attack) {
        log("[공격확정] 좌클릭 at (%d,%d)\n", pos.x, pos.y);
        unit_t* target = find_unit_at(pos);
        
        // 공격 건물 체크 함수
        auto is_attack_building = [&](unit_t* u) -> bool {
            if (!u) return false;
            return (
                actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
                actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
                actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
                actions.unit_is(u, UnitTypes::Terran_Bunker) ||
                actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
            );
        };
        
        auto issue_attack_for = [&](int pid) {
            // 선택된 유닛 중 공격 건물이 있는지 확인
            bool has_attack_building = false;
            for (unit_t* u : action_st.selection[pid]) {
                if (is_attack_building(u)) {
                    has_attack_building = true;
                    break;
                }
            }
            
            if (target) {
                if (has_attack_building) {
                    // 공격 건물: TowerAttack order 사용
                    actions.action_order(pid, actions.get_order_type(Orders::TowerAttack), pos, target, nullptr, shift_pressed);
                    log("[ATTACK_BUILDING] pid=%d tower-attack target owner=%d type=%d at (%d,%d)\n", 
                        pid, target->owner, (int)target->unit_type->id, target->sprite->position.x, target->sprite->position.y);
                } else {
                    // 일반 유닛: AttackDefault order 사용
                    actions.action_order(pid, actions.get_order_type(Orders::AttackDefault), pos, target, nullptr, shift_pressed);
                    log("[ATTACK] pid=%d unit-target owner=%d type=%d at (%d,%d)\n", 
                        pid, target->owner, (int)target->unit_type->id, target->sprite->position.x, target->sprite->position.y);
                }
            } else {
                if (has_attack_building) {
                    // 공격 건물은 이동 불가 - 타겟 없으면 무시
                    log("[ATTACK_BUILDING] pid=%d no target - ignored (buildings can't move)\n", pid);
                } else {
                    // 일반 유닛: AttackMove
                    actions.action_order(pid, actions.get_order_type(Orders::AttackMove), pos, nullptr, nullptr, shift_pressed);
                    log("[ATTACKMOVE] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
                }
            }
        };
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_attack_for(pid);
        } else issue_attack_for(player_id);
        reset_command_mode();
        return;
    }
    
    // Move 모드: 좌클릭으로 이동 위치 선택
    if (command_mode == CommandMode::Move) {
        log("[이동확정] 좌클릭 at (%d,%d)\n", pos.x, pos.y);

        // 이동 명령 실행
        auto issue_move_for = [&](int pid) {
            actions.action_order(pid, actions.get_order_type(Orders::Move), pos, nullptr, nullptr, shift_pressed);
            log("[MOVE] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
        };
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_move_for(pid);
        } else issue_move_for(player_id);

        // 이동 확정 시에도 타겟 링 표시 (멀티링)
        unit_t* move_target = find_unit_at(pos);
        xy ring_pos = pos;
        int ring_radius = 24;      // 기본 반경 (지면 기준)
        int ring_frames = 120;     // 약 5초 유지
        int ring_kind = 0;         // 0=self/ally, 1=enemy, 2=resource
        unit_t* ring_unit = nullptr;

        if (move_target && move_target->sprite) {
            // 유닛/건물/자원을 직접 클릭한 경우, 그 타겟 중심으로 링 생성
            ring_pos = move_target->sprite->position;
            int unit_width = move_target->unit_type->dimensions.to.x - move_target->unit_type->dimensions.from.x;
            int unit_height = move_target->unit_type->dimensions.to.y - move_target->unit_type->dimensions.from.y;
            ring_radius = std::max(unit_width, unit_height) / 2 + 15;

            bool is_resource = (
                move_target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                move_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                move_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                move_target->unit_type->id == UnitTypes::Resource_Vespene_Geyser ||
                move_target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                move_target->unit_type->id == UnitTypes::Zerg_Extractor ||
                move_target->unit_type->id == UnitTypes::Terran_Refinery
            );

            if (is_resource) {
                ring_kind = 2;
            } else if (move_target->owner == player_id) {
                ring_kind = 0;
            } else {
                ring_kind = 1;
            }

            ring_unit = move_target; // 타겟을 따라가도록 설정
        }

        TargetRing tr;
        tr.pos = ring_pos;
        tr.radius = ring_radius;
        tr.frames = ring_frames;
        tr.kind = ring_kind;
        tr.target_unit = ring_unit;
        if (target_rings.size() >= MAX_TARGET_RINGS) {
            target_rings.erase(target_rings.begin());
        }
        target_rings.push_back(tr);

        log("[TARGET_RING] 이동 타겟 링: pos=(%d,%d), radius=%d, kind=%d (멀티링 size=%d)\n",
            ring_pos.x, ring_pos.y, ring_radius, ring_kind, (int)target_rings.size());

        reset_command_mode();
        return;
    }
    
    // Patrol 모드: 좌클릭으로 정찰 위치 선택 (선택 차단)
    if (command_mode == CommandMode::Patrol) {
        log("[정찰확정] 좌클릭 at (%d,%d)\n", pos.x, pos.y);
        auto issue_patrol_for = [&](int pid) {
            actions.action_order(pid, actions.get_order_type(Orders::Patrol), pos, nullptr, nullptr, shift_pressed);
            log("[PATROL] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
        };
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_patrol_for(pid);
        } else issue_patrol_for(player_id);
        reset_command_mode();
        return;
    }
    
    // Rally 모드: 좌클릭으로 렐리 포인트 설정 (모든 위치/유닛 가능)
    if (command_mode == CommandMode::Rally) {
        log("[렐리확정] 좌클릭 at (%d,%d), cmd_mode=%d\n", pos.x, pos.y, (int)command_mode);
        
        int pid = get_effective_owner();
        unit_t* target = find_unit_at(pos);
        
        // 선택된 건물 가져오기
        unit_t* building = nullptr;
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                if (!action_st.selection[p].empty()) {
                    building = action_st.selection[p][0];
                    pid = p;
                    break;
                }
            }
        } else {
            if (!action_st.selection[pid].empty()) {
                building = action_st.selection[pid][0];
            }
        }
        
        // 렐리 포인트 설정
        if (building && (building->unit_type->flags & unit_type_t::flag_building)) {
            log("[RALLY] 건물 확인: type=%d, owner=%d\n", (int)building->unit_type->id, building->owner);
            if (target) {
                log("[RALLY] 타겟 유닛 확인: type=%d, owner=%d, pos=(%d,%d)\n", 
                    (int)target->unit_type->id, target->owner, target->sprite->position.x, target->sprite->position.y);
                // 유닛 타겟: RallyPointUnit
                actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), pos, target, nullptr, false);
                log("[RALLY] pid=%d 렐리 포인트 설정 완료 (유닛): pos=(%d,%d), target=%p\n", 
                    pid, pos.x, pos.y, (void*)target);
            } else {
                log("[RALLY] 위치 타겟: pos=(%d,%d)\n", pos.x, pos.y);
                // 위치 타겟: RallyPointTile
                actions.action_order(pid, actions.get_order_type(Orders::RallyPointTile), pos, nullptr, nullptr, false);
                log("[RALLY] pid=%d 렐리 포인트 설정 완료 (위치): pos=(%d,%d)\n", 
                    pid, pos.x, pos.y);
            }
            
            // 시각화: 타겟 링 표시
            xy local_pos = pos;
            int local_radius = 30;

            // 멀티 타겟 링에도 추가 (렐리는 항상 아군/자신 기준)
            TargetRing tr;
            tr.pos = local_pos;
            tr.radius = local_radius;
            tr.frames = 120; // 5초
            tr.kind = 0; // self/ally
            if (target_rings.size() >= MAX_TARGET_RINGS) {
                target_rings.erase(target_rings.begin());
            }
            target_rings.push_back(tr);
            
            // 렐리 포인트 선 시각화 (초록색)
            rally_visual.building_pos = building->sprite->position;
            rally_visual.rally_pos = pos;
            rally_visual.is_active = true;
            rally_visual.is_auto_rally = false;  // 일반 렐리 = 초록색
            rally_visual.frames_remaining = 120;  // 약 5초
            
            // ★ 일반 렐리 저장 (5번 버튼)
            set_normal_rally(building, pos, target);
            
            if (toast_cb) toast_cb("Rally point set", 30);
        } else {
            log("[RALLY] 건물이 선택되지 않음: building=%p, is_building=%d\n", 
                (void*)building, building ? (int)(building->unit_type->flags & unit_type_t::flag_building) : 0);
            if (toast_cb) toast_cb("Select building first", 30);
        }
        
        command_mode = CommandMode::Normal;
        g_ui_active_button = -1;  // 하이라이트 해제
        g_ui_command_mode_active = false;  // 명령 모드 비활성화
        if (selection_suppress_cb) selection_suppress_cb(false);
        return;
    }
    
    // AutoRally 모드: 좌클릭 무시 (우클릭만 가능)
    if (command_mode == CommandMode::AutoRally) {
        log("[자동렐리] 좌클릭 무시 - 우클릭을 사용하세요\n");
        if (toast_cb) toast_cb("Right-click on mineral or gas", 30);
        return;
    }
    
    // Gather 모드: 좌클릭으로 자원 선택 (미네랄/가스만 가능)
    if (command_mode == CommandMode::Gather) {
        log("[채취확정] 좌클릭 at (%d,%d)\n", pos.x, pos.y);
        
        // 선택된 유닛 수 확인 (선택이 풀렸을 수 있으므로 재확인)
        int selected_count = 0;
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) selected_count += (int)action_st.selection[pid].size();
        } else {
            selected_count = (int)action_st.selection[player_id].size();
        }
        
        log("[GATHER] 선택된 유닛: %d개\n", selected_count);
        
        if (selected_count == 0) {
            log("[GATHER] 선택된 유닛이 없어 명령 무시\n");
            if (toast_cb) toast_cb("No units selected", 30);
            reset_command_mode();
            return;
        }
        
        unit_t* target = find_unit_at(pos);
        
        // 자원인지 확인 (미네랄 또는 가스만 허용)
        bool is_resource = false;
        if (target) {
            is_resource = (target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                          target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                          target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                          target->unit_type->id == UnitTypes::Resource_Vespene_Geyser ||
                          target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                          target->unit_type->id == UnitTypes::Zerg_Extractor ||
                          target->unit_type->id == UnitTypes::Terran_Refinery);
            log("[GATHER] 타겟: type=%d, is_resource=%d\n", (int)target->unit_type->id, (int)is_resource);
        } else {
            log("[GATHER] 타겟 없음\n");
        }
        
        if (!is_resource) {
            log("[GATHER] 자원이 아님 - 명령 취소\n");
            if (toast_cb) toast_cb("Click on mineral or gas", 30);
            reset_command_mode();
            return;
        }
        
        // 자원 채취 명령 실행
        auto issue_gather_for = [&](int pid) {
            actions.action_default_order(pid, pos, target, nullptr, shift_pressed);
            log("[GATHER] pid=%d 자원 채취: type=%d at (%d,%d)\n", 
                pid, (int)target->unit_type->id, pos.x, pos.y);
        };
        
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) {
                if (!action_st.selection[pid].empty()) {
                    issue_gather_for(pid);
                }
            }
        } else {
            if (!action_st.selection[player_id].empty()) {
                issue_gather_for(player_id);
            }
        }

        // 자원 채취 확정: 자원 타겟에 멀티 타겟 링만 추가 (단일 타겟 링은 비활성화)
        if (target && target->sprite) {
            // 단일 타겟 링(g_target_ring_*)은 이미 비활성화되어 있으므로,
            // 여기서는 멀티 타겟 링(TargetRing)만 추가해 자원 위의 큰 링을 표시한다.

            xy ring_pos = target->sprite->position;
            int unit_width = target->unit_type->dimensions.to.x - target->unit_type->dimensions.from.x;
            int unit_height = target->unit_type->dimensions.to.y - target->unit_type->dimensions.from.y;
            int ring_radius = std::max(unit_width, unit_height) / 2 + 15;
            int ring_frames = 120;  // 5초

            TargetRing tr;
            tr.pos = ring_pos;
            tr.radius = ring_radius;
            tr.frames = ring_frames;
            tr.kind = 2;              // 자원은 항상 2 (노란색)
            tr.target_unit = target;   // 자원 유닛을 따라가도록 설정
            if (target_rings.size() >= MAX_TARGET_RINGS) {
                target_rings.erase(target_rings.begin());
            }
            target_rings.push_back(tr);

            log("[TARGET_RING] 채취 타겟 링: pos=(%d,%d), radius=%d (멀티링 size=%d)\n",
                ring_pos.x, ring_pos.y, ring_radius, (int)target_rings.size());
        }
        
        if (toast_cb) toast_cb("Gathering", 30);
        reset_command_mode();
        return;
    }
    // Build / AdvancedBuild confirm
    if ((command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) && building_to_place) {
        size_t tile_x = (size_t)(pos.x / 32);
        size_t tile_y = (size_t)(pos.y / 32);
        xy center = xy(int(tile_x*32) + building_to_place->placement_size.x/2,
                       int(tile_y*32) + building_to_place->placement_size.y/2);
        auto [pid_sel, worker] = find_selected_worker_any_owner();
        bool valid = false;
        bool used_relaxed = false;
        if (worker) {
            bool is_zerg = actions.unit_is(worker, UnitTypes::Zerg_Drone);
            if (is_zerg && zerg_build_anywhere) valid = true;
            else {
                valid = actions.can_place_building(worker, pid_sel, building_to_place, center, false, false);
                if (!valid && allow_enemy_control) {
                    // 적컨 테스트 모드에서는 크립/파워 제약을 무시한 배치 허용 시도
                    valid = actions.can_place_building(worker, pid_sel, building_to_place, center, true, true);
                    used_relaxed = valid;
                }
            }
        }
        log("[BUILD_CONFIRM] pid=%d allow_enemy=%d zerg_anywhere=%d tile=(%u,%u) center=(%d,%d) bld_id=%d size=(%d,%d) worker=%p worker_owner=%d valid=%d relaxed=%d\n",
            (int)pid_sel, (int)allow_enemy_control, (int)zerg_build_anywhere,
            (unsigned)tile_x, (unsigned)tile_y, center.x, center.y,
            (int)building_to_place->id, (int)building_to_place->placement_size.x, (int)building_to_place->placement_size.y,
            (void*)worker, worker ? worker->owner : -1, (int)valid, (int)used_relaxed);
        if (valid && worker) {
            bool is_protoss = actions.unit_is(worker, UnitTypes::Protoss_Probe);
            bool is_zerg = actions.unit_is(worker, UnitTypes::Zerg_Drone);
            const order_type_t* order = actions.get_order_type(
                is_zerg ? Orders::DroneStartBuild : (is_protoss ? Orders::PlaceProtossBuilding : Orders::PlaceBuilding)
            );
            // action_build는 해당 owner의 단일 선택 일꾼을 요구하므로, 임시로 선택을 일꾼 단독으로 바꿨다가 복원
            auto& sel = action_st.selection[pid_sel];
            auto sel_backup = sel;
            sel.clear();
            sel.push_back(worker);
            bool ok = false;
            try {
                ok = actions.action_build(pid_sel, order, building_to_place, xy_t<size_t>{tile_x, tile_y});
            } catch (const std::exception& ex) {
                log("[CRASH_BUILD] pid=%d ex=%s bld_id=%d tile=(%u,%u)\n", (int)pid_sel, ex.what(), (int)building_to_place->id, (unsigned)tile_x, (unsigned)tile_y);
            } catch (...) {
                log("[CRASH_BUILD] pid=%d unknown bld_id=%d tile=(%u,%u)\n", (int)pid_sel, (int)building_to_place->id, (unsigned)tile_x, (unsigned)tile_y);
            }
            log("[BUILD_RESULT] ok=%d pid=%d tile=(%u,%u) worker=%p bld_id=%d\n", (int)ok, (int)pid_sel, (unsigned)tile_x, (unsigned)tile_y, (void*)worker, (int)building_to_place->id);
            sel = sel_backup;
            if (ok) exit_build_mode();
            else {
                log("[BUILD] action_build failed after temp-select pid=%d worker=%p\n", pid_sel, (void*)worker);
                if (toast_cb) toast_cb("Build failed", 60);
            }
        } else {
            log("[BUILD_INVALID] tile=(%u,%u) worker=%p pid=%d bld_id=%d center=(%d,%d)\n", (unsigned)tile_x, (unsigned)tile_y, (void*)worker, (int)pid_sel, (int)building_to_place->id, center.x, center.y);
            if (toast_cb) toast_cb("Invalid placement", 60);
        }
        return;
    }

    // Down에서 공격/정찰 명령을 실행했으면 Up에서 선택 무시
    if (command_issued_on_down) {
        command_issued_on_down = false;
        log("[BTN_UP] Down에서 명령 실행됨 - 선택 무시\n");
        return;
    }
    
    if (is_dragging) {
        // 드래그 선택 (좌표는 이미 handle_event에서 제한됨)
        log("[DRAG_DEBUG] 맵크기: %dx%d 픽셀\n", 
            (int)st.game->map_width, (int)st.game->map_height);
        log("[DRAG_DEBUG] 입력좌표: start=(%d,%d) end=(%d,%d)\n", 
            drag_start_pos.x, drag_start_pos.y, pos.x, pos.y);
        
        // 안전성 체크: 좌표가 유효한 범위인지 확인
        int mw = (int)st.game->map_width - 1;
        int mh = (int)st.game->map_height - 1;
        bool valid_coords = (drag_start_pos.x >= 0 && drag_start_pos.x <= mw &&
                            drag_start_pos.y >= 0 && drag_start_pos.y <= mh &&
                            pos.x >= 0 && pos.x <= mw &&
                            pos.y >= 0 && pos.y <= mh);
        
        if (valid_coords) {
            rect area;
            area.from.x = std::min(drag_start_pos.x, pos.x);
            area.from.y = std::min(drag_start_pos.y, pos.y);
            area.to.x = std::max(drag_start_pos.x, pos.x);
            area.to.y = std::max(drag_start_pos.y, pos.y);
            
            log("[DRAG_DEBUG] 영역계산 후: area=(%d,%d)-(%d,%d)\n", 
                area.from.x, area.from.y, area.to.x, area.to.y);
            
            log(">>> 드래그 선택: 영역=(%d,%d)-(%d,%d)\n", 
                area.from.x, area.from.y, area.to.x, area.to.y);
            
            select_units_in_rect(area);
        } else {
            log("[DRAG_INVALID] 잘못된 좌표로 드래그 무시: start=(%d,%d) end=(%d,%d)\n",
                drag_start_pos.x, drag_start_pos.y, pos.x, pos.y);
        }
        is_dragging = false;
    } else {
        // 단일 클릭 선택
        unit_t* u = find_unit_at(pos);
        if (u) {
            // 유닛/건물/자원 구분
            bool is_building = (u->unit_type->flags & unit_type_t::flag_building);
            bool is_resource = actions.unit_is_mineral_field(u) || 
                              actions.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                              actions.unit_is_refinery(u);
            const char* type_str = is_resource ? "자원" : (is_building ? "건물" : "유닛");
            
            log(">>> %s 클릭: 소유자=%d, 타입=%d, 위치=(%d,%d)\n", 
                type_str, u->owner, (int)u->unit_type->id, u->sprite->position.x, u->sprite->position.y);
            
            // 자원은 선택 가능하지만 명령은 불가 (UI에서 처리)
            if (is_resource) {
                log(">>> 자원 선택됨 (명령 불가)\n");
                // 선택은 허용하고 계속 진행
            }
            
            // 원작 규칙: 기본은 아군만 선택. 허용 시 적군도 선택
            else if (!allow_enemy_control && u->owner != player_id) {
                log(">>> 적 유닛 클릭 - 정보만 표시, 선택 변경 안함\n");
                return;
            }
            
            if (!is_resource) {
                log(">>> %s 선택됨\n", type_str);
            }

            // Shift 없이 다른 유닛을 클릭해서 선택을 변경하면,
            // 기존 Build/AdvancedBuild 모드는 종료한다.
            if (!shift_pressed && (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                exit_build_mode();
            }

            if (allow_enemy_control) {
                if (!shift_pressed) clear_all_selections();
                if (shift_pressed) actions.action_shift_select(u->owner, u);
                else actions.action_select(u->owner, u);
                if (u->owner != player_id && toast_cb) toast_cb("ENEMY UNIT SELECTED", 90);
            } else {
                if (shift_pressed) actions.action_shift_select(player_id, u);
                else actions.action_select(player_id, u);
            }
        } else {
            log(">>> 클릭 위치에 유닛 없음: (%d,%d)\n", pos.x, pos.y);
            // 빈 곳 클릭 시 (Shift 미사용), 선택 해제
            if (!shift_pressed) {
                if (allow_enemy_control) clear_all_selections();
                else {
                    action_st.selection[player_id].clear();
                    // 자신 소유의 선택만 직접 비우는 경우에도,
                    // 빌드/고급 빌드 모드는 함께 종료해준다.
                    if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
                        exit_build_mode();
                    }
                }
                log(">>> 선택 해제\n");
            }
        }
    }
}

void player_input_handler::on_right_click(xy pos) {
    // ========== 디버깅 로그 시작 ==========
    log("\n========== [RIGHT_CLICK] 우클릭 디버깅 ==========\n");
    log("[RIGHT_CLICK] 클릭 위치: (%d,%d)\n", pos.x, pos.y);
    log("[RIGHT_CLICK] 현재 명령 모드: %d (0=Normal, 4=Rally, 6=AutoRally)\n", (int)command_mode);
    
    // 클릭 위치에 유닛이 있는지 확인
    unit_t* debug_target = find_unit_at(pos);
    
    bool target_ring_spawned = false;

    // 타겟 링 시각화 (유닛/건물/자원 우클릭 시만)
    if (debug_target) {
        command_ring_frames = 0;
        // 자기 자신을 우클릭한 경우에는 디버그 타겟 링을 표시하지 않음
        bool is_self_target = false;
        int eff_owner = get_effective_owner();
        if (!allow_enemy_control) {
            auto& sel = action_st.selection[eff_owner];
            if (sel.size() == 1 && sel.front() == debug_target) is_self_target = true;
        } else {
            for (int owner = 0; owner < 8 && !is_self_target; ++owner) {
                auto& sel = action_st.selection[owner];
                if (sel.size() == 1 && sel.front() == debug_target) is_self_target = true;
            }
        }

        if (!is_self_target) {
            xy local_pos = debug_target->sprite->position;
            int unit_width = debug_target->unit_type->dimensions.to.x - debug_target->unit_type->dimensions.from.x;
            int unit_height = debug_target->unit_type->dimensions.to.y - debug_target->unit_type->dimensions.from.y;
            int local_radius = std::max(unit_width, unit_height) / 2 + 10;
            int local_frames = 120; // 5초

            // 타겟 종류에 따라 링 색상 구분: 0=아군/자신, 1=적군, 2=자원
            bool is_resource = (
                debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                debug_target->unit_type->id == UnitTypes::Resource_Vespene_Geyser ||
                debug_target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                debug_target->unit_type->id == UnitTypes::Zerg_Extractor ||
                debug_target->unit_type->id == UnitTypes::Terran_Refinery
            );
            int local_kind = is_resource ? 2 : (debug_target->owner == player_id ? 0 : 1);

            // 멀티 타겟 링에도 추가
            TargetRing tr;
            tr.pos = local_pos;
            tr.radius = local_radius;
            tr.frames = local_frames;
            tr.kind = local_kind;
            tr.target_unit = debug_target; // 유닛/건물/자원 타겟일 때 해당 유닛을 따라가도록 설정
            if (target_rings.size() >= MAX_TARGET_RINGS) {
                target_rings.erase(target_rings.begin());
            }
            target_rings.push_back(tr);
            target_ring_spawned = true;

            log("[TARGET_RING] 우클릭 타겟 링: pos=(%d,%d), radius=%d (멀티링 size=%d)\n", 
                local_pos.x, local_pos.y, local_radius, (int)target_rings.size());
        }
    }
    
    if (debug_target) {
        log("[RIGHT_CLICK] ★ 타겟 발견: type=%d, owner=%d, pos=(%d,%d)\n", 
            (int)debug_target->unit_type->id, debug_target->owner, 
            debug_target->sprite->position.x, debug_target->sprite->position.y);
        log("[RIGHT_CLICK] 타겟 dimensions: from=(%d,%d), to=(%d,%d)\n",
            debug_target->unit_type->dimensions.from.x, debug_target->unit_type->dimensions.from.y,
            debug_target->unit_type->dimensions.to.x, debug_target->unit_type->dimensions.to.y);
        
        // 타겟 종류 판별
        bool is_mineral = (debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                          debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                          debug_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3);
        bool is_gas = (debug_target->unit_type->id == UnitTypes::Resource_Vespene_Geyser ||
                      debug_target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                      debug_target->unit_type->id == UnitTypes::Zerg_Extractor ||
                      debug_target->unit_type->id == UnitTypes::Terran_Refinery);
        bool is_building = (debug_target->unit_type->flags & unit_type_t::flag_building);
        
        if (is_mineral) {
            log("[RIGHT_CLICK] 타겟 종류: ★★★ 미네랄 ★★★\n");
        } else if (is_gas) {
            log("[RIGHT_CLICK] 타겟 종류: ★★★ 가스 ★★★\n");
        } else if (is_building) {
            log("[RIGHT_CLICK] 타겟 종류: ★★★ 건물 ★★★\n");
        } else {
            log("[RIGHT_CLICK] 타겟 종류: ★★★ 유닛 ★★★\n");
        }
    } else {
        log("[RIGHT_CLICK] ★ 타겟 없음: ★★★ 땅 ★★★\n");
    }
    
    // 선택된 유닛 정보
    int debug_pid = get_effective_owner();
    int selected_count = action_st.selection[debug_pid].size();
    log("[RIGHT_CLICK] 선택된 유닛 수: %d\n", selected_count);
    
    if (selected_count > 0) {
        unit_t* first_unit = action_st.selection[debug_pid][0];
        log("[RIGHT_CLICK] 첫 번째 선택 유닛: type=%d, is_building=%d\n",
            (int)first_unit->unit_type->id, 
            (int)(first_unit->unit_type->flags & unit_type_t::flag_building));
    }
    log("[RIGHT_CLICK] ==========================================\n\n");
    // ========== 디버깅 로그 종료 ==========
    
    // Rally 모드: 우클릭으로 렐리 포인트 설정 (모든 위치/유닛 가능)
    if (command_mode == CommandMode::Rally) {
        log("[렐리확정] 우클릭 at (%d,%d)\n", pos.x, pos.y);
        
        int pid = get_effective_owner();
        unit_t* target = find_unit_at(pos);
        
        // 선택된 건물 가져오기
        unit_t* building = nullptr;
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                if (!action_st.selection[p].empty()) {
                    building = action_st.selection[p][0];
                    pid = p;
                    break;
                }
            }
        } else {
            if (!action_st.selection[pid].empty()) {
                building = action_st.selection[pid][0];
            }
        }
        
        // 렐리 포인트 설정
        if (building && (building->unit_type->flags & unit_type_t::flag_building)) {
            log("[RALLY_RC] 건물 확인: type=%d, owner=%d\n", (int)building->unit_type->id, building->owner);
            if (target) {
                log("[RALLY_RC] 타겟 유닛 확인: type=%d, owner=%d, pos=(%d,%d)\n", 
                    (int)target->unit_type->id, target->owner, target->sprite->position.x, target->sprite->position.y);
                // 유닛 타겟: RallyPointUnit
                actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), pos, target, nullptr, false);
                log("[RALLY_RC] pid=%d 렐리 포인트 설정 완료 (유닛): pos=(%d,%d), target=%p\n", 
                    pid, pos.x, pos.y, (void*)target);
            } else {
                log("[RALLY_RC] 위치 타겟: pos=(%d,%d)\n", pos.x, pos.y);
                // 위치 타겟: RallyPointTile
                actions.action_order(pid, actions.get_order_type(Orders::RallyPointTile), pos, nullptr, nullptr, false);
                log("[RALLY_RC] pid=%d 렐리 포인트 설정 완료 (위치): pos=(%d,%d)\n", 
                    pid, pos.x, pos.y);
            }
            
            // 시각화: 타겟 링 표시
            target_ring_pos = pos;
            target_ring_radius = 30;
            target_ring_frames = 120;  // 5초
            
            // 렐리 포인트 선 시각화 (초록색)
            rally_visual.building_pos = building->sprite->position;
            rally_visual.rally_pos = pos;
            rally_visual.is_active = true;
            rally_visual.is_auto_rally = false;  // 일반 렐리 = 초록색
            rally_visual.frames_remaining = 120;  // 약 5초
            
            // ★ 일반 렐리 저장 (5번 버튼)
            set_normal_rally(building, pos, target);
            
            if (toast_cb) toast_cb("Rally point set", 30);
        } else {
            log("[RALLY_RC] 건물이 선택되지 않음: building=%p, is_building=%d\n", 
                (void*)building, building ? (int)(building->unit_type->flags & unit_type_t::flag_building) : 0);
            if (toast_cb) toast_cb("Select building first", 30);
        }
        
        command_mode = CommandMode::Normal;
        g_ui_active_button = -1;  // 하이라이트 해제
        g_ui_command_mode_active = false;  // 명령 모드 비활성화
        if (selection_suppress_cb) selection_suppress_cb(false);
        return;
    }
    
    // AutoRally 모드: 우클릭으로 자동 렐리 포인트 설정 (자원만 가능)
    if (command_mode == CommandMode::AutoRally) {
        log("[자동렐리확정] 우클릭 at (%d,%d)\n", pos.x, pos.y);
        
        int pid = get_effective_owner();
        unit_t* target = find_unit_at(pos);
        
        // 선택된 건물 가져오기
        unit_t* building = nullptr;
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                if (!action_st.selection[p].empty()) {
                    building = action_st.selection[p][0];
                    pid = p;
                    break;
                }
            }
        } else {
            if (!action_st.selection[pid].empty()) {
                building = action_st.selection[pid][0];
            }
        }
        
        // 자원인지 확인
        bool is_resource = false;
        if (target) {
            is_resource = (
                target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                target->unit_type->id == UnitTypes::Zerg_Extractor ||
                target->unit_type->id == UnitTypes::Terran_Refinery
            );
            log("[AUTO_RALLY] 타겟: type=%d, is_resource=%d\n", (int)target->unit_type->id, (int)is_resource);
        } else {
            log("[AUTO_RALLY] 타겟 없음 (빈 땅 클릭)\n");
        }
        
        if (!is_resource) {
            log("[AUTO_RALLY] 자원이 아님 - 명령 취소\n");
            if (toast_cb) toast_cb("Right-click on mineral or gas", 30);
            // 모드 유지 (다시 클릭 가능)
            return;
        }
        
        // 자동 렐리 포인트 설정
        if (building && (building->unit_type->flags & unit_type_t::flag_building)) {
            // 자원 타겟: RallyPointUnit 사용
            actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), pos, target, nullptr, false);
            log("[AUTO_RALLY] pid=%d 자동 렐리 설정: resource_type=%d at (%d,%d)\n", 
                pid, (int)target->unit_type->id, pos.x, pos.y);
            
            // 시각화: 타겟 링 표시
            target_ring_pos = pos;
            target_ring_radius = 30;
            target_ring_frames = 120;  // 5초
            
            // 렐리 포인트 선 시각화 (노란색)
            rally_visual.building_pos = building->sprite->position;
            rally_visual.rally_pos = pos;
            rally_visual.is_active = true;
            rally_visual.is_auto_rally = true;  // 자동 렐리 = 노란색
            rally_visual.frames_remaining = 120;  // 약 5초
            
            // ★ 오토 렐리 저장 (6번 버튼)
            set_auto_rally(building, pos, target);
            
            if (toast_cb) toast_cb("Auto rally to resource", 60);
        } else {
            log("[AUTO_RALLY] 건물이 선택되지 않음\n");
            if (toast_cb) toast_cb("Select building first", 30);
        }
        
        command_mode = CommandMode::Normal;
        g_ui_active_button = -1;  // 하이라이트 해제
        g_ui_command_mode_active = false;  // 명령 모드 비활성화
        if (selection_suppress_cb) selection_suppress_cb(false);
        return;
    }
    
    // 명령 모드에서 우클릭: 모드 취소
    if (command_mode == CommandMode::Attack) {
        log("[RC] Cancel attack mode by right-click\n");
        reset_command_mode();
        if (toast_cb) toast_cb("Attack mode canceled", 30);
        return;
    }
    if (command_mode == CommandMode::Move) {
        log("[RC] Cancel move mode by right-click\n");
        reset_command_mode();
        if (toast_cb) toast_cb("Move mode canceled", 30);
        return;
    }
    if (command_mode == CommandMode::Patrol) {
        log("[RC] Cancel patrol mode by right-click\n");
        reset_command_mode();
        if (toast_cb) toast_cb("Patrol mode canceled", 30);
        return;
    }
    if (command_mode == CommandMode::Gather) {
        log("[RC] Cancel gather mode by right-click\n");
        reset_command_mode();
        if (toast_cb) toast_cb("Gather mode canceled", 30);
        return;
    }
    if ((command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) && building_to_place) {
        log("[RC] Cancel build preview by right-click\n");
        if (toast_cb) toast_cb("Build canceled", 30);
        exit_build_mode();
        return;
    }
    if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
        log("[RC] Cancel build mode by right-click\n");
        reset_command_mode();
        if (toast_cb) toast_cb("Build mode canceled", 30);
        return;
    }
    
    // 선택된 유닛/건물 수 확인
    int selected_unit_count = 0;
    int selected_building_count = 0;
    
    if (allow_enemy_control) {
        for (int pid = 0; pid < 8; ++pid) {
            for (unit_t* u : action_st.selection[pid]) {
                if (u->unit_type->flags & unit_type_t::flag_building) {
                    selected_building_count++;
                } else {
                    selected_unit_count++;
                }
            }
        }
    } else {
        for (unit_t* u : action_st.selection[player_id]) {
            if (u->unit_type->flags & unit_type_t::flag_building) {
                selected_building_count++;
            } else {
                selected_unit_count++;
            }
        }
    }
    
    int total_selected = selected_unit_count + selected_building_count;
    
    log("\n========== [우클릭 명령] ==========\n");
    log("위치: 게임=(%d,%d)\n", pos.x, pos.y);
    log("선택된 유닛: %d개, 건물: %d개\n", selected_unit_count, selected_building_count);
    log("명령 모드: %d | Shift: %s\n", (int)command_mode, shift_pressed ? "ON" : "OFF");
    
    // 선택된 유닛이 없으면 명령 무시
    if (total_selected == 0) {
        log("[경고] 선택된 유닛/건물이 없어 명령 무시\n");
        log("====================================\n\n");
        return;
    }
    
    // 타겟 유닛 찾기
    static auto last_rclick_time = std::chrono::high_resolution_clock::now();
    static xy last_rclick_pos{ -99999, -99999 };

    auto now_rc = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_rc - last_rclick_time).count();
    bool is_double_rclick = (elapsed_ms < 500) && (last_rclick_pos.x != -99999);
    int dx_d = pos.x - last_rclick_pos.x;
    int dy_d = pos.y - last_rclick_pos.y;
    int dist2_d = dx_d*dx_d + dy_d*dy_d;
    if (is_double_rclick && dist2_d > 32*32) is_double_rclick = false;
    if (is_double_rclick) log("[RC_DBL] double right click detected: elapsed=%lldms dist2=%d\n", elapsed_ms, dist2_d);
    unit_t* target = find_unit_at(pos);
    last_rclick_time = now_rc;
    last_rclick_pos = pos;
    
    log("\n[타겟 판별 결과]\n");
    log("[RC_DEBUG] 클릭 위치: (%d,%d)\n", pos.x, pos.y);
    if (target) {
        int dx_t = pos.x - target->sprite->position.x;
        int dy_t = pos.y - target->sprite->position.y;
        int dist2_t = dx_t * dx_t + dy_t * dy_t;
        log("[RC_DEBUG] find_unit_at 결과: ptr=%p owner=%d type=%d pos=(%d,%d) dist2=%d\n",
            target,
            target->owner,
            (int)target->unit_type->id,
            target->sprite->position.x,
            target->sprite->position.y,
            dist2_t);
    } else {
        log("[RC_DEBUG] find_unit_at 결과: target=nullptr (유닛 없음)\n");
    }

    // 현재 선택된 유닛/건물 목록을 덤프하여, 실제 링 대상과 비교할 수 있게 한다.
    for (int pid = 0; pid < 8; ++pid) {
        auto& sel = action_st.selection[pid];
        if (sel.empty()) continue;
        log("[RC_DEBUG] selection[pid=%d] size=%d\n", pid, (int)sel.size());
        int idx = 0;
        for (unit_t* u : sel) {
            if (!u || !u->sprite) continue;
            log("  [SEL] #%d ptr=%p owner=%d type=%d pos=(%d,%d)\n",
                idx++, u, u->owner, (int)u->unit_type->id,
                u->sprite->position.x, u->sprite->position.y);
        }
    }
    if (target) {
        // 타겟 타입 판별
        bool is_resource = (target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                           target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                           target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                           target->unit_type->id == UnitTypes::Resource_Vespene_Geyser);
        bool is_building = target->unit_type->flags & unit_type_t::flag_building;
        
        std::string target_category = "일반 유닛";
        if (is_building) target_category = "건물";
        else if (is_resource) target_category = "자원";
        
        log(">>> 타겟: %s - %s (owner=%d)\n", 
            target_category.c_str(), get_unit_name((int)target->unit_type->id), target->owner);
        log("    위치: (%d,%d)\n", target->sprite->position.x, target->sprite->position.y);
        log("    Dimensions: (%d,%d)\n", 
            target->unit_type->dimensions.from.x, target->unit_type->dimensions.from.y);
        
        // 타겟 링 표시 (유닛/건물/자원) - 자기 자신을 우클릭한 경우에는 표시하지 않음
        bool is_self_target = false;
        int eff_owner = get_effective_owner();
        if (!allow_enemy_control) {
            auto& sel = action_st.selection[eff_owner];
            if (sel.size() == 1 && sel.front() == target) is_self_target = true;
        } else {
            for (int owner = 0; owner < 8 && !is_self_target; ++owner) {
                auto& sel = action_st.selection[owner];
                if (sel.size() == 1 && sel.front() == target) is_self_target = true;
            }
        }
        if (!is_self_target) {
            xy local_pos = target->sprite->position;
            int unit_width = target->unit_type->dimensions.to.x - target->unit_type->dimensions.from.x;
            int unit_height = target->unit_type->dimensions.to.y - target->unit_type->dimensions.from.y;
            int local_radius = std::max(unit_width, unit_height) / 2 + 10;

            int local_kind = 0;
            if (is_resource) {
                local_kind = 2;
            } else if (target->owner == player_id) {
                local_kind = 0;
            } else {
                local_kind = 1;
            }

            // 멀티 타겟 링으로 추가 (유닛 추적 가능)
            TargetRing tr;
            tr.pos = local_pos;
            tr.radius = local_radius;
            tr.frames = 120;
            tr.kind = local_kind;
            tr.target_unit = target;
            if (target_rings.size() >= MAX_TARGET_RINGS) {
                target_rings.erase(target_rings.begin());
            }
            target_rings.push_back(tr);
            target_ring_spawned = true;

            log("[RC_DEBUG] RING_SET (Multi): target=%p owner=%d type=%d ring_pos=(%d,%d) radius=%d frames=%d\n",
                target,
                target->owner,
                (int)target->unit_type->id,
                local_pos.x,
                local_pos.y,
                local_radius,
                120);
        }
    } else {
        log(">>> 타겟: 빈 땅 (유닛/건물/자원 없음)\n");
        
        // 빈 땅 클릭 시에도 작은 링 표시하되,
        // 단일 선택된 유닛의 몸통 근처(충돌 박스 근처)를 클릭한 경우에는
        // 자기 자신 클릭으로 간주하고 링을 만들지 않는다 (선택 링 깜빡임 방지)
        bool suppress_ground_ring = false;
        int eff_owner = get_effective_owner();
        if (!allow_enemy_control) {
            auto& sel = action_st.selection[eff_owner];
            if (sel.size() == 1 && sel.front() && sel.front()->sprite) {
                xy s_pos = sel.front()->sprite->position;
                int dx = pos.x - s_pos.x;
                int dy = pos.y - s_pos.y;
                int dist_sq = dx * dx + dy * dy;
                // 유닛 중심에서 반지름 약 60픽셀 이내면 자기 자신 주변으로 간주
                if (dist_sq <= 60 * 60) suppress_ground_ring = true;
            }
        } else {
            for (int owner = 0; owner < 8 && !suppress_ground_ring; ++owner) {
                auto& sel = action_st.selection[owner];
                if (sel.size() == 1 && sel.front() && sel.front()->sprite) {
                    xy s_pos = sel.front()->sprite->position;
                    int dx = pos.x - s_pos.x;
                    int dy = pos.y - s_pos.y;
                    int dist_sq = dx * dx + dy * dy;
                    if (dist_sq <= 60 * 60) suppress_ground_ring = true;
                }
            }
        }
        // 타겟 링이 이미 생성되었다면 커맨드 링을 강제로 억제
        if (target_ring_spawned) {
            suppress_ground_ring = true;
            command_ring_frames = 0;
            log("[RC_DEBUG] COMMAND_RING_SUPPRESS: target ring spawned, ground ring skipped\n");
        }

        // 빈 땅 클릭 시 명령 링 표시 (선택 링과 별도)
        if (!suppress_ground_ring) {
            command_ring_pos = pos;
            command_ring_radius = 20;
            command_ring_frames = 120;  // 5초 지속
            command_ring_kind = 0;  // 기본 색상
            log("[RC_DEBUG] COMMAND_RING_SET: pos=(%d,%d) radius=%d frames=%d (땅 클릭)\n",
                pos.x, pos.y, command_ring_radius, command_ring_frames);
        }
    }
    log("\n");
    
    // 공격 건물 체크 함수
    auto is_attack_building = [&](unit_t* u) -> bool {
        if (!u) return false;
        return (
            actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
            actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
            actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
            actions.unit_is(u, UnitTypes::Terran_Bunker) ||
            actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
        );
    };
    
    // 선택된 유닛 중 공격 건물이 있는지 확인
    bool has_attack_building = false;
    if (allow_enemy_control) {
        for (int pid = 0; pid < 8; ++pid) {
            for (unit_t* u : action_st.selection[pid]) {
                if (is_attack_building(u)) {
                    has_attack_building = true;
                    break;
                }
            }
            if (has_attack_building) break;
        }
    } else {
        for (unit_t* u : action_st.selection[player_id]) {
            if (is_attack_building(u)) {
                has_attack_building = true;
                break;
            }
        }
    }
    
    // 생산 건물 체크 함수
    auto is_production_building = [&](unit_t* u) -> bool {
        if (!u) return false;
        return (
            actions.unit_is(u, UnitTypes::Protoss_Nexus) ||
            actions.unit_is(u, UnitTypes::Protoss_Gateway) ||
            actions.unit_is(u, UnitTypes::Zerg_Hatchery) ||
            actions.unit_is(u, UnitTypes::Zerg_Lair) ||
            actions.unit_is(u, UnitTypes::Zerg_Hive) ||
            actions.unit_is(u, UnitTypes::Terran_Command_Center) ||
            actions.unit_is(u, UnitTypes::Terran_Barracks) ||
            actions.unit_is(u, UnitTypes::Terran_Factory) ||
            actions.unit_is(u, UnitTypes::Terran_Starport)
        );
    };
    
    // 생산 건물이 하나라도 선택되어 있는지 확인 (유닛은 제외)
    bool has_production_buildings = false;
    if (selected_building_count > 0 && selected_unit_count == 0) {
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) {
                for (unit_t* u : action_st.selection[pid]) {
                    if (is_production_building(u)) {
                        has_production_buildings = true;
                        break;
                    }
                }
                if (has_production_buildings) break;
            }
        } else {
            for (unit_t* u : action_st.selection[player_id]) {
                if (is_production_building(u)) {
                    has_production_buildings = true;
                    break;
                }
            }
        }
    }
    
    // 생산 건물이 하나라도 선택되어 있으면 렐리 포인트 설정 (우클릭)
    if (has_production_buildings) {
        log("  [명령] 생산 건물 렐리 포인트 설정 (우클릭)\n");
        
        // ★ 선택된 모든 건물에 렐리 포인트 설정
        int rally_count = 0;
        unit_t* first_building = nullptr;
        
        auto set_rally_for_player = [&](int pid) {
            for (unit_t* building : action_st.selection[pid]) {
                if (!building) continue;
                
                // 생산 건물인지 확인
                if (!is_production_building(building)) continue;
                
                // 렐리 포인트 설정
                if (target) {
                    // 유닛 타겟: RallyPointUnit
                    actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), pos, target, nullptr, false);
                    log("  [RALLY_AUTO] 렐리 설정 (유닛): building_type=%d, pos=(%d,%d), target=%p\n", 
                        (int)building->unit_type->id, pos.x, pos.y, (void*)target);
                } else {
                    // 위치 타겟: RallyPointTile
                    actions.action_order(pid, actions.get_order_type(Orders::RallyPointTile), pos, nullptr, nullptr, false);
                    log("  [RALLY_AUTO] 렐리 설정 (위치): building_type=%d, pos=(%d,%d)\n", 
                        (int)building->unit_type->id, pos.x, pos.y);
                }
                
                // ★ 일반 렐리 저장 (5번 버튼 - 우클릭 스마트 명령)
                set_normal_rally(building, pos, target);
                
                rally_count++;
                if (!first_building) first_building = building;
            }
        };
        
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                if (!action_st.selection[p].empty()) {
                    set_rally_for_player(p);
                }
            }
        } else {
            set_rally_for_player(player_id);
        }
        
        // 렐리 포인트 선 시각화 (첫 번째 건물 기준, 초록색 - 일반 우클릭)
        if (first_building) {
            rally_visual.building_pos = first_building->sprite->position;
            rally_visual.rally_pos = pos;
            rally_visual.is_active = true;
            rally_visual.is_auto_rally = false;  // 일반 우클릭 = 초록색
            rally_visual.frames_remaining = 120;  // 약 5초
            
            log("  [RALLY_VISUAL] 시각화 활성화: %d개 건물, first_building=(%d,%d), rally=(%d,%d), color=green\n",
                rally_count, rally_visual.building_pos.x, rally_visual.building_pos.y,
                rally_visual.rally_pos.x, rally_visual.rally_pos.y);
        }
        
        log("[우클릭 디버그 끝]\n\n");
        return;
    }
    
    // 우클릭 목적지 조정: 자기 자신 클릭 또는 매우 근접 클릭 시 오프셋 적용
    auto adjust_pos_if_needed = [&](int pid, xy orig_pos, unit_t* orig_target) {
        xy adj = orig_pos;
        bool adjusted = false;
        unit_t* primary = nullptr;
        if (!action_st.selection[pid].empty()) {
            for (unit_t* u : action_st.selection[pid]) {
                if (!u) continue;
                if (!(u->unit_type->flags & unit_type_t::flag_building)) {
                    primary = u;
                    break;
                }
            }
            if (!primary) primary = action_st.selection[pid].front();
        }
        if (primary) {
            bool self_click = (orig_target == primary);
            int dx = orig_pos.x - primary->sprite->position.x;
            int dy = orig_pos.y - primary->sprite->position.y;
            int d2 = dx*dx + dy*dy;
            const int same_thresh = 12*12;
            if (self_click || d2 <= same_thresh) {
                int offx = 0, offy = 0;
                int base = 8;
                if (std::abs(dx) > std::abs(dy)) offx = (dx >= 0 ? base : -base);
                else offy = (dy >= 0 ? base : -base);
                adj.x = orig_pos.x + offx;
                adj.y = orig_pos.y + offy;
                // 맵 경계 클램프
                int mw = (int)st.game->map_width - 1;
                int mh = (int)st.game->map_height - 1;
                if (adj.x < 0) adj.x = 0; if (adj.y < 0) adj.y = 0;
                if (adj.x > mw) adj.x = mw; if (adj.y > mh) adj.y = mh;
                adjusted = true;
                log("[RC_ADJUST] pid=%d primary=(%d,%d) orig=(%d,%d) -> adj=(%d,%d) self=%d d2=%d base_off=%d\n",
                    pid,
                    primary->sprite->position.x, primary->sprite->position.y,
                    orig_pos.x, orig_pos.y,
                    adj.x, adj.y,
                    (int)self_click, d2, base);
            } else {
                log("[RC_ADJUST] no-adjust pid=%d primary=(%d,%d) orig=(%d,%d) self=%d dx=%d dy=%d d2=%d thresh=%d\n",
                    pid,
                    primary->sprite->position.x, primary->sprite->position.y,
                    orig_pos.x, orig_pos.y,
                    (int)self_click, dx, dy, d2, same_thresh);
            }
        }
        return std::make_pair(adj, adjusted);
    };

    auto issue_for_player = [&](int pid) {
        // ★ 혼합 선택 시 건물 렐리 설정 추가
        // 선택된 항목 중 건물과 유닛 분리
        a_vector<unit_t*> buildings_in_selection;
        a_vector<unit_t*> units_in_selection;
        
        for (unit_t* u : action_st.selection[pid]) {
            if (!u) continue;
            if (u->unit_type->flags & unit_type_t::flag_building) {
                buildings_in_selection.push_back(u);
            } else {
                units_in_selection.push_back(u);
            }
        }
        
        // 건물이 있으면 렐리/공격 건물 처리
        if (!buildings_in_selection.empty()) {
            // 선택된 건물 중 공격 건물이 있는지 (해당 pid 기준)
            bool has_attack_building_local = false;
            for (unit_t* building : buildings_in_selection) {
                if (is_attack_building(building)) {
                    has_attack_building_local = true;
                    break;
                }
            }

            for (unit_t* building : buildings_in_selection) {
                // 생산 건물인지 확인
                bool is_production = (
                    actions.unit_is(building, UnitTypes::Protoss_Nexus) ||
                    actions.unit_is(building, UnitTypes::Protoss_Gateway) ||
                    actions.unit_is(building, UnitTypes::Zerg_Hatchery) ||
                    actions.unit_is(building, UnitTypes::Zerg_Lair) ||
                    actions.unit_is(building, UnitTypes::Zerg_Hive) ||
                    actions.unit_is(building, UnitTypes::Terran_Command_Center)
                );
                
                if (is_production && actions.unit_is_factory(building)) {
                    // 렐리 포인트 설정
                    if (target) {
                        actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), pos, target, nullptr, false);
                    } else {
                        actions.action_order(pid, actions.get_order_type(Orders::RallyPointTile), pos, nullptr, nullptr, false);
                    }
                    
                    // 렐리 저장
                    set_normal_rally(building, pos, target);
                    
                    log("  [혼합선택] 건물 렐리 설정: building_type=%d, pos=(%d,%d)\n", 
                        (int)building->unit_type->id, pos.x, pos.y);
                }
            }

            // 공격 건물만 선택된 경우: 우클릭 적군 유닛 → TowerAttack으로 타겟 변경
            if (has_attack_building_local && units_in_selection.empty() && target) {
                bool is_enemy = (target->owner != pid);
                if (is_enemy) {
                    log("  [명령] 공격 건물(단독 선택) → 적군 공격: target_owner=%d, target_type=%d\n",
                        target->owner, (int)target->unit_type->id);
                    actions.action_order(pid, actions.get_order_type(Orders::TowerAttack), pos, target, nullptr, shift_pressed);
                    // 공격 건물 전용 처리이므로 여기서 반환
                    return;
                }
            }
        }
        
        // 유닛이 있으면 이동/공격 명령
        if (!units_in_selection.empty()) {
            // 1) 공격 건물이 선택되었고 타겟이 적군일 때: 공격 건물에 TowerAttack 발행
            if (has_attack_building && target) {
                bool is_enemy = (target->owner != pid);
                if (is_enemy) {
                    log("  [명령] 공격 건물 → 적군 공격: target_owner=%d, target_type=%d\n", 
                        target->owner, (int)target->unit_type->id);
                    // 공격 건물은 TowerAttack order 사용 (유닛 명령과 별도로 발행)
                    actions.action_order(pid, actions.get_order_type(Orders::TowerAttack), pos, target, nullptr, shift_pressed);
                } else if (target) {
                    log("  [명령] 공격 건물 → 아군 무시: target_owner=%d (우클릭은 아군 공격 안 함)\n", target->owner);
                }
            }

            // 2) 유닛에 대해서는 항상 기본 스마트 명령 처리
            if (target && target->owner == pid) {
                // 아군 유닛 우클릭: 단일 자기 자신 근접 클릭이면 이동으로 처리, 그 외에는 Follow 허용
                bool handled = false;
                if (units_in_selection.size() == 1) {
                    unit_t* primary = units_in_selection.front();
                    if (primary && target == primary) {
                        int dx = pos.x - primary->sprite->position.x;
                        int dy = pos.y - primary->sprite->position.y;
                        const int same_thresh = 12*12;
                        if (dx*dx + dy*dy <= same_thresh) {
                            auto ap = adjust_pos_if_needed(pid, pos, target);
                            xy use_pos = ap.first;
                            log("  [명령] 자기 자신 근접 우클릭 → 이동 처리\n");
                            ui::log("[RC_SELF_MOVE] to (%d,%d)\n", use_pos.x, use_pos.y);
                            actions.action_default_order(pid, use_pos, nullptr, nullptr, shift_pressed);
                            handled = true;
                        }
                    }
                }
                if (!handled) {
                    log("  [명령] 아군 유닛 타겟 → Follow/Interact (기본 명령)\n");
                    ui::log("[RC] Default (ally target)\n");
                    actions.action_default_order(pid, pos, target, nullptr, shift_pressed);
                }
            } else {
                // 타겟 없거나 적군/적대적 대상: 근접 보정 후 기본 명령 (유닛 기준)
                auto ap = adjust_pos_if_needed(pid, pos, target);
                xy use_pos = ap.first;
                log("  [명령] 기본 명령 (스마트 명령)\n");
                ui::log("[RC] Default order to (%d,%d)\n", use_pos.x, use_pos.y);
                actions.action_default_order(pid, use_pos, target, nullptr, shift_pressed);
            }
        }
    };

    // 여러 소유자 처리: 허용시 각 선택 버킷에 직접 명령 발행
    if (allow_enemy_control) {
        bool any = false;
        for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) { any = true; break; }
        if (!any) {
            log("  [경고] 유효한 선택이 없어 명령 무시(적군 컨트롤 모드)\n");
            log("[우클릭 디버그 끝]\n\n");
            return;
        }
        // per-owner selection summary
        for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) {
            log("  [선택] pid=%d count=%d\n", pid, (int)action_st.selection[pid].size());
        }
        for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) {
            log("  [발행] pid=%d -> ", pid);
            try {
                issue_for_player(pid);
            } catch (const std::exception& ex) {
                log("  [오류] pid=%d 명령 발행 중 예외: %s\n", pid, ex.what());
            } catch (...) {
                log("  [오류] pid=%d 명령 발행 중 알 수 없는 예외\n", pid);
            }
        }
    } else {
        // Normal 모드: 일반 플레이어 명령 실행
        log("  [발행] pid=%d -> ", player_id);
        
        // 좌표 유효성 검사
        if (pos.x < 0 || pos.y < 0 || pos.x > 32768 || pos.y > 32768) {
            log("  [오류] 잘못된 좌표 감지! 명령 무시\n");
            log("[우클릭 디버그 끝]\n\n");
            return;
        }
        
        try {
            issue_for_player(player_id);
        } catch (const std::exception& ex) {
            log("[CRASH_ORDER] default pid=%d ex=%s pos=(%d,%d)\n", player_id, ex.what(), pos.x, pos.y);
        } catch (...) {
            log("[CRASH_ORDER] default pid=%d unknown pos=(%d,%d)\n", player_id, pos.x, pos.y);
        }
    }
    
    log("[우클릭 디버그 끝]\n\n");
}

void player_input_handler::on_command_button_click(int btn_index) {
    log("[CMD_BTN] 버튼 클릭: %d, 카테고리=%d\n", btn_index, g_ui_selected_category);
    
    // ★ 8번 버튼(ESC)은 카테고리와 관계없이 최우선 처리
    if (btn_index == 8) {
        int pid = get_effective_owner();
        unit_t* selected = nullptr;
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                if (!action_st.selection[p].empty()) {
                    selected = action_st.selection[p][0];
                    pid = p;
                    break;
                }
            }
        } else {
            if (!action_st.selection[pid].empty()) {
                selected = action_st.selection[pid][0];
            }
        }
        
        log(">>> 버튼 8 (ESC): 취소 명령, selected=%p, pid=%d\n", (void*)selected, pid);
        
        // 0) 오토렐리 모드에서 ESC → 오토렐리 삭제
        if (command_mode == CommandMode::AutoRally) {
            log(">>> ESC 버튼: 오토렐리 모드 취소 및 오토렐리 삭제\n");
            g_ui_active_button = 8;  // ESC 버튼 하이라이트
            
            // 선택된 건물의 오토렐리 삭제
            if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
                auto* brp = get_building_rally_storage(selected);
                if (brp && brp->has_auto_rally) {
                    brp->has_auto_rally = false;
                    brp->auto_rally_pos = {0, 0};
                    brp->auto_rally_target = nullptr;
                    
                    // 엔진의 렐리 포인트도 초기화
                    selected->building.rally.unit = selected;
                    selected->building.rally.pos = selected->sprite->position;
                    
                    log("[AUTO_RALLY_CLEAR] 오토렐리 삭제: building=%p\n", (void*)selected);
                    
                    // 오토렐리 시각화 제거
                    rally_visual.is_active = false;
                    rally_visual.frames_remaining = 0;
                    multi_rally_points.erase(
                        std::remove_if(multi_rally_points.begin(), multi_rally_points.end(),
                            [&](const MultiRallyPoint& mrp) {
                                return mrp.building_pos == selected->sprite->position && mrp.is_auto_rally;
                            }),
                        multi_rally_points.end()
                    );
                }
            }
            
            reset_command_mode();
            if (toast_cb) toast_cb("Auto rally cleared", 60);
            return;
        }
        
        // 0-1) 일반 명령 모드(Attack/Move/Patrol/Gather/Rally 등) 취소
        // command_mode가 Normal이 아니거나 UI 플래그가 살아 있으면 모드만 취소
        if (command_mode != CommandMode::Normal || g_ui_command_mode_active) {
            log(">>> ESC 버튼: 명령 모드/UI 취소 (cmd_mode=%d, ui_active=%d)\n", (int)command_mode, (int)g_ui_command_mode_active);
            g_ui_active_button = 8;  // ESC 버튼 하이라이트
            reset_command_mode();
            if (toast_cb) toast_cb("Command canceled", 30);
            return;
        }
        
        // 1) Build/AdvancedBuild 모드 취소
        if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
            log(">>> ESC 버튼: 건설 모드 취소\n");
            g_ui_active_button = 8;  // ESC 버튼 하이라이트
            reset_command_mode();
            if (toast_cb) toast_cb("Build mode canceled", 30);
            return;
        }
        
        // 2) 건물 선택 시: 건설/생산 취소 (오토렐리 삭제보다 우선)
        if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
            log("[ESC_BTN] selected type_id=%d, is_building=%d, remaining_build_time=%d, build_queue_size=%d\n",
                (int)selected->unit_type->id, 
                (int)(selected->unit_type->flags & unit_type_t::flag_building),
                selected->remaining_build_time,
                (int)selected->build_queue.size());
            
            // 건설 중인 건물 취소
            if (selected->remaining_build_time > 0) {
                log(">>> ESC 버튼: 건설 중인 건물 취소 (pid=%d)\n", pid);
                g_ui_active_button = 8;  // ESC 버튼 하이라이트
                bool result = actions.action_cancel_building_unit(pid);
                log("[ESC_BTN] action_cancel_building_unit 결과: %d\n", (int)result);
                if (toast_cb) toast_cb("Construction canceled", 30);
                return;
            }
            
            // 생산 중인 유닛 취소
            if (!selected->build_queue.empty()) {
                log(">>> ESC 버튼: 생산 중인 유닛 취소 (pid=%d)\n", pid);
                g_ui_active_button = 8;  // ESC 버튼 하이라이트
                bool result = actions.action_cancel_build_queue(pid, 254);
                log("[ESC_BTN] action_cancel_build_queue 결과: %d\n", (int)result);
                if (toast_cb) toast_cb("Training canceled", 30);
                return;
            }
            
            // 3) 오토렐리가 설정된 건물 선택 시 ESC → 오토렐리 초기화 (삭제)
            // (건설/생산 중이 아닐 때만 실행)
            auto* brp = get_building_rally_storage(selected);
            if (brp && brp->has_auto_rally) {
                brp->has_auto_rally = false;
                brp->auto_rally_pos = {0, 0};
                brp->auto_rally_target = nullptr;
                
                // ★ 엔진의 렐리 포인트도 초기화 (자기 자신으로 설정 = 렐리 없음)
                selected->building.rally.unit = selected;
                selected->building.rally.pos = selected->sprite->position;
                
                log("[AUTO_RALLY_CLEAR] 오토렐리 초기화: building=%p\n", (void*)selected);
                
                // ★ 오토렐리 시각화 제거
                rally_visual.is_active = false;
                rally_visual.frames_remaining = 0;
                // multi_rally_points에서 해당 건물의 오토렐리 제거
                multi_rally_points.erase(
                    std::remove_if(multi_rally_points.begin(), multi_rally_points.end(),
                        [&](const MultiRallyPoint& mrp) {
                            return mrp.building_pos == selected->sprite->position && mrp.is_auto_rally;
                        }),
                    multi_rally_points.end()
                );
                
                g_ui_active_button = 8;  // ESC 버튼 하이라이트
                if (toast_cb) toast_cb("Auto rally cleared", 60);
                return;
            }
        }
        
        log(">>> ESC 버튼: 취소할 것이 없음\n");
        return;
    }
    
    // ★ 카테고리별 버튼 유효성 체크
    // 카테고리 7번(다른 건물 여러 개): 모든 버튼 비활성화
    if (g_ui_selected_category == 7) {
        log("[CMD_BTN] 카테고리 7번: 명령 없음\n");
        return;
    }
    
    // 카테고리 8번(건물+유닛 혼합): 5,6,7번 버튼 비활성화
    if (g_ui_selected_category == 8 && (btn_index == 5 || btn_index == 6 || btn_index == 7)) {
        log("[CMD_BTN] 카테고리 8번: 버튼 %d 사용 불가 (G,B,V 없음)\n", btn_index);
        return;
    }
    
    // 카테고리 0번(유닛만): 7번 버튼(V) 체크는 일꾼 1기일 때만 허용
    if (g_ui_selected_category == 0 && btn_index == 7) {
        // 일꾼 1기만 선택되었는지 확인
        int selected_count = 0;
        bool has_worker = false;
        
        if (allow_enemy_control) {
            for (int p = 0; p < 8; ++p) {
                for (unit_t* u : action_st.selection[p]) {
                    selected_count++;
                    if (actions.unit_is(u, UnitTypes::Protoss_Probe) || 
                        actions.unit_is(u, UnitTypes::Zerg_Drone) ||
                        actions.unit_is(u, UnitTypes::Terran_SCV)) {
                        has_worker = true;
                    }
                }
            }
        } else {
            for (unit_t* u : action_st.selection[player_id]) {
                selected_count++;
                if (actions.unit_is(u, UnitTypes::Protoss_Probe) || 
                    actions.unit_is(u, UnitTypes::Zerg_Drone) ||
                    actions.unit_is(u, UnitTypes::Terran_SCV)) {
                    has_worker = true;
                }
            }
        }
        
        if (selected_count != 1 || !has_worker) {
            log("[CMD_BTN] V버튼: 일꾼 1기만 선택 시 사용 가능\n");
            return;
        }
    }
    
    int pid = get_effective_owner();
    
    // 선택된 유닛/건물 가져오기
    unit_t* selected = nullptr;
    if (allow_enemy_control) {
        for (int p = 0; p < 8; ++p) {
            if (!action_st.selection[p].empty()) {
                selected = action_st.selection[p][0];
                pid = p;
                break;
            }
        }
    } else {
        if (!action_st.selection[pid].empty()) {
            selected = action_st.selection[pid][0];
        }
    }
    
    // 자원만 선택된 카테고리(6)일 때만 명령 무시 (조용히)
    if (selected) {
        bool is_resource = actions.unit_is_mineral_field(selected) || 
                          actions.unit_is(selected, UnitTypes::Resource_Vespene_Geyser) ||
                          actions.unit_is_refinery(selected);
        if (is_resource && g_ui_selected_category == 6) {
            log("[CMD_BTN] 자원 카테고리에서 명령 무시: type=%d, cat=%d\n", (int)selected->unit_type->id, g_ui_selected_category);
            return;  // 토스트 메시지 없이 조용히 무시
        }
    }
    
    // 건물 전용 명령 처리: 건물 카테고리(1~5)일 때만 실행
    if (selected && (selected->unit_type->flags & unit_type_t::flag_building) &&
        (g_ui_selected_category >= 1 && g_ui_selected_category <= 5)) {
        // 넥서스 명령
        if (actions.unit_is(selected, UnitTypes::Protoss_Nexus)) {
            switch (btn_index) {
                case 0:  // 프로브 생산 (즉시 실행 + 하이라이트)
                    log(">>> 버튼 0: 프로브 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Probe));
                    if (toast_cb) toast_cb("Training Probe", 60);
                    break;
                case 5:  // 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 5: 렐리 포인트 모드\n");
                    g_ui_active_button = 5;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::Rally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Rally - Click anywhere", 90);
                    break;
                case 6:  // 자동 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 6: 자동 렐리 포인트 모드\n");
                    g_ui_active_button = 6;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::AutoRally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Auto Rally - Right-click resource", 90);
                    break;
                case 8:  // ESC - 생산 취소
                    if (!selected->build_queue.empty()) {
                        log(">>> 버튼 8: 넥서스 생산 취소\n");
                        actions.action_cancel_build_queue(pid, 254);
                        if (toast_cb) toast_cb("Training canceled", 30);
                        g_ui_active_button = -1;
                    }
                    break;
                default:
                    log(">>> 넥서스: 알 수 없는 버튼 %d\n", btn_index);
                    break;
            }
            return;
        }
        
        // 해처리 명령 (Zerg)
        if (actions.unit_is(selected, UnitTypes::Zerg_Hatchery) ||
            actions.unit_is(selected, UnitTypes::Zerg_Lair) ||
            actions.unit_is(selected, UnitTypes::Zerg_Hive)) {
            switch (btn_index) {
                case 0:  // 드론 생산 (즉시 실행 + 하이라이트)
                    log(">>> 버튼 0: 드론 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Zerg_Drone));
                    if (toast_cb) toast_cb("Training Drone", 60);
                    break;
                case 5:  // 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 5: 렐리 포인트 모드\n");
                    g_ui_active_button = 5;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::Rally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Rally - Click anywhere", 90);
                    break;
                case 6:  // 자동 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 6: 자동 렐리 포인트 모드\n");
                    g_ui_active_button = 6;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::AutoRally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Auto Rally - Right-click resource", 90);
                    break;
                case 8:  // ESC - 생산 취소
                    if (!selected->build_queue.empty()) {
                        log(">>> 버튼 8: 해처리 생산 취소\n");
                        actions.action_cancel_build_queue(pid, 254);
                        if (toast_cb) toast_cb("Training canceled", 30);
                        g_ui_active_button = -1;
                    }
                    break;
                default:
                    log(">>> 해처리: 알 수 없는 버튼 %d\n", btn_index);
                    break;
            }
            return;
        }
        
        // 커맨드 센터 명령 (Terran)
        if (actions.unit_is(selected, UnitTypes::Terran_Command_Center)) {
            switch (btn_index) {
                case 0:  // SCV 생산 (즉시 실행 + 하이라이트)
                    log(">>> 버튼 0: SCV 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Terran_SCV));
                    if (toast_cb) toast_cb("Training SCV", 60);
                    break;
                case 5:  // 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 5: 렐리 포인트 모드\n");
                    g_ui_active_button = 5;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::Rally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Rally - Click anywhere", 90);
                    break;
                case 6:  // 자동 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 6: 자동 렐리 포인트 모드\n");
                    g_ui_active_button = 6;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::AutoRally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Auto Rally - Right-click resource", 90);
                    break;
                case 8:  // ESC - 생산 취소
                    if (!selected->build_queue.empty()) {
                        log(">>> 버튼 8: 커맨드 센터 생산 취소\n");
                        actions.action_cancel_build_queue(pid, 254);
                        if (toast_cb) toast_cb("Training canceled", 30);
                        g_ui_active_button = -1;
                    }
                    break;
                default:
                    log(">>> 커맨드 센터: 알 수 없는 버튼 %d\n", btn_index);
                    break;
            }
            return;
        }
        
        // 게이트웨이 명령
        if (actions.unit_is(selected, UnitTypes::Protoss_Gateway)) {
            switch (btn_index) {
                case 0:  // 질럿 생산 (즉시 실행 + 하이라이트)
                    log(">>> 버튼 0: 질럿 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Zealot));
                    if (toast_cb) toast_cb("Training Zealot", 60);
                    break;
                case 1:  // 드라군 생산 (즉시 실행 + 하이라이트)
                    log(">>> 버튼 1: 드라군 생산\n");
                    g_ui_active_button = 1;  // 1번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Dragoon));
                    if (toast_cb) toast_cb("Training Dragoon", 60);
                    break;
                case 5:  // 렐리 포인트 (모드 진입, 하이라이트 설정)
                    log(">>> 버튼 5: 렐리 포인트 모드\n");
                    g_ui_active_button = 5;  // 모드 진입 시에만 하이라이트
                    command_mode = CommandMode::Rally;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Rally - Click anywhere", 90);
                    break;
                case 8:  // ESC - 생산 취소
                    if (!selected->build_queue.empty()) {
                        log(">>> 버튼 8: 게이트웨이 생산 취소\n");
                        actions.action_cancel_build_queue(pid, 254);
                        if (toast_cb) toast_cb("Training canceled", 30);
                        g_ui_active_button = -1;
                    }
                    break;
                default:
                    log(">>> 게이트웨이: 알 수 없는 버튼 %d\n", btn_index);
                    break;
            }
            return;
        }
        
        // 공격 가능한 건물 명령 (Photon Cannon, Sunken Colony, Spore Colony, Bunker, Missile Turret)
        bool is_attack_building = (
            actions.unit_is(selected, UnitTypes::Protoss_Photon_Cannon) ||
            actions.unit_is(selected, UnitTypes::Zerg_Sunken_Colony) ||
            actions.unit_is(selected, UnitTypes::Zerg_Spore_Colony) ||
            actions.unit_is(selected, UnitTypes::Terran_Bunker) ||
            actions.unit_is(selected, UnitTypes::Terran_Missile_Turret)
        );
        
        if (is_attack_building) {
            switch (btn_index) {
                case 0:  // A - 공격 모드
                    log(">>> 버튼 A: 공격 건물 - 공격 모드 활성화\n");
                    command_mode = CommandMode::Attack;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Attack mode - Left click target", 90);
                    break;
                case 1:  // S - 정지
                    log(">>> 버튼 S: 공격 건물 - 정지 명령\n");
                    if (allow_enemy_control) {
                        for (int p = 0; p < 8; ++p) if (!action_st.selection[p].empty()) actions.action_stop(p, shift_pressed);
                    } else actions.action_stop(pid, shift_pressed);
                    if (toast_cb) toast_cb("Stop", 30);
                    break;
                default:
                    log(">>> 공격 건물: 알 수 없는 버튼 %d\n", btn_index);
                    break;
            }
            return;
        }
        
        // 다른 건물들...
        log(">>> 건물 명령: 지원되지 않는 건물\n");
        return;
    }
    
    // 유닛 명령 처리 (건물이 아닐 때만)
    auto available = get_available_commands();
    
    switch (btn_index) {
        case 0:  // M - Move (유닛만) / A - Attack (공격 건물)
            // 카테고리 9: 프로브 건설 모드 - 0번 (B모드=Nexus, V모드=Robotics Facility)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Nexus));
                        if (toast_cb) toast_cb("Nexus", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Robotics_Facility));
                        if (toast_cb) toast_cb("Robotics Facility", 30);
                    }
                    g_ui_active_button = 0;
                    return;
                }
            }

            if (g_ui_selected_category == 5) {
                // 공격 건물: A 버튼 (공격 모드)
                command_mode = CommandMode::Attack;
                g_ui_command_mode_active = true;
                g_ui_active_button = 0;  // A 버튼 하이라이트
                log(">>> 버튼 0 (A): 공격 모드 활성화\n");
                if (selection_suppress_cb) selection_suppress_cb(true);
                if (toast_cb) toast_cb("Attack mode", 90);
            } else if (available.count(CommandMode::Move)) {
                // 일반 유닛: M 버튼 (이동 모드)
                command_mode = CommandMode::Move;
                g_ui_active_button = 0;  // M 버튼 하이라이트
                g_ui_command_mode_active = true;
                if (selection_suppress_cb) selection_suppress_cb(true);
                log(">>> 버튼 0 (M): 이동 모드 활성화\n");
                if (toast_cb) toast_cb("Move mode", 90);
            } else {
                log(">>> 버튼 0: 명령 사용 불가\n");
            }
            break;
        case 1:  // S - Stop (유닛 또는 공격 가능한 건물)
        {
            // 카테고리 9: 프로브 건설 모드 - 1번 (B모드=Pylon, V모드=Stargate)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Pylon));
                        if (toast_cb) toast_cb("Pylon", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Stargate));
                        if (toast_cb) toast_cb("Stargate", 30);
                    }
                    g_ui_active_button = 1;
                    return;
                }
            }

            bool can_stop = false;
            
            // 공격 가능한 건물인지 체크
            auto is_attack_building = [&](unit_t* u) -> bool {
                if (!u) return false;
                return (
                    actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
                    actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
                    actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
                    actions.unit_is(u, UnitTypes::Terran_Bunker) ||
                    actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
                );
            };
            
            // 유닛이거나 공격 가능한 건물인지 확인
            if (allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    for (unit_t* u : action_st.selection[p]) {
                        if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                            if (is_attack_building(u)) {
                                can_stop = true;
                                break;
                            }
                        } else if (u) {
                            can_stop = true;
                            break;
                        }
                    }
                    if (can_stop) break;
                }
            } else {
                for (unit_t* u : action_st.selection[pid]) {
                    if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                        if (is_attack_building(u)) {
                            can_stop = true;
                            break;
                        }
                    } else if (u) {
                        can_stop = true;
                        break;
                    }
                }
            }
            
            if (can_stop) {
                log(">>> 버튼 S: 정지 명령\n");
                g_ui_active_button = 1;  // S 버튼 하이라이트
                if (allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) if (!action_st.selection[p].empty()) actions.action_stop(p, shift_pressed);
                } else actions.action_stop(pid, shift_pressed);
                if (toast_cb) toast_cb("Stop", 30);
            } else {
                log(">>> 버튼 1: 명령 사용 불가 (공격 불가능한 건물)\n");
            }
            break;
        }
        case 2:  // A - Attack (유닛만)
            // 카테고리 9: 프로브 건설 모드 - 2번 (B모드=Assimilator, V모드=Citadel of Adun)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Assimilator));
                        if (toast_cb) toast_cb("Assimilator", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Citadel_of_Adun));
                        if (toast_cb) toast_cb("Citadel of Adun", 30);
                    }
                    g_ui_active_button = 2;
                    return;
                }
            }
            if (available.count(CommandMode::Attack)) {
                command_mode = CommandMode::Attack;
                // 공격 건물은 인덱스 0, 일반 유닛은 인덱스 2
                g_ui_active_button = (g_ui_selected_category == 5) ? 0 : 2;
                g_ui_command_mode_active = true;
                log(">>> 버튼 A: 공격 모드 활성화\n");
                if (selection_suppress_cb) selection_suppress_cb(true);
                if (toast_cb) toast_cb("Attack mode - Left click target", 90);
            } else {
                log(">>> 버튼 2: 명령 사용 불가 (건물 선택됨)\n");
            }
            break;
        case 3:  // P - Patrol (유닛만)
            // 카테고리 9: 프로브 건설 모드 - 3번 (B모드=Gateway, V모드=Robotics Support Bay)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Gateway));
                        if (toast_cb) toast_cb("Gateway", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Robotics_Support_Bay));
                        if (toast_cb) toast_cb("Robotics Support Bay", 30);
                    }
                    g_ui_active_button = 3;
                    return;
                }
            }
            if (available.count(CommandMode::Patrol)) {
                command_mode = CommandMode::Patrol;
                g_ui_active_button = 3;  // P 버튼 하이라이트
                g_ui_command_mode_active = true;
                log(">>> 버튼 P: 정찰 모드 활성화\n");
                if (selection_suppress_cb) selection_suppress_cb(true);
                if (toast_cb) toast_cb("Patrol mode", 90);
            } else {
                log(">>> 버튼 3: 명령 사용 불가 (건물 선택됨)\n");
            }
            break;
        case 4:  // H - Hold (유닛만)
            // 카테고리 9: 프로브 건설 모드 - 4번 (B모드=Forge, V모드=Fleet Beacon)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Forge));
                        if (toast_cb) toast_cb("Forge", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Fleet_Beacon));
                        if (toast_cb) toast_cb("Fleet Beacon", 30);
                    }
                    g_ui_active_button = 4;
                    return;
                }
            }

            if (available.count(CommandMode::Hold)) {
                log(">>> 버튼 H: 홀드 명령\n");
                g_ui_active_button = 4;  // H 버튼 하이라이트
                if (allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) if (!action_st.selection[p].empty()) actions.action_hold_position(p, shift_pressed);
                } else actions.action_hold_position(pid, shift_pressed);
                if (toast_cb) toast_cb("Hold position", 30);
            } else {
                log(">>> 버튼 4: 명령 사용 불가 (건물 선택됨)\n");
            }
            break;
        case 5:  // G - Gather (유닛만) / Rally (건물)
            // 카테고리 9: 프로브 건설 모드 - 5번 (B모드=Photon Cannon, V모드=Templar Archives)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Photon_Cannon));
                        if (toast_cb) toast_cb("Photon Cannon", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Templar_Archives));
                        if (toast_cb) toast_cb("Templar Archives", 30);
                    }
                    g_ui_active_button = 5;
                    return;
                }
            }

            // ★ 카테고리 체크: G버튼이 있는 카테고리만 허용
            if (g_ui_selected_category == 0 || g_ui_selected_category == 9) {
                // 유닛만 선택: Gather 가능
                if (available.count(CommandMode::Gather)) {
                    command_mode = CommandMode::Gather;
                    g_ui_active_button = 5;  // G 버튼 하이라이트
                    g_ui_command_mode_active = true;
                    log(">>> 버튼 G: Gather 모드 활성화\n");
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Gather mode - Left click resource", 90);
                } else {
                    log(">>> 버튼 5: Gather 명령 사용 불가 (cat=%d)\n", g_ui_selected_category);
                }
            } else if (g_ui_selected_category >= 1 && g_ui_selected_category <= 4) {
                // 생산 건물: Rally
                if (available.count(CommandMode::Rally)) {
                    command_mode = CommandMode::Rally;
                    g_ui_active_button = 5;  // R 버튼 하이라이트
                    g_ui_command_mode_active = true;  // 명령 모드 활성화
                    log(">>> 버튼 5: 렐리 포인트 모드\n");
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Rally - Click anywhere", 90);
                } else {
                    log(">>> 버튼 5: Rally 명령 사용 불가\n");
                }
            } else {
                log(">>> 버튼 5: 명령 사용 불가 (카테고리=%d, 버튼 없음)\n", g_ui_selected_category);
            }
            break;
        case 6:  // B - Build (일꾼) / AutoRally (넥서스/해처리/커맨드)
            // 카테고리 9: 프로브 건설 모드 - 6번 (B모드=Cybernetics Core, V모드=Observatory)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Cybernetics_Core));
                        if (toast_cb) toast_cb("Cybernetics Core", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Observatory));
                        if (toast_cb) toast_cb("Observatory", 30);
                    }
                    g_ui_active_button = 6;
                    return;
                }
            }

            if (available.count(CommandMode::Build)) {
                command_mode = CommandMode::Build;
                g_ui_active_button = -1; // 진입 시 하이라이트 초기화
                log(">>> 버튼 B: 빌드 모드 활성화\n");
                if (toast_cb) toast_cb("Build mode - Select building", 90);
            } else if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
                // 넥서스/해처리/커맨드 센터인 경우 AutoRally
                bool is_resource_building = (
                    actions.unit_is(selected, UnitTypes::Protoss_Nexus) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Hatchery) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Lair) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Hive) ||
                    actions.unit_is(selected, UnitTypes::Terran_Command_Center)
                );
                if (is_resource_building) {
                    command_mode = CommandMode::AutoRally;
                    log(">>> 버튼 6: 자동 렐리 포인트 모드\n");
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Auto Rally - Right-click resource", 90);
                } else {
                    log(">>> 버튼 6: 명령 사용 불가\n");
                }
            } else {
                log(">>> 버튼 6: 명령 사용 불가\n");
            }
            break;
        case 7:  // V - Advanced Build
            // 카테고리 9: 프로브 건설 모드 - 7번 (B모드=Shield Battery, V모드=Arbiter Tribunal)
            if (g_ui_selected_category == 9 &&
                (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                auto sel_worker = find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    if (command_mode == CommandMode::Build) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Shield_Battery));
                        if (toast_cb) toast_cb("Shield Battery", 30);
                    } else if (command_mode == CommandMode::AdvancedBuild) {
                        set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Arbiter_Tribunal));
                        if (toast_cb) toast_cb("Arbiter Tribunal", 30);
                    }
                    g_ui_active_button = 7;
                    return;
                }
            }

            if (available.count(CommandMode::AdvancedBuild)) {
                command_mode = CommandMode::AdvancedBuild;
                g_ui_active_button = -1; // 진입 시 하이라이트 초기화
                log(">>> 버튼 V: 고급 빌드 모드 활성화\n");
                if (toast_cb) toast_cb("Advanced build - Select building", 90);
            } else {
                log(">>> 버튼 V: 명령 사용 불가\n");
                if (toast_cb) toast_cb("Select worker first", 60);
            }
            break;
        default:
            log(">>> 알 수 없는 버튼: %d\n", btn_index);
            break;
    }
}

// 자동 렐리 포인트 설정 (시야 내 가장 가까운 자원)
void player_input_handler::set_auto_rally_point(unit_t* building, int pid) {
    if (!building) return;
    
    xy building_pos = building->sprite->position;
    unit_t* closest_resource = nullptr;
    int closest_dist = 999999;
    
    // 시야 내 모든 유닛 검색
    for (unit_t* u : ptr(st.visible_units)) {
        if (u->sprite->flags & sprite_t::flag_hidden) continue;
        
        // 미네랄 또는 가스 체크
        bool is_mineral = (u->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                          u->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                          u->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3);
        
        bool is_gas = (u->unit_type->id == UnitTypes::Protoss_Assimilator ||
                      u->unit_type->id == UnitTypes::Zerg_Extractor ||
                      u->unit_type->id == UnitTypes::Terran_Refinery);
        
        // 가스 건물은 완성되고 소유자가 같을 때만
        if (is_gas && (u->remaining_build_time > 0 || u->owner != pid)) {
            is_gas = false;
        }
        
        if (is_mineral || is_gas) {
            xy resource_pos = u->sprite->position;
            int dx = building_pos.x - resource_pos.x;
            int dy = building_pos.y - resource_pos.y;
            int dist = dx * dx + dy * dy;
            
            if (dist < closest_dist) {
                closest_dist = dist;
                closest_resource = u;
            }
        }
    }
    
    if (closest_resource) {
        xy rally_pos = closest_resource->sprite->position;
        actions.action_order(pid, actions.get_order_type(Orders::RallyPointUnit), rally_pos, closest_resource, nullptr, false);
        log("[AUTO_RALLY] pid=%d 자동 렐리: resource_type=%d at (%d,%d)\n", 
            pid, (int)closest_resource->unit_type->id, rally_pos.x, rally_pos.y);
        if (toast_cb) toast_cb("Auto rally to resource", 60);
    } else {
        log("[AUTO_RALLY] pid=%d 자원을 찾을 수 없음\n", pid);
        if (toast_cb) toast_cb("No resource found", 60);
    }
}

void player_input_handler::on_key_down(int scancode) {
    // Shift, Ctrl, Alt
    if (scancode == 225 || scancode == 229) shift_pressed = true;
    if (scancode == 224 || scancode == 228) ctrl_pressed = true;
    if (scancode == 226 || scancode == 230) alt_pressed = true;
    
    // 명령 키
    ui::log("[KEY DOWN] scancode=%d, command_mode=%d\n", scancode, (int)command_mode);
    
    // 알파벳 키 매핑 디버그 (A=4, B=5, ..., N=17, ...)
    if (scancode >= 4 && scancode <= 29) {
        char key_char = 'a' + (scancode - 4);
        log("[KEY_MAP] scancode=%d -> '%c'\n", scancode, key_char);
    }
    
    // 숫자 키 매핑 디버그
    if (scancode >= 30 && scancode <= 38) {
        int num = scancode - 30 + 1;
        log("[KEY_MAP] scancode=%d -> 메인 키보드 '%d'\n", scancode, num);
    } else if (scancode == 39) {
        log("[KEY_MAP] scancode=%d -> 메인 키보드 '0'\n", scancode);
    } else if (scancode >= 89 && scancode <= 97) {
        int num = scancode - 89 + 1;
        log("[KEY_MAP] scancode=%d -> 넘패드 '%d'\n", scancode, num);
    } else if (scancode == 98) {
        log("[KEY_MAP] scancode=%d -> 넘패드 '0'\n", scancode);
    }
    
    // 카테고리 7(혼합 건물 상태, 명령 없음)일 때는 알파벳 단축키(A~Z, scancode 4~29)를 모두 무시
    // 시스템 키(ESC, F-keys, 숫자키/넘패드, 부대지정 등)는 그대로 동작하도록 허용
    if (g_ui_selected_category == 7 && scancode >= 4 && scancode <= 29) {
        log("[KEY_BLOCK] category 7: ignore command hotkey (scancode=%d)\n", scancode);
        return;
    }
    
    switch (scancode) {
        case 4:  // A - 공격 모드 / (Build) Protoss Assimilator / (V) Arbiter Tribunal
            // V 모드: Arbiter Tribunal (패널 7)
            if (command_mode == CommandMode::AdvancedBuild && g_ui_selected_category == 9 && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Arbiter_Tribunal));
                g_ui_active_button = 7;
                if (toast_cb) toast_cb("Arbiter Tribunal", 30);
                break;
            }

            // B 모드: Assimilator (패널 2)
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Assimilator));
                if (g_ui_selected_category == 9) g_ui_active_button = 2; // B 모드 패널: 버튼 2 = Assimilator
                if (toast_cb) toast_cb("Assimilator", 30);
                break;
            }
            
            // A 버튼이 없는 카테고리에서는 A키 무시
            // 카테고리: 0=유닛(A있음), 1=Nexus(A없음), 2=Gateway(A없음), 3=Hatchery(A없음), 4=Command(A없음), 5=AttackBuilding(A있음), 6=Resource(A없음)
            if (g_ui_selected_category == 1 || g_ui_selected_category == 2 || 
                g_ui_selected_category == 3 || g_ui_selected_category == 4 || 
                g_ui_selected_category == 6) {
                log(">>> A키: 명령 패널에 A 버튼 없음 - 무시 (category=%d)\n", g_ui_selected_category);
                break;
            }
            
            // 공격 가능한 건물 체크
            {
                bool can_attack = false;
                int pid = get_effective_owner();
                
                auto is_attack_building = [&](unit_t* u) -> bool {
                    if (!u) return false;
                    return (
                        actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
                        actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
                        actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
                        actions.unit_is(u, UnitTypes::Terran_Bunker) ||
                        actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
                    );
                };
                
                // 유닛이거나 공격 가능한 건물인지 확인
                if (allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) {
                        for (unit_t* u : action_st.selection[p]) {
                            if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                                if (is_attack_building(u)) {
                                    can_attack = true;
                                    log("[A_KEY] 공격 건물 발견: type=%d, owner=%d\n", (int)u->unit_type->id, u->owner);
                                    break;
                                }
                            } else if (u) {
                                can_attack = true;
                                log("[A_KEY] 유닛 발견: type=%d, owner=%d\n", (int)u->unit_type->id, u->owner);
                                break;
                            }
                        }
                        if (can_attack) break;
                    }
                } else {
                    for (unit_t* u : action_st.selection[pid]) {
                        if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                            if (is_attack_building(u)) {
                                can_attack = true;
                                log("[A_KEY] 공격 건물 발견: type=%d, owner=%d\n", (int)u->unit_type->id, u->owner);
                                break;
                            }
                        } else if (u) {
                            can_attack = true;
                            log("[A_KEY] 유닛 발견: type=%d, owner=%d\n", (int)u->unit_type->id, u->owner);
                            break;
                        }
                    }
                }
                
                if (can_attack) {
                    command_mode = CommandMode::Attack;
                    // 공격 건물은 인덱스 0, 일반 유닛은 인덱스 2
                    g_ui_active_button = (g_ui_selected_category == 5) ? 0 : 2;
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Attack - Left click target", 60);
                } else {
                    					log(">>> A키: 명령 사용 불가 (공격 불가능한 건물)\n");
				}
			}
			break;
			
		case 16:  // M - 이동 모드
			// M 버튼이 없는 카테고리에서는 M키 무시 (1,2,3,4,5,6,9 = 건물/자원/프로브건설)
			if (g_ui_selected_category >= 1 && g_ui_selected_category <= 6) {
				log(">>> M키: 명령 패널에 M 버튼 없음 - 무시 (category=%d)\n", g_ui_selected_category);
				break;
			}
			// 카테고리 9 (프로브 건설 모드)에서는 M키 무시
			if (g_ui_selected_category == 9) {
				log(">>> M키: 건설 모드에는 M 버튼 없음 - 무시\n");
				break;
			}
			if (can_use_command(CommandMode::Move)) {
				command_mode = CommandMode::Move;
				g_ui_active_button = 0;  // M 버튼 하이라이트
				g_ui_command_mode_active = true;
				log(">>> M키: 이동 모드 활성화\n");
				if (selection_suppress_cb) selection_suppress_cb(true);
				if (toast_cb) toast_cb("Move mode", 30);
			} else {
				log(">>> M키: 명령 사용 불가 (건물 선택됨)\n");
			}
			break;
			
		case 19:  // P - 정찰 모드 (Patrol) / (Build) Pylon
		{
			// 1) Build 모드에서 프로토스 일꾼이 선택되어 있으면: 파일런 건설 선택
			if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
				set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Pylon));
				g_ui_active_button = 1; // B 모드 패널: 버튼 1 = Pylon
				if (toast_cb) toast_cb("Pylon", 30);
				break;
			}
			
			// 카테고리 9 (프로브 건설 모드)에서 V 모드일 때는 P키 무시
			if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild) {
				log(">>> P키: V 건설 모드에는 P 버튼 없음 - 무시\n");
				break;
			}
			
			int pid = get_effective_owner();
			unit_t* selected = nullptr;
			if (allow_enemy_control) {
				for (int p = 0; p < 8; ++p) {
					if (!action_st.selection[p].empty()) {
						selected = action_st.selection[p][0];
						pid = p;
						break;
					}
				}
			} else {
				if (!action_st.selection[pid].empty()) {
					selected = action_st.selection[pid][0];
				}
			}
			
			if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
				// 넥서스/해처리/커맨드 센터인 경우 프로브 생산
				bool is_resource_building = (
					actions.unit_is(selected, UnitTypes::Protoss_Nexus) ||
					actions.unit_is(selected, UnitTypes::Zerg_Hatchery) ||
					actions.unit_is(selected, UnitTypes::Zerg_Lair) ||
					actions.unit_is(selected, UnitTypes::Zerg_Hive) ||
					actions.unit_is(selected, UnitTypes::Terran_Command_Center)
				);
				if (is_resource_building) {
					log(">>> P키: 프로브 생산\n");
					g_ui_active_button = 0;
					actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Probe));
					if (toast_cb) toast_cb("Training Probe", 60);
					break;
				}
			}
			
			// 2) 일반 Patrol 모드 진입
			// P 버튼이 없는 카테고리에서는 P키 무시 (1,2,3,4,5,6 = 건물/자원)
			if (g_ui_selected_category >= 1 && g_ui_selected_category <= 6) {
				log(">>> P키: 명령 패널에 P 버튼 없음 - 무시 (category=%d)\n", g_ui_selected_category);
				break;
			}
			if (can_use_command(CommandMode::Patrol)) {
				command_mode = CommandMode::Patrol;
				g_ui_active_button = 3;  // P 버튼 하이라이트
				g_ui_command_mode_active = true;
				log(">>> P키: 정찰 모드 활성화\n");
				if (selection_suppress_cb) selection_suppress_cb(true);
				if (toast_cb) toast_cb("Patrol mode", 30);
			} else {
				log(">>> P키: 명령 사용 불가 (건물 선택됨 또는 유닛 없음)\n");
			}
			break;
		}
			
		case 22:  // S - 정지 / (Build) Zerg Spawning Pool / (V) Protoss Stargate
            // 프로브 V 모드: S키로 Stargate 선택
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                // V 모드: Stargate (패널 1)
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Stargate));
                g_ui_active_button = 1;
                if (toast_cb) toast_cb("Stargate", 30);
                break;
            }
            
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Spawning_Pool));
                if (toast_cb) toast_cb("Spawning Pool", 30);
                break;
            }
            
            // S 버튼이 없는 카테고리에서는 S키 무시
            // 카테고리: 0=유닛(S있음), 1=Nexus(S없음), 2=Gateway(S없음), 3=Hatchery(S없음), 4=Command(S없음), 5=AttackBuilding(S있음), 6=Resource(S없음), 9=프로브건설(B모드에는 S없음, V모드에만 S있음)
            if (g_ui_selected_category == 1 || g_ui_selected_category == 2 || 
                g_ui_selected_category == 3 || g_ui_selected_category == 4 || 
                g_ui_selected_category == 6) {
                log(">>> S키: 명령 패널에 S 버튼 없음 - 무시 (category=%d)\n", g_ui_selected_category);
                break;
            }
            // 카테고리 9 (프로브 건설 모드)에서 B 모드일 때는 S키 무시
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build) {
                log(">>> S키: B 건설 모드에는 S 버튼 없음 - 무시\n");
                break;
            }
            
            // 이동 가능한 유닛 또는 공격 가능한 건물에 정지 명령 실행
            {
                bool can_stop = false;
                int pid = get_effective_owner();
                
                // 자원인지 체크하는 함수
                auto is_resource_unit = [&](unit_t* u) -> bool {
                    if (!u) return false;
                    return (actions.unit_is_mineral_field(u) || 
                           actions.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                           actions.unit_is_refinery(u));
                };
                
                // 공격 가능한 건물인지 체크하는 함수
                auto is_attack_building = [&](unit_t* u) -> bool {
                    if (!u) return false;
                    return (
                        // Protoss
                        actions.unit_is(u, UnitTypes::Protoss_Photon_Cannon) ||
                        // Zerg
                        actions.unit_is(u, UnitTypes::Zerg_Sunken_Colony) ||
                        actions.unit_is(u, UnitTypes::Zerg_Spore_Colony) ||
                        // Terran
                        actions.unit_is(u, UnitTypes::Terran_Bunker) ||
                        actions.unit_is(u, UnitTypes::Terran_Missile_Turret)
                    );
                };
                
                // 선택된 유닛/건물 체크
                if (allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) {
                        for (unit_t* u : action_st.selection[p]) {
                            if (is_resource_unit(u)) {
                                log("[S_KEY] 자원 발견 - 정지 불가: type=%d\n", (int)u->unit_type->id);
                                continue;  // 자원은 건너뛰기
                            }
                            if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                                if (is_attack_building(u)) {
                                    can_stop = true;
                                    log("[S_KEY] 공격 가능한 건물 발견: type=%d\n", (int)u->unit_type->id);
                                    break;
                                } else {
                                    log("[S_KEY] 공격 불가능한 건물: type=%d\n", (int)u->unit_type->id);
                                }
                            } else if (u) {
                                can_stop = true;  // 유닛은 모두 정지 가능
                                log("[S_KEY] 유닛 발견: type=%d\n", (int)u->unit_type->id);
                                break;
                            }
                        }
                        if (can_stop) break;
                    }
                } else {
                    for (unit_t* u : action_st.selection[pid]) {
                        if (is_resource_unit(u)) {
                            log("[S_KEY] 자원 발견 - 정지 불가: type=%d\n", (int)u->unit_type->id);
                            continue;  // 자원은 건너뛰기
                        }
                        if (u && (u->unit_type->flags & unit_type_t::flag_building)) {
                            if (is_attack_building(u)) {
                                can_stop = true;
                                log("[S_KEY] 공격 가능한 건물 발견: type=%d\n", (int)u->unit_type->id);
                                break;
                            } else {
                                log("[S_KEY] 공격 불가능한 건물: type=%d\n", (int)u->unit_type->id);
                            }
                        } else if (u) {
                            can_stop = true;
                            log("[S_KEY] 유닛 발견: type=%d\n", (int)u->unit_type->id);
                            break;
                        }
                    }
                }
                
                if (can_stop) {
                    log(">>> S키: 정지 명령 실행\n");
                    g_ui_active_button = 1;  // S 버튼 하이라이트
                    if (allow_enemy_control) {
                        for (int p = 0; p < 8; ++p) if (!action_st.selection[p].empty()) actions.action_stop(p, shift_pressed);
                    } else actions.action_stop(pid, shift_pressed);
                    if (toast_cb) toast_cb("Stop", 30);
                } else {
                    log(">>> S키: 명령 사용 불가 (공격 불가능한 건물만 선택됨)\n");
                    if (toast_cb) toast_cb("Cannot stop this building", 30);
                }
            }
            break;
            
        		case 11:  // H - 홀드 / (Build) Zerg Hatchery
			// 카테고리 9 (프로브 건설 모드)에서는 H키 무시
			if (g_ui_selected_category == 9) {
				log(">>> H키: 건설 모드에는 H 버튼 없음 - 무시\n");
				break;
			}
			
			if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
				set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Hatchery));
				if (toast_cb) toast_cb("Hatchery", 30);
				break;
			}
			// 이동 가능한 유닛이 있을 때만 홀드 명령 실행
			if (can_use_command(CommandMode::Hold)) {
				log(">>> H키: 홀드 명령\n");
				g_ui_active_button = 4;  // H 버튼 하이라이트
				if (allow_enemy_control) {
					for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) actions.action_hold_position(pid, shift_pressed);
				} else actions.action_hold_position(player_id, shift_pressed);
				if (toast_cb) toast_cb("Hold position", 30);
			} else {
				log(">>> H키: 명령 사용 불가 (건물 선택됨)\n");
			}
			break;

		case 41:  // ESC - 취소: (0) 빌드 모드 취소, (1) 명령 모드 취소, (2) 오토렐리 초기화, (3) 생산/건설 취소
		{
			log("[ESC_KEY] ESC 키 입력, command_mode=%d\n", (int)command_mode);

			// 0) Build/AdvancedBuild 모드는 전용 처리
			if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
				log(">>> ESC: 빌드 모드 취소\n");
				exit_build_mode();
				break;
			}

			// 1) 오토렐리 모드에서 ESC → 오토렐리 삭제
			if (command_mode == CommandMode::AutoRally) {
				log(">>> ESC: 오토렐리 모드 취소 및 오토렐리 삭제\n");
				
				// 선택된 건물의 오토렐리 삭제
				int eff_pid = get_effective_owner();
				unit_t* selected_building = actions.get_single_selected_unit(eff_pid);
				if (selected_building && (selected_building->unit_type->flags & unit_type_t::flag_building)) {
					auto* brp = get_building_rally_storage(selected_building);
					if (brp && brp->has_auto_rally) {
						brp->has_auto_rally = false;
						brp->auto_rally_pos = {0, 0};
						brp->auto_rally_target = nullptr;
						
						// 엔진의 렐리 포인트도 초기화
						selected_building->building.rally.unit = selected_building;
						selected_building->building.rally.pos = selected_building->sprite->position;
						
						log("[AUTO_RALLY_CLEAR] 오토렐리 삭제: building=%p\n", (void*)selected_building);
						
						// 오토렐리 시각화 제거
						rally_visual.is_active = false;
						rally_visual.frames_remaining = 0;
						multi_rally_points.erase(
							std::remove_if(multi_rally_points.begin(), multi_rally_points.end(),
								[&](const MultiRallyPoint& mrp) {
									return mrp.building_pos == selected_building->sprite->position && mrp.is_auto_rally;
								}),
							multi_rally_points.end()
						);
					}
				}
				
				reset_command_mode();
				if (toast_cb) toast_cb("Auto rally cleared", 60);
				break;
			}
			
			// 1-1) 일반 명령 모드(Attack/Move/Patrol/Gather/Rally 등) 취소
			// command_mode가 Normal이 아니거나 UI 플래그가 살아 있으면 모드만 취소
			if (command_mode != CommandMode::Normal || g_ui_command_mode_active) {
				log(">>> ESC: 명령 모드/UI 취소 (cmd_mode=%d, ui_active=%d)\n", (int)command_mode, (int)g_ui_command_mode_active);
				reset_command_mode();
				break;
			}

			bool canceled = false;

			// 2) 생산 중인 유닛 취소 (건물 선택 시) - 오토렐리 삭제보다 우선
			unit_t* selected = nullptr;
			int pid = player_id;
			if (allow_enemy_control) {
				for (int p = 0; p < 8; ++p) {
					if (!action_st.selection[p].empty()) {
						selected = action_st.selection[p][0];
						pid = p;
						break;
					}
				}
			} else {
				if (!action_st.selection[player_id].empty()) {
					selected = action_st.selection[player_id][0];
				}
			}

			log("[ESC_KEY] selected=%p, is_building=%d\n", (void*)selected,
				selected ? (int)(selected->unit_type->flags & unit_type_t::flag_building) : 0);

			if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
				log("[ESC_KEY] build_queue.size()=%d, remaining_build_time=%d\n",
					(int)selected->build_queue.size(), selected->remaining_build_time);

				// 생산 중인 유닛이 있으면 취소
				if (!selected->build_queue.empty()) {
					log(">>> ESC: 생산 중인 유닛 취소 (pid=%d)\n", pid);
					actions.action_cancel_build_queue(pid, 254);
					g_ui_active_button = -1;  // 하이라이트 해제
					if (toast_cb) toast_cb("Training canceled", 30);
					canceled = true;
				}
			}

			// 3) 건설 중인 건물 취소
			if (!canceled) {
				if (allow_enemy_control) {
					for (int p = 0; p < 8; ++p) {
						if (!action_st.selection[p].empty()) {
							if (actions.action_cancel_building_unit(p)) {
								log(">>> ESC: pid=%d 건설 중인 건물 취소\n", p);
								canceled = true;
							}
						}
					}
				} else {
					if (actions.action_cancel_building_unit(player_id)) {
						log(">>> ESC: 건설 중인 건물 취소\n");
						canceled = true;
					}
				}
			}
			
			// 4) 오토렐리가 설정된 건물 선택 시 ESC → 오토렐리 초기화 (삭제)
			// (건설/생산 취소가 없었을 때만 실행)
			if (!canceled && selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
				auto* brp = get_building_rally_storage(selected);
				if (brp && brp->has_auto_rally) {
					brp->has_auto_rally = false;
					brp->auto_rally_pos = {0, 0};
					brp->auto_rally_target = nullptr;
					
					// ★ 엔진의 렐리 포인트도 초기화 (자기 자신으로 설정 = 렐리 없음)
					selected->building.rally.unit = selected;
					selected->building.rally.pos = selected->sprite->position;
					
					log("[AUTO_RALLY_CLEAR] 오토렐리 초기화: building=%p\n", (void*)selected);
					
					// ★ 오토렐리 시각화 제거
					rally_visual.is_active = false;
					rally_visual.frames_remaining = 0;
					// multi_rally_points에서 해당 건물의 오토렐리 제거
					multi_rally_points.erase(
						std::remove_if(multi_rally_points.begin(), multi_rally_points.end(),
							[&](const MultiRallyPoint& mrp) {
								return mrp.building_pos == selected->sprite->position && mrp.is_auto_rally;
							}),
						multi_rally_points.end()
					);
					
					if (toast_cb) toast_cb("Auto rally cleared", 60);
				}
			}
			break;
		}

		case 30: case 31: case 32: case 33: case 34:
        case 35: case 36: case 37: case 38:
            {
                int group = scancode - 30;  // 메인 키보드 1-9 -> 그룹 0~8
                
                log("[MAIN_KEYBOARD] scancode=%d, group=%d, ctrl=%d, shift=%d\n", 
                    scancode, group, (int)ctrl_pressed, (int)shift_pressed);
                
                int pid = get_effective_owner();
                
                if (ctrl_pressed) {
                    // Ctrl+숫자: 부대 지정
                    if (allow_enemy_control) {
                        // 적군 컨트롤 모드: 모든 플레이어의 해당 그룹을 초기화한 후, 선택된 유닛만 지정
                        // 1단계: 모든 플레이어의 해당 그룹 초기화
                        for (int p = 0; p < 8; ++p) {
                            action_st.control_groups[p][group].clear();
                        }
                        // 2단계: 선택된 유닛이 있는 플레이어만 부대 지정
                        int total_count = 0;
                        for (int p = 0; p < 8; ++p) {
                            if (!action_st.selection[p].empty()) {
                                actions.action_control_group(p, group, 0);
                                total_count += action_st.control_groups[p][group].size();
                            }
                        }
                        log(">>> Ctrl+%d: 메인 그룹 %d 설정 (적군 컨트롤 모드, 총 유닛: %d개)\n", 
                            group + 1, group + 1, total_count);
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Main Group %d set (All)", group + 1);
                            toast_cb(msg, 30);
                        }
                    } else {
                        // 일반 모드: 단일 플레이어만
                        int before_count = action_st.selection[pid].size();
                        log(">>> Ctrl+%d: 메인 그룹 %d 설정 (pid=%d, 현재 선택: %d개)\n", 
                            group + 1, group + 1, pid, before_count);
                        
                        bool result = actions.action_control_group(pid, group, 0);
                        
                        int group_size = action_st.control_groups[pid][group].size();
                        log("[MAIN_KEYBOARD_RESULT] 설정 결과: %d, 그룹에 저장된 유닛 수: %d\n", 
                            (int)result, group_size);
                        
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Main Group %d set", group + 1);
                            toast_cb(msg, 30);
                        }
                    }
                } else if (shift_pressed) {
                    // Shift+숫자: 부대에 추가
                    if (allow_enemy_control) {
                        // 적군 컨트롤 모드: 모든 플레이어의 선택을 각각 부대에 추가
                        int total_count = 0;
                        for (int p = 0; p < 8; ++p) {
                            if (!action_st.selection[p].empty()) {
                                actions.action_control_group(p, group, 2);
                                total_count += action_st.control_groups[p][group].size();
                            }
                        }
                        log(">>> Shift+%d: 메인 그룹 %d에 추가 (적군 컨트롤 모드, 총 유닛: %d개)\n", 
                            group + 1, group + 1, total_count);
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Added to Main Group %d (All)", group + 1);
                            toast_cb(msg, 30);
                        }
                    } else {
                        // 일반 모드
                        log(">>> Shift+%d: 메인 그룹 %d에 추가 (pid=%d)\n", group + 1, group + 1, pid);
                        bool result = actions.action_control_group(pid, group, 2);
                        log("[MAIN_KEYBOARD_RESULT] 추가 결과: %d\n", (int)result);
                        
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Added to Main Group %d", group + 1);
                            toast_cb(msg, 30);
                        }
                    }
                } else {
                    // 숫자만: 메인 부대 호출
                    auto now = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_group_select_time[group]).count();
                    
                    bool is_double_click = (last_group_index_for_double_click == group && elapsed < DOUBLE_CLICK_MS);
                    last_group_select_time[group] = now;
                    last_group_index_for_double_click = group;
                    
                    if (allow_enemy_control) {
                        // 적군 컨트롤 모드: 그룹에 유닛이 있는 플레이어만 호출하고, 나머지는 선택 초기화
                        int total_count = 0;
                        for (int p = 0; p < 8; ++p) {
                            int group_size = action_st.control_groups[p][group].size();
                            if (group_size > 0) {
                                // 그룹에 유닛이 있으면 호출
                                actions.action_control_group(p, group, 1);
                                total_count += action_st.selection[p].size();
                            } else {
                                // 그룹이 비어있으면 해당 플레이어의 선택 초기화
                                action_st.selection[p].clear();
                            }
                        }
                        log(">>> %d: 메인 그룹 %d 호출 (적군 컨트롤 모드, 총 선택: %d개, 더블클릭=%d)\n", 
                            group + 1, group + 1, total_count, (int)is_double_click);
                        
                        // 더블클릭: 화면 이동
                        if (is_double_click && move_camera_cb && total_count > 0) {
                            xy sum_pos{0, 0};
                            int count = 0;
                            for (int p = 0; p < 8; ++p) {
                                for (unit_t* u : action_st.selection[p]) {
                                    if (u && u->sprite) {
                                        sum_pos.x += u->sprite->position.x;
                                        sum_pos.y += u->sprite->position.y;
                                        count++;
                                    }
                                }
                            }
                            if (count > 0) {
                                xy center{sum_pos.x / count, sum_pos.y / count};
                                log(">>> 메인 부대 %d 화면 이동: (%d,%d)\n", group + 1, center.x, center.y);
                                move_camera_cb(center);
                                if (toast_cb) {
                                    char msg[32];
                                    snprintf(msg, sizeof(msg), "Main Group %d (All)", group + 1);
                                    toast_cb(msg, 30);
                                }
                            }
                        }
                    } else {
                        // 일반 모드
                        int group_size = action_st.control_groups[pid][group].size();
                        log(">>> %d: 메인 그룹 %d 호출 (pid=%d, 그룹 유닛 수: %d, 더블클릭=%d)\n", 
                            group + 1, group + 1, pid, group_size, (int)is_double_click);
                        
                        bool result = actions.action_control_group(pid, group, 1);
                        
                        int after_count = action_st.selection[pid].size();
                        log("[MAIN_KEYBOARD_RESULT] 호출 결과: %d, 선택된 유닛 수: %d\n", 
                            (int)result, after_count);
                        
                        // 더블클릭: 화면 이동 (부대에 유닛이 있을 때만)
                        if (is_double_click && move_camera_cb && group_size > 0 && after_count > 0) {
                            xy sum_pos{0, 0};
                            int count = 0;
                            for (unit_t* u : action_st.selection[pid]) {
                                if (u && u->sprite) {
                                    sum_pos.x += u->sprite->position.x;
                                    sum_pos.y += u->sprite->position.y;
                                    count++;
                                }
                            }
                            if (count > 0) {
                                xy center{sum_pos.x / count, sum_pos.y / count};
                                log(">>> 메인 부대 %d 화면 이동: (%d,%d)\n", group + 1, center.x, center.y);
                                move_camera_cb(center);
                                if (toast_cb) {
                                    char msg[32];
                                    snprintf(msg, sizeof(msg), "Main Group %d", group + 1);
                                    toast_cb(msg, 30);
                                }
                            }
                        }
                    }
                    
                    skip_selection_sync_frames = 2;
                }
            }
            break;
            
        // 넘패드 1-9 (독립 부대지정: 인덱스 10~18, 한 번 누르면 지정, 다시 누르면 호출)
        case 89: case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            {
                int numpad_group = scancode - 89;  // 넘패드 1-9
                int group = numpad_group + 10;  // 그룹 인덱스 10~18 (넘패드 전용)
                
                log("[NUMPAD] scancode=%d, numpad_group=%d, group_index=%d\n", 
                    scancode, numpad_group + 1, group);
                
                int pid = get_effective_owner();
                
                if (allow_enemy_control) {
                    // 적군 컨트롤 모드: 모든 플레이어 확인
                    int total_group_size = 0;
                    int total_selection = 0;
                    for (int p = 0; p < 8; ++p) {
                        total_group_size += action_st.control_groups[p][group].size();
                        total_selection += action_st.selection[p].size();
                    }
                    
                    if (total_group_size == 0 && total_selection > 0) {
                        // 그룹이 비어있고 선택된 유닛이 있으면 -> 부대 지정
                        // 1단계: 모든 플레이어의 해당 그룹 초기화
                        for (int p = 0; p < 8; ++p) {
                            action_st.control_groups[p][group].clear();
                        }
                        // 2단계: 선택된 유닛이 있는 플레이어만 부대 지정
                        for (int p = 0; p < 8; ++p) {
                            if (!action_st.selection[p].empty()) {
                                actions.action_control_group(p, group, 0);
                            }
                        }
                        log(">>> 넘패드%d: 넘패드 그룹 %d 지정 (적군 컨트롤 모드, 총 유닛: %d개)\n", 
                            numpad_group + 1, numpad_group + 1, total_selection);
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Numpad Group %d set (All)", numpad_group + 1);
                            toast_cb(msg, 30);
                        }
                    } else if (total_group_size > 0) {
                        // 그룹에 유닛이 있으면 -> 부대 호출
                        auto now = std::chrono::high_resolution_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_group_select_time[group]).count();
                        
                        bool is_double_click = (last_group_index_for_double_click == group && elapsed < DOUBLE_CLICK_MS);
                        last_group_select_time[group] = now;
                        last_group_index_for_double_click = group;
                        
                        int total_selected = 0;
                        for (int p = 0; p < 8; ++p) {
                            if (action_st.control_groups[p][group].size() > 0) {
                                // 그룹에 유닛이 있으면 호출
                                actions.action_control_group(p, group, 1);
                                total_selected += action_st.selection[p].size();
                            } else {
                                // 그룹이 비어있으면 해당 플레이어의 선택 초기화
                                action_st.selection[p].clear();
                            }
                        }
                        log(">>> 넘패드%d: 넘패드 그룹 %d 호출 (적군 컨트롤 모드, 총 선택: %d개, 더블클릭=%d)\n", 
                            numpad_group + 1, numpad_group + 1, total_selected, (int)is_double_click);
                        
                        // 더블클릭: 화면 이동
                        if (is_double_click && move_camera_cb && total_selected > 0) {
                            xy sum_pos{0, 0};
                            int count = 0;
                            for (int p = 0; p < 8; ++p) {
                                for (unit_t* u : action_st.selection[p]) {
                                    if (u && u->sprite) {
                                        sum_pos.x += u->sprite->position.x;
                                        sum_pos.y += u->sprite->position.y;
                                        count++;
                                    }
                                }
                            }
                            if (count > 0) {
                                xy center{sum_pos.x / count, sum_pos.y / count};
                                log(">>> 넘패드 부대 %d 화면 이동: (%d,%d)\n", numpad_group + 1, center.x, center.y);
                                move_camera_cb(center);
                                if (toast_cb) {
                                    char msg[32];
                                    snprintf(msg, sizeof(msg), "Numpad Group %d (All)", numpad_group + 1);
                                    toast_cb(msg, 30);
                                }
                            }
                        }
                        skip_selection_sync_frames = 2;
                    }
                } else {
                    // 일반 모드: 단일 플레이어만
                    int group_size = action_st.control_groups[pid][group].size();
                    bool has_selection = !action_st.selection[pid].empty();
                    
                    if (group_size == 0 && has_selection) {
                        // 그룹이 비어있고 선택된 유닛이 있으면 -> 부대 지정
                        log(">>> 넘패드%d: 넘패드 그룹 %d 지정 (pid=%d, 현재 선택: %d개)\n", 
                            numpad_group + 1, numpad_group + 1, pid, (int)action_st.selection[pid].size());
                        
                        bool result = actions.action_control_group(pid, group, 0);
                        
                        int new_group_size = action_st.control_groups[pid][group].size();
                        log("[NUMPAD_RESULT] 지정 결과: %d, 그룹에 저장된 유닛 수: %d\n", 
                            (int)result, new_group_size);
                        
                        if (toast_cb) {
                            char msg[32];
                            snprintf(msg, sizeof(msg), "Numpad Group %d set", numpad_group + 1);
                            toast_cb(msg, 30);
                        }
                    } else if (group_size > 0) {
                        // 그룹에 유닛이 있으면 -> 부대 호출
                        auto now = std::chrono::high_resolution_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - last_group_select_time[group]).count();
                        
                        bool is_double_click = (last_group_index_for_double_click == group && elapsed < DOUBLE_CLICK_MS);
                        last_group_select_time[group] = now;
                        last_group_index_for_double_click = group;
                        
                        log(">>> 넘패드%d: 넘패드 그룹 %d 호출 (pid=%d, 그룹 유닛 수: %d, 더블클릭=%d)\n", 
                            numpad_group + 1, numpad_group + 1, pid, group_size, (int)is_double_click);
                        
                        bool result = actions.action_control_group(pid, group, 1);
                        
                        int after_count = action_st.selection[pid].size();
                        log("[NUMPAD_RESULT] 호출 결과: %d, 선택된 유닛 수: %d\n", 
                            (int)result, after_count);
                        
                        // 더블클릭: 화면 이동
                        if (is_double_click && move_camera_cb && after_count > 0) {
                            xy sum_pos{0, 0};
                            int count = 0;
                            for (unit_t* u : action_st.selection[pid]) {
                                if (u && u->sprite) {
                                    sum_pos.x += u->sprite->position.x;
                                    sum_pos.y += u->sprite->position.y;
                                    count++;
                                }
                            }
                            if (count > 0) {
                                xy center{sum_pos.x / count, sum_pos.y / count};
                                log(">>> 넘패드 부대 %d 화면 이동: (%d,%d)\n", numpad_group + 1, center.x, center.y);
                                move_camera_cb(center);
                                if (toast_cb) {
                                    char msg[32];
                                    snprintf(msg, sizeof(msg), "Numpad Group %d", numpad_group + 1);
                                    toast_cb(msg, 30);
                                }
                            }
                        }
                        
                        skip_selection_sync_frames = 2;
                    } else {
                        // 그룹도 비어있고 선택된 유닛도 없으면 아무것도 안 함
                        log("[NUMPAD] 넘패드%d: 그룹 비어있고 선택된 유닛 없음\n", numpad_group + 1);
                    }
                }
            }
            break;
        
        case 99: // 넘패드 . (Del) - 넘패드 부대지정 전체 초기화
        {
            int pid = get_effective_owner();
            int cleared_count = 0;
            
            // 넘패드 그룹 10~18 초기화
            for (int i = 10; i < 19; ++i) {
                if (!action_st.control_groups[pid][i].empty()) {
                    action_st.control_groups[pid][i].clear();
                    cleared_count++;
                }
            }
            
            log("[NUMPAD_DEL] 넘패드 부대 초기화: %d개 그룹 삭제됨\n", cleared_count);
            
            if (toast_cb) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Numpad Groups Cleared (%d groups)", cleared_count);
                toast_cb(msg, 60);
            }
            break;
        }
            
        // 건물 건설 단축키 (B)
        case 5:  // B -> Shield Battery (B) / Robotics Support Bay (V)
            // B 모드: Shield Battery (패널 7)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Shield_Battery));
                g_ui_active_button = 7;
                if (toast_cb) toast_cb("Shield Battery", 30);
                break;
            }
            // V 모드: Robotics Support Bay (패널 3)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Robotics_Support_Bay));
                g_ui_active_button = 3;
                if (toast_cb) toast_cb("Robotics Support Bay", 30);
                break;
            }

            // 2) 그 외에는 기존처럼: 카테고리 0(유닛만)에서만 Build 모드 진입 키로 사용
            if (g_ui_selected_category != 0) {
                log(">>> B키: 명령 사용 불가 (카테고리=%d, B버튼 없음)\n", g_ui_selected_category);
                break;
            }
            
            log("[KEY_B] cmd_mode=%d, can_build=%d\n", 
                (int)command_mode, (int)can_use_command(CommandMode::Build));
            
            if (command_mode != CommandMode::Build) {
                // 일꾼 1기만 선택되어 있을 때만 빌드 모드 진입
                if (can_use_command(CommandMode::Build)) {
                    auto [pid, worker] = find_selected_worker_any_owner();
                    log("[KEY_B] worker=%p, pid=%d\n", (void*)worker, pid);
                    
                    if (worker) {
                        // 선택된 유닛 개수 확인
                        int selected_count = 0;
                        if (allow_enemy_control) {
                            for (int p = 0; p < 8; ++p) selected_count += action_st.selection[p].size();
                        } else {
                            selected_count = action_st.selection[player_id].size();
                        }
                        
                        log("[KEY_B] selected_count=%d\n", selected_count);
                        
                        if (selected_count == 1) {
                            log(">>> B키: 건물 모드 진입 (cmd_mode -> Build)\n");
                            command_mode = CommandMode::Build;
                            g_ui_active_button = -1;  // 빌드 모드 진입 시 하이라이트 초기화
                            if (toast_cb) toast_cb("Build mode - Select building", 90);
                        } else {
                            log(">>> B키: 일꾼 2기 이상 선택되어 무시\n");
                        }
                    } else {
                        log("[KEY_B] 일꾼이 선택되지 않음\n");
                    }
                } else {
                    log("[KEY_B] can_use_command(Build) = false\n");
                }
            } else {
                // 두 번째 B는 아무 것도 자동 선택하지 않음 (원작 UX)
                log(">>> B키: 이미 건물 모드 - 자동 선택 없음\n");
            }
            break;
        case 25:  // V - Advanced Build
            // ★ 카테고리 체크: 0번(유닛만)일 때만 허용
            if (g_ui_selected_category != 0) {
                log(">>> V키: 명령 사용 불가 (카테고리=%d, V버튼 없음)\n", g_ui_selected_category);
                break;
            }
            
            if (command_mode == CommandMode::AdvancedBuild) {
                // 이미 AdvancedBuild 모드면 무시
                log(">>> V키: 이미 고급 건물 모드\n");
            } else if (can_use_command(CommandMode::AdvancedBuild)) {
                // 일꾼 1기만 선택되어 있을 때만 AdvancedBuild 모드 진입
                auto [pid, worker] = find_selected_worker_any_owner();
                if (worker) {
                    // 선택된 유닛 개수 확인
                    int selected_count = 0;
                    if (allow_enemy_control) {
                        for (int p = 0; p < 8; ++p) selected_count += action_st.selection[p].size();
                    } else {
                        selected_count = action_st.selection[player_id].size();
                    }
                    
                    if (selected_count == 1) {
                        log(">>> V키: 고급 건물 모드 진입\n");
                        command_mode = CommandMode::AdvancedBuild;
                        g_ui_active_button = -1;  // 고급 빌드 모드 진입 시 하이라이트 초기화
                        if (toast_cb) toast_cb("Advanced build - Select building", 90);
                    }
                }
            }
            break;
            
        case 10: // G - Gather 모드 / (Build) Gateway
            // B 모드: Gateway 선택 (패널 3)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Gateway));
                g_ui_active_button = 3;
                if (toast_cb) toast_cb("Gateway", 30);
                break;
            }
            // V 모드에서는 G키 무시 (UI에 G 버튼 없음)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild) {
                log(">>> G키: V 건설 모드에는 G 버튼 없음 - 무시\n");
                break;
            }
            
            {
                // ★ 카테고리 체크: 0번(유닛만) 또는 9번(건물/자원+일꾼)일 때만 Gather 허용
                if (g_ui_selected_category != 0 && g_ui_selected_category != 9) {
                    log(">>> G키: 명령 사용 불가 (카테고리=%d, G버튼 없음)\n", g_ui_selected_category);
                    break;
                }
                
                if (can_use_command(CommandMode::Gather)) {
                    // 일반 모드에서는 Gather 모드 활성화 (허용된 카테고리 내에서만)
                    command_mode = CommandMode::Gather;
                    g_ui_active_button = 5;  // G 버튼 하이라이트
                    log(">>> G키: Gather 모드 활성화 (cat=%d)\n", g_ui_selected_category);
                    if (selection_suppress_cb) selection_suppress_cb(true);
                    if (toast_cb) toast_cb("Gather mode - Left click resource", 90);
                } else {
                    log(">>> G키: 명령 사용 불가 (건물 선택됨 또는 일꾼 없음)\n");
                }
            }
            break;
        case 9: // F -> Forge (B) / Fleet Beacon (V)
            // B 모드: Forge (패널 4)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Forge));
                g_ui_active_button = 4;
                if (toast_cb) toast_cb("Forge", 30);
                break;
            }
            // V 모드: Fleet Beacon (패널 4)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Fleet_Beacon));
                g_ui_active_button = 4;
                if (toast_cb) toast_cb("Fleet Beacon", 30);
                break;
            }
            break;
        case 6: // C -> Photon Cannon (B) / Citadel of Adun (V) / Ctrl+C: 선택된 유닛으로 화면 이동
            if (ctrl_pressed && command_mode == CommandMode::Normal) {
                // Ctrl+C: 현재 선택된 유닛들의 중심으로 화면 이동 (스타크래프트 원작 기능)
                int pid = get_effective_owner();
                auto& selection = action_st.selection.at(pid);
                if (!selection.empty() && move_camera_cb) {
                    // 선택된 유닛들의 중심 계산
                    xy sum_pos{0, 0};
                    int count = 0;
                    for (unit_t* u : selection) {
                        if (u && u->sprite) {
                            sum_pos.x += u->sprite->position.x;
                            sum_pos.y += u->sprite->position.y;
                            count++;
                        }
                    }
                    if (count > 0) {
                        xy center{sum_pos.x / count, sum_pos.y / count};
                        log(">>> Ctrl+C: 선택된 유닛으로 화면 이동 (%d,%d)\n", center.x, center.y);
                        move_camera_cb(center);
                        
                        // 이 위치를 Space용 알림 위치로도 저장
                        last_alert_pos = center;
                        has_alert_pos = true;
                    }
                }
                break;
            }
            // B 모드: Photon Cannon (패널 5)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Photon_Cannon));
                g_ui_active_button = 5;
                if (toast_cb) toast_cb("Photon Cannon", 30);
                break;
            }
            // V 모드: Citadel of Adun (패널 2)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Citadel_of_Adun));
                g_ui_active_button = 2;
                if (toast_cb) toast_cb("Citadel of Adun", 30);
                break;
            }
            // 저그 빌드 모드: Creep Colony
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Creep_Colony));
                if (toast_cb) toast_cb("Creep Colony", 30);
            }
            break;
        case 23: // T -> Templar Archives (V 모드)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                // V 모드: Templar Archives (패널 5)
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Templar_Archives));
                g_ui_active_button = 5;
                if (toast_cb) toast_cb("Templar Archives", 30);
            }
            break;
        case 27: // X -> Zerg Extractor
            // 카테고리 9 (프로브 건설 모드)에서는 X키 무시
            if (g_ui_selected_category == 9 && (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                log(">>> X키: 건설 모드에는 X 버튼 없음 - 무시\n");
                break;
            }
            
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Extractor));
                break;
            }
            break;
        case 17: // N -> Nexus (B 모드만)
            // B 모드: Nexus (패널 0)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Nexus));
                g_ui_active_button = 0;
                if (toast_cb) toast_cb("Nexus", 30);
                break;
            }
            // V 모드에서는 N키 무시
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild) {
                log(">>> N키: V 건설 모드에는 N 버튼 없음 - 무시\n");
                break;
            }
            break;
        
        case 21: // R -> Rally Point (모든 생산 건물) / (V) Robotics Facility
            // V 모드: Robotics Facility (패널 0)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Robotics_Facility));
                g_ui_active_button = 0;
                if (toast_cb) toast_cb("Robotics Facility", 30);
                break;
            }
            // B 모드에서는 R키 무시
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build) {
                log(">>> R키: B 건설 모드에는 R 버튼 없음 - 무시\n");
                break;
            }

            if (can_use_command(CommandMode::Rally)) {
                command_mode = CommandMode::Rally;
                g_ui_active_button = 5;  // R키 = 5번 버튼 하이라이트
                log(">>> R키: 렐리 포인트 모드 활성화 (cmd_mode=%d)\n", (int)command_mode);
                if (selection_suppress_cb) {
                    selection_suppress_cb(true);
                    log(">>> R키: selection_suppress_cb(true) 호출됨\n");
                }
                if (toast_cb) toast_cb("Rally - Click anywhere", 90);
            } else {
                log(">>> R키: 명령 사용 불가 (생산 건물 아님)\n");
            }
            break;
        
        case 18: // O -> Auto Rally Point (넥서스/해처리/커맨드만) / (V) Observatory
        {
            // V 모드: Observatory (패널 6)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Observatory));
                g_ui_active_button = 6;
                if (toast_cb) toast_cb("Observatory", 30);
                break;
            }
            // B 모드에서는 O키 무시
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build) {
                log(">>> O키: B 건설 모드에는 O 버튼 없음 - 무시\n");
                break;
            }
            int pid = get_effective_owner();
            unit_t* selected = nullptr;
            if (allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    if (!action_st.selection[p].empty()) {
                        selected = action_st.selection[p][0];
                        pid = p;
                        break;
                    }
                }
            } else {
                if (!action_st.selection[pid].empty()) {
                    selected = action_st.selection[pid][0];
                }
            }
            
            // 넥서스/해처리/커맨드 센터인지 확인
            bool is_resource_building = false;
            if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
                is_resource_building = (
                    actions.unit_is(selected, UnitTypes::Protoss_Nexus) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Hatchery) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Lair) ||
                    actions.unit_is(selected, UnitTypes::Zerg_Hive) ||
                    actions.unit_is(selected, UnitTypes::Terran_Command_Center)
                );
            }
            
            if (is_resource_building) {
                command_mode = CommandMode::AutoRally;
                g_ui_active_button = 6;  // O키 = 6번 버튼 하이라이트
                log(">>> O키: 자동 렐리 포인트 모드\n");
                if (selection_suppress_cb) selection_suppress_cb(true);
                if (toast_cb) toast_cb("Auto Rally - Right-click resource", 90);
            } else {
                log(">>> O키: 명령 사용 불가 (넥서스/해처리/커맨드 아님)\n");
            }
            break;
        }
        case 28: // Y -> Cybernetics Core (B 모드만)
            // B 모드: Cybernetics Core (패널 6)
            if (g_ui_selected_category == 9 && command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Cybernetics_Core));
                g_ui_active_button = 6;
                if (toast_cb) toast_cb("Cybernetics Core", 30);
                break;
            }
            // V 모드에서는 Y키 무시
            if (g_ui_selected_category == 9 && command_mode == CommandMode::AdvancedBuild) {
                log(">>> Y키: V 건설 모드에는 Y 버튼 없음 - 무시\n");
                break;
            }
            break;

        case 7: // D -> Citadel of Adun (Protoss) / Hydralisk Den (Zerg) / Dragoon (Gateway)
            // 카테고리 9 (프로브 건설 모드)에서는 D키 무시 (UI에 D 버튼 없음)
            if (g_ui_selected_category == 9 && (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                log(">>> D키: 건설 모드에는 D 버튼 없음 - 무시\n");
                break;
            }
            
            if (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) {
                if (is_selected_worker_protoss()) {
                    set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Citadel_of_Adun));
                    if (toast_cb) toast_cb("Citadel of Adun", 30);
                } else if (is_selected_worker_zerg()) {
                    set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Hydralisk_Den));
                    if (toast_cb) toast_cb("Hydralisk Den", 30);
                }
                break;
            }
            // 게이트웨이: 드라군 생산 (1번 버튼)
            {
                int pid = get_effective_owner();
                unit_t* selected = nullptr;
                if (allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) {
                        if (!action_st.selection[p].empty()) {
                            selected = action_st.selection[p][0];
                            pid = p;
                            break;
                        }
                    }
                } else {
                    if (!action_st.selection[pid].empty()) {
                        selected = action_st.selection[pid][0];
                    }
                }
                
                if (selected && actions.unit_is(selected, UnitTypes::Protoss_Gateway)) {
                    log(">>> D키: 드라군 생산\n");
                    g_ui_active_button = 1;  // 1번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Dragoon));
                    if (toast_cb) toast_cb("Training Dragoon", 60);
                }
            }
            break;
        case 8: // E -> Evolution Chamber (Zerg)
            // 카테고리 9 (프로브 건설 모드)에서는 E키 무시
            if (g_ui_selected_category == 9 && (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                log(">>> E키: 건설 모드에는 E 버튼 없음 - 무시\n");
                break;
            }
            
            if ((command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild) && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Evolution_Chamber));
                if (toast_cb) toast_cb("Evolution Chamber", 30);
                break;
            }
            break;
        case 29: // Z -> 질럿 생산 (Gateway)
        {
            // 카테고리 9 (프로브 건설 모드)에서는 Z키 무시
            if (g_ui_selected_category == 9 && (command_mode == CommandMode::Build || command_mode == CommandMode::AdvancedBuild)) {
                log(">>> Z키: 건설 모드에는 Z 버튼 없음 - 무시\n");
                break;
            }
            
            log("[Z_KEY] Z키 입력 감지\n");
            int pid = get_effective_owner();
            unit_t* selected = nullptr;
            if (allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    if (!action_st.selection[p].empty()) {
                        selected = action_st.selection[p][0];
                        pid = p;
                        break;
                    }
                }
            } else {
                if (!action_st.selection[pid].empty()) {
                    selected = action_st.selection[pid][0];
                }
            }
            
            if (selected) {
                log("[Z_KEY] 선택된 유닛: type_id=%d, is_building=%d\n", 
                    (int)selected->unit_type->id, (int)(selected->unit_type->flags & unit_type_t::flag_building));
                
                if (actions.unit_is(selected, UnitTypes::Protoss_Gateway)) {
                    log(">>> Z키: 질럿 생산 시작\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Zealot));
                    if (toast_cb) toast_cb("Training Zealot", 60);
                } else {
                    log("[Z_KEY] 게이트웨이가 아님 (type_id=%d)\n", (int)selected->unit_type->id);
                }
            } else {
                log("[Z_KEY] 선택된 유닛 없음\n");
            }
            break;
        }
        
        case 98: // 0 (넘패드) -> 일꾼 생산 (Nexus/Hatchery/Command Center)
        {
            int pid = get_effective_owner();
            unit_t* selected = nullptr;
            if (allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    if (!action_st.selection[p].empty()) {
                        selected = action_st.selection[p][0];
                        pid = p;
                        break;
                    }
                }
            } else {
                if (!action_st.selection[pid].empty()) {
                    selected = action_st.selection[pid][0];
                }
            }
            
            if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
                // 넥서스: 프로브 생산
                if (actions.unit_is(selected, UnitTypes::Protoss_Nexus)) {
                    log(">>> 넘패드0: 프로브 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Protoss_Probe));
                    if (toast_cb) toast_cb("Training Probe", 60);
                }
                // 해처리: 드론 생산
                else if (actions.unit_is(selected, UnitTypes::Zerg_Hatchery) ||
                         actions.unit_is(selected, UnitTypes::Zerg_Lair) ||
                         actions.unit_is(selected, UnitTypes::Zerg_Hive)) {
                    log(">>> 넘패드0: 드론 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Zerg_Drone));
                    if (toast_cb) toast_cb("Training Drone", 60);
                }
                // 커맨드 센터: SCV 생산
                else if (actions.unit_is(selected, UnitTypes::Terran_Command_Center)) {
                    log(">>> 넘패드0: SCV 생산\n");
                    g_ui_active_button = 0;  // 0번 버튼 하이라이트
                    actions.action_train(pid, actions.get_unit_type(UnitTypes::Terran_SCV));
                    if (toast_cb) toast_cb("Training SCV", 60);
                }
            }
            break;
        }
        
        case 44: // Space - 마지막 알림 위치로 화면 이동
            if (has_alert_pos && move_camera_cb) {
                log(">>> Space: 알림 위치로 이동 (%d,%d)\n", last_alert_pos.x, last_alert_pos.y);
                move_camera_cb(last_alert_pos);
                if (toast_cb) toast_cb("Jump to alert", 30);
            } else {
                log(">>> Space: 저장된 알림 위치 없음\n");
            }
            break;
        
        case 62: // F5 - 적군 컨트롤 토글
            allow_enemy_control = !allow_enemy_control;
            log(">>> F5: 적군 컨트롤 %s\n", allow_enemy_control ? "ON" : "OFF");
            ui::log("시스템: 적군 컨트롤 모드 %s\n", allow_enemy_control ? "ON" : "OFF");
            if (toast_cb) toast_cb(allow_enemy_control ? "ENEMY CONTROL: ON" : "ENEMY CONTROL: OFF", 120);
            break;
        case 63: // F6 - 저그 빌드-애니웨어 테스트 토글
            zerg_build_anywhere = !zerg_build_anywhere;
            log(">>> F6: Zerg build-anywhere %s\n", zerg_build_anywhere ? "ON" : "OFF");
            if (toast_cb) toast_cb(zerg_build_anywhere ? "ZERG BUILD ANYWHERE: ON" : "ZERG BUILD ANYWHERE: OFF", 120);
            break;
        
        case 75: // Page Up - 생산 속도 증가 (prev_keys에서 처리)
            log("[PAGE_UP] Key pressed\n");
            break;
        
        case 78: // Page Down - 생산 속도 감소 (prev_keys에서 처리)
            log("[PAGE_DOWN] Key pressed\n");
            break;
        
        default:
            log("[KEY_UNHANDLED] scancode=%d - switch 문에서 처리되지 않음\n", scancode);
            break;
    }
}

void player_input_handler::on_key_up(int scancode) {
    if (scancode == 225 || scancode == 229) shift_pressed = false;
    if (scancode == 224 || scancode == 228) ctrl_pressed = false;
    if (scancode == 226 || scancode == 230) alt_pressed = false;
    
    // F9 키: UI 좌표 표시 토글
    if (scancode == 66) {  // F9
        g_ui_show_coordinates = !g_ui_show_coordinates;
        log("[F9] UI 좌표 표시: %s\n", g_ui_show_coordinates ? "ON" : "OFF");
        if (toast_cb) toast_cb(g_ui_show_coordinates ? "UI Coordinates: ON" : "UI Coordinates: OFF", 60);
    }
}

unit_t* player_input_handler::find_unit_at(xy pos) {
    log("\n========== [FIND_UNIT_AT] ==========\n");
    log("클릭 위치: (%d,%d)\n", pos.x, pos.y);
    
    unit_t* best = nullptr;
    int best_dist = 128 * 128;
    int checked = 0;
    int candidates = 0;
    
    for (unit_t* u : ptr(st.visible_units)) {
        if (u->sprite->flags & sprite_t::flag_hidden) continue;
        checked++;
        
        xy unit_pos = u->sprite->position;
        int dx = pos.x - unit_pos.x;
        int dy = pos.y - unit_pos.y;
        int dist = dx * dx + dy * dy;
        
        // 유닛 타입 판별
        bool is_resource = (u->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                           u->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                           u->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                           u->unit_type->id == UnitTypes::Resource_Vespene_Geyser);
        bool is_building = u->unit_type->flags & unit_type_t::flag_building;
        
        std::string type_str = is_building ? "[건물]" : (is_resource ? "[자원]" : "[유닛]");
        
        // 유닛 크기 계산 (dimensions는 중심에서의 오프셋)
        // from은 음수, to는 양수이므로 절댓값을 더해야 함
        int unit_width = std::abs(u->unit_type->dimensions.from.x) + std::abs(u->unit_type->dimensions.to.x);
        int unit_height = std::abs(u->unit_type->dimensions.from.y) + std::abs(u->unit_type->dimensions.to.y);
        
        // 판정 범위: 스타크래프트 원작 방식 (dimensions 사용)
        int click_radius;
        if (is_building) {
            // 건물: dimensions 최대값 + 여유 (클릭하기 쉽게)
            int radius = std::max(unit_width, unit_height) / 2;
            click_radius = radius + 16;  // 몸통 크기 + 16픽셀 여유
        } else if (is_resource) {
            // 자원: dimensions 최대값 사용 (몸통 크기의 절반)
            click_radius = std::max(unit_width, unit_height) / 2;
        } else {
            // 유닛: dimensions 최대값 + 약간 여유 (움직이는 유닛 클릭하기 쉽게)
            int radius = std::max(unit_width, unit_height) / 2;
            if (radius < 8) radius = 8;
            click_radius = radius + 12;
        }
        int max_dist = click_radius * click_radius;
        
        // 범위 내에 있는 후보만 로그
        if (dist < max_dist) {
            candidates++;
            log("[후보 %d] %s %s (owner=%d)\n", candidates, type_str.c_str(), get_unit_name((int)u->unit_type->id), u->owner);
            log("  중심: (%d,%d) | dimensions: (%dx%d)\n", 
                unit_pos.x, unit_pos.y, 
                unit_width, unit_height);
            if (is_building) {
                log("  건물 placement: (%dx%d 타일)\n", 
                    u->unit_type->placement_size.x, u->unit_type->placement_size.y);
            }
            log("  실제거리²: %d | 최대거리²: %d | 클릭반경: %d픽셀\n", 
                dist, max_dist, click_radius);
            log("  실제거리: %.1f 픽셀 | 판정범위: %d 픽셀\n", 
                std::sqrt((double)dist), click_radius);
            
            if (dist < best_dist) {
                best = u;
                best_dist = dist;
                log("  ✓ 현재 최선 후보로 선택됨!\n");
            } else {
                log("  ✗ 더 가까운 후보가 있음\n");
            }
        }
    }
    
    log("\n[결과] 총 체크: %d개 | 범위 내 후보: %d개 | 최종 선택: %s\n", 
        checked, candidates, best ? get_unit_name((int)best->unit_type->id) : "없음");
    if (best) {
        log("  최종 선택 유닛 중심: (%d,%d) | 거리: %.1f 픽셀\n", 
            best->sprite->position.x, best->sprite->position.y, std::sqrt((double)best_dist));
    }
    log("====================================\n\n");
    
    return best;
}

void player_input_handler::select_units_in_rect(rect area) {
    std::vector<unit_t*> units;
    
    int total_checked = 0;
    int hidden_count = 0;
    int wrong_owner = 0;
    int in_area = 0;
    
    for (unit_t* u : ptr(st.visible_units)) {
        total_checked++;
        if (u->sprite->flags & sprite_t::flag_hidden) {
            hidden_count++;
            continue;
        }
        
        // 자원(중립)은 owner 체크 건너뛰기
        bool is_resource = actions.unit_is_mineral_field(u) || 
                          actions.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                          actions.unit_is_refinery(u);
        
        if (!is_resource && !allow_enemy_control && u->owner != player_id) {
            wrong_owner++;
            continue;
        }
        
        xy unit_pos = u->sprite->position;
        if (unit_pos.x >= area.from.x && unit_pos.x <= area.to.x &&
            unit_pos.y >= area.from.y && unit_pos.y <= area.to.y) {
            units.push_back(u);
            in_area++;
            log("[DRAG_SELECT] 유닛 추가: owner=%d, type=%d, pos=(%d,%d), is_resource=%d\n", 
                u->owner, (int)u->unit_type->id, unit_pos.x, unit_pos.y, (int)is_resource);
        }
    }
    
    log("[DRAG_SELECT] 검사: total=%d, hidden=%d, wrong_owner=%d, in_area=%d\n", 
        total_checked, hidden_count, wrong_owner, in_area);
    
    // 유닛/건물/자원 구분
    int unit_count = 0;
    int building_count = 0;
    int resource_count = 0;
    for (unit_t* u : units) {
        if (u) {
            bool is_resource = actions.unit_is_mineral_field(u) || 
                              actions.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                              actions.unit_is_refinery(u);
            if (is_resource) {
                resource_count++;
            } else if (u->unit_type->flags & unit_type_t::flag_building) {
                building_count++;
            } else {
                unit_count++;
            }
        }
    }
    
    // 항상 자원 카운트 표시
    if (resource_count > 0) {
        log(">>> 드래그로 %d개 감지 (유닛:%d, 건물:%d, 자원:%d) - 자원은 선택 불가\n", 
            (int)units.size(), unit_count, building_count, resource_count);
    } else {
        log(">>> 드래그로 %d개 선택 (유닛:%d, 건물:%d, 자원:%d)\n", 
            (int)units.size(), unit_count, building_count, resource_count);
    }
    
    // 자원을 제외한 유닛만 선택
    std::vector<unit_t*> selectable_units;
    for (unit_t* u : units) {
        if (u) {
            bool is_resource = actions.unit_is_mineral_field(u) || 
                              actions.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                              actions.unit_is_refinery(u);
            if (!is_resource) {
                selectable_units.push_back(u);
            }
        }
    }
    
    if (!selectable_units.empty()) {
        if (allow_enemy_control) {
            clear_all_selections();
            std::array<std::vector<unit_t*>, 8> by_owner;
            for (auto* u : selectable_units) if (u) by_owner[u->owner].push_back(u);
            for (int pid = 0; pid < 8; ++pid) if (!by_owner[pid].empty()) {
                if (by_owner[pid].size() > 24) {
                    log("[SELECT_TRIM] pid=%d trimmed %d->24 to avoid overflow\n", pid, (int)by_owner[pid].size());
                    by_owner[pid].resize(24);
                }
                if (shift_pressed) actions.action_shift_select(pid, by_owner[pid]);
                else actions.action_select(pid, by_owner[pid]);
            }
        } else {
            // ★ 24개 제한 (이 프로젝트 최대 선택 수)
            if (selectable_units.size() > 24) {
                log("[SELECT_TRIM] pid=%d trimmed %d->24 to avoid overflow\n", player_id, (int)selectable_units.size());
                selectable_units.resize(24);
            }
            if (shift_pressed) actions.action_shift_select(player_id, selectable_units);
            else actions.action_select(player_id, selectable_units);
        }
    }
}

// 커스텀 UI 클래스 (유닛 정보 표시 기능 추가)
struct custom_ui_functions : ui_functions {
    using ui_functions::ui_functions;
    
    // 선택된 유닛 정보를 가져오기 위한 참조
    player_input_handler* input_handler = nullptr;
    
    // draw_callback은 ui_custom.h의 draw_ui에서 처리
};

// 메인 게임 구조체
struct player_game {
    custom_ui_functions ui;
    player_input_handler input;
    
    std::chrono::high_resolution_clock clock;
    std::chrono::high_resolution_clock::time_point last_tick;
    
    // 스타크래프트 고정 게임 속도 (8단계 + 최고속도)
    static constexpr int SPEED_LEVELS[] = {167, 111, 83, 67, 56, 48, 42, 8}; // tick_ms 값
    static constexpr const char* SPEED_NAMES[] = {"Slowest", "Slower", "Slow", "Normal", "Fast", "Faster", "Fastest", "TURBO"};
    int current_speed_index = 7; // TURBO (8ms) - 기본값 터보
    int tick_ms = SPEED_LEVELS[7];
    
    // 생산 속도 배율 (키패드 1~5로 조절)
    float production_speed_multiplier = 1.0f;
    static constexpr float PRODUCTION_SPEED_LEVELS[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    static constexpr const char* PRODUCTION_SPEED_NAMES[] = {
        "Production Speed: 0.5x (Slow)",
        "Production Speed: 1.0x (Normal)",
        "Production Speed: 2.0x (Fast)",
        "Production Speed: 5.0x (Very Fast)",
        "Production Speed: 10.0x (Instant)"
    };
    
    // 속도 변경 딜레이 (0.5초)
    std::chrono::high_resolution_clock::time_point last_speed_change;
    
    // 비침투 입력 상태 추적
    std::array<bool, 512> prev_keys{};
    std::array<bool, 6> prev_mouse{};
    
    // 오토 렐리 일꾼 생산 완료 감지용: 이전 프레임에서 생산 중이던 유닛 추적
    // key: 건물 포인터, value: 생산 중이던 유닛 포인터
    std::map<unit_t*, unit_t*> prev_building_units;
    
    // 명령 버튼 클릭 감지 (3x3 그리드, 오른쪽 끝-2칸)
    int get_command_button_at(int mx, int my) {
        // 실제 화면 크기 가져오기
        const int screen_width = ui.rgba_surface ? ui.rgba_surface->w : 640;
        const int screen_height = ui.rgba_surface ? ui.rgba_surface->h : 480;
        // ui_custom.h와 동일한 계산
        const int panel_height = 220;
        const int panel_y = screen_height - panel_height;
        const int btn_size = 60;
        const int btn_gap = 6;
        const int cmd_bg_margin = 10;
        const int right_margin = 2;
        const int cmd_bg_w = btn_size * 3 + btn_gap * 2 + cmd_bg_margin;
        const int cmd_x = screen_width - cmd_bg_w - right_margin + 5;
        const int cmd_y = panel_y + 10;
        
        log("[BTN_CHECK] 클릭 위치=(%d,%d), 화면=(%dx%d), panel_y=%d, cmd_영역=(%d,%d), 크기=%dx%d\n", 
            mx, my, screen_width, screen_height, panel_y, cmd_x, cmd_y, cmd_bg_w, btn_size * 3 + btn_gap * 2);
        
        // 3x3 그리드 체크
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                int bx = cmd_x + col * (btn_size + btn_gap);
                int by = cmd_y + row * (btn_size + btn_gap);
                
                if (mx >= bx && mx < bx + btn_size && my >= by && my < by + btn_size) {
                    log("[BTN_HIT] 버튼 클릭: row=%d, col=%d, idx=%d, pos=(%d,%d), 버튼영역=(%d,%d)-(%d,%d)\n", 
                        row, col, row * 3 + col, mx, my, bx, by, bx + btn_size, by + btn_size);
                    return row * 3 + col;  // 0~8
                }
            }
        }
        log("[BTN_MISS] 버튼 영역 밖 클릭\n");
        return -1;  // 버튼 영역 밖
    }
    
    // 대기열 칸 클릭 감지 (5칸: 1번 위, 2345 아래)
    int get_queue_slot_at(int mx, int my) {
        // 실제 화면 크기 가져오기
        const int screen_width = ui.rgba_surface ? ui.rgba_surface->w : 640;
        const int screen_height = ui.rgba_surface ? ui.rgba_surface->h : 480;

        // ui_custom.h의 draw_ui 레이아웃과 동일한 계산 사용
        const int panel_height = 220;
        const int panel_y = screen_height - panel_height;

        const int btn_size = 60;
        const int btn_gap = 6;
        const int cmd_bg_margin = 10;
        const int right_margin = 2;
        const int cmd_bg_w = btn_size * 3 + btn_gap * 2 + cmd_bg_margin;
        const int cmd_x = (int)screen_width - cmd_bg_w - right_margin + 5;

        const int portrait_w = 140;
        const int portrait_x = cmd_x - portrait_w - 10;  // ui_custom.h와 동일

        const int info_x = 220;
        const int info_y = panel_y + 10;
        const int info_w = portrait_x - info_x - 10;
        const int info_h = 200;  // ui_custom.h의 info_h와 동일

        // 대기열 5칸 반대 ㄴ자 배치 (1번 위, 2345 아래)
        const int queue_icon_size = 50;
        const int queue_gap = 3;
        const int queue_bottom_y = info_y + info_h - queue_icon_size - 6;  // 하단 줄
        const int queue_top_y = queue_bottom_y - queue_icon_size - queue_gap;  // 상단 1번 칸

        // 초상화 패널 2칸 남기고 오른쪽 배치 + 1칸 더 오른쪽 (ui_custom.h와 동일)
        const int portrait_margin = 2 * (queue_icon_size + queue_gap);
        const int queue_start_x = portrait_x - portrait_margin - (4 * queue_icon_size + 3 * queue_gap) + (queue_icon_size + queue_gap);

        // 5칸 체크
        for (int i = 0; i < 5; ++i) {
            int qx, qy;

            if (i == 0) {
                // 1번 칸: 상단 왼쪽 (맨 위부터 생산)
                qx = queue_start_x;
                qy = queue_top_y;
            } else {
                // 2345번 칸: 하단 가로 배치
                qx = queue_start_x + (i - 1) * (queue_icon_size + queue_gap);
                qy = queue_bottom_y;
            }

            if (mx >= qx && mx < qx + queue_icon_size && my >= qy && my < qy + queue_icon_size) {
                log("[QUEUE_HIT] 대기열 칸 클릭: slot=%d, pos=(%d,%d), 영역=(%d,%d)-(%d,%d)\n",
                    i, mx, my, qx, qy, qx + queue_icon_size, qy + queue_icon_size);
                return i;  // 0~4
            }
        }

        return -1;  // 대기열 영역 밖
    }
    
    // Wireframe Grid 클릭 감지 (8x3 = 24칸)
    int get_wireframe_slot_at(int mx, int my) {
        // 실제 화면 크기 가져오기
        const int screen_width = ui.rgba_surface ? ui.rgba_surface->w : 640;
        const int screen_height = ui.rgba_surface ? ui.rgba_surface->h : 480;
        
        // ui_custom.h와 동일한 계산
        const int panel_height = 220;
        const int panel_y = screen_height - panel_height;
        
        // 명령 패널 및 초상화 위치 계산 (ui_custom.h와 동일)
        const int btn_size = 60;
        const int btn_gap = 6;
        const int cmd_bg_margin = 10;
        const int right_margin = 2;
        const int cmd_bg_w = btn_size * 3 + btn_gap * 2 + cmd_bg_margin;
        const int cmd_x = screen_width - cmd_bg_w - right_margin + 5;
        
        const int portrait_w = 140;
        const int portrait_x = cmd_x - portrait_w - 10;
        
        // 정보 패널 (미니맵 오른쪽부터 초상화 왼쪽까지)
        const int info_x = 220;  // 미니맵 192 + 여백
        const int info_y = panel_y + 10;
        const int info_w = portrait_x - info_x - 10;
        const int info_h = 200;
        
        // Wireframe 그리드 설정 (ui_custom.h와 동일 - 동적 계산)
        const int wireframe_cols = 8;
        const int wireframe_rows = 3;
        const int wireframe_gap = 1;  // ui_custom.h와 동일
        
        // 정보 패널 크기에 맞춰 wireframe 크기 자동 계산 (ui_custom.h와 동일)
        const int available_w = info_w - 4;
        const int available_h = info_h - 4;
        const int wireframe_size_w = (available_w - (wireframe_cols - 1) * wireframe_gap) / wireframe_cols;
        const int wireframe_size_h = (available_h - (wireframe_rows - 1) * wireframe_gap) / wireframe_rows;
        const int wireframe_size = std::min(wireframe_size_w, wireframe_size_h);
        const int wireframe_w = wireframe_cols * wireframe_size + (wireframe_cols - 1) * wireframe_gap;
        const int wireframe_h = wireframe_rows * wireframe_size + (wireframe_rows - 1) * wireframe_gap;
        
        // 그리드 중앙 정렬
        const int grid_start_x = info_x + (info_w - wireframe_w) / 2;
        const int grid_start_y = info_y + (info_h - wireframe_h) / 2;
        
        log("[WIREFRAME_CHECK] 클릭=(%d,%d), 화면=%dx%d, panel_y=%d, info=(%d,%d,%dx%d), grid_start=(%d,%d), grid_size=%dx%d\n",
            mx, my, screen_width, screen_height, panel_y, info_x, info_y, info_w, info_h, 
            grid_start_x, grid_start_y, wireframe_w, wireframe_h);
        
        // 24칸 체크
        for (int row = 0; row < wireframe_rows; ++row) {
            for (int col = 0; col < wireframe_cols; ++col) {
                int idx = row * wireframe_cols + col;
                int wx = grid_start_x + col * (wireframe_size + wireframe_gap);
                int wy = grid_start_y + row * (wireframe_size + wireframe_gap);
                
                // 첫 번째 칸만 로그 출력 (디버깅용)
                if (idx == 0) {
                    log("[WIREFRAME_SLOT0] 0번 칸: (%d,%d)-(%d,%d)\n", 
                        wx, wy, wx + wireframe_size, wy + wireframe_size);
                }
                
                if (mx >= wx && mx < wx + wireframe_size && my >= wy && my < wy + wireframe_size) {
                    log("[WIREFRAME_HIT] 와이어프레임 클릭: slot=%d, pos=(%d,%d), 영역=(%d,%d)-(%d,%d)\n", 
                        idx, mx, my, wx, wy, wx + wireframe_size, wy + wireframe_size);
                    return idx;  // 0~23
                }
            }
        }
        
        log("[WIREFRAME_MISS] 모든 칸 밖 클릭\n");
        return -1;  // Wireframe 영역 밖
    }
    
    player_game(game_player player) 
        : ui(std::move(player))
        , input(ui.st, ui.action_st) 
    {
        input.player_id = 0;
        ui.input_handler = &input;  // UI에 input_handler 연결
        // Connect toast callback to UI toast
        input.toast_cb = [this](const char* s, int frames) {
            ui.show_toast(a_string(s), frames);
        };
        // Connect selection suppression to UI
        input.selection_suppress_cb = [this](bool v) {
            ui.set_selection_suppressed(v);
        };
        // Connect camera movement callback
        input.move_camera_cb = [this](xy pos) {
            // 화면 중앙에 위치가 오도록 카메라 이동
            int half_w = (int)ui.view_width / 2;
            int half_h = (int)ui.view_height / 2;
            ui.screen_pos.x = pos.x - half_w;
            ui.screen_pos.y = pos.y - half_h;
            
            // 맵 경계 체크
            if (ui.screen_pos.x < 0) ui.screen_pos.x = 0;
            if (ui.screen_pos.y < 0) ui.screen_pos.y = 0;
            if (ui.screen_pos.x + (int)ui.view_width > (int)ui.game_st.map_width) {
                ui.screen_pos.x = ui.game_st.map_width - ui.view_width;
            }
            if (ui.screen_pos.y + (int)ui.view_height > (int)ui.game_st.map_height) {
                ui.screen_pos.y = ui.game_st.map_height - ui.view_height;
            }
            
            log("[CAMERA_MOVE] 화면 이동: target=(%d,%d), screen_pos=(%d,%d)\n", 
                pos.x, pos.y, ui.screen_pos.x, ui.screen_pos.y);
        };
        
        ui.load_data_file = [](a_vector<uint8_t>& data, a_string filename) {
            data_loading::data_files_directory("")(data, std::move(filename));
        };
        
        ui.global_volume = 50;
        
        // 속도 변경 딜레이 초기화
        last_speed_change = clock.now();
    }
    
    void update() {
        auto now = clock.now();
        auto tick_speed = std::chrono::milliseconds(tick_ms);
        
        // 마우스 좌표 업데이트 (매 프레임)
        ui.wnd.get_cursor_pos(&g_ui_mouse_x, &g_ui_mouse_y);
        
        // 3초마다 상태 출력 (디버깅)
        static auto last_debug_time = now;
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_debug_time).count() >= 3) {
            int mx = g_ui_mouse_x, my = g_ui_mouse_y;
            bool is_ui = (my > 550);
            int sel_count = 0;
            for (int i = 0; i < 8; ++i) sel_count += input.action_st.selection[i].size();
            log("[DEBUG_1SEC] frame=%d, left_btn=%d, is_drag=%d, screen_pos=(%d,%d), mouse=(%d,%d), is_ui=%d, selected=%d, cmd_mode=%d\n",
                (int)ui.st.current_frame, (int)input.left_button_down, (int)input.is_dragging,
                ui.screen_pos.x, ui.screen_pos.y, mx, my, (int)is_ui, 
                sel_count, (int)input.command_mode);
            last_debug_time = now;
        }
        
        // 게임 로직 업데이트
        if (now - last_tick >= tick_speed) {
            last_tick = now;
            state_functions funcs(ui.st);
            funcs.next_frame();
            
            // ★ 오토 렐리 일꾼 생산 완료 처리
            // 이전 프레임에서 생산 중이던 유닛이 완료되었는지 확인하고,
            // 해당 건물에 오토 렐리가 설정되어 있으면 자원 채취 명령 발행
            for (unit_t* building : ptr(ui.st.visible_units)) {
                if (!building || !building->sprite) continue;
                if (!(building->unit_type->flags & unit_type_t::flag_building)) continue;
                
                // 자원 생산 건물인지 확인 (넥서스, 해처리/레어/하이브, 커맨드센터)
                bool is_resource_building = (
                    input.actions.unit_is(building, UnitTypes::Protoss_Nexus) ||
                    input.actions.unit_is(building, UnitTypes::Zerg_Hatchery) ||
                    input.actions.unit_is(building, UnitTypes::Zerg_Lair) ||
                    input.actions.unit_is(building, UnitTypes::Zerg_Hive) ||
                    input.actions.unit_is(building, UnitTypes::Terran_Command_Center)
                );
                if (!is_resource_building) continue;
                
                // 이전 프레임에서 생산 중이던 유닛 확인
                auto prev_it = prev_building_units.find(building);
                unit_t* prev_build_unit = (prev_it != prev_building_units.end()) ? prev_it->second : nullptr;
                unit_t* curr_build_unit = building->current_build_unit;
                
                // 생산 완료 감지: 이전에 생산 중이던 유닛이 있었는데 현재는 없음
                if (prev_build_unit && !curr_build_unit) {
                    // 생산 완료된 유닛이 일꾼인지 확인
                    bool is_worker = (
                        input.actions.unit_is(prev_build_unit, UnitTypes::Protoss_Probe) ||
                        input.actions.unit_is(prev_build_unit, UnitTypes::Zerg_Drone) ||
                        input.actions.unit_is(prev_build_unit, UnitTypes::Terran_SCV)
                    );
                    
                    if (is_worker && prev_build_unit->sprite) {
                        // 해당 건물에 오토 렐리가 설정되어 있는지 확인
                        auto* brp = input.get_building_rally_storage(building);
                        bool handled = false;
                        
                        // ★ 우선순위 1: 오토 렐리 (자원 채취)
                        if (brp && brp->has_auto_rally && brp->auto_rally_target) {
                            // 오토 렐리 타겟이 자원인지 확인
                            unit_t* resource = brp->auto_rally_target;
                            bool is_mineral = input.actions.unit_is_mineral_field(resource);
                            bool is_gas = input.actions.unit_is(resource, UnitTypes::Resource_Vespene_Geyser) ||
                                          input.actions.unit_is_refinery(resource);
                            
                            if (is_mineral || is_gas) {
                                // 자원 채취 명령 발행
                                int pid = building->owner;
                                xy target_pos = resource->sprite->position;
                                
                                // Gather 명령 발행
                                const order_type_t* gather_order = input.actions.get_order_type(Orders::Harvest1);
                                
                                // 직접 유닛에 명령 설정
                                funcs.set_unit_order(prev_build_unit, gather_order, resource);
                                
                                log("[AUTO_RALLY_GATHER] 일꾼 생산 완료! 자원 채취 명령 발행: "
                                    "worker=%p (type=%d), building=%p, resource=%p (type=%d), pos=(%d,%d)\n",
                                    (void*)prev_build_unit, (int)prev_build_unit->unit_type->id,
                                    (void*)building, (void*)resource, (int)resource->unit_type->id,
                                    target_pos.x, target_pos.y);
                                handled = true;
                            }
                        }
                        
                        // ★ 우선순위 2: 일반 렐리 (오토렐리가 없거나 자원이 아닌 경우)
                        if (!handled && brp && brp->has_normal_rally) {
                            xy rally_pos = brp->normal_rally_pos;
                            unit_t* rally_target = brp->normal_rally_target;
                            
                            if (rally_target && rally_target->sprite) {
                                // 유닛 타겟이 있으면 Follow
                                funcs.set_unit_order(prev_build_unit, input.actions.get_order_type(Orders::Follow), rally_target);
                                log("[NORMAL_RALLY] 일꾼 생산 완료! 일반 렐리 (유닛 따라가기): "
                                    "worker=%p, target=%p, pos=(%d,%d)\n",
                                    (void*)prev_build_unit, (void*)rally_target,
                                    rally_target->sprite->position.x, rally_target->sprite->position.y);
                            } else if (rally_pos.x != 0 || rally_pos.y != 0) {
                                // 위치 타겟이면 Move
                                funcs.set_unit_order(prev_build_unit, input.actions.get_order_type(Orders::Move), rally_pos);
                                log("[NORMAL_RALLY] 일꾼 생산 완료! 일반 렐리 (위치 이동): "
                                    "worker=%p, pos=(%d,%d)\n",
                                    (void*)prev_build_unit, rally_pos.x, rally_pos.y);
                            }
                        }
                    }
                }
                
                // 현재 프레임의 생산 중인 유닛 저장
                prev_building_units[building] = curr_build_unit;
            }
            
            // 생산 속도 배율 적용
            if (production_speed_multiplier != 1.0f) {
                for (unit_t* u : ptr(ui.st.visible_units)) {
                    // 건설 중인 건물 또는 생산 중인 유닛
                    if (u->remaining_build_time > 0) {
                        int reduction = (int)((production_speed_multiplier - 1.0f) * 10);
                        if (reduction > 0 && u->remaining_build_time > reduction) {
                            u->remaining_build_time -= reduction;
                        }
                    }
                    // 생산 중인 유닛 (current_build_unit)
                    if (u->current_build_unit && u->current_build_unit->remaining_build_time > 0) {
                        int reduction = (int)((production_speed_multiplier - 1.0f) * 10);
                        if (reduction > 0 && u->current_build_unit->remaining_build_time > reduction) {
                            u->current_build_unit->remaining_build_time -= reduction;
                        }
                    }
                }
            }
            
            // 자원 유지
            if (input.allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) {
                    if (ui.st.current_minerals[pid] < 5000) {
                        ui.st.current_minerals[pid] = 5000;
                        ui.st.current_gas[pid] = 2000;
                    }
                }
            } else {
                if (ui.st.current_minerals[0] < 5000) {
                    ui.st.current_minerals[0] = 5000;
                    ui.st.current_gas[0] = 2000;
                }
                if (ui.st.current_minerals[1] < 5000) {
                    ui.st.current_minerals[1] = 5000;
                    ui.st.current_gas[1] = 2000;
                }
            }
        }
        
        // 화면 경계 체크
        if (ui.screen_pos.y + (int)ui.view_height > (int)ui.game_st.map_height) {
            ui.screen_pos.y = ui.game_st.map_height - ui.view_height;
        }
        if (ui.screen_pos.y < 0) ui.screen_pos.y = 0;
        if (ui.screen_pos.x + (int)ui.view_width > (int)ui.game_st.map_width) {
            ui.screen_pos.x = ui.game_st.map_width - ui.view_width;
        }
        if (ui.screen_pos.x < 0) ui.screen_pos.x = 0;
        
        // 타겟 링 업데이트
        input.update_target_ring();
        
        // 명령 모드를 UI에 전달 (시각화용)
        // 0=Normal, 1=Attack, 2=Move, 3=Patrol, 4=Hold, 5=Build, 6=Gather, 7=AdvancedBuild
        int cmd_mode_for_ui = 0;
        switch (input.command_mode) {
            case player_input_handler::CommandMode::Attack: cmd_mode_for_ui = 1; break;
            case player_input_handler::CommandMode::Move: cmd_mode_for_ui = 2; break;
            case player_input_handler::CommandMode::Patrol: cmd_mode_for_ui = 3; break;
            case player_input_handler::CommandMode::Hold: cmd_mode_for_ui = 4; break;
            case player_input_handler::CommandMode::Build: cmd_mode_for_ui = 5; break;
            case player_input_handler::CommandMode::Gather: cmd_mode_for_ui = 6; break;
            case player_input_handler::CommandMode::AdvancedBuild: cmd_mode_for_ui = 7; break;
            default: cmd_mode_for_ui = 0; break;
        }
        ui.current_command_mode = cmd_mode_for_ui;
        
        // 선택 동기화: UI의 현재 선택 -> action_st.selection[*]
        // Gather 모드 또는 Gather 종료 직후에는 건너뛰기 (자원 클릭 시 선택이 풀리는 것 방지)
        bool is_gather_mode = (input.command_mode == player_input_handler::CommandMode::Gather);
        bool should_skip = is_gather_mode || (input.skip_selection_sync_frames > 0);
        
        if (input.skip_selection_sync_frames > 0) {
            input.skip_selection_sync_frames--;
            log("[SYNC] 선택 동기화 건너뛰기 (남은 프레임: %d)\n", input.skip_selection_sync_frames);
            
            // ★ 역방향 동기화: action_st.selection -> current_selection
            // 컨트롤 그룹 선택 시 action_st.selection이 변경되므로 current_selection도 업데이트
            ui.current_selection_clear();
            if (input.allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) {
                    for (unit_t* u : ui.action_st.selection[pid]) {
                        if (u && u->sprite) ui.current_selection_add(u);
                    }
                }
            } else {
                for (unit_t* u : ui.action_st.selection[0]) {
                    if (u && u->sprite) ui.current_selection_add(u);
                }
            }
            log("[SYNC] 역방향 동기화 완료: current_selection 크기=%d\n", (int)ui.current_selection.size());
        }
        
        if (!should_skip) {
            if (input.allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) ui.action_st.selection[pid].clear();
                for (unit_t* u : ptr(ui.st.visible_units)) {
                    if (!u || !u->sprite) continue;
                    if (ui.current_selection_is_selected(u)) {
                        auto& bucket = ui.action_st.selection[u->owner];
                        if (bucket.size() < bucket.max_size()) bucket.push_back(u);
                    }
                }
            } else {
                auto& sel = ui.action_st.selection[0];
                sel.clear();
                for (unit_t* u : ptr(ui.st.visible_units)) {
                    if (!u || !u->sprite) continue;
                    if (u->owner != 0) continue;
                    if (ui.current_selection_is_selected(u)) {
                        if (sel.size() < sel.max_size()) sel.push_back(u);
                    }
                }
            }
        }
        
        // UI 업데이트 (이벤트 처리 + 렌더링)
        // 렌더 전에 명령 카드 카테고리 동기화
        {
            int cat = 0;
            // 선택된 항목에서 대표 하나를 선택
            // 건물이 있으면 건물 우선, 없으면 첫 번째 유닛
            unit_t* selected = nullptr;
            int pid = 0;
            if (input.allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    if (!ui.action_st.selection[p].empty()) {
                        // 건물 우선 선택
                        unit_t* building = nullptr;
                        for (auto* u : ui.action_st.selection[p]) {
                            if (u->unit_type->flags & unit_type_t::flag_building) {
                                building = u;
                                break;
                            }
                        }
                        selected = building ? building : ui.action_st.selection[p][0];
                        pid = p;
                        break;
                    }
                }
            } else {
                if (!ui.action_st.selection[0].empty()) {
                    // 건물 우선 선택
                    unit_t* building = nullptr;
                    for (auto* u : ui.action_st.selection[0]) {
                        if (u->unit_type->flags & unit_type_t::flag_building) {
                            building = u;
                            break;
                        }
                    }
                    selected = building ? building : ui.action_st.selection[0][0];
                    pid = 0;
                }
            }
            
            // ★ 선택된 유닛/건물 분석
            int building_count = 0;
            int unit_count = 0;
            int resource_count = 0;
            int building_type_count = 0;  // 서로 다른 건물 종류 수
            a_vector<int> building_types;  // 건물 타입 ID 저장

            // 건설 상태/유닛 상세 카운트
            int completed_building_count = 0;      // 완성된 건물 수
            int constructing_building_count = 0;   // 건설 중인 건물 수
            int real_unit_count = 0;               // 실제 유닛 수 (건물이 아닌 것)
            int worker_unit_count = 0;             // 일꾼 유닛 수 (Probe/Drone/SCV)
            int non_worker_unit_count = 0;         // 일꾼이 아닌 일반 유닛 수
            
            auto count_selection = [&](int p) {
                for (unit_t* u : ui.action_st.selection[p]) {
                    if (!u) continue;
                    
                    // 자원 체크
                    bool is_res = (ui.unit_is_mineral_field(u) || 
                                  ui.unit_is(u, UnitTypes::Resource_Vespene_Geyser) ||
                                  ui.unit_is_refinery(u));
                    if (is_res) {
                        resource_count++;
                        continue;
                    }
                    
                    // 건물 체크 (건설 중/완성 분리)
                    if (u->unit_type->flags & unit_type_t::flag_building) {
                        // 건설 중인지 확인 (remaining_build_time > 0)
                        bool is_constructing = (u->remaining_build_time > 0);
                        
                        if (!is_constructing) {
                            // 완성된 건물만 카운트
                            building_count++;
                            completed_building_count++;
                            
                            // 건물 타입 추가 (중복 체크)
                            int type_id = (int)u->unit_type->id;
                            bool found = false;
                            for (int bt : building_types) {
                                if (bt == type_id) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                building_types.push_back(type_id);
                            }
                        } else {
                            // 건설 중인 건물
                            constructing_building_count++;
                            // 기존 로직 유지: 건설 중인 건물은 유닛으로도 취급
                            unit_count++;
                        }
                    } else {
                        // 실제 유닛
                        unit_count++;
                        real_unit_count++;

                        bool is_worker_unit = (
                            ui.unit_is(u, UnitTypes::Protoss_Probe) ||
                            ui.unit_is(u, UnitTypes::Zerg_Drone) ||
                            ui.unit_is(u, UnitTypes::Terran_SCV)
                        );
                        if (is_worker_unit) ++worker_unit_count;
                        else ++non_worker_unit_count;
                    }
                }
            };
            
            if (input.allow_enemy_control) {
                for (int p = 0; p < 8; ++p) {
                    if (!ui.action_st.selection[p].empty()) {
                        count_selection(p);
                    }
                }
            } else {
                count_selection(0);
            }
            
            building_type_count = (int)building_types.size();
            
            // ★ 카테고리 판별
            if (selected) {
                // 자원만 선택 (카테고리 6)
                if (resource_count > 0 && building_count == 0 && unit_count == 0) {
                    cat = 6;  // Resource - no commands
                }
                // 건설 중 건물 + 완성된 건물이 함께 선택되고, 실제 유닛은 없는 경우 (카테고리 7)
                // 예: 완성된 넥서스 + 건설 중 넥서스
                else if (completed_building_count > 0 && constructing_building_count > 0 && real_unit_count == 0) {
                    cat = 7;  // Mixed building states - no commands
                    log("[UI_CAT] 건설중+완성 건물 혼합 선택 → 카테고리 7 (명령 없음)\n");
                }
                // 건물/자원 + 일꾼 유닛만 선택도 혼합 카테고리 8을 사용 (M,S,A,P,H)
                else if ((building_count > 0 || resource_count > 0) && worker_unit_count > 0 && non_worker_unit_count == 0) {
                    cat = 8;
                    log("[UI_CAT] 건물/자원 + 일꾼 유닛만 선택 → 카테고리 8 (유닛 기본 명령)\n");
                }
                // 건물 + 유닛 혼합 선택 (카테고리 8 - 유닛 기본 명령만)
                else if (building_count > 0 && unit_count > 0) {
                    cat = 8;  // Mixed - basic unit commands only (M,S,A,P,H)
                    log("[UI_CAT] 혼합 선택: 건물 %d개 + 유닛 %d개 → 카테고리 8 (유닛 기본 명령)\n", 
                        building_count, unit_count);
                }
                // 서로 다른 건물 여러 개 (카테고리 7 - 명령 없음)
                else if (building_count > 1 && building_type_count > 1 && unit_count == 0) {
                    cat = 7;  // Multiple different buildings - no commands
                    log("[UI_CAT] 다른 건물 여러 개: %d개 종류 → 카테고리 7 (명령 없음)\n", 
                        building_type_count);
                }
                // 건물만 선택 (같은 건물 여러 개 포함)
                else if (building_count > 0 && unit_count == 0) {
                    bool is_building = (selected->unit_type->flags & unit_type_t::flag_building);
                    
                    if (is_building) {
                        // 공격 가능한 건물 체크 (카테고리 5)
                        if (ui.unit_is(selected, UnitTypes::Protoss_Photon_Cannon) ||
                            ui.unit_is(selected, UnitTypes::Zerg_Sunken_Colony) ||
                            ui.unit_is(selected, UnitTypes::Zerg_Spore_Colony) ||
                            ui.unit_is(selected, UnitTypes::Terran_Bunker) ||
                            ui.unit_is(selected, UnitTypes::Terran_Missile_Turret)) {
                            cat = 5;  // Attack Building
                        }
                        // 생산 건물 (같은 건물 여러 개도 포함)
                        else if (ui.unit_is(selected, UnitTypes::Protoss_Nexus)) cat = 1;
                        else if (ui.unit_is(selected, UnitTypes::Protoss_Gateway)) cat = 2;
                        else if (ui.unit_is(selected, UnitTypes::Zerg_Hatchery) || ui.unit_is(selected, UnitTypes::Zerg_Lair) || ui.unit_is(selected, UnitTypes::Zerg_Hive)) cat = 3;
                        else if (ui.unit_is(selected, UnitTypes::Terran_Command_Center)) cat = 4;
                        else cat = 0; // unsupported building -> default
                    } else {
                        cat = 0; // units only
                    }
                }
                // 유닛만 선택
                else {
                    cat = 0; // units only
                }
            } else {
                cat = 0; // no selection
            }
            
            // 카테고리가 변경될 때만 로그 출력
            static int last_cat = -1;
            static void* last_selected = nullptr;
            if (cat != last_cat || selected != last_selected) {
                if (selected) {
                    bool is_building = (selected->unit_type->flags & unit_type_t::flag_building);
                    log("[UI_CAT] selected=%p, type_id=%d, is_building=%d, category=%d\n", 
                        (void*)selected, (int)selected->unit_type->id, (int)is_building, cat);
                } else {
                    log("[UI_CAT] no selection, category=%d\n", cat);
                }
                last_cat = cat;
                last_selected = selected;
            }
            
            // 매 프레임마다 선택된 유닛/건물의 현재 order를 확인하여 활성 버튼 업데이트
            // 단, 명령 모드가 활성화되어 있을 때는 자동 업데이트 건너뛰기 (사용자 입력 우선)
            bool is_command_mode_active = (input.command_mode != player_input_handler::CommandMode::Normal);
            g_ui_command_mode_active = is_command_mode_active;  // UI에 명령 모드 상태 전달
            
            // cat 0: 유닛만, cat 5: 공격 건물, cat 8/9: 혼합/건물+일꾼 카테고리에서도
            // order 기반 자동 하이라이트 동작하도록 확장
            if (selected && (cat == 0 || cat == 5 || cat == 8 || cat == 9) && !is_command_mode_active) {
                // 하이라이트 기준 유닛/건물 선택
                unit_t* highlight_target = selected;
                int highlight_cat = cat;
                
                // cat 8/9에서는 건물이 아니라 실제 유닛(일꾼/전투 유닛)을 기준으로 order를 본다
                if (cat == 8 || cat == 9) {
                    unit_t* unit_for_highlight = nullptr;
                    if (input.allow_enemy_control) {
                        for (int pid = 0; pid < 8 && !unit_for_highlight; ++pid) {
                            for (unit_t* u : ui.action_st.selection[pid]) {
                                if (!u) continue;
                                if (!(u->unit_type->flags & unit_type_t::flag_building)) {
                                    unit_for_highlight = u;
                                    break;
                                }
                            }
                        }
                    } else {
                        int pid = 0;
                        for (unit_t* u : ui.action_st.selection[pid]) {
                            if (!u) continue;
                            if (!(u->unit_type->flags & unit_type_t::flag_building)) {
                                unit_for_highlight = u;
                                break;
                            }
                        }
                    }
                    if (unit_for_highlight) {
                        highlight_target = unit_for_highlight;
                        highlight_cat = 0;  // 유닛 기준으로 버튼 인덱스 매핑
                    }
                }
                
                if (highlight_target) {
                    // 유닛/건물의 현재 order 확인
                    int order_id = (int)highlight_target->order_type->id;
                    
                    // 디버그: order 변경 시 로그
                    static int last_order = -1;
                    static int last_active_btn = -999;
                    if (order_id != last_order) {
                        log("[ORDER_CHANGE] order_id=%d, cat=%d\n", order_id, cat);
                        last_order = order_id;
                    }
                    
                    // Order ID에 따라 활성 버튼 결정
                    // Move: 0=M 버튼
                    // Stop: 1=S 버튼
                    // Attack: 2=A 버튼 (일반 유닛) 또는 0=A 버튼 (공격 건물)
                    // Patrol: 3=P 버튼
                    // Hold: 4=H 버튼
                    // Gather: 5=G 버튼
                    
                    if (order_id == (int)Orders::Move) {
                        g_ui_active_button = 0;  // M 버튼
                    } else if (order_id == (int)Orders::Stop || order_id == (int)Orders::Nothing) {
                        if (highlight_cat == 5) {
                            g_ui_active_button = 1;  // 공격 건물: S 버튼 (인덱스 1)
                        } else {
                            g_ui_active_button = 1;  // 일반 유닛: S 버튼
                        }
                    } else if (order_id == (int)Orders::TowerGuard) {
                        // 공격 건물 대기 상태 (TowerGuard)
                        g_ui_active_button = 1;  // S 버튼 (정지 상태)
                    } else if (order_id == (int)Orders::TowerAttack) {
                        // 공격 건물 공격 중 (TowerAttack)
                        g_ui_active_button = 0;  // A 버튼 (공격 중)
                    } else if (order_id == (int)Orders::AttackUnit || order_id == (int)Orders::AttackMove) {
                        if (highlight_cat == 5) {
                            g_ui_active_button = 0;  // 공격 건물: A 버튼 (인덱스 0)
                        } else {
                            g_ui_active_button = 2;  // 일반 유닛: A 버튼 (인덱스 2)
                        }
                    } else if (order_id == (int)Orders::Patrol) {
                        g_ui_active_button = 3;  // P 버튼
                    } else if (order_id == (int)Orders::HoldPosition) {
                        g_ui_active_button = 4;  // H 버튼
                    } else if (order_id == (int)Orders::Harvest1 || order_id == (int)Orders::Harvest2 || 
                              order_id == (int)Orders::MoveToGas || order_id == (int)Orders::WaitForGas ||
                              order_id == (int)Orders::HarvestGas || order_id == (int)Orders::ReturnGas ||
                              order_id == (int)Orders::MoveToMinerals || order_id == (int)Orders::WaitForMinerals ||
                              order_id == (int)Orders::MiningMinerals || order_id == (int)Orders::ReturnMinerals) {
                        g_ui_active_button = 5;  // G 버튼
                    } else {
                        // 기타 order는 S 버튼으로 표시
                        g_ui_active_button = 1;  // S 버튼
                    }
                    
                    // 디버그: 버튼 변경 시 로그
                    if (g_ui_active_button != last_active_btn) {
                        log("[BTN_CHANGE] order_id=%d → active_button=%d\n", order_id, g_ui_active_button);
                        last_active_btn = g_ui_active_button;
                    }
                }
            } else if (selected && (cat == 1 || cat == 2 || cat == 3 || cat == 4)) {
                // 생산 건물: 매 프레임 build_queue 상태에 따라 0번 버튼 하이라이트 결정
                bool is_training = !selected->build_queue.empty();

                if (is_training) {
                    // 유닛을 하나라도 생산/대기 중이면 0번 버튼(P/Drone/SCV/Z 등) 하이라이트
                    if (g_ui_active_button != 0) {
                        g_ui_active_button = 0;
                        log("[BUILDING_TRAIN_ACTIVE] cat=%d, queue_size=%d → active_btn=0\n", cat, (int)selected->build_queue.size());
                    }
                } else {
                    // 생산이 전혀 없으면 0/1번 생산 버튼 하이라이트 해제
                    if (g_ui_active_button == 0 || g_ui_active_button == 1) {
                        log("[BUILDING_TRAIN_END] cat=%d, queue_size=0 → 생산 버튼 해제 (btn=%d)\n", cat, g_ui_active_button);
                        g_ui_active_button = -1;
                    }
                }

                // 명령 모드가 아닐 때는 렐리 버튼(5,6)만 하이라이트 해제
                if (!is_command_mode_active && (g_ui_active_button == 5 || g_ui_active_button == 6)) {
                    log("[BUILDING_FORCE_CLEAR] 렐리 버튼 하이라이트 해제 (btn=%d)\n", g_ui_active_button);
                    g_ui_active_button = -1;
                }
            } else if (cat == 6) {
                // 자원: 활성 버튼 없음
                g_ui_active_button = -1;
            }
            
            g_ui_selected_category = cat;
            
            // 프로브 건설 모드일 때는 전용 명령 카테고리 9번을 사용
            if (input.command_mode == player_input_handler::CommandMode::Build ||
                input.command_mode == player_input_handler::CommandMode::AdvancedBuild) {
                auto sel_worker = input.find_selected_worker_any_owner();
                unit_t* worker = sel_worker.second;
                if (worker && worker->unit_type && worker->unit_type->id == UnitTypes::Protoss_Probe) {
                    g_ui_selected_category = 9;
                }
            }
            
            // 최종 안전장치: 생산 건물이고 명령 모드가 아니면 렐리 버튼(5,6) 하이라이트만 해제
            // 생산 버튼(0,1)은 명령 모드가 아니어도 하이라이트 유지
            if ((cat >= 1 && cat <= 4) && !is_command_mode_active) {
                // 5번(렐리), 6번(오토 렐리) 버튼만 해제
                if (g_ui_active_button == 5 || g_ui_active_button == 6) {
                    log("[FINAL_SAFETY] 렐리 버튼 하이라이트 해제 (cat=%d, btn=%d)\n", cat, g_ui_active_button);
                    g_ui_active_button = -1;
                }
                // 0번, 1번(생산 버튼)은 유지
            }
            
            // ★ 렐리 포인트 지속적 시각화: 선택된 모든 생산 건물의 렐리 포인트 표시
            input.multi_rally_points.clear();  // 매 프레임 초기화
            
            // 선택된 항목에 건물이 있다면 카테고리와 상관없이 렐리 포인트를 수집
            // (생산 가능 여부/카테고리 제한 제거: 모든 건물+자원+유닛 조합에서 렐리 시각화 지원)
            bool should_collect_rally = true;
            
            if (should_collect_rally) {
                // 선택된 모든 건물의 렐리 포인트 수집
                auto collect_rally_points = [&](int pid) {
                    for (unit_t* u : ui.action_st.selection[pid]) {
                        if (!u || !u->sprite) continue;
                        
                        // 건물인지 확인 (모든 건물 대상)
                        if (!(u->unit_type->flags & unit_type_t::flag_building)) continue;
                        
                        // ★ 렐리 포인트 확인 (저장소 우선, 없으면 엔진 백업)
                        auto* brp = input.get_building_rally_storage(u);
                        bool has_stored_rally = false;
                        
                        // 저장소 확인 (5번/6번 독립 저장)
                        if (brp) {
                            // 5번 일반 렐리 (초록색)
                            if (brp->has_normal_rally) {
                                xy rally_pos = brp->normal_rally_pos;
                                // 타겟이 유닛이면 현재 위치 업데이트
                                if (brp->normal_rally_target && brp->normal_rally_target->sprite) {
                                    rally_pos = brp->normal_rally_target->sprite->position;
                                }
                                
                                player_input_handler::MultiRallyPoint mrp;
                                mrp.building_pos = u->sprite->position;
                                mrp.rally_pos = rally_pos;
                                mrp.is_auto_rally = false;  // 초록색
                                input.multi_rally_points.push_back(mrp);
                                has_stored_rally = true;
                            }
                            
                            // 6번 오토 렐리 (노란색)
                            if (brp->has_auto_rally) {
                                xy rally_pos = brp->auto_rally_pos;
                                // 타겟이 유닛이면 현재 위치 업데이트
                                if (brp->auto_rally_target && brp->auto_rally_target->sprite) {
                                    rally_pos = brp->auto_rally_target->sprite->position;
                                }
                                
                                player_input_handler::MultiRallyPoint mrp;
                                mrp.building_pos = u->sprite->position;
                                mrp.rally_pos = rally_pos;
                                mrp.is_auto_rally = true;  // 노란색
                                input.multi_rally_points.push_back(mrp);
                                has_stored_rally = true;
                            }
                        }
                        
                        // 저장소에 없으면 엔진 확인 (백업)
                        if (!has_stored_rally) {
                            bool has_engine_rally = (u->building.rally.unit || (u->building.rally.pos.x != 0 || u->building.rally.pos.y != 0));
                            
                            if (has_engine_rally) {
                                xy rally_pos{0, 0};
                                bool is_auto = false;
                                
                                if (u->building.rally.unit) {
                                    rally_pos = u->building.rally.unit->sprite->position;
                                    // 자원인지 확인
                                    unit_t* rally_target = u->building.rally.unit;
                                    is_auto = (
                                        rally_target->unit_type->id == UnitTypes::Resource_Mineral_Field ||
                                        rally_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2 ||
                                        rally_target->unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3 ||
                                        rally_target->unit_type->id == UnitTypes::Protoss_Assimilator ||
                                        rally_target->unit_type->id == UnitTypes::Zerg_Extractor ||
                                        rally_target->unit_type->id == UnitTypes::Terran_Refinery
                                    );
                                } else {
                                    rally_pos = u->building.rally.pos;
                                    is_auto = false;
                                }
                                
                                player_input_handler::MultiRallyPoint mrp;
                                mrp.building_pos = u->sprite->position;
                                mrp.rally_pos = rally_pos;
                                mrp.is_auto_rally = is_auto;
                                input.multi_rally_points.push_back(mrp);
                            }
                        }
                    }
                };
                
                // 모든 플레이어의 선택 확인
                if (input.allow_enemy_control) {
                    for (int p = 0; p < 8; ++p) {
                        if (!ui.action_st.selection[p].empty()) {
                            collect_rally_points(p);
                        }
                    }
                } else {
                    collect_rally_points(0);
                }
                
                // 디버그 로그 (1초마다)
                static int rally_log_counter = 0;
                rally_log_counter++;
                if (rally_log_counter >= 60) {
                    if (!input.multi_rally_points.empty()) {
                        log("[RALLY_MULTI] %d개 건물의 렐리 포인트 표시\n", (int)input.multi_rally_points.size());
                    }
                    rally_log_counter = 0;
                }
                
                // 단일 렐리 시각화는 첫 번째 건물 사용 (하위 호환)
                if (!input.multi_rally_points.empty()) {
                    input.rally_visual.building_pos = input.multi_rally_points[0].building_pos;
                    input.rally_visual.rally_pos = input.multi_rally_points[0].rally_pos;
                    input.rally_visual.is_active = true;
                    input.rally_visual.is_auto_rally = input.multi_rally_points[0].is_auto_rally;
                } else {
                    input.rally_visual.is_active = false;
                }
            } else {
                // 생산 건물이 선택되지 않았을 때
                // frames_remaining이 남아있으면 일시적으로 표시 (설정 직후 강조)
                if (input.rally_visual.frames_remaining <= 0) {
                    input.rally_visual.is_active = false;
                }
                input.rally_visual.last_selected_building = nullptr;
            }
            
            // UI_SYNC 로그는 3초마다만 출력 (60 FPS 기준 180프레임마다)
            static int ui_sync_frame_counter = 0;
            ui_sync_frame_counter++;
            if (ui_sync_frame_counter >= 180) {
                log("[UI_SYNC] g_ui_selected_category=%d, g_ui_active_button=%d, rally_active=%d\n", 
                    g_ui_selected_category, g_ui_active_button, (int)input.rally_visual.is_active);
                ui_sync_frame_counter = 0;
            }
        }
        // ★ 렐리 포인트 시각화 정보를 UI에 전달
        // 다중 렐리 포인트 전달
        ui.multi_rally_points.clear();
        for (const auto& mrp : input.multi_rally_points) {
            ui_functions::MultiRallyPoint ui_mrp;
            ui_mrp.building_pos = mrp.building_pos;
            ui_mrp.rally_pos = mrp.rally_pos;
            ui_mrp.is_auto_rally = mrp.is_auto_rally;
            ui.multi_rally_points.push_back(ui_mrp);
        }
        
        // 단일 렌더링 (하위 호환)
        if (input.rally_visual.is_active) {
            ui.rally_visual_active = true;
            ui.rally_building_pos = input.rally_visual.building_pos;
            ui.rally_point_pos = input.rally_visual.rally_pos;
            ui.rally_is_auto = input.rally_visual.is_auto_rally;
        } else {
            ui.rally_visual_active = false;
        }
        
        // 타겟 링 정보 전달 (전역 변수 - 하위 호환용, 마지막 링만)
        g_target_ring_active = false;
        g_target_ring_x = 0;
        g_target_ring_y = 0;
        g_target_ring_radius = 0;
        g_target_ring_frames = 0;

        // 멀티 타겟 링 정보 전달 (ui_custom.h에서 스프라이트 하이라이트에 사용)
        bwgame::g_target_rings.clear();
        for (const auto& ring : input.target_rings) {
            if (ring.frames <= 0) continue;
            bwgame::ui_target_ring_t ur;
            ur.x = ring.pos.x;
            ur.y = ring.pos.y;
            ur.radius = ring.radius;
            ur.frames = ring.frames;
            ur.kind = ring.kind;
            bwgame::g_target_rings.push_back(ur);
        }
        
        // 명령 링 정보 전달 (전역 변수)
        g_command_ring_x = input.command_ring_pos.x;
        g_command_ring_y = input.command_ring_pos.y;
        g_command_ring_radius = input.command_ring_radius;
        g_command_ring_frames = input.command_ring_frames;
        g_command_ring_kind = input.command_ring_kind;
        
        if (g_target_ring_active) {
            log("[GLOBAL_RING] active=%d, frames=%d, pos=(%d,%d), radius=%d\n",
                g_target_ring_active, g_target_ring_frames, g_target_ring_x, g_target_ring_y, g_target_ring_radius);
        }
        if (g_command_ring_frames > 0) {
            log("[COMMAND_RING] frames=%d, pos=(%d,%d), radius=%d\n",
                g_command_ring_frames, g_command_ring_x, g_command_ring_y, g_command_ring_radius);
        }
        // 렌더링 직전 상태 요약 (타겟 링 개수 / 명령 링 남은 프레임) - 5초마다 출력
        static auto last_ring_state_log = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_ring_state_log).count() >= 5) {
            log("[RING_STATE] target_rings=%d, command_ring_frames=%d\n",
                (int)bwgame::g_target_rings.size(), g_command_ring_frames);
            last_ring_state_log = now;
        }
        
        // UI 내부에서 이벤트를 소비하므로 별도 폴링하지 않음
        try {
            ui.update();
        } catch (const std::exception& e) {
            log("[CRASH_UI] ui.update exception: %s\n", e.what());
            fflush(log_file);
            return; // skip rest of frame
        } catch (...) {
            log("[CRASH_UI] ui.update unknown exception\n");
            fflush(log_file);
            return;
        }
        
        // 타겟 링 렌더링 (ui.update() 직후, 화면 위에 오버레이)
        // 멀티 타겟 링은 GRP 기반(ui_custom.h의 g_target_rings)으로만 사용하고,
        // 여기 RGBA 오버레이는 비활성화하여 이중 링(초록 링 2개) 현상을 방지한다.
        if (false && ui.rgba_surface) {
            void* ptr = ui.rgba_surface->lock();
            if (ptr) {
                uint32_t* pixels = (uint32_t*)ptr;
                int pitch_words = ui.rgba_surface->pitch / 4;

                // 원 그리기 함수 (Bresenham's circle algorithm)
                auto draw_circle = [&](int cx, int cy, int radius, uint32_t color) {
                    int x = 0;
                    int y = radius;
                    int d = 3 - 2 * radius;
                    
                    auto put_pixel_safe = [&](int px, int py) {
                        if (px >= 0 && px < ui.rgba_surface->w && py >= 0 && py < ui.rgba_surface->h) {
                            put_pixel_rgba_blend(pixels, pitch_words, px, py, color);
                        }
                    };
                    
                    while (y >= x) {
                        put_pixel_safe(cx + x, cy + y);
                        put_pixel_safe(cx - x, cy + y);
                        put_pixel_safe(cx + x, cy - y);
                        put_pixel_safe(cx - x, cy - y);
                        put_pixel_safe(cx + y, cy + x);
                        put_pixel_safe(cx - y, cy + x);
                        put_pixel_safe(cx + y, cy - x);
                        put_pixel_safe(cx - y, cy - x);
                        
                        x++;
                        if (d > 0) {
                            y--;
                            d = d + 4 * (x - y) + 10;
                        } else {
                            d = d + 4 * x + 6;
                        }
                    }
                };

                // 멀티 타겟 링이 있으면 우선적으로 모두 렌더링
                if (!input.target_rings.empty()) {
                    for (const auto& ring : input.target_rings) {
                        if (ring.frames <= 0) continue;

                        // 게임 좌표를 화면 좌표로 변환
                        int screen_x = ring.pos.x - ui.screen_pos.x;
                        int screen_y = ring.pos.y - ui.screen_pos.y;

                        // 타겟 종류에 따른 색상 선택
                        // 0=self/ally(초록), 1=enemy(빨강), 2=resource(노랑)
                        uint32_t ring_color;
                        if (ring.kind == 1) {
                            // 적군: 빨간색
                            ring_color = pack(180, 255, 0, 0);
                        } else if (ring.kind == 2) {
                            // 자원: 노란색
                            ring_color = pack(180, 255, 255, 0);
                        } else {
                            // 아군/자신: 초록색
                            ring_color = pack(180, 0, 255, 0);
                        }

                        // 두꺼운 링 그리기
                        for (int i = -2; i <= 2; ++i) {
                            draw_circle(screen_x, screen_y, ring.radius + i, ring_color);
                        }
                    }
                }
                // 멀티 링이 비어 있으면 기존 단일 링 로직으로 렌더링 (하위 호환)
                else if (g_target_ring_active && g_target_ring_frames > 0) {
                    log("[RENDER_RING] 렌더링 시작! frames=%d\n", g_target_ring_frames);

                    // 게임 좌표를 화면 좌표로 변환
                    int screen_x = g_target_ring_x - ui.screen_pos.x;
                    int screen_y = g_target_ring_y - ui.screen_pos.y;
                    
                    if (g_target_ring_frames > 0) {
                        // 타겟 종류에 따른 색상 선택 (단일 링 하위 호환)
                        // 0=self/ally(초록), 1=enemy(빨강), 2=resource(노랑)
                        uint32_t ring_color;
                        if (g_target_ring_kind == 1) {
                            ring_color = pack(180, 255, 0, 0);
                        } else if (g_target_ring_kind == 2) {
                            ring_color = pack(180, 255, 255, 0);
                        } else {
                            ring_color = pack(180, 0, 255, 0);
                        }

                        for (int i = -2; i <= 2; ++i) {
                            draw_circle(screen_x, screen_y, g_target_ring_radius + i, ring_color);
                        }
                    }
                }

                ui.rgba_surface->unlock();
            }
        } else if (g_command_ring_frames > 0 && !bwgame::g_target_rings.empty()) {
            // 디버그: 타겟 링이 있어서 명령 링이 스킵된 경우
            log("[RENDER_COMMAND_RING_SKIP] target_rings=%d, cmd_frames=%d\n",
                (int)bwgame::g_target_rings.size(), g_command_ring_frames);
        }
        
        // 명령 링 렌더링 (땅 클릭 시 표시)
        // 타겟 링이 없을 때만 표시 (중복 방지)
        if (g_command_ring_frames > 0 && ui.rgba_surface && bwgame::g_target_rings.empty()) {
            log("[RENDER_COMMAND_RING] 렌더링 시작! frames=%d\n", g_command_ring_frames);
            void* ptr = ui.rgba_surface->lock();
            if (ptr) {
                uint32_t* pixels = (uint32_t*)ptr;
                int pitch_words = ui.rgba_surface->pitch / 4;
                
                // 게임 좌표를 화면 좌표로 변환
                int screen_x = g_command_ring_x - ui.screen_pos.x;
                int screen_y = g_command_ring_y - ui.screen_pos.y;
                
                log("[COMMAND_RING_SCREEN] game=(%d,%d) screen=(%d,%d) camera=(%d,%d)\n",
                    g_command_ring_x, g_command_ring_y, screen_x, screen_y, ui.screen_pos.x, ui.screen_pos.y);
                
                // 깜빡임 효과: 일정 간격으로 on/off (스타크래프트 스타일)
                // 4프레임마다 깜빡임 (on 2프레임, off 2프레임)
                bool show = ((g_command_ring_frames / 4) % 2 == 1);
                if (show) {
                    // 명령 종류에 따른 색상 선택
                    uint32_t ring_color;
                    if (g_command_ring_kind == 1) {
                        // 공격: 빨간색
                        ring_color = pack(180, 255, 100, 0);
                    } else if (g_command_ring_kind == 2) {
                        // 정찰: 노란색
                        ring_color = pack(180, 255, 255, 0);
                    } else {
                        // 기본 (이동): 초록색
                        ring_color = pack(180, 0, 255, 0);
                    }

                    // 원 그리기 함수
                    auto draw_circle = [&](int cx, int cy, int radius, uint32_t color) {
                        int x = 0;
                        int y = radius;
                        int d = 3 - 2 * radius;
                        
                        auto put_pixel_safe = [&](int px, int py) {
                            if (px >= 0 && px < ui.rgba_surface->w && py >= 0 && py < ui.rgba_surface->h) {
                                put_pixel_rgba_blend(pixels, pitch_words, px, py, color);
                            }
                        };
                        
                        while (y >= x) {
                            put_pixel_safe(cx + x, cy + y);
                            put_pixel_safe(cx - x, cy + y);
                            put_pixel_safe(cx + x, cy - y);
                            put_pixel_safe(cx - x, cy - y);
                            put_pixel_safe(cx + y, cy + x);
                            put_pixel_safe(cx - y, cy + x);
                            put_pixel_safe(cx + y, cy - x);
                            put_pixel_safe(cx - y, cy - x);
                            
                            x++;
                            if (d > 0) {
                                y--;
                                d = d + 4 * (x - y) + 10;
                            } else {
                                d = d + 4 * x + 6;
                            }
                        }
                    };
                    
                    // 두꺼운 링 그리기
                    for (int i = -2; i <= 2; ++i) {
                        draw_circle(screen_x, screen_y, g_command_ring_radius + i, ring_color);
                    }
                }
                
                ui.rgba_surface->unlock();
            }
        }



        // 비침투 입력 합성: UI가 이벤트를 처리한 뒤 상태를 읽어 필요한 것만 전달
        {
            // 키 목록 (SDL scancode)
            static const int keys[] = {
                4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19, 21, 22, 23, 25, 27, 28, 29, 39, 41, 44, // A,B,C,D,E,F,G,H,M,N,O,P,R,S,T,V,X,Y,Z,0, ESC(41), Space(44)
                58, 59, 62, 63, 64, 65, 66, 67, // F1(TURBO), F2(생산10배), F5, F6, F7(slower), F8(faster), F9(HUD), F10
                30, 31, 32, 33, 34, 35, 36, 37, 38, // 1-9 (메인 키보드)
                89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, // 넘패드 1-9, 0, . (scancode 89-99)
                75, 78, // Page Up(75), Page Down(78)
                225, 229, 224, 228, 226, 230 // Shift/Ctrl/Alt 좌우
            };
            for (int sc : keys) {
                bool cur = ui.wnd.get_key_state(sc);
                bool& prv = prev_keys[sc];
                if (cur != prv) {
                    if (cur) input.on_key_down(sc);
                    else input.on_key_up(sc);
                    prv = cur;
                }
            }

            // F7/F8/F10 게임 속도 조절 (고정 단계 + 딜레이)
            auto now_time = clock.now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_speed_change).count();
            
            if (elapsed >= 500) { // 0.5초 딜레이
                if (prev_keys[64]) { // F7 slower
                    if (current_speed_index > 0) {
                        current_speed_index--;
                        tick_ms = SPEED_LEVELS[current_speed_index];
                        ui.show_toast(a_string("Game Speed: ") + SPEED_NAMES[current_speed_index], 90);
                        last_speed_change = now_time;
                    }
                    prev_keys[64] = false; // consume edge
                }
                if (prev_keys[65]) { // F8 faster
                    if (current_speed_index < 7) {
                        current_speed_index++;
                        tick_ms = SPEED_LEVELS[current_speed_index];
                        ui.show_toast(a_string("Game Speed: ") + SPEED_NAMES[current_speed_index], 90);
                        last_speed_change = now_time;
                    }
                    prev_keys[65] = false; // consume edge
                }
            } else {
                // 딜레이 중에는 키 입력 무시
                prev_keys[64] = false;
                prev_keys[65] = false;
            }
            
            // 키패드 1~9: 부대지정 전용으로 변경 (생산 속도 조절 기능 제거)
            // 생산 속도 조절은 Page Up/Down 사용
            
            // Page Up (스캔코드 75): 생산 속도 증가
            if (prev_keys[75]) {
                // 현재 레벨 찾기
                int current_level = 1;  // 기본값 1.0x
                for (int i = 0; i < 5; ++i) {
                    if (production_speed_multiplier == PRODUCTION_SPEED_LEVELS[i]) {
                        current_level = i;
                        break;
                    }
                }
                
                // 다음 레벨로 증가 (최대 4)
                if (current_level < 4) {
                    current_level++;
                    production_speed_multiplier = PRODUCTION_SPEED_LEVELS[current_level];
                    ui.show_toast(a_string(PRODUCTION_SPEED_NAMES[current_level]), 120);
                    log("[PAGE_UP] Production speed increased: %s\n", PRODUCTION_SPEED_NAMES[current_level]);
                } else {
                    ui.show_toast(a_string("Max speed reached (10x)"), 60);
                    log("[PAGE_UP] Already at max speed\n");
                }
                prev_keys[75] = false;
            }
            
            // Page Down (스캔코드 78): 생산 속도 감소
            if (prev_keys[78]) {
                // 현재 레벨 찾기
                int current_level = 1;  // 기본값 1.0x
                for (int i = 0; i < 5; ++i) {
                    if (production_speed_multiplier == PRODUCTION_SPEED_LEVELS[i]) {
                        current_level = i;
                        break;
                    }
                }
                
                // 이전 레벨로 감소 (최소 0)
                if (current_level > 0) {
                    current_level--;
                    production_speed_multiplier = PRODUCTION_SPEED_LEVELS[current_level];
                    ui.show_toast(a_string(PRODUCTION_SPEED_NAMES[current_level]), 120);
                    log("[PAGE_DOWN] Production speed decreased: %s\n", PRODUCTION_SPEED_NAMES[current_level]);
                } else {
                    ui.show_toast(a_string("Min speed reached (0.5x)"), 60);
                    log("[PAGE_DOWN] Already at min speed\n");
                }
                prev_keys[78] = false;
            }
            
            // 넘패드 . (Del, 스캔코드 99): 넘패드 부대 전체 초기화
            if (prev_keys[99]) {
                int pid = input.get_effective_owner();
                int cleared_count = 0;
                
                // 넘패드 그룹 10~18 초기화
                for (int i = 10; i < 19; ++i) {
                    if (!input.action_st.control_groups[pid][i].empty()) {
                        input.action_st.control_groups[pid][i].clear();
                        cleared_count++;
                    }
                }
                
                log("[NUMPAD_DEL] 넘패드 부대 초기화: %d개 그룹 삭제됨\n", cleared_count);
                
                ui.show_toast(a_string("Numpad Groups Cleared (") + std::to_string(cleared_count) + " groups)", 60);
                
                prev_keys[99] = false;
            }

            // F9: HUD toggle/settings (0.5초 딜레이)
            static auto last_f9_time = now_time;
            auto f9_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_f9_time).count();
            
            if (prev_keys[66] && f9_elapsed >= 500) {
                bool m_shift = ui.wnd.get_key_state(225) || ui.wnd.get_key_state(229);
                bool m_ctrl  = ui.wnd.get_key_state(224) || ui.wnd.get_key_state(228);
                bool m_alt   = ui.wnd.get_key_state(226) || ui.wnd.get_key_state(230);

                if (m_ctrl) {
                    // Cycle HUD scale 1..4
                    int old = ui.hud_scale;
                    ui.hud_scale = (ui.hud_scale % 4) + 1;
                    ui.show_toast(a_string("HUD SCALE: ") + std::to_string(ui.hud_scale).c_str(), 90);
                } else if (m_alt) {
                    // Cycle background alpha levels
                    const uint8_t levels[] = {0x00, 0x40, 0x60, 0x80, 0xC0, 0xFF};
                    // find current index
                    size_t idx = 0;
                    for (; idx < sizeof(levels); ++idx) if (levels[idx] == (uint8_t)ui.hud_bg_alpha) break;
                    if (idx >= sizeof(levels)) idx = 0;
                    idx = (idx + 1) % (sizeof(levels));
                    ui.hud_bg_alpha = levels[idx];
                    ui.show_toast(a_string("HUD BG ALPHA: ") + std::to_string((int)ui.hud_bg_alpha).c_str(), 90);
                } else if (m_shift) {
                    // Cycle text color presets
                    const uint32_t colors[] = {0xFFFFFFFFu, 0xFF00FFFFu, 0xFFFFFF00u, 0xFF18FC10u}; // white, cyan, yellow, lime
                    size_t idx = 0;
                    for (; idx < sizeof(colors)/sizeof(colors[0]); ++idx) if (colors[idx] == ui.hud_text_color) break;
                    if (idx >= sizeof(colors)/sizeof(colors[0])) idx = 0;
                    idx = (idx + 1) % (sizeof(colors)/sizeof(colors[0]));
                    ui.hud_text_color = colors[idx];
                    ui.show_toast(a_string("HUD COLOR CHANGED"), 90);
                } else {
                    // Toggle HUD on/off
                    ui.hud_show = !ui.hud_show;
                    ui.show_toast(ui.hud_show ? a_string("HUD: ON") : a_string("HUD: OFF"), 90);
                }
                last_f9_time = now_time;  // 딜레이 타이머 갱신
                prev_keys[66] = false; // consume edge
            }
            
            // F1: TURBO 모드 (최고속도) - F10에서 이동
            if (prev_keys[58]) {
                auto now_turbo = clock.now();
                auto elapsed_turbo = std::chrono::duration_cast<std::chrono::milliseconds>(now_turbo - last_speed_change).count();
                if (elapsed_turbo >= 500) {
                    current_speed_index = 7; // TURBO
                    tick_ms = SPEED_LEVELS[7];
                    ui.show_toast(a_string("F1: Game Speed TURBO (Maximum Speed)"), 90);
                    last_speed_change = now_turbo;
                }
                prev_keys[58] = false; // consume edge
            }
            
            // F2: 생산 속도 10배
            if (prev_keys[59]) {
                production_speed_multiplier = 10.0f;
                ui.show_toast(a_string("F2: Production Speed 10x"), 90);
                log("[F2] Production speed set to 10x\n");
                prev_keys[59] = false;
            }

            // 마우스 합성: 공격/빌드 확정 위해 좌클릭 업도 전달, 우클릭은 업 시점만
            int mx = 0, my = 0;
            ui.wnd.get_cursor_pos(&mx, &my);
            
            // UI 영역 체크 (미니맵 및 하단 패널) - ui_custom.h와 동일한 계산
            const int screen_height = ui.rgba_surface ? ui.rgba_surface->h : 480;
            const int panel_height = 220;
            const int ui_panel_y = screen_height - panel_height;
            bool is_ui_area = (my > ui_panel_y);
            
            xy game_pos = { 0, 0 };
            
            // UI 영역이 아닐 때만 game_pos 계산 (크래시 방지)
            if (!is_ui_area) {
                game_pos = { ui.screen_pos.x + mx, ui.screen_pos.y + my };
                // 맵 경계 내로 제한 (크래시 방지)
                int mw = (int)ui.st.game->map_width - 1;
                int mh = (int)ui.st.game->map_height - 1;
                game_pos.x = std::max(0, std::min(game_pos.x, mw));
                game_pos.y = std::max(0, std::min(game_pos.y, mh));
                // update HUD cursor position
                ui.hud_cursor_pos = game_pos;
            }

            // UI 영역이 아닐 때만 마우스 이동 전달
            if (!is_ui_area) {
                input.on_mouse_move(game_pos, xy{mx, my});
            }

            // Build preview update (only when a building is chosen AND not in UI area)
            if (!is_ui_area &&
                (input.command_mode == player_input_handler::CommandMode::Build ||
                 input.command_mode == player_input_handler::CommandMode::AdvancedBuild) &&
                input.building_to_place) {
                input.build_preview_active = true;
                
                // 타일 좌표 계산 (32픽셀 = 1타일)
                size_t tile_x = (size_t)(game_pos.x / 32);
                size_t tile_y = (size_t)(game_pos.y / 32);
                
                // 건물 중심 좌표 계산
                xy center = { int(tile_x*32) + input.building_to_place->placement_size.x/2,
                              int(tile_y*32) + input.building_to_place->placement_size.y/2 };
                
                // 프리뷰 박스는 타일 좌상단 기준으로 표시
                input.build_preview_pos = { int(tile_x*32), int(tile_y*32) };
                auto sel_worker = input.find_selected_worker_any_owner();
                int pid = sel_worker.first;
                unit_t* worker = sel_worker.second;
                bool valid = false;
                bool relaxed_preview = false;
                if (worker) {
                    bool is_zerg = input.actions.unit_is(worker, UnitTypes::Zerg_Drone);
                    if (is_zerg && input.zerg_build_anywhere) valid = true;
                    else {
                        valid = input.actions.can_place_building(worker, pid, input.building_to_place, center, false, false);
                        if (!valid && input.allow_enemy_control) {
                            // 미리보기도 적컨 모드에서는 제약 완화
                            bool r = input.actions.can_place_building(worker, pid, input.building_to_place, center, true, true);
                            if (r) { valid = true; relaxed_preview = true; }
                        }
                    }
                }
                input.build_preview_valid = valid;
                // push to UI overlay
                ui.build_preview_active = true;
                ui.build_preview_pos = input.build_preview_pos;
                ui.build_preview_size = input.building_to_place->placement_size;
                ui.build_preview_valid = valid;
                // 프리뷰 변경 로깅 (타일/유효성 변경시에만)
                xy preview_tile = { (int)tile_x, (int)tile_y };
                if (preview_tile.x != input.last_logged_preview_tile.x ||
                    preview_tile.y != input.last_logged_preview_tile.y ||
                    valid != input.last_logged_preview_valid) {
                    log("[BUILD_PREVIEW] pid=%d allow_enemy=%d zerg_anywhere=%d tile=(%u,%u) center=(%d,%d) bld_id=%d size=(%d,%d) worker=%p worker_owner=%d valid=%d relaxed=%d\n",
                        (int)pid, (int)input.allow_enemy_control, (int)input.zerg_build_anywhere,
                        (unsigned)tile_x, (unsigned)tile_y, center.x, center.y,
                        (int)input.building_to_place->id, (int)input.building_to_place->placement_size.x, (int)input.building_to_place->placement_size.y,
                        (void*)worker, worker ? worker->owner : -1, (int)valid, (int)relaxed_preview);
                    input.last_logged_preview_tile = preview_tile;
                    input.last_logged_preview_valid = valid;
                }
            } else {
                ui.build_preview_active = false;
            }
            
            for (int btn : {1,3}) {
                bool cur = ui.wnd.get_mouse_button_state(btn);
                bool& prv = prev_mouse[btn];
                if (cur != prv) {
                    if (btn == 1) {
                        if (cur) {
                            // 마우스 다운
                            if (!is_ui_area) {
                                input.on_left_button_down(game_pos, xy{mx, my});
                            } else {
                                // UI 영역에서는 드래그 상태 강제 초기화
                                input.left_button_down = false;
                                input.is_dragging = false;
                                log("[UI] 마우스 다운 UI 영역: (%d,%d)\n", mx, my);
                            }
                        } else {
                            // 마우스 업
                            log("[MOUSE_UP] is_ui_area=%d, game_pos=(%d,%d), cmd_mode=%d\n", 
                                (int)is_ui_area, game_pos.x, game_pos.y, (int)input.command_mode);
                            if (!is_ui_area) {
                                log("[MOUSE_UP] 게임 영역 - on_left_button_up 호출\n");
                                input.on_left_button_up(game_pos);
                                ui.click_indicator_pos = game_pos;
                                ui.click_indicator_frames = 12;
                            } else {
                                // UI 영역에서 마우스 업: Wireframe, 대기열, 명령 버튼 클릭 체크
                                log("[UI] 마우스 업 UI 영역: (%d,%d)\n", mx, my);
                                
                                // Shift/Ctrl 상태 확인
                                bool is_shift = ui.wnd.get_key_state(225) || ui.wnd.get_key_state(229);
                                bool is_ctrl = ui.wnd.get_key_state(224) || ui.wnd.get_key_state(228);
                                
                                // 선택된 유닛 수 확인
                                int total_selected = 0;
                                if (input.allow_enemy_control) {
                                    for (int p = 0; p < 8; ++p) {
                                        total_selected += ui.action_st.selection[p].size();
                                    }
                                } else {
                                    total_selected = ui.action_st.selection[0].size();
                                }
                                log("[UI] 선택된 유닛 수: %d\n", total_selected);
                                
                                // 먼저 Wireframe 클릭 체크 (2개 이상 선택 시만)
                                int wireframe_slot = -1;
                                if (total_selected >= 2) {
                                    wireframe_slot = get_wireframe_slot_at(mx, my);
                                    log("[UI] Wireframe 체크 결과: slot=%d\n", wireframe_slot);
                                } else {
                                    log("[UI] Wireframe 체크 스킵 (선택 유닛 %d개)\n", total_selected);
                                }
                                if (wireframe_slot >= 0) {
                                    log("[UI] Wireframe 클릭 감지: slot=%d, shift=%d, ctrl=%d\n", 
                                        wireframe_slot, (int)is_shift, (int)is_ctrl);
                                    
                                    int pid = input.get_effective_owner();
                                    
                                    // 현재 선택된 유닛 목록 가져오기 (UI와 동일한 순서로)
                                    // UI는 st.visible_units 순서로 wireframe을 그리므로, 여기서도 동일하게 해야 함
                                    a_vector<unit_t*> selected_units;
                                    for (unit_t* u : ptr(ui.st.visible_units)) {
                                        if (u && u->sprite && ui.current_selection_is_selected(u)) {
                                            selected_units.push_back(u);
                                            if (selected_units.size() >= 24) break;  // 최대 24개
                                        }
                                    }
                                    
                                    // Wireframe 슬롯에 해당하는 유닛 찾기
                                    if (wireframe_slot < (int)selected_units.size()) {
                                        unit_t* clicked_unit = selected_units[wireframe_slot];
                                        
                                        if (is_ctrl) {
                                            // Ctrl + 클릭: 같은 타입만 선택 (다른 타입 제거)
                                            log("[WIREFRAME] Ctrl+클릭: 같은 타입만 선택 (type=%d)\n", 
                                                (int)clicked_unit->unit_type->id);
                                            
                                            // 같은 타입 유닛을 플레이어별로 분류
                                            std::map<int, a_vector<unit_t*>> same_type_by_player;
                                            for (unit_t* u : selected_units) {
                                                if (u->unit_type == clicked_unit->unit_type) {
                                                    same_type_by_player[u->owner].push_back(u);
                                                }
                                            }
                                            
                                            if (input.allow_enemy_control) {
                                                // 적군 컨트롤 모드: 모든 플레이어의 선택 초기화 후 같은 타입만 선택
                                                for (int p = 0; p < 8; ++p) {
                                                    ui.action_st.selection[p].clear();
                                                }
                                                for (auto& [p, units] : same_type_by_player) {
                                                    input.actions.action_select(p, units);
                                                }
                                            } else {
                                                // 일반 모드
                                                input.actions.action_select(pid, same_type_by_player[pid]);
                                            }
                                            
                                            // UI current_selection 업데이트
                                            ui.current_selection_clear();
                                            int total_count = 0;
                                            for (auto& [p, units] : same_type_by_player) {
                                                for (unit_t* u : units) {
                                                    ui.current_selection_add(u);
                                                    total_count++;
                                                }
                                            }
                                            
                                            if (input.toast_cb) {
                                                char msg[64];
                                                snprintf(msg, sizeof(msg), "%d units selected", total_count);
                                                input.toast_cb(msg, 30);
                                            }
                                        } else if (is_shift) {
                                            // Shift + 클릭: 해당 유닛을 선택에서 제거
                                            log("[WIREFRAME] Shift+클릭: 유닛 제거 (type=%d, owner=%d)\n", 
                                                (int)clicked_unit->unit_type->id, clicked_unit->owner);
                                            
                                            // 해당 유닛의 소유자에서만 제거
                                            input.actions.action_deselect(clicked_unit->owner, clicked_unit);
                                            
                                            // UI current_selection도 업데이트
                                            ui.current_selection_remove(clicked_unit);
                                            
                                            if (input.toast_cb) input.toast_cb("Unit removed", 20);
                                        } else {
                                            // 일반 클릭: 해당 유닛만 선택
                                            log("[WIREFRAME] 일반 클릭: 유닛 단독 선택 (type=%d, owner=%d)\n", 
                                                (int)clicked_unit->unit_type->id, clicked_unit->owner);
                                            
                                            if (input.allow_enemy_control) {
                                                // 적군 컨트롤 모드: 모든 플레이어의 선택 초기화 후 해당 유닛만 선택
                                                for (int p = 0; p < 8; ++p) {
                                                    ui.action_st.selection[p].clear();
                                                }
                                                input.actions.action_select(clicked_unit->owner, clicked_unit);
                                            } else {
                                                input.actions.action_select(pid, clicked_unit);
                                            }
                                            
                                            // UI current_selection 업데이트
                                            ui.current_selection_clear();
                                            ui.current_selection_add(clicked_unit);
                                            
                                            if (input.toast_cb) input.toast_cb("Unit selected", 20);
                                        }
                                        
                                        // 선택 동기화 스킵 (다음 2프레임)
                                        input.skip_selection_sync_frames = 2;
                                    }
                                } else {
                                    // Wireframe이 아니면 대기열 클릭 체크
                                    int queue_slot = get_queue_slot_at(mx, my);
                                    if (queue_slot >= 0) {
                                    log("[UI] 대기열 칸 클릭 감지: slot=%d, ui=(%d,%d)\n", queue_slot, mx, my);
                                    
                                    // 선택된 건물 가져오기
                                    int pid = input.get_effective_owner();
                                    unit_t* selected = nullptr;
                                    if (input.allow_enemy_control) {
                                        for (int p = 0; p < 8; ++p) {
                                            if (!ui.action_st.selection[p].empty()) {
                                                selected = ui.action_st.selection[p][0];
                                                pid = p;
                                                break;
                                            }
                                        }
                                    } else {
                                        if (!ui.action_st.selection[pid].empty()) {
                                            selected = ui.action_st.selection[pid][0];
                                        }
                                    }
                                    
                                    // 건물이고 대기열이 있는지 확인
                                    if (selected && (selected->unit_type->flags & unit_type_t::flag_building)) {
                                        if (queue_slot < (int)selected->build_queue.size()) {
                                            log("[QUEUE_CANCEL] 대기열 %d번 취소 시도: pid=%d, building_type=%d\n", 
                                                queue_slot, pid, (int)selected->unit_type->id);
                                            
                                            // 대기열 취소
                                            bool result = input.actions.action_cancel_build_queue(pid, queue_slot);
                                            log("[QUEUE_CANCEL] 취소 결과: %d\n", (int)result);
                                            
                                            if (result && input.toast_cb) {
                                                input.toast_cb("Training canceled", 30);
                                            }
                                        } else {
                                            log("[QUEUE_CANCEL] 빈 칸 클릭: slot=%d, queue_size=%d\n", 
                                                queue_slot, (int)selected->build_queue.size());
                                        }
                                    } else {
                                        log("[QUEUE_CANCEL] 건물이 선택되지 않음 또는 대기열 없음\n");
                                    }
                                    } else {
                                        // 대기열도 아니면 명령 버튼 체크
                                        int cmd_btn = get_command_button_at(mx, my);
                                        if (cmd_btn >= 0) {
                                            log("[UI] 명령 버튼 클릭 감지: %d\n", cmd_btn);
                                            input.on_command_button_click(cmd_btn);
                                        } else {
                                            log("[UI] 모든 UI 영역 밖 클릭\n");
                                        }
                                    }
                                }
                                
                                // 드래그 상태 초기화
                                input.left_button_down = false;
                                input.is_dragging = false;
                            }
                        }
                    } else if (btn == 3) {
                        // 우클릭 업 시점에만 처리 (UI와 충돌 최소화)
                        if (!cur && !is_ui_area) {
                            input.on_right_click(game_pos);
                            ui.click_indicator_pos = game_pos;
                            ui.click_indicator_frames = 12;
                        }
                    }
                    prv = cur;
                }
            }
        }
    }
    
    bool is_running() {
        return (bool)ui.wnd;
    }
};

// 스타크래프트 팔레트 로드 (전역)
bwgame::a_vector<uint8_t> g_sc_palette;

void load_sc_palette() {
    // 스타크래프트 팔레트 파일 로드
    auto load_data_file = bwgame::data_loading::data_files_directory(".");
    try {
        load_data_file(g_sc_palette, "Tileset/badlands.wpe");
        if (g_sc_palette.size() == 256 * 4) {
            log("스타크래프트 팔레트 로드 성공 (256색)\n");
        }
    } catch (...) {
        log("팔레트 로드 실패 - 기본 팔레트 사용\n");
    }
}

// 프로토스 아이콘 PNG 이미지 로드 함수
void load_protoss_icons() {
    #ifndef OPENBW_NO_SDL_IMAGE
    log("=== 프로토스 아이콘 로드 시작 ===\n");
    
    // 팔레트 먼저 로드
    if (g_sc_palette.empty()) {
        load_sc_palette();
    }
    
    // IMG_Init 호출
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log("SDL_image 초기화 실패: %s\n", IMG_GetError());
        return;
    }
    
    // PNG 파일 로드 (images 폴더에서)
    const char* paths[] = {
        "images/protoss_icons.png",
        "game_integrated/images/protoss_icons.png",
        "./images/protoss_icons.png",
        "./game_integrated/images/protoss_icons.png",
        "game_integrated\\images\\protoss_icons.png"
    };
    
    SDL_Surface* surf = nullptr;
    for (const char* path : paths) {
        log("경로 시도: %s\n", path);
        surf = IMG_Load(path);
        if (surf) {
            log("✓ 프로토스 아이콘 로드 성공: %s\n", path);
            break;
        } else {
            log("  실패: %s\n", IMG_GetError());
        }
    }
    
    if (!surf) {
        log("✗ 프로토스 아이콘 로드 실패 - 모든 경로 실패\n");
        return;
    }
    
    g_protoss_icon_width = surf->w;
    g_protoss_icon_height = surf->h;
    g_protoss_icon_rgba.resize(g_protoss_icon_width * g_protoss_icon_height);
    
    log("이미지 크기: %dx%d\n", g_protoss_icon_width, g_protoss_icon_height);
    
    // SDL_GetRGBA를 사용하여 정확한 색상 추출
    SDL_LockSurface(surf);
    SDL_PixelFormat* fmt = surf->format;
    uint8_t* pixels = (uint8_t*)surf->pixels;
    
    log("이미지 포맷: BPP=%d, Rmask=0x%08X, Gmask=0x%08X, Bmask=0x%08X, Amask=0x%08X\n",
        fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
    
    for (int y = 0; y < g_protoss_icon_height; ++y) {
        for (int x = 0; x < g_protoss_icon_width; ++x) {
            uint32_t pixel;
            uint8_t* p = pixels + y * surf->pitch + x * fmt->BytesPerPixel;
            
            switch (fmt->BytesPerPixel) {
                case 1: pixel = *p; break;
                case 2: pixel = *(uint16_t*)p; break;
                case 3:
                    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    pixel = p[0] << 16 | p[1] << 8 | p[2];
                    #else
                    pixel = p[0] | p[1] << 8 | p[2] << 16;
                    #endif
                    break;
                case 4: pixel = *(uint32_t*)p; break;
                default: pixel = 0; break;
            }
            
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, fmt, &r, &g, &b, &a);
            
            // 검정색 배경은 투명 처리 (PNG 알파가 없는 경우)
            if (fmt->Amask == 0 && r < 10 && g < 10 && b < 10) {
                a = 0;
            }
            
            // ARGB 형식으로 저장 (우리 렌더러가 사용하는 형식)
            g_protoss_icon_rgba[y * g_protoss_icon_width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    
    g_protoss_icons_loaded = true;
    log("프로토스 아이콘 로드 완료! (RGBA, %d 픽셀)\n", (int)g_protoss_icon_rgba.size());
    #else
    log("SDL_image가 비활성화되어 있습니다.\n");
    #endif
}

// Armor 툴팁 이미지 로드 함수
void load_armor_tooltip() {
    #ifndef OPENBW_NO_SDL_IMAGE
    log("=== Armor 툴팁 이미지 로드 시작 ===\n");
    
    // IMG_Init 호출 (이미 초기화되어 있을 수 있음)
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log("SDL_image 초기화 실패: %s\n", IMG_GetError());
        return;
    }
    
    // PNG 파일 로드 (images 폴더에서)
    const char* paths[] = {
        "images/p armor 1-1.png",
        "game_integrated/images/p armor 1-1.png",
        "./images/p armor 1-1.png",
        "./game_integrated/images/p armor 1-1.png",
        "game_integrated\\images\\p armor 1-1.png"
    };
    
    SDL_Surface* surf = nullptr;
    for (const char* path : paths) {
        log("경로 시도: %s\n", path);
        surf = IMG_Load(path);
        if (surf) {
            log("✓ Armor 툴팁 로드 성공: %s\n", path);
            break;
        } else {
            log("  실패: %s\n", IMG_GetError());
        }
    }
    
    if (!surf) {
        log("✗ Armor 툴팁 로드 실패 - 모든 경로 실패\n");
        return;
    }
    
    g_armor_tooltip_width = surf->w;
    g_armor_tooltip_height = surf->h;
    g_armor_tooltip_rgba.resize(g_armor_tooltip_width * g_armor_tooltip_height);
    
    log("Armor 툴팁 크기: %dx%d\n", g_armor_tooltip_width, g_armor_tooltip_height);
    
    // SDL_GetRGBA를 사용하여 정확한 색상 추출
    SDL_LockSurface(surf);
    SDL_PixelFormat* fmt = surf->format;
    uint8_t* pixels = (uint8_t*)surf->pixels;
    
    for (int y = 0; y < g_armor_tooltip_height; ++y) {
        for (int x = 0; x < g_armor_tooltip_width; ++x) {
            uint32_t pixel;
            uint8_t* p = pixels + y * surf->pitch + x * fmt->BytesPerPixel;
            
            switch (fmt->BytesPerPixel) {
                case 1: pixel = *p; break;
                case 2: pixel = *(uint16_t*)p; break;
                case 3:
                    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    pixel = p[0] << 16 | p[1] << 8 | p[2];
                    #else
                    pixel = p[0] | p[1] << 8 | p[2] << 16;
                    #endif
                    break;
                case 4: pixel = *(uint32_t*)p; break;
                default: pixel = 0; break;
            }
            
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, fmt, &r, &g, &b, &a);
            
            // ARGB 형식으로 저장
            g_armor_tooltip_rgba[y * g_armor_tooltip_width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    
    g_armor_tooltip_loaded = true;
    log("Armor 툴팁 로드 완료! (RGBA, %d 픽셀)\n", (int)g_armor_tooltip_rgba.size());
    #else
    log("SDL_image가 비활성화되어 있습니다.\n");
    #endif
}

// Shield 툴팁 이미지 로드 함수
void load_shield_tooltip() {
    #ifndef OPENBW_NO_SDL_IMAGE
    log("=== Shield 툴팁 이미지 로드 시작 ===\n");
    
    // IMG_Init 호출 (이미 초기화되어 있을 수 있음)
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log("SDL_image 초기화 실패: %s\n", IMG_GetError());
        return;
    }
    
    // PNG 파일 로드 (images 폴더에서)
    const char* paths[] = {
        "images/p plasma shields 1 .png",
        "game_integrated/images/p plasma shields 1 .png",
        "./images/p plasma shields 1 .png",
        "./game_integrated/images/p plasma shields 1 .png",
        "game_integrated\\images\\p plasma shields 1 .png"
    };
    
    SDL_Surface* surf = nullptr;
    for (const char* path : paths) {
        log("경로 시도: %s\n", path);
        surf = IMG_Load(path);
        if (surf) {
            log("✓ Shield 툴팁 로드 성공: %s\n", path);
            break;
        } else {
            log("  실패: %s\n", IMG_GetError());
        }
    }
    
    if (!surf) {
        log("✗ Shield 툴팁 로드 실패 - 모든 경로 실패\n");
        return;
    }
    
    g_shield_tooltip_width = surf->w;
    g_shield_tooltip_height = surf->h;
    g_shield_tooltip_rgba.resize(g_shield_tooltip_width * g_shield_tooltip_height);
    
    log("Shield 툴팁 크기: %dx%d\n", g_shield_tooltip_width, g_shield_tooltip_height);
    
    // SDL_GetRGBA를 사용하여 정확한 색상 추출
    SDL_LockSurface(surf);
    SDL_PixelFormat* fmt = surf->format;
    uint8_t* pixels = (uint8_t*)surf->pixels;
    
    for (int y = 0; y < g_shield_tooltip_height; ++y) {
        for (int x = 0; x < g_shield_tooltip_width; ++x) {
            uint32_t pixel;
            uint8_t* p = pixels + y * surf->pitch + x * fmt->BytesPerPixel;
            
            switch (fmt->BytesPerPixel) {
                case 1: pixel = *p; break;
                case 2: pixel = *(uint16_t*)p; break;
                case 3:
                    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    pixel = p[0] << 16 | p[1] << 8 | p[2];
                    #else
                    pixel = p[0] | p[1] << 8 | p[2] << 16;
                    #endif
                    break;
                case 4: pixel = *(uint32_t*)p; break;
                default: pixel = 0; break;
            }
            
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, fmt, &r, &g, &b, &a);
            
            // ARGB 형식으로 저장
            g_shield_tooltip_rgba[y * g_shield_tooltip_width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    
    g_shield_tooltip_loaded = true;
    log("Shield 툴팁 로드 완료! (RGBA, %d 픽셀)\n", (int)g_shield_tooltip_rgba.size());
    #else
    log("SDL_image가 비활성화되어 있습니다.\n");
    #endif
}

int main(int argc, char** argv) {
    install_crash_handlers();
    log("=== OpenBW 완전 플레이 가능 게임 ===\n");
    
#ifdef EMSCRIPTEN
    // Web build: MPQs are mounted by the browser after user interaction.
    // Do not start game here; JS calls openbw_web_start() after mounting.
    (void)argc;
    (void)argv;
    log("[WEB] Waiting for MPQ upload. Call openbw_web_start('/data') from JS.\n");
    return 0;
#else
    auto load_data_file = data_loading::data_files_directory(".");

    log("게임 플레이어 초기화 중...\n");
    game_player player(load_data_file);
    
    // 프로토스 아이콘 이미지 로드
    log("프로토스 아이콘 로드 중...\n");
    load_protoss_icons();
    
    // Armor 툴팁 이미지 로드
    log("Armor 툴팁 로드 중...\n");
    load_armor_tooltip();
    
    // Shield 툴팁 이미지 로드
    log("Shield 툴팁 로드 중...\n");
    load_shield_tooltip();
    
    try {
        log("맵 로딩 중...\n");
        player.load_map_file("maps/(2)Astral Balance.scm", false);
        log("맵 로딩 성공!\n");
        
        auto& st = player.st();
        
        // 플레이어 설정
        st.players[0].controller = player_t::controller_occupied;
        st.players[0].race = race_t::protoss;
        st.current_minerals[0] = 5000;
        st.current_gas[0] = 2000;
        
        st.players[1].controller = player_t::controller_occupied;
        st.players[1].race = race_t::zerg;
        st.current_minerals[1] = 5000;
        st.current_gas[1] = 2000;
        
        log("플레이어 설정 완료!\n");
        
    } catch (const std::exception& e) {
        log("맵 로딩 실패: %s\n", e.what());
        return 1;
    }
    
    log("UI 초기화 중...\n");
    player_game game(std::move(player));
    
    try {
        game.ui.init();
        log("UI 초기화 성공!\n");
    } catch (const std::exception& e) {
        log("UI 초기화 실패: %s\n", e.what());
        return 1;
    }
    
    game.ui.set_image_data();
    
    // 유닛 생성
    state_functions funcs(game.ui.st);
    
    // ★ 아군 넥서스 위치 (지정된 좌표)
    xy nexus_pos = {2072, 2646};
    unit_t* nexus = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Nexus), nexus_pos, 0);
    if (nexus) {
        funcs.finish_building_unit(nexus);
        funcs.complete_unit(nexus);
        log("넥서스 생성 성공: (%d, %d)\n", nexus_pos.x, nexus_pos.y);
    }
    
    // ★ 프로브 4기 생성 (지정된 좌표)
    xy probe_base = {2280, 2620};
    int created = 0;
    xy probe_offsets[] = { xy{0,0}, xy{40,0}, xy{80,0}, xy{120,0}, xy{0,40}, xy{40,40}, xy{80,40}, xy{120,40} };
    for (auto off : probe_offsets) {
        if (created >= 4) break;
        xy pos = probe_base + off;
        unit_t* probe = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Probe), pos, 0);
        if (probe) {
            funcs.finish_building_unit(probe);
            funcs.complete_unit(probe);
            if (probe->sprite) probe->sprite->flags &= ~sprite_t::flag_hidden;
            log("프로브 생성 성공: (%d, %d)\n", pos.x, pos.y);
            created++;
        } else {
            log("프로브 생성 실패: (%d, %d)\n", pos.x, pos.y);
        }
    }
    
    // 첫 프로브 자동 선택 (테스트 편의)
    if (!game.ui.action_st.selection[0].empty()) game.ui.action_st.selection[0].clear();
    for (auto* u : ptr(game.ui.st.visible_units)) {
        if (u->unit_type->id == UnitTypes::Protoss_Probe && u->owner == 0) {
            game.ui.action_st.selection[0].push_back(u);
            break;
        }
    }
    
    // ★ 적군 해처리 위치 (지정된 좌표)
    xy hatchery_pos = {2983, 2523};
    unit_t* hatchery = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Hatchery), hatchery_pos, 1);
    if (hatchery) {
        funcs.finish_building_unit(hatchery);
        funcs.complete_unit(hatchery);
        log("해처리 생성 성공: (%d, %d)\n", hatchery_pos.x, hatchery_pos.y);
    }
    
    // 드론 20기 생성 (해처리 주변)
    {
        int created_e = 0;
        xy base = hatchery_pos;
        // 맵 경계 보정
        if (base.x < 0) base.x = 0;
        if (base.y < 0) base.y = 0;
        if (base.x >= (int)game.ui.st.game->map_width) base.x = game.ui.st.game->map_width - 64;
        if (base.y >= (int)game.ui.st.game->map_height) base.y = game.ui.st.game->map_height - 64;

        log("[DRONE] 목표 기준점 base=(%d,%d)\n", base.x, base.y);

        // 드론 20기 생성 (해처리 주변) - 더 넓은 영역에 배치
        xy drone_offsets[] = { 
            // 1열 (가까운 곳)
            xy{-80,120}, xy{-40,160}, xy{0,200}, xy{40,160}, xy{80,120},
            // 2열 (중간)
            xy{-120,200}, xy{-80,240}, xy{-40,280}, xy{0,320}, xy{40,280}, xy{80,240}, xy{120,200},
            // 3열 (먼 곳)
            xy{-160,240}, xy{-120,280}, xy{-80,320}, xy{-40,360}, xy{0,400}, xy{40,360}, xy{80,320}, xy{120,280}
        };
        for (auto off : drone_offsets) {
            if (created_e >= 20) break;
            xy pos = base + off;
            unit_t* drone = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Drone), pos, 1);
            if (drone) {
                funcs.finish_building_unit(drone);
                funcs.complete_unit(drone);
                if (drone->sprite) drone->sprite->flags &= ~sprite_t::flag_hidden;
                log("드론 생성 성공: (%d, %d)\n", pos.x, pos.y);
                ++created_e;
            } else {
                log("드론 생성 실패: (%d, %d)\n", pos.x, pos.y);
            }
        }
        log("[DRONE] 최종 생성 수: %d\n", created_e);

        // 검증 편의를 위해 카메라를 잠시 드론 생성 지점으로 이동
        if (created_e > 0) {
            game.ui.screen_pos = {base.x - 400, base.y - 300};
            if (game.ui.screen_pos.x < 0) game.ui.screen_pos.x = 0;
            if (game.ui.screen_pos.y < 0) game.ui.screen_pos.y = 0;
        }
    }
    
    game.ui.resize(1280, 800);
    game.ui.screen_pos = xy{nexus_pos.x - 640, nexus_pos.y - 400};
    
    log("\n=== 게임 시작! ===\n");
    log("조작법:\n");
    log("  좌클릭: 유닛 선택\n");
    log("  우클릭: 이동/공격\n");
    log("  드래그: 다중 선택\n");
    log("  A + 클릭: 강제 공격\n");
    log("  M + 클릭: 이동\n");
    log("  P + 클릭: 정찰\n");
    log("  S: 정지\n");
    log("  H: 홀드\n");
    log("  1-9: 컨트롤 그룹\n");
    log("  ESC: 종료\n");
    log("\n");
    
    int frame_count = 0;

    while (game.is_running()) {
        try {
            game.update();
            frame_count++;

            if (frame_count % 100 == 0) {
                int sel_count = 0;
                int unit_count = 0;
                int building_count = 0;
                int resource_count = 0;  // 자원은 선택 불가하므로 항상 0

                if (game.input.allow_enemy_control) {
                    for (int pid = 0; pid < 8; ++pid) {
                        for (unit_t* u : game.ui.action_st.selection[pid]) {
                            if (u) {
                                sel_count++;
                                if (u->unit_type->flags & unit_type_t::flag_building) building_count++;
                                else unit_count++;
                            }
                        }
                    }
                } else {
                    for (unit_t* u : game.ui.action_st.selection[0]) {
                        if (u) {
                            sel_count++;
                            if (u->unit_type->flags & unit_type_t::flag_building) building_count++;
                            else unit_count++;
                        }
                    }
                }

                log("Frame %d - Minerals: %d, Selected: %d (유닛:%d, 건물:%d, 자원:%d)\n",
                    frame_count,
                    game.ui.st.current_minerals[0],
                    sel_count, unit_count, building_count, resource_count);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        } catch (const std::exception& e) {
            log("[CRASH] 게임 루프 예외: %s\n", e.what());
            // dump selection summary per owner
            if (game.input.allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) {
                    log("[CRASH] sel pid=%d count=%d\n", pid, (int)game.ui.action_st.selection[pid].size());
                }
            } else {
                log("[CRASH] sel pid=0 count=%d\n", (int)game.ui.action_st.selection[0].size());
            }
            log("[CRASH] command_mode=%d allow_enemy=%d\n", (int)game.input.command_mode, (int)game.input.allow_enemy_control);
            break;
        }
    }

    log("\n=== 게임 종료 ===\n");
    log("총 프레임: %d\n", frame_count);

    return 0;
#endif
}

#ifdef EMSCRIPTEN
extern "C" {
static player_game* g_web_game = nullptr;
static int g_web_frame_count = 0;

static void web_tick() {
    try {
        if (!g_web_game || !g_web_game->is_running()) {
            emscripten_cancel_main_loop();
            return;
        }
        g_web_game->update();
        ++g_web_frame_count;
    } catch (const std::exception& e) {
        log("[CRASH] 게임 루프 예외: %s\n", e.what());
        emscripten_cancel_main_loop();
    } catch (...) {
        log("[CRASH] 게임 루프 알 수 없는 예외\n");
        emscripten_cancel_main_loop();
    }
}

// Called from JavaScript after MPQs are mounted into the Emscripten virtual filesystem.
// Returns 0 on success, non-zero on immediate failure.
int openbw_web_start(const char* data_dir) {
    try {
        if (g_web_game) return 0;

        a_string dir = data_dir && *data_dir ? a_string(data_dir) : a_string("/data");
        auto load_data_file = data_loading::data_files_directory(dir);

        log("[WEB] Starting game with data dir: %s\n", dir.c_str());

#ifdef EMSCRIPTEN
        // Many existing loaders use relative paths like "./Patch_rt.mpq".
        // Ensure those resolve against the user-provided mounted directory.
        if (chdir(dir.c_str()) != 0) {
            log("[WEB] chdir(%s) failed: errno=%d\n", dir.c_str(), errno);
        } else {
            log("[WEB] chdir OK\n");
        }
#endif

        // Preflight: verify MPQ files are visible to stdio (MEMFS) before heavy init.
        try {
            a_string test_path = dir;
            if (!test_path.empty() && test_path[test_path.size() - 1] != '/' && test_path[test_path.size() - 1] != '\\') test_path += '/';
            test_path += "Patch_rt.mpq";
            log("[WEB] Preflight open: %s\n", test_path.c_str());
            data_loading::file_reader<> fr(test_path);
            (void)fr;
            log("[WEB] Preflight open OK\n");
        } catch (const std::exception& e) {
            log("[WEB] Preflight open FAILED: %s\n", e.what());
        }
        game_player player(load_data_file);

        // Optional UI images; safe if not present.
        log("[WEB] Loading UI images...\n");
        load_protoss_icons();
        load_armor_tooltip();
        load_shield_tooltip();

        log("[WEB] Loading map...\n");
        player.load_map_file("maps/(2)Astral Balance.scm", false);

        log("[WEB] Initializing UI...\n");
        g_web_game = new player_game(std::move(player));
        g_web_game->ui.init();
        g_web_game->ui.set_image_data();

        state_functions funcs(g_web_game->ui.st);

        xy nexus_pos = {2072, 2646};
        unit_t* nexus = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Nexus), nexus_pos, 0);
        if (nexus) {
            funcs.finish_building_unit(nexus);
            funcs.complete_unit(nexus);
            log("[WEB] 넥서스 생성 성공: (%d, %d)\n", nexus_pos.x, nexus_pos.y);
        }

        xy probe_base = {2280, 2620};
        int created = 0;
        xy probe_offsets[] = { xy{0,0}, xy{40,0}, xy{80,0}, xy{120,0}, xy{0,40}, xy{40,40}, xy{80,40}, xy{120,40} };
        for (auto off : probe_offsets) {
            if (created >= 4) break;
            xy pos = probe_base + off;
            unit_t* probe = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Probe), pos, 0);
            if (probe) {
                funcs.finish_building_unit(probe);
                funcs.complete_unit(probe);
                if (probe->sprite) probe->sprite->flags &= ~sprite_t::flag_hidden;
                log("[WEB] 프로브 생성 성공: (%d, %d)\n", pos.x, pos.y);
                created++;
            } else {
                log("[WEB] 프로브 생성 실패: (%d, %d)\n", pos.x, pos.y);
            }
        }

        if (!g_web_game->ui.action_st.selection[0].empty()) g_web_game->ui.action_st.selection[0].clear();
        for (auto* u : ptr(g_web_game->ui.st.visible_units)) {
            if (u->unit_type->id == UnitTypes::Protoss_Probe && u->owner == 0) {
                g_web_game->ui.action_st.selection[0].push_back(u);
                break;
            }
        }

        xy hatchery_pos = {2983, 2523};
        unit_t* hatchery = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Hatchery), hatchery_pos, 1);
        if (hatchery) {
            funcs.finish_building_unit(hatchery);
            funcs.complete_unit(hatchery);
            log("[WEB] 해처리 생성 성공: (%d, %d)\n", hatchery_pos.x, hatchery_pos.y);
        }

        {
            int created_e = 0;
            xy base = hatchery_pos;
            if (base.x < 0) base.x = 0;
            if (base.y < 0) base.y = 0;
            if (base.x >= (int)g_web_game->ui.st.game->map_width) base.x = g_web_game->ui.st.game->map_width - 64;
            if (base.y >= (int)g_web_game->ui.st.game->map_height) base.y = g_web_game->ui.st.game->map_height - 64;

            log("[WEB][DRONE] 목표 기준점 base=(%d,%d)\n", base.x, base.y);

            xy drone_offsets[] = {
                xy{-80,120}, xy{-40,160}, xy{0,200}, xy{40,160}, xy{80,120},
                xy{-120,200}, xy{-80,240}, xy{-40,280}, xy{0,320}, xy{40,280}, xy{80,240}, xy{120,200},
                xy{-160,240}, xy{-120,280}, xy{-80,320}, xy{-40,360}, xy{0,400}, xy{40,360}, xy{80,320}, xy{120,280}
            };
            for (auto off : drone_offsets) {
                if (created_e >= 20) break;
                xy pos = base + off;
                unit_t* drone = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Drone), pos, 1);
                if (drone) {
                    funcs.finish_building_unit(drone);
                    funcs.complete_unit(drone);
                    if (drone->sprite) drone->sprite->flags &= ~sprite_t::flag_hidden;
                    log("[WEB] 드론 생성 성공: (%d, %d)\n", pos.x, pos.y);
                    ++created_e;
                } else {
                    log("[WEB] 드론 생성 실패: (%d, %d)\n", pos.x, pos.y);
                }
            }
            log("[WEB][DRONE] 최종 생성 수: %d\n", created_e);

            if (created_e > 0) {
                g_web_game->ui.screen_pos = {base.x - 400, base.y - 300};
                if (g_web_game->ui.screen_pos.x < 0) g_web_game->ui.screen_pos.x = 0;
                if (g_web_game->ui.screen_pos.y < 0) g_web_game->ui.screen_pos.y = 0;
            }
        }

        g_web_game->ui.resize(1280, 800);

        g_web_frame_count = 0;
        emscripten_set_main_loop(web_tick, 0, 1);
        return 0;
    } catch (const std::exception& e) {
        log("[WEB] openbw_web_start exception: %s\n", e.what());
        return 1;
    } catch (...) {
        log("[WEB] openbw_web_start unknown exception\n");
        return 1;
    }
}
}
#endif
