// OpenBW 완전 플레이 가능 게임 - 모든 기능 구현

#include "bwgame.h"
#include "actions.h"
#include "ui/native_window.h"
#include "ui_custom.h"  // ★ 원본 UI로 복귀
#include "ui/common.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <functional>
#include <exception>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace bwgame;
using ui::log;

FILE* log_file = nullptr;

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
        Attack,      // A키
        Move,        // M키
        Patrol,      // P키
        Hold,        // H키
        Build        // B키
    };
    CommandMode command_mode = CommandMode::Normal;
    
    // 타임스탬프
    std::chrono::high_resolution_clock::time_point start_time;
    
    player_input_handler(state& st, action_state& action_st) 
        : st(st), action_st(action_st), actions(st, action_st) {
        start_time = std::chrono::high_resolution_clock::now();
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
        if (selection_suppress_cb) selection_suppress_cb(false);
    }

    void set_building_to_place(const unit_type_t* ut) {
        building_to_place = ut;
        if (selection_suppress_cb) selection_suppress_cb(true);
    }
    
    // 이벤트 처리
    void handle_event(const native_window::event_t& e, xy screen_pos);
    
    // 마우스 이벤트
    void on_mouse_move(xy pos, xy screen_mouse_pos);
    void on_left_button_down(xy pos, xy screen_mouse_pos);
    void on_left_button_up(xy pos);
    void on_right_click(xy pos);
    
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
};

void player_input_handler::handle_event(const native_window::event_t& e, xy screen_pos) {
    long long ts = get_timestamp();
    
    switch (e.type) {
        case native_window::event_t::type_mouse_motion:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            mouse_pos = screen_to_game(mouse_screen_pos, screen_pos);
            on_mouse_move(mouse_pos, mouse_screen_pos);
            break;
            
        case native_window::event_t::type_mouse_button_down:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            mouse_pos = screen_to_game(mouse_screen_pos, screen_pos);
            log("[%lld ms] 마우스 DOWN: 버튼=%d, 화면=(%d,%d), screen_pos=(%d,%d), 게임=(%d,%d)\n", 
                ts, e.button, e.mouse_x, e.mouse_y, screen_pos.x, screen_pos.y, mouse_pos.x, mouse_pos.y);
            if (e.button == 1) {
                on_left_button_down(mouse_pos, mouse_screen_pos);
            }
            break;
            
        case native_window::event_t::type_mouse_button_up:
            mouse_screen_pos = xy(e.mouse_x, e.mouse_y);
            mouse_pos = screen_to_game(mouse_screen_pos, screen_pos);
            log("[%lld ms] 마우스 UP: 버튼=%d, 화면=(%d,%d), screen_pos=(%d,%d), 게임=(%d,%d)\n", 
                ts, e.button, e.mouse_x, e.mouse_y, screen_pos.x, screen_pos.y, mouse_pos.x, mouse_pos.y);
            if (e.button == 1) {
                on_left_button_up(mouse_pos);
            } else if (e.button == 3) {
                on_right_click(mouse_pos);
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
    if (left_button_down) {
        if (!is_dragging) {
            // 드래그 시작 위치와 현재 위치의 차이 계산 (게임 좌표 기준)
            int dx = pos.x - drag_start_pos.x;
            int dy = pos.y - drag_start_pos.y;
            int dist_sq = dx * dx + dy * dy;
            
            log("[DEBUG] 드래그 체크: 시작=(%d,%d), 현재=(%d,%d), 거리^2=%d\n", 
                drag_start_pos.x, drag_start_pos.y, pos.x, pos.y, dist_sq);
            
            if (dist_sq > 64) {  // 8픽셀 이상 이동 (64 = 8^2)
                is_dragging = true;
                log(">>> 드래그 시작 확정: 시작=(%d,%d), 현재=(%d,%d)\n", 
                    drag_start_pos.x, drag_start_pos.y, pos.x, pos.y);
            } else {
                // 시간 기반 보조: 150ms 이상 경과했고 최소 이동이 있으면 드래그로 간주
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - drag_start_time).count();
                if (ms > 150 && (dx != 0 || dy != 0)) {
                    is_dragging = true;
                    log(">>> 드래그(시간보조) 시작: dt=%lldms 시작=(%d,%d) 현재=(%d,%d)\n", ms, drag_start_pos.x, drag_start_pos.y, pos.x, pos.y);
                }
            }
        }
    }
}

void player_input_handler::on_left_button_down(xy pos, xy screen_mouse_pos) {
    left_button_down = true;
    drag_start_pos = pos;
    is_dragging = false;
    drag_start_time = std::chrono::high_resolution_clock::now();
}

void player_input_handler::on_left_button_up(xy pos) {
    left_button_down = false;

    // Attack confirm takes precedence
    if (command_mode == CommandMode::Attack) {
        log("[공격확정] 좌클릭 at (%d,%d)\n", pos.x, pos.y);
        unit_t* target = find_unit_at(pos);
        auto issue_attack_for = [&](int pid) {
            if (target) {
                actions.action_order(pid, actions.get_order_type(Orders::AttackDefault), pos, target, nullptr, shift_pressed);
                log("[ATTACK] pid=%d unit-target owner=%d type=%d at (%d,%d)\n", pid, target->owner, (int)target->unit_type->id, target->sprite->position.x, target->sprite->position.y);
            } else {
                actions.action_order(pid, actions.get_order_type(Orders::AttackMove), pos, nullptr, nullptr, shift_pressed);
                log("[ATTACKMOVE] pid=%d to (%d,%d)\n", pid, pos.x, pos.y);
            }
        };
        if (allow_enemy_control) {
            for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) issue_attack_for(pid);
        } else issue_attack_for(player_id);
        command_mode = CommandMode::Normal;
        if (selection_suppress_cb) selection_suppress_cb(false);
        return;
    }
    // Build confirm
    if (command_mode == CommandMode::Build && building_to_place) {
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

    if (is_dragging) {
        // 드래그 선택
        rect area;
        area.from.x = std::min(drag_start_pos.x, pos.x);
        area.from.y = std::min(drag_start_pos.y, pos.y);
        area.to.x = std::max(drag_start_pos.x, pos.x);
        area.to.y = std::max(drag_start_pos.y, pos.y);
        // map bounds clamp
        if (area.from.x < 0) area.from.x = 0;
        if (area.from.y < 0) area.from.y = 0;
        int mw = (int)st.game->map_width - 1;
        int mh = (int)st.game->map_height - 1;
        if (area.to.x > mw) area.to.x = mw;
        if (area.to.y > mh) area.to.y = mh;
        
        log(">>> 드래그 선택: 영역=(%d,%d)-(%d,%d)\n", 
            area.from.x, area.from.y, area.to.x, area.to.y);
        
        select_units_in_rect(area);
        is_dragging = false;
    } else {
        // 단일 클릭 선택
        unit_t* u = find_unit_at(pos);
        if (u) {
            log(">>> 유닛 선택: 소유자=%d, 타입=%d, 위치=(%d,%d)\n", 
                u->owner, (int)u->unit_type->id, u->sprite->position.x, u->sprite->position.y);
            // 원작 규칙: 기본은 아군만 선택. 허용 시 적군도 선택
            if (!allow_enemy_control && u->owner != player_id) {
                log(">>> 적 유닛 클릭 - 정보만 표시, 선택 변경 안함\n");
                return;
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
                else action_st.selection[player_id].clear();
                log(">>> 선택 해제\n");
            }
        }
    }
}

void player_input_handler::on_right_click(xy pos) {
    // 빌드 프리뷰 상태에서 우클릭: 프리뷰/빌드모드 취소 (F5 포함 공통)
    if (command_mode == CommandMode::Build && building_to_place) {
        log("[RC] Cancel build preview by right-click\n");
        if (toast_cb) toast_cb("Build canceled", 60);
        exit_build_mode();
        return;
    }
    // 선택된 유닛 수 확인
    int selected_count = 0;
    if (allow_enemy_control) {
        for (int pid = 0; pid < 8; ++pid) selected_count += (int)action_st.selection[pid].size();
    } else selected_count = (int)action_st.selection[player_id].size();
    
    log("\n[우클릭 디버그 시작]\n");
    log("  위치: 게임=(%d,%d)\n", pos.x, pos.y);
    log("  선택된 유닛: %d개\n", selected_count);
    log("  명령 모드: %d\n", (int)command_mode);
    log("  Shift: %s\n", shift_pressed ? "ON" : "OFF");
    
    // 선택된 유닛이 없으면 명령 무시
    if (selected_count == 0) {
        log("  [경고] 선택된 유닛이 없어 명령 무시\n");
        log("[우클릭 디버그 끝]\n\n");
        return;
    }
    
    // 타겟 유닛 찾기
    unit_t* target = find_unit_at(pos);
    if (target) {
        log("  타겟 유닛: 소유자=%d, 타입=%d, 위치=(%d,%d)\n", 
            target->owner, (int)target->unit_type->id, 
            target->sprite->position.x, target->sprite->position.y);
    } else {
        log("  타겟 유닛: 없음\n");
    }
    
    auto issue_for_player = [&](int pid) {
        switch (command_mode) {
            case CommandMode::Attack:
                log("  [명령] 강제 공격 (A키 모드)\n");
                ui::log("[RC] AttackMove to (%d,%d)\n", pos.x, pos.y);
                actions.action_order(pid, actions.get_order_type(Orders::AttackMove), pos, target, nullptr, shift_pressed);
                break;
            case CommandMode::Move:
                log("  [명령] 강제 이동 (M키 모드)\n");
                ui::log("[RC] Move to (%d,%d)\n", pos.x, pos.y);
                actions.action_order(pid, actions.get_order_type(Orders::Move), pos, nullptr, nullptr, shift_pressed);
                break;
            case CommandMode::Patrol:
                log("  [명령] 정찰 (P키 모드)\n");
                actions.action_order(pid, actions.get_order_type(Orders::Patrol), pos, nullptr, nullptr, shift_pressed);
                break;
            default:
                log("  [명령] 기본 명령 (스마트 명령)\n");
                ui::log("[RC] Default order to (%d,%d)\n", pos.x, pos.y);
                actions.action_default_order(pid, pos, target, nullptr, shift_pressed);
                break;
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
        command_mode = CommandMode::Normal;
    } else {
        // 명령 모드에 따라 다른 동작
        switch (command_mode) {
        case CommandMode::Attack:
            log("  [명령] 강제 공격 (A키 모드)\n");
            ui::log("[RC] AttackMove to (%d,%d)\n", pos.x, pos.y);
            actions.action_order(player_id, actions.get_order_type(Orders::AttackMove), pos, target, nullptr, shift_pressed);
            command_mode = CommandMode::Normal;
            break;
            
        case CommandMode::Move:
            log("  [명령] 강제 이동 (M키 모드)\n");
            ui::log("[RC] Move to (%d,%d)\n", pos.x, pos.y);
            actions.action_order(player_id, actions.get_order_type(Orders::Move), pos, nullptr, nullptr, shift_pressed);
            command_mode = CommandMode::Normal;
            break;
            
        case CommandMode::Patrol:
            log("  [명령] 정찰 (P키 모드)\n");
            actions.action_order(player_id, actions.get_order_type(Orders::Patrol), pos, nullptr, nullptr, shift_pressed);
            command_mode = CommandMode::Normal;
            break;
            
        default:
            log("  [명령] 기본 명령 (스마트 명령)\n");
            ui::log("[RC] Default order to (%d,%d)\n", pos.x, pos.y);
            // 좌표 유효성 검사
            if (pos.x < 0 || pos.y < 0 || pos.x > 32768 || pos.y > 32768) {
                log("  [오류] 잘못된 좌표 감지! 명령 무시\n");
                log("[우클릭 디버그 끝]\n\n");
                return;
            }
            try {
                actions.action_default_order(player_id, pos, target, nullptr, shift_pressed);
            } catch (const std::exception& ex) {
                log("[CRASH_ORDER] default pid=%d ex=%s pos=(%d,%d)\n", player_id, ex.what(), pos.x, pos.y);
            } catch (...) {
                log("[CRASH_ORDER] default pid=%d unknown pos=(%d,%d)\n", player_id, pos.x, pos.y);
            }
            break;
        }
    }
    
    log("[우클릭 디버그 끝]\n\n");
}

void player_input_handler::on_key_down(int scancode) {
    // Shift, Ctrl, Alt
    if (scancode == 225 || scancode == 229) shift_pressed = true;
    if (scancode == 224 || scancode == 228) ctrl_pressed = true;
    if (scancode == 226 || scancode == 230) alt_pressed = true;
    
    // 명령 키
    ui::log("[KEY DOWN] scancode=%d\n", scancode);
    switch (scancode) {
        case 4:  // A - 공격 모드 / (Build) Protoss Assimilator
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Assimilator));
                break;
            }
            command_mode = CommandMode::Attack;
            log(">>> A키: 공격 모드 활성화\n");
            if (selection_suppress_cb) selection_suppress_cb(true);
            break;
            
        case 16:  // M - 이동 모드
            command_mode = CommandMode::Move;
            log(">>> M키: 이동 모드 활성화\n");
            break;
            
        case 19:  // P - 정찰 모드 / (Build) Protoss Pylon
            if (command_mode == CommandMode::Build) {
                if (is_selected_worker_protoss()) set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Pylon));
                break;
            }
            command_mode = CommandMode::Patrol;
            log(">>> P키: 정찰 모드 활성화\n");
            break;
            
        case 22:  // S - 정지 / (Build) Zerg Spawning Pool
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Spawning_Pool));
                break;
            }
            log(">>> S키: 정지 명령\n");
            if (allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) actions.action_stop(pid, shift_pressed);
            } else actions.action_stop(player_id, shift_pressed);
            break;
            
        case 11:  // H - 홀드 / (Build) Zerg Hatchery
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Hatchery));
                break;
            }
            log(">>> H키: 홀드 명령\n");
            if (allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) if (!action_st.selection[pid].empty()) actions.action_hold_position(pid, shift_pressed);
            } else actions.action_hold_position(player_id, shift_pressed);
            break;
            
        case 41:  // ESC - 취소: (0) 프리뷰 취소, (1) 건설 중 건물 취소, (2) 명령 모드 취소
        {
            bool canceled = false;
            // 0) 프리뷰가 켜져 있으면 무조건 우선 취소
            if (command_mode == CommandMode::Build && building_to_place) {
                log(">>> ESC: 빌드 프리뷰 취소\n");
                exit_build_mode();
                break;
            }
            if (allow_enemy_control) {
                for (int pid = 0; pid < 8; ++pid) {
                    if (!action_st.selection[pid].empty()) {
                        if (actions.action_cancel_building_unit(pid)) {
                            log(">>> ESC: pid=%d 건설 취소\n", pid);
                            canceled = true;
                        }
                    }
                }
            } else {
                if (actions.action_cancel_building_unit(player_id)) {
                    log(">>> ESC: 내 건설 취소\n");
                    canceled = true;
                }
            }
            if (canceled) break;
            if (command_mode != CommandMode::Normal) {
                log(">>> ESC: 명령 모드 취소\n");
                if (command_mode == CommandMode::Build) {
                    exit_build_mode();
                } else {
                    command_mode = CommandMode::Normal;
                    if (selection_suppress_cb) selection_suppress_cb(false);
                }
            }
            break;
        }
            
        // 컨트롤 그룹 (1-9)
        case 30: case 31: case 32: case 33: case 34:
        case 35: case 36: case 37: case 38:
            {
                int group = scancode - 30;
                if (ctrl_pressed) {
                    log(">>> Ctrl+%d: 그룹 설정\n", group + 1);
                    actions.action_control_group(player_id, group, 0);
                } else if (shift_pressed) {
                    log(">>> Shift+%d: 그룹에 추가\n", group + 1);
                    actions.action_control_group(player_id, group, 2);
                } else {
                    log(">>> %d: 그룹 선택\n", group + 1);
                    actions.action_control_group(player_id, group, 1);
                }
            }
            break;
            
        // 건물 건설 단축키 (B)
        case 5:  // B
            if (command_mode != CommandMode::Build) {
                // 빌드 모드는 일꾼 선택 상태에서만 진입, 기본 건물 자동선택 없음
                if (is_selected_worker_protoss() || is_selected_worker_zerg()) {
                    log(">>> B키: 건물 모드 진입\n");
                    building_to_place = nullptr;
                    enter_build_mode();
                } else {
                    log(">>> B키: 일꾼이 선택되어 있지 않아 무시\n");
                    if (toast_cb) toast_cb("Select a worker to build", 120);
                }
            } else {
                // 두 번째 B는 아무 것도 자동 선택하지 않음 (원작 UX)
                log(">>> B키: 이미 건물 모드 - 자동 선택 없음\n");
            }
            break;
        case 10: // G -> Gateway (Protoss)
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Gateway));
                break;
            }
            break;
        case 9: // F -> Forge (Protoss)
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Forge));
                break;
            }
            break;
        case 6: // C -> Photon Cannon (Protoss) / Creep Colony (Zerg)
            if (command_mode == CommandMode::Build) {
                if (is_selected_worker_protoss()) set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Photon_Cannon));
                else if (is_selected_worker_zerg()) set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Creep_Colony));
                break;
            }
            break;
        case 27: // X -> Zerg Extractor
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Extractor));
                break;
            }
            break;
        case 28: // Y -> Cybernetics Core (Protoss)
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Cybernetics_Core));
                break;
            }
            break;
        case 21: // R -> Robotics (Protoss) / Spire (Zerg)
            if (command_mode == CommandMode::Build) {
                if (is_selected_worker_protoss()) set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Robotics_Facility));
                else if (is_selected_worker_zerg()) set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Spire));
                break;
            }
            break;
        case 18: // O -> Observatory (Protoss)
            if (command_mode == CommandMode::Build && is_selected_worker_protoss()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Observatory));
                break;
            }
            break;
        case 7: // D -> Citadel of Adun (Protoss) / Hydralisk Den (Zerg)
            if (command_mode == CommandMode::Build) {
                if (is_selected_worker_protoss()) set_building_to_place(actions.get_unit_type(UnitTypes::Protoss_Citadel_of_Adun));
                else if (is_selected_worker_zerg()) set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Hydralisk_Den));
                break;
            }
            break;
        case 8: // E -> Evolution Chamber (Zerg)
            if (command_mode == CommandMode::Build && is_selected_worker_zerg()) {
                set_building_to_place(actions.get_unit_type(UnitTypes::Zerg_Evolution_Chamber));
                break;
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
    }
}

void player_input_handler::on_key_up(int scancode) {
    if (scancode == 225 || scancode == 229) shift_pressed = false;
    if (scancode == 224 || scancode == 228) ctrl_pressed = false;
    if (scancode == 226 || scancode == 230) alt_pressed = false;
}

// (duplicate on_left_button_up removed)

unit_t* player_input_handler::find_unit_at(xy pos) {
    unit_t* best = nullptr;
    int best_dist = 128 * 128;
    
    // Debug tracking for nearest unit regardless of threshold
    unit_t* nearest_any = nullptr;
    int nearest_any_dist = std::numeric_limits<int>::max();
    int nearest_any_radius = 0;
    int nearest_any_max_dist = 0;

    for (unit_t* u : ptr(st.visible_units)) {
        if (u->sprite->flags & sprite_t::flag_hidden) continue;
        
        xy unit_pos = u->sprite->position;
        int dx = pos.x - unit_pos.x;
        int dy = pos.y - unit_pos.y;
        int dist = dx * dx + dy * dy;
        
        int radius = std::max(u->unit_type->dimensions.from.x, u->unit_type->dimensions.from.y);
        int max_dist = (radius + 12) * (radius + 12);
        
        if (dist < nearest_any_dist) {
            nearest_any = u;
            nearest_any_dist = dist;
            nearest_any_radius = radius;
            nearest_any_max_dist = max_dist;
        }

        if (dist < max_dist && dist < best_dist) {
            best = u;
            best_dist = dist;
        }
    }
    
    // Detailed debug logging of selection decision
    if (best) {
        log("[FIND_UNIT_AT] click=(%d,%d) SELECTED: type=%d owner=%d pos=(%d,%d) dims.from=(%d,%d) dims.to=(%d,%d) radius=%d dist=%d max_dist=%d\n",
            pos.x, pos.y,
            (int)best->unit_type->id, best->owner,
            best->sprite->position.x, best->sprite->position.y,
            best->unit_type->dimensions.from.x, best->unit_type->dimensions.from.y,
            best->unit_type->dimensions.to.x, best->unit_type->dimensions.to.y,
            std::max(best->unit_type->dimensions.from.x, best->unit_type->dimensions.from.y),
            best_dist,
            (std::max(best->unit_type->dimensions.from.x, best->unit_type->dimensions.from.y) + 12) * (std::max(best->unit_type->dimensions.from.x, best->unit_type->dimensions.from.y) + 12)
        );
    } else {
        if (nearest_any) {
            log("[FIND_UNIT_AT] click=(%d,%d) NO_SELECT. Nearest: type=%d owner=%d pos=(%d,%d) dims.from=(%d,%d) dims.to=(%d,%d) radius=%d dist=%d max_dist=%d\n",
                pos.x, pos.y,
                (int)nearest_any->unit_type->id, nearest_any->owner,
                nearest_any->sprite->position.x, nearest_any->sprite->position.y,
                nearest_any->unit_type->dimensions.from.x, nearest_any->unit_type->dimensions.from.y,
                nearest_any->unit_type->dimensions.to.x, nearest_any->unit_type->dimensions.to.y,
                nearest_any_radius,
                nearest_any_dist,
                nearest_any_max_dist
            );
        } else {
            log("[FIND_UNIT_AT] click=(%d,%d) NO_SELECT. No visible units.\n", pos.x, pos.y);
        }
    }

    return best;
}

void player_input_handler::select_units_in_rect(rect area) {
    std::vector<unit_t*> units;
    
    for (unit_t* u : ptr(st.visible_units)) {
        if (u->sprite->flags & sprite_t::flag_hidden) continue;
        if (!allow_enemy_control && u->owner != player_id) continue;
        
        xy unit_pos = u->sprite->position;
        if (unit_pos.x >= area.from.x && unit_pos.x <= area.to.x &&
            unit_pos.y >= area.from.y && unit_pos.y <= area.to.y) {
            units.push_back(u);
        }
    }
    
    log(">>> 드래그로 %d개 유닛 선택\n", (int)units.size());
    
    if (!units.empty()) {
        if (allow_enemy_control) {
            clear_all_selections();
            std::array<std::vector<unit_t*>, 8> by_owner;
            for (auto* u : units) if (u) by_owner[u->owner].push_back(u);
            for (int pid = 0; pid < 8; ++pid) if (!by_owner[pid].empty()) {
                if (by_owner[pid].size() > 12) {
                    log("[SELECT_TRIM] pid=%d trimmed %d->12 to avoid overflow\n", pid, (int)by_owner[pid].size());
                    by_owner[pid].resize(12);
                }
                if (shift_pressed) actions.action_shift_select(pid, by_owner[pid]);
                else actions.action_select(pid, by_owner[pid]);
            }
        } else {
            if (shift_pressed) actions.action_shift_select(player_id, units);
            else actions.action_select(player_id, units);
        }
    }
}

// 메인 게임 구조체
struct player_game {
    ui_functions ui;
    player_input_handler input;
    
    std::chrono::high_resolution_clock clock;
    std::chrono::high_resolution_clock::time_point last_tick;
    int tick_ms = 16;
    // 비침투 입력 상태 추적
    std::array<bool, 512> prev_keys{};
    std::array<bool, 6> prev_mouse{};
    
    player_game(game_player player) 
        : ui(std::move(player))
        , input(ui.st, ui.action_st) 
    {
        input.player_id = 0;
        // Connect toast callback to UI toast
        input.toast_cb = [this](const char* s, int frames) {
            ui.show_toast(a_string(s), frames);
        };
        // Connect selection suppression to UI
        input.selection_suppress_cb = [this](bool v) {
            ui.set_selection_suppressed(v);
        };
        
        ui.load_data_file = [](a_vector<uint8_t>& data, a_string filename) {
            data_loading::data_files_directory("")(data, std::move(filename));
        };
        
        ui.global_volume = 50;
    }
    
    void update() {
        auto now = clock.now();
        auto tick_speed = std::chrono::milliseconds(tick_ms);
        
        // 게임 로직 업데이트
        if (now - last_tick >= tick_speed) {
            last_tick = now;
            state_functions funcs(ui.st);
            funcs.next_frame();
            
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
        
        // UI 업데이트 (이벤트 처리 + 렌더링)
        // UI 내부에서 이벤트를 소비하므로 별도 폴링하지 않음
        try {
            ui.update();
        } catch (const std::exception& e) {
            log("[CRASH_UI] ui.update exception: %s\n", e.what());
            return; // skip rest of frame
        } catch (...) {
            log("[CRASH_UI] ui.update unknown exception\n");
            return;
        }

        // 선택 동기화: UI의 현재 선택 -> action_st.selection[*]
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

        // 비침투 입력 합성: UI가 이벤트를 처리한 뒤 상태를 읽어 필요한 것만 전달
        {
            // 키 목록 (SDL scancode)
            static const int keys[] = {
                4, 5, 11, 16, 19, 22, 27, 41, // A,B,H,M,P,S,'X', ESC(41)
                62, 63, 64, 65, // F5, F6, F7(slower), F8(faster)
                30,31,32,33,34,35,36,37,38, // 1..9
                225,229,224,228,226,230 // Shift/Ctrl/Alt 좌우
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

            // F7/F8 speed control on key down edges
            if (prev_keys[64]) { // F7 slower
                int old = tick_ms;
                tick_ms = std::min(100, tick_ms + 5);
                if (tick_ms != old) ui.show_toast(a_string("SPEED: ") + std::to_string(1000 / tick_ms).c_str(), 90);
                prev_keys[64] = false; // consume edge
            }
            if (prev_keys[65]) { // F8 faster
                int old = tick_ms;
                tick_ms = std::max(8, tick_ms - 5);
                if (tick_ms != old) ui.show_toast(a_string("SPEED: ") + std::to_string(1000 / tick_ms).c_str(), 90);
                prev_keys[65] = false; // consume edge
            }

            // 마우스 합성: 공격/빌드 확정 위해 좌클릭 업도 전달, 우클릭은 업 시점만
            int mx = 0, my = 0;
            ui.wnd.get_cursor_pos(&mx, &my);
            xy game_pos = { ui.screen_pos.x + mx, ui.screen_pos.y + my };
            // update HUD cursor position
            ui.hud_cursor_pos = game_pos;

            // 항상 마우스 이동 전달: 드래그 신뢰성 향상
            input.on_mouse_move(game_pos, xy{mx, my});

            // Build preview update (only when a building is chosen)
            if (input.command_mode == player_input_handler::CommandMode::Build && input.building_to_place) {
                input.build_preview_active = true;
                input.build_preview_pos = game_pos;
                size_t tile_x = (size_t)(game_pos.x / 32);
                size_t tile_y = (size_t)(game_pos.y / 32);
                xy center = { int(tile_x*32) + input.building_to_place->placement_size.x/2,
                              int(tile_y*32) + input.building_to_place->placement_size.y/2 };
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
                            input.on_left_button_down(game_pos, xy{mx, my});
                        } else {
                            input.on_left_button_up(game_pos);
                            ui.click_indicator_pos = game_pos;
                            ui.click_indicator_frames = 12;
                        }
                    } else if (btn == 3) {
                        // 우클릭을 DOWN 시점에 처리하여 반응속도 향상
                        if (cur && !input.right_button_down) {
                            input.right_button_down = true;
                            input.on_right_click(game_pos);
                            ui.click_indicator_pos = game_pos;
                            ui.click_indicator_frames = 12;
                        } else if (!cur && input.right_button_down) {
                            // 버튼 해제 시 상태만 정리 (중복 명령 방지)
                            input.right_button_down = false;
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

int main(int argc, char** argv) {
    install_crash_handlers();
    log("=== OpenBW 완전 플레이 가능 게임 ===\n");
    
    auto load_data_file = data_loading::data_files_directory(".");
    
    log("게임 플레이어 초기화 중...\n");
    game_player player(load_data_file);
    
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
    
    xy mineral_pos = {2048, 1536};
    for (auto* u : ptr(game.ui.st.visible_units)) {
        if (u->unit_type->id == UnitTypes::Resource_Mineral_Field) {
            mineral_pos = u->sprite->position;
            break;
        }
    }
    
    xy spawn_pos = mineral_pos + xy{-300, 0};
    unit_t* nexus = nullptr;
    {
        xy try_offsets[] = { xy{-300,0}, xy{-250,100}, xy{-200,-100}, xy{-350,50}, xy{-150,150} };
        for (auto off : try_offsets) {
            xy try_pos = mineral_pos + off;
            nexus = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Nexus), try_pos, 0);
            if (nexus) {
                funcs.finish_building_unit(nexus);
                funcs.complete_unit(nexus);
                log("넥서스 생성 성공!\n");
                spawn_pos = try_pos;
                break;
            }
        }
    }
    if (nexus) {
        // 프로브 4기 생성 (충돌 피하기 위해 넓은 간격)
        int created = 0;
        xy probe_offsets[] = { xy{-80,120}, xy{-40,160}, xy{0,200}, xy{40,160}, xy{80,120}, xy{-120,200}, xy{120,200}, xy{0,240} };
        for (auto off : probe_offsets) {
            if (created >= 4) break;
            xy pos = nexus->sprite->position + off;
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
        if (created < 4) {
            // 추가 시도: 더 많은 반경에서 시도
            for (int r = 260; r <= 360 && created < 4; r += 40) {
                for (int dy = -160; dy <= 200 && created < 4; dy += 40) {
                    xy pos = nexus->sprite->position + xy{r - 300, dy};
                    unit_t* probe = funcs.create_unit(funcs.get_unit_type(UnitTypes::Protoss_Probe), pos, 0);
                    if (probe) {
                        funcs.finish_building_unit(probe);
                        funcs.complete_unit(probe);
                        if (probe->sprite) probe->sprite->flags &= ~sprite_t::flag_hidden;
                        log("프로브 추가 생성 성공: (%d, %d)\n", pos.x, pos.y);
                        created++;
                    }
                }
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
    }
    
    // 적 유닛
    xy enemy_pos = mineral_pos + xy{300, 0};
    unit_t* hatchery = nullptr;
    {
        xy try_offsets_e[] = { xy{300,0}, xy{260,120}, xy{200,-100}, xy{350,50}, xy{180,160} };
        for (auto off : try_offsets_e) {
            xy try_pos = mineral_pos + off;
            hatchery = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Hatchery), try_pos, 1);
            if (hatchery) {
                funcs.finish_building_unit(hatchery);
                funcs.complete_unit(hatchery);
                enemy_pos = try_pos;
                break;
            }
        }
    }
    // 드론은 해처리 주변 충돌을 피하기 위해 '빨간 동그라미' 근처 영역을 스캔하여 생성
    {
        int created_e = 0;
        // 기준점: 미네랄 기준 우하단 넓은 바닥 (스크린샷의 목표 영역)
        xy base = mineral_pos + xy{520, 120};
        // 맵 경계 보정
        if (base.x < 0) base.x = 0;
        if (base.y < 0) base.y = 0;
        if (base.x >= (int)game.ui.st.game->map_width) base.x = game.ui.st.game->map_width - 64;
        if (base.y >= (int)game.ui.st.game->map_height) base.y = game.ui.st.game->map_height - 64;

        log("[DRONE] 목표 기준점 base=(%d,%d)\n", base.x, base.y);

        // base 주변 직사각형(가로 240, 세로 160)을 20px 그리드로 스캔하며 생성 시도
        const int span_x = 240;
        const int span_y = 160;
        const int step = 20;
        int attempts = 0;
        for (int dy = -span_y/2; dy <= span_y/2 && created_e < 4; dy += step) {
            for (int dx = -span_x/2; dx <= span_x/2 && created_e < 4; dx += step) {
                xy pos = base + xy{dx, dy};
                // 경계 보정
                if (pos.x < 0) pos.x = 0;
                if (pos.y < 0) pos.y = 0;
                if (pos.x >= (int)game.ui.st.game->map_width) pos.x = game.ui.st.game->map_width - 2;
                if (pos.y >= (int)game.ui.st.game->map_height) pos.y = game.ui.st.game->map_height - 2;

                ++attempts;
                unit_t* drone = funcs.create_unit(funcs.get_unit_type(UnitTypes::Zerg_Drone), pos, 1);
                if (drone) {
                    funcs.finish_building_unit(drone);
                    funcs.complete_unit(drone);
                    if (drone->sprite) drone->sprite->flags &= ~sprite_t::flag_hidden;
                    log("드론 생성 성공(스캔): (%d, %d) [attempt=%d]\n", pos.x, pos.y, attempts);
                    ++created_e;
                } else {
                    if (attempts % 20 == 0) log("드론 생성 실패 누적: attempt=%d, 최근 시도=(%d,%d)\n", attempts, pos.x, pos.y);
                }
            }
        }
        log("[DRONE] 최종 생성 수: %d, 총 시도: %d\n", created_e, attempts);

        // 검증 편의를 위해 카메라를 잠시 드론 생성 지점으로 이동
        if (created_e > 0) {
            game.ui.screen_pos = {base.x - 400, base.y - 300};
            if (game.ui.screen_pos.x < 0) game.ui.screen_pos.x = 0;
            if (game.ui.screen_pos.y < 0) game.ui.screen_pos.y = 0;
        }
    }
    
    game.ui.resize(1280, 800);
    game.ui.screen_pos = {spawn_pos.x - 640, spawn_pos.y - 400};
    
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
                if (game.input.allow_enemy_control) {
                    for (int pid = 0; pid < 8; ++pid) sel_count += (int)game.ui.action_st.selection[pid].size();
                } else sel_count = (int)game.ui.action_st.selection[0].size();
                log("Frame %d - Minerals: %d, Selected: %d\n", 
                    frame_count,
                    game.ui.st.current_minerals[0],
                    sel_count);
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
}
