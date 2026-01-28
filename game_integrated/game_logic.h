#pragma once

#include "bwgame.h"
#include "actions.h"

namespace bwgame {

// 게임 상태를 관리하는 클래스
class game_state {
public:
    state& st;
    action_state& action_st;
    
    // 생성자
    game_state(state& st, action_state& action_st);
    
    // 게임 상태 업데이트
    void update();
    
    // 게임 초기화
    void initialize();
    
    // 게임 종료 확인
    bool is_game_over() const;
    
    // 플레이어 승리/패배 처리
    void handle_victory(int player_id);
    void handle_defeat(int player_id);
    
private:
    // 게임 로직 업데이트
    void update_game_logic();
    
    // 유닛 업데이트
    void update_units();
    
    // 건물 업데이트
    void update_buildings();
    
    // 자원 업데이트
    void update_resources();
};

} // namespace bwgame
