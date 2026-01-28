#pragma once

#include "bwgame.h"
#include "actions.h"
#include "ui/native_window.h"

namespace bwgame {

// 입력 핸들러 클래스 선언
struct player_input_handler {
    state& st;
    action_state& action_st;
    action_functions actions;
    
    int player_id = 0;
    
    xy mouse_pos;
    xy last_screen_click = {0, 0};
    bool left_button_down = false;
    bool right_button_down = false;
    xy drag_start_pos;
    bool is_dragging = false;
    
    bool shift_pressed = false;
    bool ctrl_pressed = false;
    
    // 명령 모드
    enum class CommandMode {
        Normal,
        Attack,
        Move,
        Patrol,
        Build
    };
    CommandMode command_mode = CommandMode::Normal;
    
    // 건물 건설 관련
    const unit_type_t* building_to_place = nullptr;
    xy building_preview_pos = {0, 0};
    bool building_preview_valid = false;
    
    // 미니맵 클릭 관련
    bool minimap_clicked = false;
    xy minimap_target_pos = {0, 0};
    bool is_minimap_dragging = false;
    
    // 생산 속도 배율
    float production_speed_multiplier = 1.0f;
    
    // 부드러운 스크롤 관련
    xy smooth_scroll_target = {0, 0};
    bool is_smooth_scrolling = false;
    const float scroll_speed = 0.2f;
    
    player_input_handler(state& st, action_state& action_st);
    
    void update_smooth_scroll(xy& screen_pos, int screen_width, int screen_height);
    void handle_event(const native_window::event_t& e, xy screen_pos);
    void on_mouse_move(xy pos);
    void on_left_click(xy pos, bool shift); // 구현 필요
    void on_right_click(xy pos, bool shift);
    void on_left_button_down(xy pos);
    void on_left_button_up(xy pos);
    void on_key_down(int scancode);
    void on_key_up(int scancode);
    unit_t* find_unit_at(xy pos);
    void select_units_in_rect(rect area);
};

} // namespace bwgame
