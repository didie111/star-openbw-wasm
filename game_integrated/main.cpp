#include "player_input.h"
#include "game_logic.h"
#include "ui_custom.h"
#include "ui_starcraft_panel.h"
#include <chrono>
#include <thread>

// 전역 로그 파일
FILE* g_log_file = nullptr;

// 간단한 로깅 함수
void write_log(const char* msg) {
    if (!g_log_file) g_log_file = fopen("log.txt", "ab");
    if (g_log_file) {
        fputs(msg, g_log_file);
        fflush(g_log_file);
    }
}

int main(int argc, char* argv[]) {
    try {
        // 게임 상태 초기화
        bwgame::state st;
        bwgame::action_state action_st;
        
        // 게임 로직 초기화
        bwgame::game_state game(st, action_st);
        game.initialize();
        
        // 입력 핸들러 초기화
        bwgame::player_input_handler player_input(st, action_st);
        
        // UI 초기화
        bwgame::ui::ui_functions ui(st);
        
        // 윈도우 생성
        if (!ui.wnd.create("StarClone - OpenBW", 0, 0, 1024, 768)) {
            printf("윈도우 생성 실패\n");
            return 1;
        }
        
        // 메인 게임 루프
        auto last_time = std::chrono::high_resolution_clock::now();
        bool running = true;
        
        while (running) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - last_time).count();
            last_time = current_time;
            
            // 이벤트 처리
            bwgame::native_window::event_t e;
            while (ui.wnd.peek_message(e)) {
                if (e.type == bwgame::native_window::event_t::type_quit) {
                    running = false;
                    break;
                }
                
                // 입력 처리
                player_input.handle_event(e, ui.screen_pos);
            }
            
            // 부드러운 스크롤 업데이트
            player_input.update_smooth_scroll(ui.screen_pos, ui.screen_width, ui.screen_height);
            
            // 게임 상태 업데이트
            game.update();
            
            // UI 업데이트
            ui.update();
            
            // 스타크래프트 스타일 UI 패널 렌더링
            auto window_surface = bwgame::native_window_drawing::get_window_surface(&ui.wnd);
            if (window_surface) {
                void* pixels = window_surface->lock();
                uint32_t* pixel_data = (uint32_t*)pixels;
                int pitch_words = window_surface->pitch / 4;
                
                // UI 패널 렌더링 (예시 자원 값)
                bwgame::ui::render_starcraft_ui_panel(
                    pixel_data, 
                    pitch_words, 
                    window_surface->w, 
                    window_surface->h,
                    5000,  // 미네랄
                    2000,  // 가스
                    2,     // 서플라이 사용
                    4      // 서플라이 최대
                );
                
                window_surface->unlock();
            }
            
            // 화면 갱신
            ui.wnd.update_surface();
            
            // 프레임 레이트 제한
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
        
        // 정리
        if (g_log_file) {
            fclose(g_log_file);
            g_log_file = nullptr;
        }
        
        return 0;
    } catch (const std::exception& e) {
        // 오류 처리
        printf("오류 발생: %s\n", e.what());
        return 1;
    }
}
