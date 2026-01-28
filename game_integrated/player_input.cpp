#include "player_input.h"
#include <cstdio>

namespace bwgame {

// 생성자
player_input_handler::player_input_handler(state& st, action_state& action_st) 
    : st(st), action_st(action_st), actions(st, action_st) {}

// 부드러운 스크롤 업데이트
void player_input_handler::update_smooth_scroll(xy& screen_pos, int screen_width, int screen_height) {
    if (!is_smooth_scrolling) return;
    
    // 현재 위치에서 목표 위치로 부드럽게 이동
    xy delta = smooth_scroll_target - screen_pos;
    if (length_squared(delta) < 1.0f) {
        // 목표 위치에 도달했으면 스크롤링 중지
        screen_pos = smooth_scroll_target;
        is_smooth_scrolling = false;
        return;
    }
    
    // 선형 보간을 사용한 부드러운 이동
    screen_pos += delta * scroll_speed;
}

// 이벤트 처리
void player_input_handler::handle_event(const native_window::event_t& e, xy screen_pos) {
    // 마우스 좌표를 게임 월드 좌표로 변환
    xy screen_mouse = { e.mouse_x, e.mouse_y };
    xy world_pos = screen_mouse + screen_pos;
    
    switch (e.type) {
        case native_window::event_t::type_mouse_motion:
            printf("[DEBUG] Mouse move - screen: (%d, %d), world: (%d, %d)\n", 
                   e.mouse_x, e.mouse_y, world_pos.x, world_pos.y);
            on_mouse_move(world_pos);
            break;
            
        case native_window::event_t::type_mouse_button_down:
            if (e.button == 1) {
                printf("[DEBUG] Left button down - screen: (%d, %d), world: (%d, %d)\n", 
                       e.mouse_x, e.mouse_y, world_pos.x, world_pos.y);
                on_left_button_down(world_pos);
            }
            else if (e.button == 3) {
                printf("[DEBUG] Right button down - screen: (%d, %d), world: (%d, %d)\n", 
                       e.mouse_x, e.mouse_y, world_pos.x, world_pos.y);
                right_button_down = true;
            }
            break;
            
        case native_window::event_t::type_mouse_button_up:
            if (e.button == 1) {
                printf("[DEBUG] Left button up - screen: (%d, %d), world: (%d, %d)\n", 
                       e.mouse_x, e.mouse_y, world_pos.x, world_pos.y);
                on_left_button_up(world_pos);
            }
            else if (e.button == 3) {
                printf("[DEBUG] Right button up - screen: (%d, %d), world: (%d, %d), screen_pos: (%d, %d)\n", 
                       e.mouse_x, e.mouse_y, world_pos.x, world_pos.y, screen_pos.x, screen_pos.y);
                right_button_down = false;
                on_right_click(world_pos, shift_pressed);
            }
            break;
            
        case native_window::event_t::type_key_down:
            on_key_down(e.scancode);
            if (e.scancode == 42 || e.scancode == 54) {
                printf("[DEBUG] Shift pressed\n");
                shift_pressed = true;
            }
            else if (e.scancode == 29 || e.scancode == 97) {
                printf("[DEBUG] Ctrl pressed\n");
                ctrl_pressed = true;
            }
            break;
            
        case native_window::event_t::type_key_up:
            if (e.scancode == 42 || e.scancode == 54) {
                printf("[DEBUG] Shift released\n");
                shift_pressed = false;
            }
            else if (e.scancode == 29 || e.scancode == 97) {
                printf("[DEBUG] Ctrl released\n");
                ctrl_pressed = false;
            }
            on_key_up(e.scancode);
            break;
            
        default:
            break;
    }
}

// 마우스 이동 처리
void player_input_handler::on_mouse_move(xy pos) {
    mouse_pos = pos;
    
    // 건물 미리보기 위치 업데이트
    if (building_to_place) {
        building_preview_pos = pos;
        building_preview_valid = true; // 간단화를 위해 항상 유효하다고 가정
    }
    
    // 드래그 중인 경우
    if (left_button_down && (abs(pos.x - drag_start_pos.x) > 5 || abs(pos.y - drag_start_pos.y) > 5)) {
        is_dragging = true;
    }
}

// 왼쪽 클릭 처리
void player_input_handler::on_left_button_down(xy pos) {
    left_button_down = true;
    drag_start_pos = pos;
    is_dragging = false;
}

// 왼쪽 클릭 처리
void player_input_handler::on_left_click(xy pos, bool shift) {
    printf("[DEBUG] on_left_click called - pos: (%d, %d), shift: %d\n", pos.x, pos.y, shift);
    
    // 건물 건설 모드인 경우
    if (building_to_place) {
        printf("[DEBUG] Building placement mode - building: %s\n", building_to_place->name);
        // 건물 건설 명령 실행
        // actions.action_build(player_id, building_to_place, pos);
        if (!shift) {
            building_to_place = nullptr;
        }
        return;
    }
    
    // 유닛 선택
    unit_t* clicked_unit = find_unit_at(pos);
    
    if (clicked_unit) {
        printf("[DEBUG] Unit clicked - type: %s, owner: %d\n", clicked_unit->unit_type->name, clicked_unit->owner);
        
        if (!shift) {
            // Shift 없이 클릭: 기존 선택 해제하고 새로 선택
            action_st.selection[player_id].clear();
        }
        
        // 유닛 선택에 추가
        action_st.selection[player_id].push_back(clicked_unit);
    } else {
        printf("[DEBUG] No unit clicked - clearing selection\n");
        
        if (!shift) {
            // 빈 곳 클릭: 선택 해제
            action_st.selection[player_id].clear();
        }
    }
}

// 왼쪽 버튼 뗄 때 처리
void player_input_handler::on_left_button_up(xy pos) {
    if (!is_dragging) {
        on_left_click(pos, shift_pressed);
    } else {
        // 드래그 선택 영역 처리
        rect select_rect = {
            std::min(drag_start_pos.x, pos.x),
            std::min(drag_start_pos.y, pos.y),
            std::max(drag_start_pos.x, pos.x),
            std::max(drag_start_pos.y, pos.y)
        };
        select_units_in_rect(select_rect);
    }
    
    left_button_down = false;
    is_dragging = false;
}

// 우클릭 처리
void player_input_handler::on_right_click(xy pos, bool shift) {
    // 디버그 로그 추가
    printf("[DEBUG] on_right_click called - pos: (%d, %d), shift: %d\n", pos.x, pos.y, shift);
    
    // 일반 우클릭 처리 (미니맵 우클릭은 제외)
    unit_t* target = find_unit_at(pos);
    
    if (target) {
        printf("[DEBUG] Target unit found at (%d, %d)\n", target->sprite->position.x, target->sprite->position.y);
    } else {
        printf("[DEBUG] No target unit found\n");
    }
    
    // 우클릭은 항상 기본 명령 (스마트 명령)
    // 화면 이동 로직 제거 - 우클릭은 유닛 명령만 처리
    actions.action_default_order(player_id, pos, target, nullptr, shift);
}

// 키 다운 이벤트
void player_input_handler::on_key_down(int scancode) {
    // 예: ESC 키로 건물 건설 취소
    if (scancode == 41 && building_to_place) { // ESC 키
        building_to_place = nullptr;
    }
    // 기타 키 처리...
}

// 키 업 이벤트
void player_input_handler::on_key_up(int scancode) {
    // 키 업 이벤트 처리
}

// 위치에 있는 유닛 찾기
unit_t* player_input_handler::find_unit_at(xy pos) {
    // 간단한 구현: 첫 번째로 찾은 유닛 반환
    for (unit_t* u : ptr(st.visible_units)) {
        if (u->sprite && u->sprite->is_visible && !(u->sprite->flags & sprite_t::flag_hidden)) {
            xy unit_pos = u->sprite->position;
            if (abs(unit_pos.x - pos.x) < 16 && abs(unit_pos.y - pos.y) < 16) {
                return u;
            }
        }
    }
    return nullptr;
}

// 사각형 영역 내의 유닛 선택
void player_input_handler::select_units_in_rect(rect area) {
    // 선택된 유닛 클리어
    action_st.selected_units[player_id].clear();
    
    // 영역 내의 유닛 선택
    for (unit_t* u : ptr(st.visible_units)) {
        if (u->owner == player_id && u->sprite && u->sprite->is_visible) {
            xy pos = u->sprite->position;
            if (pos.x >= area.x1 && pos.x <= area.x2 && pos.y >= area.y1 && pos.y <= area.y2) {
                action_st.selected_units[player_id].push_back(u);
            }
        }
    }
}

} // namespace bwgame
