 #ifndef BWGAME_UI_CUSTOM_H
 #define BWGAME_UI_CUSTOM_H
 
 #include "bwgame.h"
 #include "actions.h"
 #include "replay.h"
 #include "ui/common.h"
 #include "native_window.h"
 #include "native_window_drawing.h"
 #include "native_sound.h"
 
 #ifdef EMSCRIPTEN
  #include <emscripten/emscripten.h>
 #endif
 
    // ===== 스타크래프트 스타일 UI 패널 시스템 =====
    
    // 전역 변수 선언 (player_game_complete.cpp에서 정의됨)
    extern int g_ui_selected_category;
    extern int g_ui_active_button;
    extern bool g_ui_command_mode_active;
    extern int g_ui_mouse_x;  // 마우스 UI X 좌표
    extern int g_ui_mouse_y;  // 마우스 UI Y 좌표
    extern bool g_ui_show_coordinates;  // UI 좌표 표시 여부
    
    // 타겟 링 시각화 (스타크래프트 스타일)
    extern bool g_target_ring_active;
    extern int g_target_ring_x;  // 게임 좌표
    extern int g_target_ring_y;
    extern int g_target_ring_radius;
    extern int g_target_ring_frames;
    extern int g_target_ring_kind;  // 0=self/ally, 1=enemy, 2=resource

    // 멀티 타겟 링 UI용 값 타입 구조체 (포인터 보관 금지)
    namespace bwgame {
        struct ui_target_ring_t {
            int x = 0;
            int y = 0;
            int radius = 0;
            int frames = 0;
            int kind = 0; // 0=self/ally, 1=enemy, 2=resource
        };

        // 멀티 타겟 링 버퍼 (player_game_complete.cpp에서 채워짐)
        extern a_vector<ui_target_ring_t> g_target_rings;
    }
    
    // 프로토스 아이콘 이미지 데이터 (player_game_complete.cpp에서 로드됨)
    extern bwgame::a_vector<uint8_t> g_protoss_icon_indices;  // 레거시 (사용 안함)
    extern bwgame::a_vector<uint32_t> g_protoss_icon_rgba;    // RGBA 원본 데이터
    extern int g_protoss_icon_width;
    extern int g_protoss_icon_height;
    extern bool g_protoss_icons_loaded;
    
    // 프로토스 아이콘 렌더링 플래그 (draw_ui에서 설정, rgba_surface에서 사용)
    static bool g_draw_protoss_icons = false;
    
    // Armor 툴팁 이미지 데이터 (player_game_complete.cpp에서 로드됨)
    extern bwgame::a_vector<uint32_t> g_armor_tooltip_rgba;
    extern int g_armor_tooltip_width;
    extern int g_armor_tooltip_height;
    extern bool g_armor_tooltip_loaded;
    
    // Armor 툴팁 렌더링 플래그 및 데이터
    static bool g_draw_armor_tooltip = false;
    static int g_armor_value = 0;  // 표시할 armor 값
    
    // Shield 툴팁 이미지 데이터 (player_game_complete.cpp에서 로드됨)
    extern bwgame::a_vector<uint32_t> g_shield_tooltip_rgba;
    extern int g_shield_tooltip_width;
    extern int g_shield_tooltip_height;
    extern bool g_shield_tooltip_loaded;
    
    // Shield 툴팁 렌더링 플래그 및 데이터
    static bool g_draw_shield_tooltip = false;
    static int g_shield_value = 0;  // 표시할 shield 값
    
    // 색상 유틸리티
    static inline uint8_t ch(uint32_t c, int s) { return (uint8_t)((c >> s) & 0xFF); }
    static inline uint32_t pack(uint8_t a, uint8_t r, uint8_t g, uint8_t b) { return ((uint32_t)a<<24) | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b; }
    
    // 스타크래프트 UI 색상 팔레트
    static const uint32_t SC_COLOR_PANEL_BG = pack(255, 0, 0, 0);           // 검은색 배경
    static const uint32_t SC_COLOR_PANEL_BORDER = pack(255, 80, 80, 80);    // 회색 테두리
    static const uint32_t SC_COLOR_PANEL_DARK = pack(255, 20, 20, 20);      // 어두운 회색
    static const uint32_t SC_COLOR_TEXT_WHITE = pack(255, 255, 255, 255);   // 흰색 텍스트
    static const uint32_t SC_COLOR_TEXT_CYAN = pack(255, 0, 255, 255);      // 청록색 텍스트
    static const uint32_t SC_COLOR_TEXT_YELLOW = pack(255, 255, 255, 0);    // 노란색 텍스트
    static const uint32_t SC_COLOR_MINIMAP_BG = pack(255, 10, 10, 10);      // 미니맵 배경
    static const uint32_t SC_COLOR_BUTTON_BG = pack(255, 40, 40, 40);       // 버튼 배경
    static const uint32_t SC_COLOR_BUTTON_HOVER = pack(255, 60, 60, 60);    // 버튼 호버
    // 스타크래프트 원작 버튼 색상
    static const uint32_t SC_COLOR_BUTTON_NONE = pack(255, 60, 20, 60);     // 버튼 없음 (분홍색)
    static const uint32_t SC_COLOR_BUTTON_DISABLED = pack(255, 80, 60, 20); // 버튼 비활성 (갈색)
    static const uint32_t SC_COLOR_BUTTON_ACTIVE = pack(255, 20, 100, 20);  // 버튼 활성 (초록색)
    static const uint32_t SC_COLOR_BUTTON_HIGHLIGHT = pack(255, 255, 255, 255); // 하이라이트 테두리 (흰색)
    static uint32_t alpha_blend_over(uint32_t dst, uint32_t src) {
        uint8_t sa = ch(src,24);
        if (sa == 0) return dst;
        if (sa == 255) return src;
        uint8_t sr = ch(src,16), sg = ch(src,8), sb = ch(src,0);
        uint8_t dr = ch(dst,16), dg = ch(dst,8), db = ch(dst,0);
        uint32_t inv = 255 - sa;
        uint8_t r = (uint8_t)((sr*sa + dr*inv) / 255);
        uint8_t g = (uint8_t)((sg*sa + dg*inv) / 255);
        uint8_t b = (uint8_t)((sb*sa + db*inv) / 255);
        return pack(255, r, g, b);
    }
    static void put_pixel_rgba(uint32_t* data, int pitch_words, int x, int y, uint32_t color, int width = 9999, int height = 9999) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        data[y * pitch_words + x] = color;
    }
    static void put_pixel_rgba_blend(uint32_t* data, int pitch_words, int x, int y, uint32_t color) {
        if (x < 0 || y < 0) return;
        uint32_t* p = &data[y * pitch_words + x];
        *p = alpha_blend_over(*p, color);
    }
    // ★ 유닛 이름 매핑 함수
    static const char* get_unit_name(int unit_type_id) {
        switch (unit_type_id) {
            // Terran Units
            case 0: return "Marine";
            case 1: return "Ghost";
            case 2: return "Vulture";
            case 3: return "Goliath";
            case 5: return "Siege Tank";
            case 7: return "SCV";
            case 8: return "Wraith";
            case 9: return "Battlecruiser";
            case 11: return "Dropship";
            case 12: return "Science Vessel";
            case 13: return "Firebat";
            case 32: return "Medic";
            case 58: return "Valkyrie";
            
            // Terran Buildings
            case 106: return "Command Center";
            case 107: return "Comsat Station";
            case 108: return "Nuclear Silo";
            case 109: return "Supply Depot";
            case 110: return "Refinery";
            case 111: return "Barracks";
            case 112: return "Academy";
            case 113: return "Factory";
            case 114: return "Starport";
            case 115: return "Control Tower";
            case 116: return "Science Facility";
            case 117: return "Covert Ops";
            case 118: return "Physics Lab";
            case 120: return "Machine Shop";
            case 122: return "Engineering Bay";
            case 123: return "Armory";
            case 124: return "Missile Turret";
            case 125: return "Bunker";
            
            // Protoss Units
            case 64: return "Probe";
            case 65: return "Zealot";
            case 66: return "Dragoon";
            case 67: return "High Templar";
            case 68: return "Archon";
            case 69: return "Shuttle";
            case 70: return "Scout";
            case 71: return "Arbiter";
            case 72: return "Carrier";
            case 73: return "Interceptor";
            case 61: return "Dark Templar";
            case 83: return "Reaver";
            case 84: return "Observer";
            case 85: return "Scarab";
            case 60: return "Corsair";
            case 63: return "Dark Archon";
            
            // Protoss Buildings
            case 154: return "Nexus";
            case 155: return "Robotics Facility";
            case 156: return "Pylon";
            case 157: return "Assimilator";
            case 159: return "Observatory";
            case 160: return "Gateway";
            case 162: return "Photon Cannon";
            case 163: return "Citadel of Adun";
            case 164: return "Cybernetics Core";
            case 165: return "Templar Archives";
            case 166: return "Forge";
            case 167: return "Stargate";
            case 169: return "Fleet Beacon";
            case 170: return "Arbiter Tribunal";
            case 171: return "Robotics Support Bay";
            case 172: return "Shield Battery";
            
            // Zerg Units
            case 41: return "Drone";
            case 37: return "Zergling";
            case 38: return "Hydralisk";
            case 39: return "Ultralisk";
            case 42: return "Overlord";
            case 43: return "Mutalisk";
            case 44: return "Guardian";
            case 45: return "Queen";
            case 46: return "Defiler";
            case 47: return "Scourge";
            case 103: return "Lurker";
            case 50: return "Infested Terran";
            
            // Zerg Buildings
            case 131: return "Hatchery";
            case 132: return "Lair";
            case 133: return "Hive";
            case 134: return "Extractor";
            case 142: return "Nydus Canal";
            case 143: return "Hydralisk Den";
            case 144: return "Defiler Mound";
            case 145: return "Greater Spire";
            case 146: return "Queen's Nest";
            case 147: return "Evolution Chamber";
            case 148: return "Ultralisk Cavern";
            case 149: return "Spire";
            case 150: return "Spawning Pool";
            case 151: return "Creep Colony";
            case 152: return "Spore Colony";
            case 153: return "Sunken Colony";
            
            // Resources
            case 176: return "Mineral Field";
            case 177: return "Mineral Field";
            case 178: return "Mineral Field";
            case 188: return "Vespene Geyser";
            
            default: return "Unknown Unit";
        }
    }
    
    // 5x7 uppercase ASCII bitmap (MSB = top)
    static const uint8_t* glyph5x7(char c) {
        // Each glyph is 5 columns, 7 bits height, MSB at top
        static const uint8_t map[][6] = {
            // ' ' 32
            {' ', 0x00,0x00,0x00,0x00,0x00},
            // '!' 33
            {'!', 0x00,0x00,0x5F,0x00,0x00},
            // ',' 44
            {',', 0x00,0x80,0x60,0x00,0x00},
            // '-' 45
            {'-', 0x08,0x08,0x08,0x08,0x08},
            // '(' 40
            {'(', 0x00,0x1C,0x22,0x41,0x00},
            // ')' 41
            {')', 0x00,0x41,0x22,0x1C,0x00},
            // ':' 58
            {':', 0x00,0x24,0x00,0x24,0x00},
            // '/' 47
            {'/', 0x02,0x04,0x08,0x10,0x20},
            // '.' 46
            {'.', 0x00,0x01,0x01,0x00,0x00},
            // 'x' 120
            {'x', 0x22,0x14,0x08,0x14,0x22},
            // '0'
            {'0', 0x3E,0x51,0x49,0x45,0x3E},
            // '1'
            {'1', 0x00,0x42,0x7F,0x40,0x00},
            // '2'
            {'2', 0x42,0x61,0x51,0x49,0x46},
            // '3'
            {'3', 0x21,0x41,0x45,0x4B,0x31},
            // '4'
            {'4', 0x18,0x14,0x12,0x7F,0x10},
            // '5'
            {'5', 0x27,0x45,0x45,0x45,0x39},
            // '6'
            {'6', 0x3C,0x4A,0x49,0x49,0x30},
            // '7'
            {'7', 0x01,0x71,0x09,0x05,0x03},
            // '8'
            {'8', 0x36,0x49,0x49,0x49,0x36},
            // '9'
            {'9', 0x06,0x49,0x49,0x29,0x1E},
            // 'A'
            {'A', 0x7E,0x11,0x11,0x11,0x7E},
            // 'B'
            {'B', 0x7F,0x49,0x49,0x49,0x36},
            // 'C'
            {'C', 0x3E,0x41,0x41,0x41,0x22},
            // 'D'
            {'D', 0x7F,0x41,0x41,0x22,0x1C},
            // 'E'
            {'E', 0x7F,0x49,0x49,0x49,0x41},
            // 'F'
            {'F', 0x7F,0x09,0x09,0x09,0x01},
            // 'G'
            {'G', 0x3E,0x41,0x49,0x49,0x7A},
            // 'H'
            {'H', 0x7F,0x08,0x08,0x08,0x7F},
            // 'I'
            {'I', 0x00,0x41,0x7F,0x41,0x00},
            // 'J'
            {'J', 0x20,0x40,0x41,0x3F,0x01},
            // 'K'
            {'K', 0x7F,0x08,0x14,0x22,0x41},
            // 'L'
            {'L', 0x7F,0x40,0x40,0x40,0x40},
            // 'M'
            {'M', 0x7F,0x02,0x0C,0x02,0x7F},
            // 'N'
            {'N', 0x7F,0x04,0x08,0x10,0x7F},
            // 'O'
            {'O', 0x3E,0x41,0x41,0x41,0x3E},
            // 'P'
            {'P', 0x7F,0x09,0x09,0x09,0x06},
            // 'Q'
            {'Q', 0x3E,0x41,0x51,0x21,0x5E},
            // 'R'
            {'R', 0x7F,0x09,0x19,0x29,0x46},
            // 'S'
            {'S', 0x46,0x49,0x49,0x49,0x31},
            // 'T'
            {'T', 0x01,0x01,0x7F,0x01,0x01},
            // 'U'
            {'U', 0x3F,0x40,0x40,0x40,0x3F},
            // 'V'
            {'V', 0x1F,0x20,0x40,0x20,0x1F},
            // 'W'
            {'W', 0x3F,0x40,0x38,0x40,0x3F},
            // 'X'
            {'X', 0x63,0x14,0x08,0x14,0x63},
            // 'Y'
            {'Y', 0x07,0x08,0x70,0x08,0x07},
            // 'Z'
            {'Z', 0x61,0x51,0x49,0x45,0x43},
            // 'a' (lowercase)
            {'a', 0x20,0x54,0x54,0x54,0x78},
            // 'b' (lowercase)
            {'b', 0x7F,0x48,0x44,0x44,0x38},
            // 'c' (lowercase)
            {'c', 0x38,0x44,0x44,0x44,0x20},
            // 'd' (lowercase)
            {'d', 0x38,0x44,0x44,0x48,0x7F},
            // 'e' (lowercase)
            {'e', 0x38,0x54,0x54,0x54,0x18},
            // 'f' (lowercase)
            {'f', 0x08,0x7E,0x09,0x01,0x02},
            // 'g' (lowercase)
            {'g', 0x0C,0x52,0x52,0x52,0x3E},
            // 'h' (lowercase)
            {'h', 0x7F,0x08,0x04,0x04,0x78},
            // 'i' (lowercase)
            {'i', 0x00,0x44,0x7D,0x40,0x00},
            // 'j' (lowercase)
            {'j', 0x20,0x40,0x44,0x3D,0x00},
            // 'k' (lowercase)
            {'k', 0x7F,0x10,0x28,0x44,0x00},
            // 'l' (lowercase)
            {'l', 0x00,0x41,0x7F,0x40,0x00},
            // 'm' (lowercase)
            {'m', 0x7C,0x04,0x18,0x04,0x78},
            // 'n' (lowercase)
            {'n', 0x7C,0x08,0x04,0x04,0x78},
            // 'o' (lowercase)
            {'o', 0x38,0x44,0x44,0x44,0x38},
            // 'p' (lowercase)
            {'p', 0x7C,0x14,0x14,0x14,0x08},
            // 'q' (lowercase)
            {'q', 0x08,0x14,0x14,0x18,0x7C},
            // 'r' (lowercase)
            {'r', 0x7C,0x08,0x04,0x04,0x08},
            // 's' (lowercase)
            {'s', 0x48,0x54,0x54,0x54,0x20},
            // 't' (lowercase)
            {'t', 0x04,0x3F,0x44,0x40,0x20},
            // 'u' (lowercase)
            {'u', 0x3C,0x40,0x40,0x20,0x7C},
            // 'v' (lowercase)
            {'v', 0x1C,0x20,0x40,0x20,0x1C},
            // 'w' (lowercase)
            {'w', 0x3C,0x40,0x30,0x40,0x3C},
            // 'y' (lowercase)
            {'y', 0x1C,0x20,0x40,0x20,0x1C},
            // 'z' (lowercase)
            {'z', 0x44,0x64,0x54,0x4C,0x44},
        };
        for (auto& g : map) if (g[0] == (uint8_t)c) return &g[1];
        // default unknown -> empty
        static const uint8_t empty[5] = {0,0,0,0,0};
        return empty;
    }
    static void draw_glyph5x7(uint32_t* data, int pitch_words, int x, int y, uint32_t color, char c) {
        const uint8_t* col = glyph5x7(c);
        for (int i = 0; i < 5; ++i) {
            uint8_t bits = col[i];
            for (int j = 0; j < 7; ++j) {
                // Draw top-to-bottom using LSB-at-top
                if (bits & (1u << j)) put_pixel_rgba(data, pitch_words, x + i, y + j, color);
            }
        }
    }
    static void draw_text5x7(uint32_t* data, int pitch_words, int x, int y, const std::string& text, uint32_t color) {
        int cx = x;
        for (char ch : text) {
            if (ch == '\n') { y += 9; cx = x; continue; }
            draw_glyph5x7(data, pitch_words, cx, y, color, ch);
            cx += 6; // 5px + 1px space
        }
    }
    // scaled 5x7 text (수정됨)
    static void draw_text5x7_scaled(uint32_t* data, int pitch_words, int x, int y, const std::string& text, uint32_t color, int scale) {
        if (scale <= 1) { draw_text5x7(data, pitch_words, x, y, text, color); return; }
        int cx = x;
        for (char ch : text) {
            if (ch == '\n') { y += (7 + 2) * scale; cx = x; continue; }
            const uint8_t* g = glyph5x7(ch);
            // 각 글리프는 5개의 열(column)로 구성
            for (int gx = 0; gx < 5; ++gx) {
                uint8_t bits = g[gx];
                // 각 열은 7비트 높이 (LSB가 위)
                for (int gy = 0; gy < 7; ++gy) {
                    if (bits & (1u << gy)) {  // LSB부터 읽기
                        int px = cx + gx * scale;
                        int py = y + gy * scale;
                        // scale x scale 픽셀 블록 그리기
                        for (int sx = 0; sx < scale; ++sx) {
                            for (int sy = 0; sy < scale; ++sy) {
                                put_pixel_rgba(data, pitch_words, px + sx, py + sy, color);
                            }
                        }
                    }
                }
            }
            cx += 6 * scale;
        }
    }

    // 8x13 crisper HUD font (subset of ASCII) - monospaced
    struct glyph8x13_t { char c; uint16_t rows[13]; };
    static const glyph8x13_t* glyph8x13(char c) {
        // Each row uses low 8 bits as pixels (1=on). Designed for clarity at 2-3x scale.
        static const glyph8x13_t map[] = {
            {' ', {0,0,0,0,0,0,0,0,0,0,0,0,0}},
            {'(', {0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0}},
            {')', {0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0}},
            {':', {0,0,0,0,0x18,0x18,0,0,0x18,0x18,0,0,0}},
            {'.', {0,0,0,0,0,0,0,0,0,0,0x18,0x18,0}},
            {'x', {0,0,0,0,0xC3,0x66,0x3C,0x18,0x3C,0x66,0xC3,0,0}},
            {'0', {0x3C,0x66,0xC3,0xDB,0xCB,0xC3,0xC3,0xC3,0xD3,0xC3,0x66,0x3C,0}},
            {'1', {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E,0}},
            {'2', {0x3C,0x66,0xC3,0x03,0x06,0x0C,0x18,0x30,0x60,0xC0,0xFF,0xFF,0}},
            {'3', {0x7E,0xFF,0x03,0x06,0x1C,0x0E,0x07,0x03,0xC3,0xC3,0x66,0x3C,0}},
            {'4', {0x06,0x0E,0x1E,0x36,0x66,0xC6,0x86,0xFF,0xFF,0x06,0x06,0x06,0}},
            {'5', {0xFF,0xFF,0xC0,0xC0,0xFC,0x7E,0x03,0x03,0x03,0xC3,0x66,0x3C,0}},
            {'6', {0x1E,0x30,0x60,0xC0,0xFC,0xFE,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0}},
            {'7', {0xFF,0xFF,0x03,0x06,0x0C,0x18,0x30,0x30,0x60,0x60,0x60,0x60,0}},
            {'8', {0x3C,0x66,0xC3,0xC3,0x66,0x3C,0x66,0xC3,0xC3,0xC3,0x66,0x3C,0}},
            {'9', {0x3C,0x66,0xC3,0xC3,0xC3,0x7F,0x3F,0x06,0x0C,0x18,0x30,0x78,0}},
            {'A', {0x18,0x3C,0x66,0x66,0xC3,0xC3,0xFF,0xFF,0xC3,0xC3,0xC3,0xC3,0}},
            {'B', {0xFC,0xFE,0xC3,0xC3,0xC3,0xFC,0xFC,0xC3,0xC3,0xC3,0xFE,0xFC,0}},
            {'C', {0x3C,0x7E,0xC3,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC3,0x7E,0x3C,0}},
            {'D', {0xFC,0xFE,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFE,0xFC,0}},
            {'E', {0xFF,0xFF,0xC0,0xC0,0xC0,0xFC,0xFC,0xC0,0xC0,0xC0,0xFF,0xFF,0}},
            {'F', {0xFF,0xFF,0xC0,0xC0,0xFC,0xFC,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0}},
            {'G', {0x3C,0x7E,0xC3,0xC0,0xC0,0xCF,0xCF,0xC3,0xC3,0xC3,0x7E,0x3C,0}},
            {'I', {0x7E,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x7E,0}},
            {'M', {0xC3,0xE7,0xFF,0xDB,0xDB,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0}},
            {'N', {0xC3,0xC3,0xE3,0xF3,0xFB,0xDB,0xCF,0xC7,0xC3,0xC3,0xC3,0xC3,0}},
            {'O', {0x3C,0x7E,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x7E,0x3C,0}},
            {'P', {0xFE,0xFF,0xC3,0xC3,0xC3,0xFE,0xFC,0xC0,0xC0,0xC0,0xC0,0xC0,0}},
            {'R', {0xFC,0xFE,0xC3,0xC3,0xC3,0xFC,0xFC,0xCC,0xC6,0xC3,0xC3,0xC3,0}},
            {'S', {0x3E,0x7F,0xC0,0xC0,0x7E,0x3F,0x03,0x03,0x03,0xC3,0xFE,0x7C,0}},
            {'T', {0xFF,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0}},
            {'U', {0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x7E,0x3C,0}},
            {'V', {0xC3,0xC3,0xC3,0xC3,0xC3,0x66,0x66,0x66,0x3C,0x3C,0x18,0x18,0}},
            {'X', {0xC3,0xC3,0x66,0x66,0x3C,0x18,0x18,0x3C,0x66,0x66,0xC3,0xC3,0}},
            {'Y', {0xC3,0xC3,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0}},
            {'a', {0,0,0,0,0x7C,0x06,0x7E,0xC6,0xC6,0xCE,0x76,0,0}},
            {'c', {0,0,0,0,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0,0}},
            {'d', {0x06,0x06,0x06,0x06,0x3E,0x66,0xC6,0xC6,0xC6,0x66,0x3E,0,0}},
            {'e', {0,0,0,0,0x3C,0x66,0xC3,0xFF,0xC0,0x66,0x3C,0,0}},
            {'i', {0x18,0x18,0,0,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0,0}},
            {'l', {0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0}},
            {'m', {0,0,0,0,0xEC,0xFE,0xD6,0xD6,0xD6,0xC6,0xC6,0,0}},
            {'n', {0,0,0,0,0xDC,0xE6,0xC6,0xC6,0xC6,0xC6,0xC6,0,0}},
            {'o', {0,0,0,0,0x3C,0x66,0xC3,0xC3,0xC3,0x66,0x3C,0,0}},
            {'p', {0,0,0,0,0xDC,0xE6,0xC6,0xC6,0xE6,0xDC,0xC0,0xC0,0xC0}},
            {'r', {0,0,0,0,0x6E,0x73,0x60,0x60,0x60,0x60,0x60,0,0}},
            {'s', {0,0,0,0,0x7C,0xC6,0xC0,0x7C,0x06,0xC6,0x7C,0,0}},
            {'t', {0x30,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x30,0x36,0x1C,0,0}},
            {'u', {0,0,0,0,0xC6,0xC6,0xC6,0xC6,0xC6,0xCE,0x76,0,0}},
            {'w', {0,0,0,0,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0x6C,0,0}},
            {'y', {0,0,0,0,0xC3,0xC3,0x66,0x3C,0x18,0x30,0x60,0xC0,0xC0}},
            {'z', {0,0,0,0,0xFE,0xC6,0x0C,0x18,0x30,0x66,0xFE,0,0}},
            // 소문자 나머지
            {'b', {0xC0,0xC0,0xC0,0xC0,0xDC,0xE6,0xC6,0xC6,0xC6,0xE6,0xDC,0,0}},
            {'f', {0x1E,0x33,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x30,0x78,0,0}},
            {'g', {0,0,0,0,0x7E,0xC6,0xC6,0xC6,0x7E,0x06,0xC6,0x7C,0}},
            {'h', {0xC0,0xC0,0xC0,0xC0,0xDC,0xE6,0xC6,0xC6,0xC6,0xC6,0xC6,0,0}},
            {'j', {0x18,0x18,0,0,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x98,0x70}},
            {'k', {0xC0,0xC0,0xC0,0xC0,0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0,0}},
            {'q', {0,0,0,0,0x3E,0x66,0xC6,0xC6,0x66,0x3E,0x06,0x06,0x06}},
            {'v', {0,0,0,0,0xC6,0xC6,0xC6,0x6C,0x6C,0x38,0x38,0,0}},
            // 대문자 나머지
            {'H', {0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,0xFF,0xC3,0xC3,0xC3,0xC3,0xC3,0}},
            {'J', {0x0F,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x38,0}},
            {'K', {0xC3,0xC6,0xCC,0xD8,0xF0,0xE0,0xF0,0xD8,0xCC,0xC6,0xC3,0xC3,0}},
            {'L', {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFF,0xFF,0}},
            {'Q', {0x3C,0x7E,0xC3,0xC3,0xC3,0xC3,0xC3,0xDB,0xDB,0x7E,0x3C,0x0E,0x03}},
            {'W', {0xC3,0xC3,0xC3,0xC3,0xC3,0xDB,0xDB,0xDB,0xFF,0xE7,0xC3,0xC3,0}},
            {'Z', {0xFF,0xFF,0x03,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC0,0xFF,0xFF,0}},
            // 특수문자
            {'!', {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0x18,0x18,0}},
            {',', {0,0,0,0,0,0,0,0,0,0,0x18,0x18,0x30}},
            {'-', {0,0,0,0,0,0,0x7E,0x7E,0,0,0,0,0}},
            {'/', {0x03,0x03,0x06,0x06,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0xC0,0xC0}},
            {'\'', {0x18,0x18,0x30,0,0,0,0,0,0,0,0,0,0}},
            {'"', {0x66,0x66,0xCC,0,0,0,0,0,0,0,0,0,0}},
            {'?', {0x3C,0x66,0xC3,0x03,0x06,0x0C,0x18,0x18,0,0,0x18,0x18,0}},
            {';', {0,0,0,0,0x18,0x18,0,0,0x18,0x18,0x30,0,0}},
            {'@', {0x3C,0x66,0xC3,0xC3,0xCF,0xDB,0xDB,0xCF,0xC0,0xC3,0x66,0x3C,0}},
            {'#', {0x36,0x36,0xFF,0x36,0x36,0x36,0xFF,0x36,0x36,0,0,0,0}},
            {'$', {0x18,0x7E,0xDB,0xD8,0x7C,0x1E,0x1B,0xDB,0x7E,0x18,0,0,0}},
            {'%', {0xC3,0xC6,0x0C,0x18,0x30,0x60,0xC6,0xC3,0,0,0,0,0}},
            {'&', {0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xDC,0x76,0,0,0}},
            {'+', {0,0,0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18,0,0,0}},
            {'=', {0,0,0,0,0xFF,0xFF,0,0xFF,0xFF,0,0,0,0}},
            {'[', {0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0,0}},
            {']', {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0,0}},
            {'{', {0x0E,0x18,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0,0}},
            {'}', {0x70,0x18,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0,0}},
            {'<', {0,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0,0,0}},
            {'>', {0,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0,0,0}},
            {'*', {0,0x66,0x3C,0xFF,0x3C,0x66,0,0,0,0,0,0,0}},
            {'_', {0,0,0,0,0,0,0,0,0,0,0,0xFF,0xFF}},
            {'`', {0x30,0x18,0x0C,0,0,0,0,0,0,0,0,0,0}},
            {'~', {0,0,0x70,0xD8,0xD8,0x0E,0,0,0,0,0,0,0}},
            {'|', {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0}},
            {'\\', {0xC0,0xC0,0x60,0x60,0x30,0x18,0x18,0x0C,0x0C,0x06,0x06,0x03,0x03}},
            {'^', {0x10,0x38,0x6C,0xC6,0,0,0,0,0,0,0,0,0}},
        };
        for (const auto& g : map) if (g.c == c) return &g;
        static const glyph8x13_t empty{0,{0}};
        return &empty;
    }
    static void draw_glyph8x13(uint32_t* data, int pitch_words, int x, int y, uint32_t color, char c) {
        const glyph8x13_t* g = glyph8x13(c);
        for (int ry = 0; ry < 13; ++ry) {
            uint16_t row = g->rows[ry];
            for (int rx = 0; rx < 8; ++rx) if (row & (1u << (7-rx))) put_pixel_rgba(data, pitch_words, x + rx, y + ry, color);
        }
    }
    static void draw_text8x13_scaled(uint32_t* data, int pitch_words, int x, int y, const std::string& text, uint32_t color, int scale) {
        int cx = x;
        for (char ch : text) {
            if (ch == '\n') { y += (13 + 3) * scale; cx = x; continue; }
            const glyph8x13_t* g = glyph8x13(ch);
            for (int ry = 0; ry < 13; ++ry) {
                uint16_t row = g->rows[ry];
                for (int rx = 0; rx < 8; ++rx) if (row & (1u << (7-rx))) {
                    int px = cx + rx * scale;
                    int py = y + ry * scale;
                    for (int sx = 0; sx < scale; ++sx) for (int sy = 0; sy < scale; ++sy) put_pixel_rgba(data, pitch_words, px + sx, py + sy, color);
                }
            }
            cx += (8 + 1) * scale;
        }
    }

    // ===== 스타크래프트 UI 패널 렌더링 함수 =====
    
    // 사각형 채우기
    static void fill_rect(uint32_t* data, int pitch_words, int x, int y, int w, int h, uint32_t color) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                put_pixel_rgba(data, pitch_words, px, py, color);
            }
        }
    }
    
    // 테두리 그리기
    static void draw_border(uint32_t* data, int pitch_words, int x, int y, int w, int h, uint32_t color, int thickness = 1) {
        // 상단
        fill_rect(data, pitch_words, x, y, w, thickness, color);
        // 하단
        fill_rect(data, pitch_words, x, y + h - thickness, w, thickness, color);
        // 좌측
        fill_rect(data, pitch_words, x, y, thickness, h, color);
        // 우측
        fill_rect(data, pitch_words, x + w - thickness, y, thickness, h, color);
    }
    
    // 3D 효과 테두리 (스타크래프트 스타일)
    static void draw_3d_border(uint32_t* data, int pitch_words, int x, int y, int w, int h, bool raised = true) {
        uint32_t light = pack(255, 120, 120, 120);
        uint32_t dark = pack(255, 40, 40, 40);
        
        if (!raised) std::swap(light, dark);
        
        // 상단 + 좌측 (밝은색)
        for (int i = 0; i < 2; ++i) {
            for (int px = x + i; px < x + w - i; ++px) put_pixel_rgba(data, pitch_words, px, y + i, light);
            for (int py = y + i; py < y + h - i; ++py) put_pixel_rgba(data, pitch_words, x + i, py, light);
        }
        
        // 하단 + 우측 (어두운색)
        for (int i = 0; i < 2; ++i) {
            for (int px = x + i; px < x + w - i; ++px) put_pixel_rgba(data, pitch_words, px, y + h - 1 - i, dark);
            for (int py = y + i; py < y + h - i; ++py) put_pixel_rgba(data, pitch_words, x + w - 1 - i, py, dark);
        }
    }
    
    // 원 그리기 (스타크래프트 타겟 링 스타일)
    static void draw_circle(uint32_t* data, int pitch_words, int cx, int cy, int radius, uint32_t color, int thickness = 2, int width = 9999, int height = 9999) {
        // Midpoint circle algorithm
        int x = 0;
        int y = radius;
        int d = 1 - radius;
        
        auto draw_circle_points = [&](int px, int py) {
            for (int t = 0; t < thickness; ++t) {
                put_pixel_rgba(data, pitch_words, cx + px, cy + py + t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - px, cy + py + t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + px, cy - py - t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - px, cy - py - t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + py + t, cy + px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - py - t, cy + px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + py + t, cy - px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - py - t, cy - px, color, width, height);
            }
        };
        
        while (x <= y) {
            draw_circle_points(x, y);
            x++;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                y--;
                d += 2 * (x - y) + 1;
            }
        }
    }

    static void draw_dotted_circle(uint32_t* data, int pitch_words, int cx, int cy, int radius, uint32_t color, int thickness = 2, int width = 9999, int height = 9999) {
        int x = 0;
        int y = radius;
        int d = 1 - radius;
        int step = 0;

        auto draw_circle_points = [&](int px, int py) {
            int pattern = 4;
            bool draw = ((step % (pattern * 2)) < pattern);
            ++step;
            if (!draw) return;
            for (int t = 0; t < thickness; ++t) {
                put_pixel_rgba(data, pitch_words, cx + px, cy + py + t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - px, cy + py + t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + px, cy - py - t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - px, cy - py - t, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + py + t, cy + px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - py - t, cy + px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx + py + t, cy - px, color, width, height);
                put_pixel_rgba(data, pitch_words, cx - py - t, cy - px, color, width, height);
            }
        };

        while (x <= y) {
            draw_circle_points(x, y);
            x++;
            if (d < 0) {
                d += 2 * x + 1;
            } else {
                y--;
                d += 2 * (x - y) + 1;
            }
        }
    }
    // ===== 유닛 정보 표시 함수 (계급, 킬 수) =====
    
    // 유닛 계급 이름 반환 (스타크래프트 원작 방식)
    // rank_increase는 업그레이드 레벨 (+1, +2, +3 등)
    static const char* get_unit_rank_name(int rank_increase) {
        switch (rank_increase) {
            case 0: return "";  // 계급 없음
            case 1: return "+1";
            case 2: return "+2";
            case 3: return "+3";
            default: return "+3";  // 최대 +3
        }
    }
    
    // 유닛 정보를 화면에 표시 (선택된 유닛 하단에 표시)
    static void draw_unit_info_overlay(uint32_t* data, int pitch_words, int screen_width, int screen_height,
                                       const bwgame::unit_t* unit, int unit_screen_x, int unit_screen_y) {
        if (!unit) return;
        
        // 유닛 정보 텍스트 생성
        char info_text[256];
        const char* rank_name = get_unit_rank_name(unit->rank_increase);
        snprintf(info_text, sizeof(info_text), "Rank: %s | Kills: %d", rank_name, unit->kill_count);
        
        // 텍스트 위치 계산 (유닛 하단 중앙)
        int text_width = (int)strlen(info_text) * 6;  // 5x7 폰트 + 1px 간격
        int text_x = unit_screen_x - text_width / 2;
        int text_y = unit_screen_y + 40;  // 유닛 아래 40픽셀
        
        // 화면 경계 체크
        if (text_x < 0) text_x = 0;
        if (text_x + text_width > screen_width) text_x = screen_width - text_width;
        if (text_y < 0) text_y = 0;
        if (text_y + 7 > screen_height) text_y = screen_height - 7;
        
        // 배경 그리기 (반투명 검은색)
        uint32_t bg_color = pack(180, 0, 0, 0);
        for (int py = text_y - 2; py < text_y + 9; ++py) {
            for (int px = text_x - 2; px < text_x + text_width + 2; ++px) {
                if (px >= 0 && px < screen_width && py >= 0 && py < screen_height) {
                    put_pixel_rgba_blend(data, pitch_words, px, py, bg_color);
                }
            }
        }
        
        // 텍스트 그리기 (노란색)
        draw_text5x7(data, pitch_words, text_x, text_y, info_text, SC_COLOR_TEXT_YELLOW);
    }
    
    // 모든 선택된 유닛에 대해 정보 표시
    static void draw_all_selected_units_info(uint32_t* data, int pitch_words, int screen_width, int screen_height,
                                             const bwgame::a_vector<bwgame::unit_t*>& selected_units,
                                             int screen_pos_x, int screen_pos_y) {
        for (const auto* unit : selected_units) {
            if (!unit || !unit->sprite) continue;
            
            // 유닛의 화면 좌표 계산
            int unit_screen_x = unit->sprite->position.x - screen_pos_x;
            int unit_screen_y = unit->sprite->position.y - screen_pos_y;
            
            // 화면 밖이면 스킵
            if (unit_screen_x < -100 || unit_screen_x > screen_width + 100 ||
                unit_screen_y < -100 || unit_screen_y > screen_height + 100) {
                continue;
            }
            
            // 유닛 정보 표시
            draw_unit_info_overlay(data, pitch_words, screen_width, screen_height,
                                  unit, unit_screen_x, unit_screen_y);
        }
    }
    
    // (사용되지 않는 render_starcraft_ui_panel 함수 삭제됨)

namespace bwgame {

struct vr4_entry {
	using bitmap_t = std::conditional<is_native_fast_int<uint64_t>::value, uint64_t, uint32_t>::type;
	std::array<bitmap_t, 64 / sizeof(bitmap_t)> bitmap;
	std::array<bitmap_t, 64 / sizeof(bitmap_t)> inverted_bitmap;
};
struct vx4_entry {
	std::array<uint16_t, 16> images;
};

struct pcx_image {
	size_t width;
	size_t height;
	a_vector<uint8_t> data;
};

struct tileset_image_data {
	a_vector<uint8_t> wpe;
	a_vector<vr4_entry> vr4;
	a_vector<vx4_entry> vx4;
	pcx_image dark_pcx;
	std::array<pcx_image, 7> light_pcx;
	grp_t creep_grp;
	int resource_minimap_color;
	uint8_t ally_target_ring_color;
	uint8_t enemy_target_ring_color;
	uint8_t resource_target_ring_color;
	std::array<uint8_t, 256> cloak_fade_selector;
};

struct image_data {
	std::array<std::array<uint8_t, 8>, 16> player_unit_colors;
	std::array<uint8_t, 16> player_minimap_colors;
	std::array<uint8_t, 24> selection_colors;
	std::array<uint8_t, 24> hp_bar_colors;
	std::array<int, 0x100> creep_edge_frame_index{};
};

template<typename data_T>
pcx_image load_pcx_data(const data_T& data) {
	data_loading::data_reader_le r(data.data(), data.data() + data.size());
	auto base_r = r;
	auto id = r.get<uint8_t>();
	if (id != 0x0a) error("pcx: invalid identifier %#x", id);
	r.get<uint8_t>(); // version
	auto encoding = r.get<uint8_t>(); // encoding
	auto bpp = r.get<uint8_t>(); // bpp
	auto offset_x = r.get<uint16_t>();
	auto offset_y = r.get<uint16_t>();
	auto last_x = r.get<uint16_t>();
	auto last_y = r.get<uint16_t>();

	if (encoding != 1) error("pcx: invalid encoding %#x", encoding);
	if (bpp != 8) error("pcx: bpp is %d, expected 8", bpp);

	if (offset_x != 0 || offset_y != 0) error("pcx: offset %d %d, expected 0 0", offset_x, offset_y);

	r.skip(2 + 2 + 48 + 1);

	auto bit_planes = r.get<uint8_t>();
	auto bytes_per_line = r.get<uint16_t>();

	size_t width = last_x + 1;
	size_t height = last_y + 1;

	pcx_image pcx;
	pcx.width = width;
	pcx.height = height;

	pcx.data.resize(width * height);

	r = base_r;
	r.skip(128);

	auto padding = bytes_per_line * bit_planes - width;
	if (padding != 0) error("pcx: padding not supported");

	uint8_t* dst = pcx.data.data();
	uint8_t* dst_end = pcx.data.data() + pcx.data.size();

	while (dst != dst_end) {
		auto v = r.get<uint8_t>();
		if ((v & 0xc0) == 0xc0) {
			v &= 0x3f;
			auto c = r.get<uint8_t>();
			for (; v; --v) {
				if (dst == dst_end) error("pcx: failed to decode");
				*dst++ = c;
			}
		} else {
			*dst = v;
			++dst;
		}
	}

	return pcx;
}

static inline std::unique_ptr<native_window_drawing::surface> flip_image(native_window_drawing::surface* src) {
	auto tmp = native_window_drawing::create_rgba_surface(src->w, src->h);
	src->blit(&*tmp, 0, 0);
	void* ptr = tmp->lock();
	uint32_t* pixels = (uint32_t*)ptr;
	for (size_t y = 0; y != (size_t)tmp->h; ++y) {
		for (size_t x = 0; x != (size_t)tmp->w / 2; ++x) {
			std::swap(pixels[x], pixels[tmp->w - 1 - x]);
		}
		pixels += tmp->pitch / 4;
	}
	tmp->unlock();
	return tmp;
}

template<typename load_data_file_F>
void load_image_data(image_data& img, load_data_file_F&& load_data_file) {

	std::array<int, 0x100> creep_edge_neighbors_index{};
	std::array<int, 128> creep_edge_neighbors_index_n{};

	for (size_t i = 0; i != 0x100; ++i) {
		int v = 0;
		if (i & 2) v |= 0x10;
		if (i & 8) v |= 0x24;
		if (i & 0x10) v |= 9;
		if (i & 0x40) v |= 2;
		if ((i & 0xc0) == 0xc0) v |= 1;
		if ((i & 0x60) == 0x60) v |= 4;
		if ((i & 3) == 3) v |= 0x20;
		if ((i & 6) == 6) v |= 8;
		if ((v & 0x21) == 0x21) v |= 0x40;
		if ((v & 0xc) == 0xc) v |= 0x40;
		creep_edge_neighbors_index[i] = v;
	}

	int n = 0;
	for (int i = 0; i != 128; ++i) {
		auto it = std::find(creep_edge_neighbors_index.begin(), creep_edge_neighbors_index.end(), i);
		if (it == creep_edge_neighbors_index.end()) continue;
		creep_edge_neighbors_index_n[i] = n;
		++n;
	}

	for (size_t i = 0; i != 0x100; ++i) {
		img.creep_edge_frame_index[i] = creep_edge_neighbors_index_n[creep_edge_neighbors_index[i]];
	}

	a_vector<uint8_t> tmp_data;
	auto load_pcx_file = [&](a_string filename) {
		load_data_file(tmp_data, std::move(filename));
		return load_pcx_data(tmp_data);
	};

	auto tunit_pcx = load_pcx_file("game/tunit.pcx");
	if (tunit_pcx.width != 128 || tunit_pcx.height != 1) error("tunit.pcx dimensions are %dx%d (128x1 required)", tunit_pcx.width, tunit_pcx.height);
	for (size_t i = 0; i != 16; ++i) {
		for (size_t i2 = 0; i2 != 8; ++i2) {
			img.player_unit_colors[i][i2] = tunit_pcx.data[i * 8 + i2];
		}
	}
	auto tminimap_pcx = load_pcx_file("game/tminimap.pcx");
	if (tminimap_pcx.width != 16 || tminimap_pcx.height != 1) error("tminimap.pcx dimensions are %dx%d (16x1 required)", tminimap_pcx.width, tminimap_pcx.height);
	for (size_t i = 0; i != 16; ++i) {
		img.player_minimap_colors[i] = tminimap_pcx.data[i];
	}
	auto tselect_pcx = load_pcx_file("game/tselect.pcx");
	if (tselect_pcx.width != 24 || tselect_pcx.height != 1) error("tselect.pcx dimensions are %dx%d (24x1 required)", tselect_pcx.width, tselect_pcx.height);
	for (size_t i = 0; i != 24; ++i) {
		img.selection_colors[i] = tselect_pcx.data[i];
	}
	auto thpbar_pcx = load_pcx_file("game/thpbar.pcx");
	if (thpbar_pcx.width != 19 || thpbar_pcx.height != 1) error("thpbar.pcx dimensions are %dx%d (19x1 required)", thpbar_pcx.width, thpbar_pcx.height);
	for (size_t i = 0; i != 19; ++i) {
		img.hp_bar_colors[i] = thpbar_pcx.data[i];
	}

}

template<typename load_data_file_F>
void load_tileset_image_data(tileset_image_data& img, size_t tileset_index, load_data_file_F&& load_data_file) {
	using namespace data_loading;

	std::array<const char*, 8> tileset_names = {
		"badlands", "platform", "install", "AshWorld", "Jungle", "Desert", "Ice", "Twilight"
	};

	a_vector<uint8_t> vr4_data;
	a_vector<uint8_t> vx4_data;

	const char* tileset_name = tileset_names.at(tileset_index);

	load_data_file(vr4_data, format("Tileset/%s.vr4", tileset_name));
	load_data_file(vx4_data, format("Tileset/%s.vx4", tileset_name));
	load_data_file(img.wpe, format("Tileset/%s.wpe", tileset_name));

	a_vector<uint8_t> grp_data;
	load_data_file(grp_data, format("Tileset/%s.grp", tileset_name));
	img.creep_grp = read_grp(data_loading::data_reader_le(grp_data.data(), grp_data.data() + grp_data.size()));

	data_reader<true, false> vr4_r(vr4_data.data(), nullptr);
	img.vr4.resize(vr4_data.size() / 64);
	for (size_t i = 0; i != img.vr4.size(); ++i) {
		for (size_t i2 = 0; i2 != 8; ++i2) {
			auto r2 = vr4_r;
			auto v = vr4_r.get<uint64_t, true>();
			auto iv = r2.get<uint64_t, false>();
			size_t n = 8 / sizeof(vr4_entry::bitmap_t);
			for (size_t i3 = 0; i3 != n; ++i3) {
				img.vr4[i].bitmap[i2 * n + i3] = (vr4_entry::bitmap_t)v;
				img.vr4[i].inverted_bitmap[i2 * n + i3] = (vr4_entry::bitmap_t)iv;
				v >>= n == 1 ? 0 : 8 * sizeof(vr4_entry::bitmap_t);
				iv >>= n == 1 ? 0 : 8 * sizeof(vr4_entry::bitmap_t);
			}
		}
	}
	data_reader<true, false> vx4_r(vx4_data.data(), vx4_data.data() + vx4_data.size());
	img.vx4.resize(vx4_data.size() / 32);
	for (size_t i = 0; i != img.vx4.size(); ++i) {
		for (size_t i2 = 0; i2 != 16; ++i2) {
			img.vx4[i].images[i2] = vx4_r.get<uint16_t>();
		}
	}

	a_vector<uint8_t> tmp_data;
	auto load_pcx_file = [&](a_string filename) {
		load_data_file(tmp_data, std::move(filename));
		return load_pcx_data(tmp_data);
	};

	img.dark_pcx = load_pcx_file(format("Tileset/%s/dark.pcx", tileset_name));
	if (img.dark_pcx.width != 256 || img.dark_pcx.height != 32) error("invalid dark.pcx");
	for (size_t x = 0; x != 256; ++x) {
		img.dark_pcx.data[256 * 31 + x] = (uint8_t)x;
	}

	std::array<const char*, 7> light_names = {"ofire", "gfire", "bfire", "bexpl", "trans50", "red", "green"};
	for (size_t i = 0; i != 7; ++i) {
		img.light_pcx[i] = load_pcx_file(format("Tileset/%s/%s.pcx", tileset_name, light_names[i]));
	}

	if (img.wpe.size() != 256 * 4) error("wpe size invalid (%d)", img.wpe.size());

	auto get_nearest_color = [&](int r, int g, int b) {
		size_t best_index = -1;
		int best_score{};
		for (size_t i = 0; i != 256; ++i) {
			int dr = r - img.wpe[4 * i + 0];
			int dg = g - img.wpe[4 * i + 1];
			int db = b - img.wpe[4 * i + 2];
			int score = dr * dr + dg * dg + db * db;
			if (best_index == (size_t)-1 || score < best_score) {
				best_index = i;
				best_score = score;
			}
		}
		return best_index;
	};
	img.resource_minimap_color = get_nearest_color(0, 255, 255);

	for (size_t i = 0; i != 256; ++i) {
		int r = img.wpe[4 * i + 0];
		int g = img.wpe[4 * i + 1];
		int b = img.wpe[4 * i + 2];
		int strength = (r * 28 + g * 77 + b * 151 + 4096) / 8192;
		img.cloak_fade_selector[i] = strength;
	}

}

template<bool bounds_check>
void draw_tile(tileset_image_data& img, size_t megatile_index, uint8_t* dst, size_t pitch, size_t offset_x, size_t offset_y, size_t width, size_t height) {
	auto* images = &img.vx4.at(megatile_index).images[0];
	size_t x = 0;
	size_t y = 0;
	for (size_t image_iy = 0; image_iy != 4; ++image_iy) {
		for (size_t image_ix = 0; image_ix != 4; ++image_ix) {
			auto image_index = *images;
			bool inverted = (image_index & 1) == 1;
			auto* bitmap = inverted ? &img.vr4.at(image_index / 2).inverted_bitmap[0] : &img.vr4.at(image_index / 2).bitmap[0];

			for (size_t iy = 0; iy != 8; ++iy) {
				for (size_t iv = 0; iv != 8 / sizeof(vr4_entry::bitmap_t); ++iv) {
					for (size_t b = 0; b != sizeof(vr4_entry::bitmap_t); ++b) {
						if (!bounds_check || (x >= offset_x && y >= offset_y && x < width && y < height)) {
							*dst = (uint8_t)(*bitmap >> (8 * b));
						}
						++dst;
						++x;
					}
					++bitmap;
				}
				x -= 8;
				++y;
				dst -= 8;
				dst += pitch;
			}
			x += 8;
			y -= 8;
			dst -= pitch * 8;
			dst += 8;
			++images;
		}
		x -= 32;
		y += 8;
		dst += pitch * 8;
		dst -= 32;
	}
}

static inline void draw_tile(tileset_image_data& img, size_t megatile_index, uint8_t* dst, size_t pitch, size_t offset_x, size_t offset_y, size_t width, size_t height) {
	if (offset_x == 0 && offset_y == 0 && width == 32 && height == 32) {
		draw_tile<false>(img, megatile_index, dst, pitch, offset_x, offset_y, width, height);
	} else {
		draw_tile<true>(img, megatile_index, dst, pitch, offset_x, offset_y, width, height);
	}
}

template<bool bounds_check, bool flipped, bool textured, typename remap_F>
void draw_frame(const grp_t::frame_t& frame, const uint8_t* texture, uint8_t* dst, size_t pitch, size_t offset_x, size_t offset_y, size_t width, size_t height, remap_F&& remap_f) {
	for (size_t y = 0; y != offset_y; ++y) {
		dst += pitch;
		if (textured) texture += frame.size.x;
	}

	for (size_t y = offset_y; y != height; ++y) {

		if (flipped) dst += frame.size.x - 1;
		if (textured && flipped) texture += frame.size.x - 1;

		const uint8_t* d = frame.data_container.data() + frame.line_data_offset.at(y);
		for (size_t x = flipped ? frame.size.x - 1 : 0; x != (flipped ? (size_t)0 - 1 : frame.size.x);) {
			int v = *d++;
			if (v & 0x80) {
				v &= 0x7f;
				x += flipped ? -v : v;
				dst += flipped ? -v : v;
				if (textured) texture += flipped ? -v : v;
			} else if (v & 0x40) {
				v &= 0x3f;
				int c = *d++;
				for (;v; --v) {
					if (!bounds_check || (x >= offset_x && x < width)) {
						*dst = remap_f(textured ? *texture : c, *dst);
					}
					dst += flipped ? -1 : 1;
					x += flipped ? -1 : 1;
					if (textured) texture += flipped ? -1 : 1;
				}
			} else {
				for (;v; --v) {
					int c = *d++;
					if (!bounds_check || (x >= offset_x && x < width)) {
						*dst = remap_f(textured ? *texture : c, *dst);
					}
					dst += flipped ? -1 : 1;
					x += flipped ? -1 : 1;
					if (textured) texture += flipped ? -1 : 1;
				}
			}
		}

		if (!flipped) dst -= frame.size.x;
		else ++dst;
		dst += pitch;
		if (textured) {
			if (!flipped) texture -= frame.size.x;
			else ++texture;
			texture += frame.size.x;
		}

	}
}

struct no_remap {
	uint8_t operator()(uint8_t new_value, uint8_t old_value) const {
		return new_value;
	}
};

template<typename remap_F = no_remap>
void draw_frame(const grp_t::frame_t& frame, bool flipped, uint8_t* dst, size_t pitch, size_t offset_x, size_t offset_y, size_t width, size_t height, remap_F&& remap_f = remap_F()) {
	if (offset_x == 0 && offset_y == 0 && width == frame.size.x && height == frame.size.y) {
		if (flipped) draw_frame<false, true, false>(frame, nullptr, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
		else draw_frame<false, false, false>(frame, nullptr, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
	} else {
		if (flipped) draw_frame<true, true, false>(frame, nullptr, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
		else draw_frame<true, false, false>(frame, nullptr, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
	}
}

template<typename remap_F = no_remap>
void draw_frame_textured(const grp_t::frame_t& frame, const uint8_t* texture, bool flipped, uint8_t* dst, size_t pitch, size_t offset_x, size_t offset_y, size_t width, size_t height, remap_F&& remap_f = remap_F()) {
	if (offset_x == 0 && offset_y == 0 && width == frame.size.x && height == frame.size.y) {
		if (flipped) draw_frame<false, true, true>(frame, texture, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
		else draw_frame<false, false, true>(frame, texture, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
	} else {
		if (flipped) draw_frame<true, true, true>(frame, texture, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
		else draw_frame<true, false, true>(frame, texture, dst, pitch, offset_x, offset_y, width, height, std::forward<remap_F>(remap_f));
	}
}

struct apm_t {
	a_deque<int> history;
	int current_apm = 0;
	int last_frame_div = 0;
	static const int resolution = 1;
	void add_action(int frame) {
		if (!history.empty() && frame / resolution == last_frame_div) {
			++history.back();
		} else {
			if (history.size() >= 10 * 1000 / 42 / resolution) history.pop_front();
			history.push_back(1);
			last_frame_div = frame / 12;
		}
	}
	void update(int frame) {
		if (history.empty() || frame / resolution != last_frame_div) {
			if (history.size() >= 10 * 1000 / 42 / resolution) history.pop_front();
			history.push_back(0);
			last_frame_div = frame / resolution;
		}
		if (frame % resolution) return;
		if (history.size() == 0) {
			current_apm = 0;
			return;
		}
		int sum = 0;
		for (auto& v : history) sum += v;
		current_apm = (int)(sum * ((int64_t)256 * 60 * 1000 / 42 / resolution) / history.size() / 256);
	}
};

struct ui_util_functions: replay_functions {

	explicit ui_util_functions(state& st, action_state& action_st, replay_state& replay_st) : replay_functions(st, action_st, replay_st) {}

	rect sprite_clickable_bounds(const sprite_t* sprite) const {
		rect r{{(int)game_st.map_width - 1, (int)game_st.map_height - 1}, {0, 0}};
		for (const image_t* image : ptr(sprite->images)) {
			if (!i_flag(image, image_t::flag_clickable)) continue;
			xy pos = get_image_map_position(image);
			auto size = image->grp->frames.at(image->frame_index).size;
			xy to = pos + xy((int)size.x, (int)size.y);
			if (pos.x < r.from.x) r.from.x = pos.x;
			if (pos.y < r.from.y) r.from.y = pos.y;
			if (to.x > r.to.x) r.to.x = to.x;
			if (to.y > r.to.y) r.to.y = to.y;
		}
		return r;
	}

	bool unit_can_be_selected(const unit_t* u) const {
		if (unit_is(u, UnitTypes::Terran_Nuclear_Missile)) return false;
		if (unit_is(u, UnitTypes::Protoss_Scarab)) return false;
		if (unit_is(u, UnitTypes::Spell_Disruption_Web)) return false;
		if (unit_is(u, UnitTypes::Spell_Dark_Swarm)) return false;
		if (unit_is(u, UnitTypes::Special_Upper_Level_Door)) return false;
		if (unit_is(u, UnitTypes::Special_Right_Upper_Level_Door)) return false;
		if (unit_is(u, UnitTypes::Special_Pit_Door)) return false;
		if (unit_is(u, UnitTypes::Special_Right_Pit_Door)) return false;
		return true;
	}

	bool image_has_data_at(const image_t* image, xy pos) const {
		auto& frame = image->grp->frames.at(image->frame_index);
		xy map_pos = get_image_map_position(image);
		int x = pos.x - map_pos.x;
		if (i_flag(image, image_t::flag_horizontally_flipped)) x = image->grp->width - 2 * frame.offset.x - x;
		int y = pos.y - map_pos.y;
		if ((size_t)x >= frame.size.x) return false;
		if ((size_t)y >= frame.size.y) return false;

		const uint8_t* d = frame.data_container.data() + frame.line_data_offset.at(y);
		while (x > 0) {
			int v = *d++;
			if (v & 0x80) {
				v &= 0x7f;
				x -= v;
				if (x <= 0) return false;
			} else if (v & 0x40) {
				v &= 0x3f;
				d++;
				x -= v;
			} else {
				x -= v;
			}
		}
		return true;
	}

	bool unit_has_clickable_image_data_at(const unit_t* u, xy pos) const {
		if (!is_in_bounds(pos, sprite_clickable_bounds(u->sprite))) return false;
		if (ut_flag(u, unit_type_t::flag_100)) {
			for (const image_t* image : ptr(u->sprite->images)) {
				if (!i_flag(image, image_t::flag_clickable)) continue;
				if (image_has_data_at(image, pos)) return true;
			}
			return false;
		} else {
			return image_has_data_at(u->sprite->main_image, pos);
		}
	}

	unit_t* select_get_unit_at(xy pos) const {
		rect area = square_at(pos, 32);
		area.to += xy(game_st.max_unit_width, game_st.max_unit_height);
		unit_t* best_unit = nullptr;
		int best_unit_size{};
		for (unit_t* u : find_units_noexpand(area)) {
			if (!is_in_bounds(pos, sprite_clickable_bounds(u->sprite))) continue;
			u = unit_main_unit(u);
			if (!unit_can_be_selected(u)) continue;
			if (!best_unit) {
				best_unit = u;
				best_unit_size = u->unit_type->placement_size.x * u->unit_type->placement_size.y;
				continue;
			}
			if (sprite_depth_order(u->sprite) >= sprite_depth_order(best_unit->sprite)) {
				if (unit_has_clickable_image_data_at(u, pos) || (u->subunit && unit_has_clickable_image_data_at(u->subunit, pos))) {
					best_unit = u;
					best_unit_size = u->sprite->width * u->sprite->height;
					continue;
				}
			} else {
				if (unit_has_clickable_image_data_at(best_unit, pos)) continue;
				if (best_unit->subunit && unit_has_clickable_image_data_at(best_unit->subunit, pos)) continue;
			}
			if (u->unit_type->placement_size.x * u->unit_type->placement_size.y < best_unit_size) {
				best_unit = u;
				best_unit_size = u->unit_type->placement_size.x * u->unit_type->placement_size.y;
			}
		}
		return best_unit;
	}

	uint32_t sprite_depth_order(const sprite_t* sprite) const {
		uint32_t score = 0;
		score |= sprite->elevation_level;
		score <<= 13;
		score |= sprite->elevation_level <= 4 ? sprite->position.y : 0;
		score <<= 1;
		score |= s_flag(sprite, sprite_t::flag_turret) ? 1 : 0;
		return score;
	}

};

struct ui_functions: ui_util_functions {
	image_data img;
	tileset_image_data tileset_img;
	native_window::window wnd;
	bool create_window = true;
	bool draw_ui_elements = true;

	bool exit_on_close = true;
	bool window_closed = false;
	
	// 명령 모드 시각화용 (player_game에서 설정)
	int current_command_mode = 0;  // 0=Normal, 1=Attack, 2=Move, 3=Patrol, 4=Hold, 5=Build

	xy screen_pos;

	size_t screen_width;
	size_t screen_height;

	game_player player;
	replay_state current_replay_state;
	action_state current_action_state;
	std::array<apm_t, 12> apm;
	ui_functions(game_player player) : ui_util_functions(player.st(), current_action_state, current_replay_state), player(std::move(player)) {
	}

	std::function<void(a_vector<uint8_t>&, a_string)> load_data_file;

	sound_types_t sound_types;
	a_vector<a_string> sound_filenames;

	const sound_type_t* get_sound_type(Sounds id) const {
		if ((size_t)id >= (size_t)Sounds::None) error("invalid sound id %d", (size_t)id);
		return &sound_types.vec[(size_t)id];
	}

	a_vector<bool> has_loaded_sound;
	a_vector<std::unique_ptr<native_sound::sound>> loaded_sounds;
	a_vector<std::chrono::high_resolution_clock::time_point> last_played_sound;

	int global_volume = 50;

	struct sound_channel {
		bool playing = false;
		const sound_type_t* sound_type = nullptr;
		int priority = 0;
		int flags = 0;
		const unit_type_t* unit_type = nullptr;
		int volume = 0;
	};
	a_vector<sound_channel> sound_channels;

	void set_volume(int volume) {
		if (volume < 0) volume = 0;
		else if (volume > 100) volume = 100;
		global_volume = volume;
		for (auto& c : sound_channels) {
			if (c.playing) {
				native_sound::set_volume(&c - sound_channels.data(), (128 - 4) * (c.volume * global_volume / 100) / 100);
			}
		}
	}

	sound_channel* get_sound_channel(int priority) {
		sound_channel* r = nullptr;
		for (auto& c : sound_channels) {
			if (c.playing) {
				if (!native_sound::is_playing(&c - sound_channels.data())) {
					c.playing = false;
					r = &c;
				}
			} else r = &c;
		}
		if (r) return r;
		int best_prio = priority;
		for (auto& c : sound_channels) {
			if (c.flags & 0x20) continue;
			if (c.priority < best_prio) {
				best_prio = c.priority;
				r = &c;
			}
		}
		return r;
	}

	virtual void play_sound(int id, xy position, const unit_t* source_unit, bool add_race_index) override {
		if (global_volume == 0) return;
		if (add_race_index) id += 1;
		if ((size_t)id >= has_loaded_sound.size()) return;
		if (!has_loaded_sound[id]) {
			has_loaded_sound[id] = true;
			a_vector<uint8_t> data;
			load_data_file(data, "sound/" + sound_filenames[id]);
			loaded_sounds[id] = native_sound::load_wav(data.data(), data.size());
		}
		auto& s = loaded_sounds[id];
		if (!s) return;

		auto now = clock.now();
		if (now - last_played_sound[id] <= std::chrono::milliseconds(80)) return;
		last_played_sound[id] = now;

		const sound_type_t* sound_type = get_sound_type((Sounds)id);

		int volume = sound_type->min_volume;

		if (position != xy()) {
			int distance = 0;
			if (position.x < screen_pos.x) distance += screen_pos.x - position.x;
			else if (position.x > screen_pos.x + (int)screen_width) distance += position.x - (screen_pos.x + (int)screen_width);
			if (position.y < screen_pos.y) distance += screen_pos.y - position.y;
			else if (position.y > screen_pos.y + (int)screen_height) distance += position.y - (screen_pos.y + (int)screen_height);

			int distance_volume = 99 - 99 * distance / 512;

			if (distance_volume > volume) volume = distance_volume;
		}

		if (volume > 10) {
			int pan = 0;
//			if (position != xy()) {
//				int pan_x = (position.x - (screen_pos.x + (int)screen_width / 2)) / 32;
//				bool left = pan_x < 0;
//				if (left) pan_x = -pan_x;
//				if (pan_x <= 2) pan = 0;
//				else if (pan_x <= 5) pan = 52;
//				else if (pan_x <= 10) pan = 127;
//				else if (pan_x <= 20) pan = 191;
//				else if (pan_x <= 40) pan = 230;
//				else pan = 255;
//				if (left) pan = -pan;
//			}

			const unit_type_t* unit_type = source_unit ? source_unit->unit_type : nullptr;

			if (sound_type->flags & 0x10) {
				for (auto& c : sound_channels) {
					if (c.playing && c.sound_type == sound_type) {
						if (native_sound::is_playing(&c - sound_channels.data())) return;
						c.playing = false;
					}
				}
			} else if (sound_type->flags & 2 && unit_type) {
				for (auto& c : sound_channels) {
					if (c.playing && c.unit_type == unit_type && c.flags & 2) {
						if (native_sound::is_playing(&c - sound_channels.data())) return;
						c.playing = false;
					}
				}
			}

			auto* c = get_sound_channel(sound_type->priority);
			if (c) {
				native_sound::play(c - sound_channels.data(), &*s, (128 - 4) * (volume * global_volume / 100) / 100, pan);
				c->playing = true;
				c->sound_type = sound_type;
				c->flags = sound_type->flags;
				c->priority = sound_type->priority;
				c->unit_type = unit_type;
				c->volume = volume;
			}
		}
	}

	a_vector<uint8_t> creep_random_tile_indices = a_vector<uint8_t>(256 * 256);
	void init() {
		uint32_t rand_state = (uint32_t)clock.now().time_since_epoch().count();
		auto rand = [&]() {
			rand_state = rand_state * 22695477 + 1;
			return (rand_state >> 16) & 0x7fff;
		};
		for (auto& v : creep_random_tile_indices) {
			if (rand() % 100 < 4) v = 6 + rand() % 7;
			else v = rand() % 6;
		}

		a_vector<uint8_t> data;
		load_data_file(data, "arr/sfxdata.dat");
		sound_types = data_loading::load_sfxdata_dat(data);

		sound_filenames.resize(sound_types.vec.size());
		has_loaded_sound.resize(sound_filenames.size());
		loaded_sounds.resize(has_loaded_sound.size());
		last_played_sound.resize(loaded_sounds.size());

		string_table_data tbl;
		load_data_file(tbl.data, "arr/sfxdata.tbl");
		for (size_t i = 0; i != sound_types.vec.size(); ++i) {
			size_t index = sound_types.vec[i].filename_index;
			sound_filenames[i] = tbl[index];
		}
		native_sound::frequency = 44100;
		native_sound::channels = 8;
		native_sound::init();

		sound_channels.resize(8);

		load_data_file(images_tbl.data, "arr/images.tbl");

		load_all_image_data(load_data_file);
	}

	virtual void on_action(int owner, int action) override {
		apm.at(owner).add_action(st.current_frame);
	}

	size_t view_width;
	size_t view_height;
	fp16 view_scale;

	rect_t<xy_t<size_t>> screen_tile_bounds() {
		size_t from_tile_y = screen_pos.y / 32u;
		if (from_tile_y >= game_st.map_tile_height) from_tile_y = 0;
		size_t to_tile_y = (screen_pos.y + view_height + 31) / 32u;
		if (to_tile_y > game_st.map_tile_height) to_tile_y = game_st.map_tile_height;
		size_t from_tile_x = screen_pos.x / 32u;
		if (from_tile_x >= game_st.map_tile_width) from_tile_x = 0;
		size_t to_tile_x = (screen_pos.x + view_width + 31) / 32u;
		if (to_tile_x > game_st.map_tile_width) to_tile_x = game_st.map_tile_width;

		return {{from_tile_x, from_tile_y}, {to_tile_x, to_tile_y}};
	}


	void draw_tiles(uint8_t* data, size_t data_pitch) {

		auto screen_tile = screen_tile_bounds();

		size_t tile_index = screen_tile.from.y * game_st.map_tile_width + screen_tile.from.x;
		auto* megatile_index = &st.tiles_mega_tile_index[tile_index];
		auto* tile = &st.tiles[tile_index];
		size_t width = screen_tile.to.x - screen_tile.from.x;

		xy dirs[9] = {{1, 1}, {0, 1}, {-1, 1}, {1, 0}, {-1, 0}, {1, -1}, {0, -1}, {-1, -1}, {0, 0}};

		for (size_t tile_y = screen_tile.from.y; tile_y != screen_tile.to.y; ++tile_y) {
			for (size_t tile_x = screen_tile.from.x; tile_x != screen_tile.to.x; ++tile_x) {

				int screen_x = tile_x * 32 - screen_pos.x;
				int screen_y = tile_y * 32 - screen_pos.y;

				size_t offset_x = 0;
				size_t offset_y = 0;
				if (screen_x < 0) {
					offset_x = -screen_x;
				}
				if (screen_y < 0) {
					offset_y = -screen_y;
				}

				uint8_t* dst = data + screen_y * data_pitch + screen_x;

				size_t width = 32;
				size_t height = 32;

				width = std::min(width, screen_width - screen_x);
				height = std::min(height, screen_height - screen_y);

				size_t index = *megatile_index;
				if (tile->flags & tile_t::flag_has_creep) {
					index = game_st.cv5.at(1).mega_tile_index[creep_random_tile_indices[tile_x + tile_y * game_st.map_tile_width]];
				}
				draw_tile(tileset_img, index, dst, data_pitch, offset_x, offset_y, width, height);

				if (~tile->flags & tile_t::flag_has_creep) {
					size_t creep_index = 0;
					for (size_t i = 0; i != 9; ++i) {
						int add_x = dirs[i].x;
						int add_y = dirs[i].y;
						if (tile_x + add_x >= game_st.map_tile_width) continue;
						if (tile_y + add_y >= game_st.map_tile_height) continue;
						if (st.tiles[tile_x + add_x + (tile_y + add_y) * game_st.map_tile_width].flags & tile_t::flag_has_creep) creep_index |= 1 << i;
					}
					size_t creep_frame = img.creep_edge_frame_index[creep_index];

					if (creep_frame) {

						auto& frame = tileset_img.creep_grp.frames.at(creep_frame - 1);

						screen_x += frame.offset.x;
						screen_y += frame.offset.y;

						size_t width = frame.size.x;
						size_t height = frame.size.y;

						if (screen_x < (int)screen_width && screen_y < (int)screen_height) {
							if (screen_x + (int)width > 0 && screen_y + (int)height > 0) {

								size_t offset_x = 0;
								size_t offset_y = 0;
								if (screen_x < 0) {
									offset_x = -screen_x;
								}
								if (screen_y < 0) {
									offset_y = -screen_y;
								}

								uint8_t* dst = data + screen_y * data_pitch + screen_x;

								width = std::min(width, screen_width - screen_x);
								height = std::min(height, screen_height - screen_y);

								draw_frame(frame, false, dst, data_pitch, offset_x, offset_y, width, height);
							}
						}
					}
				}

				++megatile_index;
				++tile;
			}
			megatile_index -= width;
			megatile_index += game_st.map_tile_width;
			tile -= width;
			tile += game_st.map_tile_width;
		}
	}

	a_vector<uint8_t> temporary_warp_texture_buffer;

	void draw_image(const image_t* image, uint8_t* data, size_t data_pitch, size_t color_index) {

		if (is_new_image(image)) {
			image_draw_queue.push_back(image);
			return;
		}

		xy map_pos = get_image_map_position(image);

		int screen_x = map_pos.x - screen_pos.x;
		int screen_y = map_pos.y - screen_pos.y;

		if (screen_x >= (int)screen_width || screen_y >= (int)screen_height) return;

		auto& frame = image->grp->frames.at(image->frame_index);

		size_t width = frame.size.x;
		size_t height = frame.size.y;

		if (screen_x + (int)width <= 0 || screen_y + (int)height <= 0) return;

		size_t offset_x = 0;
		size_t offset_y = 0;
		if (screen_x < 0) {
			offset_x = -screen_x;
		}
		if (screen_y < 0) {
			offset_y = -screen_y;
		}

		uint8_t* dst = data + screen_y * data_pitch + screen_x;

		width = std::min(width, screen_width - screen_x);
		height = std::min(height, screen_height - screen_y);

		auto draw_alpha = [&](size_t index, auto remap_f) {
			auto& data = tileset_img.light_pcx.at(index).data;
			uint8_t* ptr = data.data();
			size_t size = data.size() / 256;
			auto glow = [ptr, size, remap_f](uint8_t new_value, uint8_t old_value) {
				new_value = remap_f(new_value, old_value);
				--new_value;
				if (new_value >= size) return (uint8_t)0;
				return ptr[256u * new_value + old_value];
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, glow);
		};

		if (image->modifier == 0 || image->modifier == 1) {
			uint8_t* ptr = img.player_unit_colors.at(color_index).data();
			auto player_color = [ptr](uint8_t new_value, uint8_t) {
				if (new_value >= 8 && new_value < 16) return ptr[new_value - 8];
				return new_value;
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, player_color);
		} else if (image->modifier == 2 || image->modifier == 4) {
			uint8_t* color_ptr = img.player_unit_colors.at(color_index).data();
			draw_alpha(4, [color_ptr](uint8_t new_value, uint8_t) {
				if (new_value >= 8 && new_value < 16) return color_ptr[new_value - 8];
				return new_value;
			});
			uint8_t* selector = tileset_img.cloak_fade_selector.data();
			int value = image->modifier_data1;
			auto cloaking = [color_ptr, selector, value](uint8_t new_value, uint8_t old_value) {
				if (selector[new_value] <= value) return old_value;
				if (new_value >= 8 && new_value < 16) return color_ptr[new_value - 8];
				return new_value;
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, cloaking);
		} else if (image->modifier == 3) {
			uint8_t* color_ptr = img.player_unit_colors.at(color_index).data();
			draw_alpha(4, [color_ptr](uint8_t new_value, uint8_t) {
				if (new_value >= 8 && new_value < 16) return color_ptr[new_value - 8];
				return new_value;
			});
		} else if (image->modifier == 8) {
			size_t data_size = data_pitch * screen_height;
			auto distortion = [data_size, dst](uint8_t new_value, uint8_t& old_value) {
				size_t offset = &old_value - dst;
				if (offset >= new_value && data_size - offset > new_value) return *(&old_value + new_value);
				return old_value;
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, distortion);
		} else if (image->modifier == 10) {
			uint8_t* ptr = &tileset_img.dark_pcx.data[256 * 18];
			auto shadow = [ptr](uint8_t, uint8_t old_value) {
				return ptr[old_value];
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, shadow);
		} else if (image->modifier == 9) {
			draw_alpha(image->image_type->color_shift - 1, no_remap());
		} else if (image->modifier == 12) {
			if (temporary_warp_texture_buffer.size() < frame.size.x * frame.size.y) temporary_warp_texture_buffer.resize(frame.size.x * frame.size.y);
			auto& texture_frame = global_st.image_grp[(size_t)ImageTypes::IMAGEID_Warp_Texture]->frames.at(image->modifier_data1);
			draw_frame(texture_frame, false, temporary_warp_texture_buffer.data(), frame.size.x, 0, 0, frame.size.x, frame.size.y);
			draw_frame_textured(frame, temporary_warp_texture_buffer.data(), i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height);
		} else if (image->modifier == 17) {
			auto& data = tileset_img.light_pcx.at(0).data;
			uint8_t* ptr = &data.at(256u * (image->modifier_data1 - 1));
			size_t size = data.data() + data.size() - ptr;
			auto glow = [ptr, size](uint8_t, uint8_t old_value) {
				if (old_value >= size) return (uint8_t)0;
				return ptr[old_value];
			};
			draw_frame(frame, i_flag(image, image_t::flag_horizontally_flipped), dst, data_pitch, offset_x, offset_y, width, height, glow);
		} else error("don't know how to draw image modifier %d", image->modifier);

	}

	a_vector<const unit_t*> current_selection_sprites_set = a_vector<const unit_t*>(2500);
	a_vector<const sprite_t*> current_selection_sprites;

	// target_kind: 0=self/ally, 1=enemy, 2=resource, -1=unknown
	// target_frames: remaining frames for this ring (for blink timing)
	void draw_selection_circle(const sprite_t* sprite, const unit_t* u, uint8_t* data, size_t pitch, bool is_target, int target_kind, int target_frames) {
		auto* image_type = get_image_type((ImageTypes)((int)ImageTypes::IMAGEID_Selection_Circle_22pixels + sprite->sprite_type->selection_circle));

		xy map_pos = sprite->position + xy(0, sprite->sprite_type->selection_circle_vpos);

		auto* grp = global_st.image_grp[(size_t)image_type->id];
		auto& frame = grp->frames.at(0);

		map_pos.x += int(frame.offset.x - grp->width / 2);
		map_pos.y += int(frame.offset.y - grp->height / 2);

		int screen_x = map_pos.x - screen_pos.x;
		int screen_y = map_pos.y - screen_pos.y;

		if (screen_x >= (int)screen_width || screen_y >= (int)screen_height) return;

		size_t width = frame.size.x;
		size_t height = frame.size.y;

		if (screen_x + (int)width <= 0 || screen_y + (int)height <= 0) return;

		size_t offset_x = 0;
		size_t offset_y = 0;
		if (screen_x < 0) {
			offset_x = -screen_x;
		}
		if (screen_y < 0) {
			offset_y = -screen_y;
		}

		uint8_t* dst = data + screen_y * pitch + screen_x;

		width = std::min(width, screen_width - screen_x);
		height = std::min(height, screen_height - screen_y);

		// 기본 선택 링 색상 계산
		size_t color_index = st.players[sprite->owner].color;
		uint8_t color = img.player_unit_colors.at(color_index)[0];
		if (u && (unit_is_mineral_field(u) || unit_is(u, UnitTypes::Resource_Vespene_Geyser))) {
			color = tileset_img.resource_minimap_color;
		}

		// 타겟 상태인 경우: 아군 초록, 적군 빨강, 자원은 기존 자원 색으로 선택 링 색상 고정
		if (is_target) {
			// target_kind: 0=self/ally, 1=enemy, 2=resource
			if (target_kind == 1) {
				// 적: 선명한 빨강 계열
				color = img.hp_bar_colors.at(6);
			} else if (target_kind == 2) {
				// 자원: 미니맵/자원 표시와 동일한 색 사용
				color = tileset_img.resource_minimap_color;
			} else {
				// 아군/자신: 초록 계열
				color = img.hp_bar_colors.at(2);
			}
			// 선택 링은 링 수명 동안 항상 표시 (target_frames>0),
			// 수명이 0이 되면 자연스럽게 사라짐
			// (별도의 깜빡임 효과 없음)
			ui::log("[RING_DRAW_TARGET] owner=%d type=%d pos=(%d,%d) kind=%d frames=%d\n",
				sprite->owner,
				u ? (int)u->unit_type->id : -1,
				sprite->position.x,
				sprite->position.y,
				target_kind,
				target_frames);
		} else if (u) {
			// 일반 선택 링 - 3초마다만 로그 출력
			static auto last_selection_log = std::chrono::high_resolution_clock::now();
			auto now_sel = std::chrono::high_resolution_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now_sel - last_selection_log).count() >= 3) {
				ui::log("[RING_DRAW_SELECTION] owner=%d type=%d pos=(%d,%d)\n",
					sprite->owner,
					(int)u->unit_type->id,
					sprite->position.x,
					sprite->position.y);
				last_selection_log = now_sel;
			}
		}
		auto player_color = [color](uint8_t new_value, uint8_t) {
			if (new_value >= 0 && new_value < 8) return color;
			return new_value;
		};
		draw_frame(frame, false, dst, pitch, offset_x, offset_y, width, height, player_color);

	}

	void draw_health_bars(const sprite_t* sprite, const unit_t* u, uint8_t* data, size_t pitch) {

		auto* selection_circle_image_type = get_image_type((ImageTypes)((int)ImageTypes::IMAGEID_Selection_Circle_22pixels + sprite->sprite_type->selection_circle));

		auto* selection_circle_grp = global_st.image_grp[(size_t)selection_circle_image_type->id];
		auto& selection_circle_frame = selection_circle_grp->frames.at(0);

		int offsety = sprite->sprite_type->selection_circle_vpos + selection_circle_frame.size.y / 2 + 8;

		bool has_shield = u->unit_type->has_shield;
		bool has_energy = ut_has_energy(u) || u_hallucination(u) || unit_is(u, UnitTypes::Zerg_Broodling);

		int width = sprite->sprite_type->health_bar_size;
		width -= (width - 1) % 3;
		if (width < 19) width = 19;
		int orig_width = width;
		int height = 5;
		if (has_shield) height += 2;
		if (has_energy) height += 6;

		xy map_pos = sprite->position + xy(0, offsety);

		map_pos.x += int(0 - width / 2);
		map_pos.y += int(0 - height / 2);

		int screen_x = map_pos.x - screen_pos.x;
		int screen_y = map_pos.y - screen_pos.y;

		if (screen_x >= (int)screen_width || screen_y >= (int)screen_height) return;
		if (screen_x + width <= 0 || screen_y + height <= 0) return;

		auto filled_width = [&](int percent) {
			int r = percent * width / 100;
			if (r < 3) r = 3;
			else if (r % 3) {
				if (r % 3 > 1) r += 3 - (r % 3);
				else r -= r % 3;
			}
			return r;
		};

		int hp_percent = unit_hp_percent(u);
		int dw = filled_width(hp_percent);

		int shield_dw = 0;
		if (has_shield) {
			int shield_percent = (int)u->shield_points.integer_part() * 100 / std::max(u->unit_type->shield_points, 1);
			shield_dw = filled_width(shield_percent);
		}

		int energy_dw = 0;
		if (has_energy) {
			int energy_percent;
			if (ut_has_energy(u)) energy_percent = (int)u->energy.integer_part() * 100 / std::max((int)unit_max_energy(u).integer_part(), 1);
			else energy_percent = (int)u->remove_timer / std::max((int)default_remove_timer(u), 1);
			energy_dw = filled_width(energy_percent);
		}

		const int no_shield_colors_66[] = {18, 0, 1, 2, 18};
		const int no_shield_colors_33[] = {18, 3, 4, 5, 18};
		const int no_shield_colors_0[] = {18, 6, 7, 8, 18};
		const int no_shield_colors_bg[] = {18, 15, 16, 17, 18};

		const int with_shield_colors_66[] = {18, 0, 0, 1, 1, 2, 18};
		const int with_shield_colors_33[] = {18, 3, 3, 4, 4, 5, 18};
		const int with_shield_colors_0[] = {18, 6, 6, 7, 7, 8, 18};
		const int with_shield_colors_bg[] = {18, 15, 15, 16, 16, 17, 18};

		const int* colors_66 = has_shield ? with_shield_colors_66 : no_shield_colors_66;
		const int* colors_33 = has_shield ? with_shield_colors_33 : no_shield_colors_33;
		const int* colors_0 = has_shield ? with_shield_colors_0 : no_shield_colors_0;
		const int* colors_bg = has_shield ? with_shield_colors_bg : no_shield_colors_bg;

		int offset_x = 0;
		int offset_y = 0;
		if (screen_x < 0) {
			offset_x = -screen_x;
			dw = std::max(dw + screen_x, 0);
			shield_dw = std::max(shield_dw + screen_x, 0);
			energy_dw = std::max(energy_dw + screen_x, 0);
			width = std::max(width + screen_x, 0);
			screen_x = 0;
		}
		if (screen_y < 0) {
			offset_y = -screen_y;
			height += screen_y;
			screen_y = 0;
		}

		uint8_t* dst = data + screen_y * pitch + screen_x;

		width = std::min(width, (int)screen_width - screen_x);
		height = std::min(height, (int)screen_height - screen_y);

		if (dw > width) dw = width;
		if (shield_dw > width) shield_dw = width;
		if (energy_dw > width) energy_dw = width;

		int hp_height = std::min(std::max((has_shield ? 7 : 5) - offset_y, 0), height);

		for (int i = offset_y; i < offset_y + hp_height; ++i) {
			int ci = hp_percent >= 66 ? colors_66[i] : hp_percent >= 33 ? colors_33[i] : colors_0[i];
			int c = img.hp_bar_colors.at(ci);

			if (dw > 0) memset(dst, c, dw);
			if (width - dw > 0) {
				c = img.hp_bar_colors.at(colors_bg[i]);
				memset(dst + dw, c, width - dw);
			}
			dst += pitch;
		}

		if (has_shield) {
			const int shield_colors[] = {18, 10, 11, 18};
			const int shield_colors_bg[] = {18, 16, 17, 18};

			dst = data + screen_y * pitch + screen_x;

			for (int i = offset_y; i < std::min(4, height); ++i) {
				int c = img.hp_bar_colors.at(shield_colors[i]);

				if (shield_dw > 0) memset(dst, c, shield_dw);
				if (width - shield_dw > 0) {
					c = img.hp_bar_colors.at(shield_colors_bg[i]);
					memset(dst + shield_dw, c, width - shield_dw);
				}
				dst += pitch;
			}
		}

		int energy_offset = std::max((has_shield ? 8 : 6) - offset_y, 0);
		int energy_begin = std::max(offset_y - (has_shield ? 8 : 6), 0);
		int energy_end = std::min(5, offset_y + height - (has_shield ? 8 : 6));

		if (has_energy ) {
			dst = data + (screen_y + energy_offset) * pitch + screen_x;
			const int energy_colors[] = {18, 12, 13, 14, 18};
			for (int i = energy_begin; i < energy_end; ++i) {
				int c = img.hp_bar_colors.at(energy_colors[i]);

				if (energy_dw > 0) memset(dst, c, energy_dw);
				if (width - energy_dw > 0) {
					c = img.hp_bar_colors.at(no_shield_colors_bg[i]);
					memset(dst + energy_dw, c, width - energy_dw);
				}
				dst += pitch;
			}
		}

		dst = data + screen_y * pitch + screen_x;
		if (offset_x % 3) dst += 3 - offset_x % 3;

		int c = img.hp_bar_colors.at(18);
		for (int x = 0; x < orig_width; x += 3) {
			if (x < offset_x || x >= offset_x + width) continue;
			for (int y = 0; y != hp_height; ++y) {
				*dst = c;
				dst += pitch;
			}
			if (has_energy && energy_end > energy_begin) {
				if (energy_offset) dst += pitch;
				for (int i = energy_begin; i != energy_end; ++i) {
					*dst = c;
					dst += pitch;
				}
				if (energy_offset) dst -= pitch;
				dst -= (energy_end - energy_begin) * pitch;
			}
			dst -= hp_height * pitch;
			dst += 3;
		}

	}

	void draw_sprite(const sprite_t* sprite, uint8_t* data, size_t pitch) {
		const unit_t* draw_selection_u = current_selection_sprites_set.at(sprite->index);
		const unit_t* draw_health_bars_u = draw_selection_u;

		// 유닛/건물/자원 스프라이트만 타겟 링 후보로 허용 (투사체/이펙트/그림자 제외)
		const image_t* main_img = sprite->main_image;
		bool sprite_clickable = (main_img && main_img->image_type && main_img->image_type->is_clickable);

		// 현재 스프라이트가 타겟 링 위치와 일치하는지 검사 (좌표 기반)
		bool is_target = false;
		int target_kind = -1;
		int target_frames = 0;
		// 멀티 타겟 링이 있으면 우선적으로 모두 검사
		if (sprite_clickable && !g_target_rings.empty()) {
			for (const auto& ring : g_target_rings) {
				if (ring.frames <= 0) continue;
				int dx = sprite->position.x - ring.x;
				int dy = sprite->position.y - ring.y;
				int dist2 = dx * dx + dy * dy;
				int r = ring.radius;
				int r_eff = r > 12 ? r - 12 : (r > 4 ? r - 4 : r);
				int r2 = r_eff * r_eff;
				// 약간 여유를 두고 판정
				if (dist2 <= r2) {
					int this_owner = sprite->owner;
					// 자원 링(kind=2)은 실제 자원/중립 스프라이트(예: owner>=8)에만 적용하고,
					// 플레이어 유닛(특히 일꾼)에는 적용하지 않는다.
					if (ring.kind == 2 && this_owner < 8) {
						continue;
					}
					is_target = true;
					// 이 스프라이트에 가장 먼저 매칭된 링의 kind/frames 사용
					target_kind = ring.kind;
					target_frames = ring.frames;
					break;
				}
			}
		}

		bool drawn_circle = false;
		for (auto* image : ptr(reverse(sprite->images))) {
			if (i_flag(image, image_t::flag_hidden)) continue;
			if (!drawn_circle && (draw_selection_u || is_target) && image->modifier != 10) {
				const unit_t* circle_u = draw_selection_u;
				draw_selection_circle(sprite, circle_u, data, pitch, is_target, target_kind, target_frames);
				draw_selection_u = nullptr;
				drawn_circle = true;
			}
			draw_image(image, data, pitch, st.players[sprite->owner].color);
		}
		if (draw_health_bars_u && !u_invincible(draw_health_bars_u)) {
			draw_health_bars(sprite, draw_health_bars_u, data, pitch);
		}
	}

	a_vector<std::pair<uint32_t, const sprite_t*>> sorted_sprites;

	void draw_sprites(uint8_t* data, size_t pitch) {

		image_draw_queue.clear();

		sorted_sprites.clear();

		// Mark selected units so selection rings render correctly this frame
		for (unit_t* u : ptr(st.visible_units)) {
			if (!u || !u->sprite) continue;
			if (current_selection_is_selected(u)) {
				current_selection_sprites_set.at(u->sprite->index) = u;
				current_selection_sprites.push_back(u->sprite);
			}
		}

		auto screen_tile = screen_tile_bounds();

		size_t from_y = screen_tile.from.y;
		if (from_y < 4) from_y = 0;
		else from_y -= 4;
		size_t to_y = screen_tile.to.y;
		if (to_y >= game_st.map_tile_height - 4) to_y = game_st.map_tile_height - 1;
		else to_y += 4;
		for (size_t y = from_y; y != to_y; ++y) {
			for (auto* sprite : ptr(st.sprites_on_tile_line.at(y))) {
				if (s_hidden(sprite)) continue;
				sorted_sprites.emplace_back(sprite_depth_order(sprite), sprite);
			}
		}

		std::sort(sorted_sprites.begin(), sorted_sprites.end());

		for (auto& v : sorted_sprites) {
			draw_sprite(v.second, data, pitch);
		}

		for (auto* s : current_selection_sprites) {
			current_selection_sprites_set.at(s->index) = nullptr;
		}
		current_selection_sprites.clear();
	}

	void fill_rectangle(uint8_t* data, size_t pitch, rect area, uint8_t index) {
		if (area.from.x < 0) area.from.x = 0;
		if (area.from.y < 0) area.from.y = 0;
		if (area.to.x > (int)screen_width) area.to.x = screen_width;
		if (area.to.y > (int)screen_height) area.to.y = screen_height;
		if (area.from.x >= area.to.x || area.from.y >= area.to.y) return;
		size_t width = area.to.x - area.from.x;
		size_t pitchw = pitch;
		size_t from_y = area.from.y;
		size_t to_y = area.to.y;
		uint8_t* ptr = data + pitch * from_y + area.from.x;
		for (size_t i = from_y; i != to_y; ++i) {
			memset(ptr, index, width);
			ptr += pitchw;
		}
	}

	void line_rectangle(uint8_t* data, size_t pitch, rect area, uint8_t index) {
		if (area.from.x < 0) area.from.x = 0;
		if (area.from.y < 0) area.from.y = 0;
		if (area.to.x > (int)screen_width) area.to.x = screen_width;
		if (area.to.y > (int)screen_height) area.to.y = screen_height;
		if (area.from.x >= area.to.x || area.from.y >= area.to.y) return;
		size_t width = area.to.x - area.from.x;
		size_t height = area.to.y - area.from.y;
		uint8_t* p = data + pitch * (size_t)area.from.y + (size_t)area.from.x;
		memset(p, index, width);
		memset(p + pitch * height, index, width);
		for (size_t y = 0; y != height; ++y) {
			p[pitch * y] = index;
			p[pitch * y + width - 1] = index;
		}
	}

	void line_rectangle_rgba(uint32_t* data, size_t pitch, rect area, uint32_t color) {
		if (area.from.x < 0) area.from.x = 0;
		if (area.from.y < 0) area.from.y = 0;
		if (area.to.x > (int)screen_width) area.to.x = screen_width;
		if (area.to.y > (int)screen_height) area.to.y = screen_height;
		if (area.from.x >= area.to.x || area.from.y >= area.to.y) return;
		size_t width = area.to.x - area.from.x;
		size_t height = area.to.y - area.from.y;
		uint32_t* p = data + pitch * (size_t)area.from.y + (size_t)area.from.x;
		for (size_t x = 0; x != width; ++x) {
			p[x] = color;
			p[height * pitch + x] = color;
		}
		for (size_t y = 0; y != height; ++y) {
			p[pitch * y] = color;
			p[pitch * y + width - 1] = color;
		}
	}

	bool unit_visble_on_minimap(unit_t* u) {
		if (u->owner < 8 && u->sprite->visibility_flags == 0) return false;
		if (ut_turret(u)) return false;
		if (unit_is_trap(u)) return false;
		if (unit_is(u, UnitTypes::Spell_Dark_Swarm)) return false;
		if (unit_is(u, UnitTypes::Spell_Disruption_Web)) return false;
		return true;
	}

	rect get_minimap_area() {
		// ★ 스타크래프트 스타일 미니맵 위치 (draw_minimap과 동일)
		const int minimap_max_w = 192;  // 가로
		const int minimap_max_h = 250;  // 세로 250으로 확대
		const int minimap_x = 8;
		int minimap_y = screen_height - 210;  // 위치 조정
		
		// 맵 비율에 맞게 미니맵 크기 계산
		int minimap_w = minimap_max_w;
		int minimap_h = minimap_max_h;
		
		if (game_st.map_tile_width > game_st.map_tile_height) {
			minimap_h = (minimap_max_h * game_st.map_tile_height) / game_st.map_tile_width;
		} else if (game_st.map_tile_height > game_st.map_tile_width) {
			minimap_w = (minimap_max_w * game_st.map_tile_width) / game_st.map_tile_height;
		}
		
		// 화면 경계 체크 (크래시 방지)
		if (minimap_y < 0) minimap_y = 0;
		if (minimap_y + minimap_h > (int)screen_height) {
			minimap_h = screen_height - minimap_y;
		}
		
		rect area;
		area.from = {minimap_x, minimap_y};
		area.to = area.from + xy{minimap_w, minimap_h};
		return area;
	}

	void draw_minimap(uint8_t* data, size_t pitch) {
		// ★ 스타크래프트 스타일: 좌측 하단 고정 위치 (192x250)
		const int minimap_max_w = 192;  // 가로
		const int minimap_max_h = 250;  // 세로 250으로 확대
		const int minimap_x = 8;
		const int minimap_y = screen_height - 210;  // 위치 조정
		
		// 맵 비율에 맞게 미니맵 크기 계산
		int minimap_w = minimap_max_w;
		int minimap_h = minimap_max_h;
		
		if (game_st.map_tile_width > game_st.map_tile_height) {
			minimap_h = (minimap_max_h * game_st.map_tile_height) / game_st.map_tile_width;
		} else if (game_st.map_tile_height > game_st.map_tile_width) {
			minimap_w = (minimap_max_w * game_st.map_tile_width) / game_st.map_tile_height;
		}
		
		rect area;
		area.from = {minimap_x, minimap_y};
		area.to = area.from + xy{minimap_w, minimap_h};
		
		// 미니맵 배경 (검은색)
		fill_rectangle(data, pitch, area, 0);
		
		// 미니맵 테두리
		line_rectangle(data, pitch, {area.from - xy(1, 1), area.to + xy(1, 1)}, 12);
		
		// 미니맵 내용 렌더링 (2x2 픽셀로 확대하여 선명도 향상)
		const int pixel_scale = 2;  // 각 타일을 2x2 픽셀로 렌더링
		float scale_x = (float)minimap_w / game_st.map_tile_width;
		float scale_y = (float)minimap_h / game_st.map_tile_height;
		
		// 타일 렌더링 (안전한 경계 체크 + 노이즈 완전 제거)
		for (size_t ty = 0; ty < game_st.map_tile_height; ++ty) {
			for (size_t tx = 0; tx < game_st.map_tile_width; ++tx) {
				int px = minimap_x + (int)(tx * scale_x);
				int py = minimap_y + (int)(ty * scale_y);
				
				// 화면 경계 체크 (크래시 방지)
				if (px >= area.from.x && px < area.to.x && py >= area.from.y && py < area.to.y) {
					size_t tile_idx = ty * game_st.map_tile_width + tx;
					if (tile_idx < st.tiles.size() && tile_idx < st.tiles_mega_tile_index.size()) {
						size_t index;
						if (~st.tiles[tile_idx].flags & tile_t::flag_has_creep) 
							index = st.tiles_mega_tile_index[tile_idx];
						else if (tile_idx < creep_random_tile_indices.size())
							index = game_st.cv5.at(1).mega_tile_index[creep_random_tile_indices[tile_idx]];
						else
							continue;
						
						if (index > 0 && index < tileset_img.vx4.size()) {
							auto* images = &tileset_img.vx4.at(index).images[0];
							size_t img_idx = *images / 2;
							if (img_idx > 0 && img_idx < tileset_img.vr4.size()) {
								auto* bitmap = &tileset_img.vr4.at(img_idx).bitmap[0];
								auto val = bitmap[55 / sizeof(vr4_entry::bitmap_t)];
								size_t shift = 8 * (55 % sizeof(vr4_entry::bitmap_t));
								val >>= shift;
								uint8_t color = (uint8_t)val;
								// 노이즈 완전 제거: 유효한 색상만 표시
								if (color >= 8 && color <= 250) {
									// 2x2 픽셀로 그려서 선명도 향상
									for (int dy = 0; dy < pixel_scale && (py + dy) < area.to.y; ++dy) {
										for (int dx = 0; dx < pixel_scale && (px + dx) < area.to.x; ++dx) {
											data[(py + dy) * pitch + (px + dx)] = color;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		
		// 유닛 표시
		for (size_t i = 12; i != 0;) {
			--i;
			for (unit_t* u : ptr(st.player_units[i])) {
				if (!unit_visble_on_minimap(u)) continue;
				int color = img.player_minimap_colors.at(st.players[u->owner].color);
				
				if (unit_is_mineral_field(u) || unit_is(u, UnitTypes::Resource_Vespene_Geyser)) {
					color = tileset_img.resource_minimap_color;
				}
				
				int ux = minimap_x + (int)((u->sprite->position.x / 32) * scale_x);
				int uy = minimap_y + (int)((u->sprite->position.y / 32) * scale_y);
				
				// 2x2 픽셀로 유닛 표시
				for (int dy = 0; dy < 2; ++dy) {
					for (int dx = 0; dx < 2; ++dx) {
						int px = ux + dx;
						int py = uy + dy;
						if (px >= area.from.x && px < area.to.x && py >= area.from.y && py < area.to.y) {
							data[py * pitch + px] = color;
						}
					}
				}
			}
		}
		
		// 화면 뷰포트 표시 (흰색 사각형)
		int vx1 = minimap_x + (int)((screen_pos.x / 32) * scale_x);
		int vy1 = minimap_y + (int)((screen_pos.y / 32) * scale_y);
		int vx2 = minimap_x + (int)(((screen_pos.x + view_width) / 32) * scale_x);
		int vy2 = minimap_y + (int)(((screen_pos.y + view_height) / 32) * scale_y);
		
		rect view_rect = {{vx1, vy1}, {vx2, vy2}};
		line_rectangle(data, pitch, view_rect, 255);
	}

	int replay_frame = 0;

	rect get_replay_slider_area() {
#ifdef EMSCRIPTEN
		return {};
#endif
		if (replay_st.end_frame == 0) return {}; // only in replay
		rect r;
		int width = 192;
		int height = 32;
		r.from.x = (int)screen_width - 8 - width;
		r.from.y = (int)screen_height - 8 - height; // move to very bottom-right
		r.to.x = r.from.x + width;
		r.to.y = r.from.y + height;
		if (r.from.x < 0 || r.from.y < 0) return {};
		return r;
	}

	void draw_ui(uint8_t* data, size_t pitch, int command_mode = 0) {
		// ★ 스타크래프트 스타일 UI
		// command_mode: 0=Normal, 1=Attack, 2=Move, 3=Patrol, 4=Hold, 5=Build
		const int panel_height = 220;
		const int panel_y = screen_height - panel_height;
		
		// === 하단 패널 배경 (검은색) - 미니맵 영역 제외 ===
		const int minimap_right = 8 + 192 + 8;  // 미니맵 우측 경계 (192로 확대)
		// 미니맵 우측부터 화면 끝까지만 채우기
		fill_rectangle(data, pitch, rect{{minimap_right, panel_y}, {(int)screen_width, (int)screen_height}}, 0);
		// 상단 테두리
		fill_rectangle(data, pitch, rect{{0, panel_y}, {(int)screen_width, panel_y + 2}}, 12);
		
		// === 좌측: 미니맵 영역 (이미 draw_minimap에서 그려짐) ===
		
		// === 우측: 커맨드 패널 (3x3) - 오른쪽 끝에서 2칸 공간 남기고 배치 ===
		const int btn_size = 60;
		const int btn_gap = 6;
		const int cmd_bg_margin = 10;
		const int right_margin = 2;  // 오른쪽 끝에서 2칸 공간
		const int cmd_bg_w = btn_size * 3 + btn_gap * 2 + cmd_bg_margin;
		const int cmd_x = (int)screen_width - cmd_bg_w - right_margin + 5;
		const int cmd_y = panel_y + 10;
		
		// === 레이아웃 재구성: 정보 패널 확장 + 초상화 이동 ===
		
		// 초상화 설정 (명령 패널 바로 왼쪽에 배치)
		const int portrait_w = 140;
		const int portrait_h = 140;
		const int portrait_x = cmd_x - portrait_w - 10;  // 명령 패널 왼쪽 10픽셀 여백
		const int portrait_y = panel_y + 10;
		
		// 정보 패널 (미니맵 오른쪽부터 초상화 왼쪽까지 확장)
		const int info_x = 220;  // 미니맵 192 + 여백
		const int info_y = panel_y + 10;
		const int info_w = portrait_x - info_x - 10;  // 초상화까지 확장
		const int info_h = 200;
		
		// Wireframe 그리드 설정 (정보 패널을 꽉 채우도록 최적화)
		const int wireframe_cols = 8;
		const int wireframe_rows = 3;
		const int wireframe_gap = 1;  // 간격 1픽셀 (구분선)
		// 정보 패널 크기에 맞춰 wireframe 크기 자동 계산
		const int available_w = info_w - 4;  // 좌우 여백
		const int available_h = info_h - 4;  // 상하 여백
		// 가로/세로 중 작은 값 사용 (넘치지 않도록)
		const int wireframe_size_w = (available_w - (wireframe_cols - 1) * wireframe_gap) / wireframe_cols;
		const int wireframe_size_h = (available_h - (wireframe_rows - 1) * wireframe_gap) / wireframe_rows;
		const int wireframe_size = std::min(wireframe_size_w, wireframe_size_h);  // 작은 값 사용
		const int wireframe_w = wireframe_cols * wireframe_size + (wireframe_cols - 1) * wireframe_gap;
		const int wireframe_h = wireframe_rows * wireframe_size + (wireframe_rows - 1) * wireframe_gap;
		
		// === 선택된 유닛들 수집 (최대 24개) ===
		a_vector<unit_t*> selected_units;
		for (unit_t* u : ptr(st.visible_units)) {
			if (u && u->sprite && current_selection_is_selected(u)) {
				selected_units.push_back(u);
				if (selected_units.size() >= 24) break;  // 최대 24개
			}
		}
		
		// === 왼쪽: 정보 패널 또는 Wireframe 그리드 ===
		if (selected_units.size() == 0) {
			// ★ 0개 선택: 빈 정보 패널
			fill_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 0);
			line_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 12);
			
		} else if (selected_units.size() == 1) {
			// ★ 1개 선택: 스타크래프트 원본 스타일 정보 패널 (완전 재설계)
			unit_t* selected_unit = selected_units[0];
			
			fill_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 0);
			line_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 12);
			
			// === 왼쪽: Wireframe 이미지 (120x120 큰 박스) ===
			const int wireframe_size = 120;
			const int wireframe_x = info_x + 8;
			const int wireframe_y = info_y + 8;
			
			// Wireframe 배경 (종족별 색상)
			uint8_t unit_color = 159;
			race_t unit_race_type = unit_race(selected_unit);
			if (unit_race_type == race_t::protoss) unit_color = 159;
			else if (unit_race_type == race_t::zerg) unit_color = 185;
			else if (unit_race_type == race_t::terran) unit_color = 112;
			
			// Wireframe 외곽 테두리 (검은색)
			fill_rectangle(data, pitch, rect{{wireframe_x - 1, wireframe_y - 1}, {wireframe_x + wireframe_size + 1, wireframe_y + wireframe_size + 1}}, 0);
			
			// Wireframe 배경
			fill_rectangle(data, pitch, rect{{wireframe_x, wireframe_y}, {wireframe_x + wireframe_size, wireframe_y + wireframe_size}}, unit_color);
			
			// Wireframe 내부 테두리 (밝은 색)
			line_rectangle(data, pitch, rect{{wireframe_x, wireframe_y}, {wireframe_x + wireframe_size, wireframe_y + wireframe_size}}, 255);
			
			// Wireframe 하단에 HP 바 + 쉴드 바 (스타크래프트 스타일)
			const int wf_hp_bar_h = 5;
			int wf_hp_bar_y = wireframe_y + wireframe_size - wf_hp_bar_h - 2;
			
			float hp_max = (float)selected_unit->unit_type->hitpoints.integer_part();
			float hp_cur = (float)selected_unit->hp.integer_part();
			if (hp_max < 1.f) hp_max = 1.f;
			float hp_ratio = hp_cur / hp_max;
			
			// 쉴드가 있는지 확인 (프로토스만)
			bool has_shield = (unit_race_type == race_t::protoss && selected_unit->unit_type->shield_points > 0);
			
			// 쉴드 바 (프로토스만 HP 바 위에 표시)
			if (has_shield) {
				float sh_max = (float)selected_unit->unit_type->shield_points;
				float sh_cur = (float)selected_unit->shield_points.integer_part();
				float sh_ratio = sh_cur / std::max(1.f, sh_max);
				
				const int wf_sh_bar_y = wf_hp_bar_y - wf_hp_bar_h - 2;  // HP 바 위
				
				// 쉴드 바 배경 (검은색)
				fill_rectangle(data, pitch, rect{{wireframe_x + 2, wf_sh_bar_y}, {wireframe_x + wireframe_size - 2, wf_sh_bar_y + wf_hp_bar_h}}, 0);
				// 쉴드 바 (파란색)
				fill_rectangle(data, pitch, rect{{wireframe_x + 2, wf_sh_bar_y}, {wireframe_x + 2 + (int)((wireframe_size - 4) * sh_ratio), wf_sh_bar_y + wf_hp_bar_h}}, 165);  // 165=파란색
			}
			
			// HP 바 배경 (검은색)
			fill_rectangle(data, pitch, rect{{wireframe_x + 2, wf_hp_bar_y}, {wireframe_x + wireframe_size - 2, wf_hp_bar_y + wf_hp_bar_h}}, 0);
			// HP 바 (빨간색)
			fill_rectangle(data, pitch, rect{{wireframe_x + 2, wf_hp_bar_y}, {wireframe_x + 2 + (int)((wireframe_size - 4) * hp_ratio), wf_hp_bar_y + wf_hp_bar_h}}, 176);  // 176=빨간색
			
			// === 체력/쉴드 텍스트 (UI 좌표 231, 752에 2배 크기) ===
			const int hp_text_x = 231;
			const int hp_text_y = 752;
			const int hp_scale = 2;
			
			// 체력 텍스트
			char hp_text[32];
			std::snprintf(hp_text, sizeof(hp_text), "HP: %d/%d", (int)hp_cur, (int)hp_max);
			
			int cx_hp = hp_text_x;
			for (int ci = 0; hp_text[ci] != '\0'; ++ci) {
				const uint8_t* g = glyph5x7(hp_text[ci]);
				for (int i = 0; i < 5; ++i) {
					uint8_t bits = g[i];
					for (int j = 0; j < 7; ++j) {
						if (bits & (1u << j)) {
							for (int sx = 0; sx < hp_scale; ++sx) {
								for (int sy = 0; sy < hp_scale; ++sy) {
									int px = cx_hp + i * hp_scale + sx;
									int py = hp_text_y + j * hp_scale + sy;
									if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
										data[py * pitch + px] = 255;  // 흰색
									}
								}
							}
						}
					}
				}
				cx_hp += 6 * hp_scale;
			}
			
			// 쉴드 텍스트 (프로토스만 표시)
			if (unit_race_type == race_t::protoss && selected_unit->unit_type->shield_points > 0) {
				float sh_max = (float)selected_unit->unit_type->shield_points;
				float sh_cur = (float)selected_unit->shield_points.integer_part();
				
				char sh_text[32];
				std::snprintf(sh_text, sizeof(sh_text), "Shield: %d/%d", (int)sh_cur, (int)sh_max);
				
				int cx_sh = hp_text_x;
				int sh_y = hp_text_y + 7 * hp_scale + 3;  // 체력 아래
				for (int ci = 0; sh_text[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(sh_text[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < hp_scale; ++sx) {
									for (int sy = 0; sy < hp_scale; ++sy) {
										int px = cx_sh + i * hp_scale + sx;
										int py = sh_y + j * hp_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = 159;  // 청록색
										}
									}
								}
							}
						}
					}
					cx_sh += 6 * hp_scale;
				}
			}
			
			// === 유닛 이름 표시하기 (UI 좌표 536, 605, 4배 크기) ===
			const char* unit_name = get_unit_name((int)selected_unit->unit_type->id);
			const int name_scale = 4;  // 6배 → 4배 (0.67배, 가장 가까운 정수)
			const int name_x = 536;
			const int name_y = 605;
			
			// 종족별 색상 (팔레트 인덱스)
			uint8_t name_color;
			if (unit_race_type == race_t::protoss) {
				name_color = 255;  // 흰색
			} else if (unit_race_type == race_t::zerg) {
				name_color = 185;  // 보라색
			} else {
				name_color = 155;  // 노란색
			}
			
			// 5x7 폰트로 렌더링 (6배 확대)
			int cx = name_x;
			for (int ci = 0; unit_name[ci] != '\0'; ++ci) {
				const uint8_t* g = glyph5x7(unit_name[ci]);
				for (int i = 0; i < 5; ++i) {
					uint8_t bits = g[i];
					for (int j = 0; j < 7; ++j) {
						if (bits & (1u << j)) {
							for (int sx = 0; sx < name_scale; ++sx) {
								for (int sy = 0; sy < name_scale; ++sy) {
									int px = cx + i * name_scale + sx;
									int py = name_y + j * name_scale + sy;
									if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
										data[py * pitch + px] = name_color;
									}
								}
							}
						}
					}
				}
				cx += 6 * name_scale;
			}
			
			// === 유닛 정보 표시하기 (UI 좌표 364, 650, 2배 크기) ===
			const int info_scale = 2;
			const int info_text_x = 364;
			const int info_text_y = 650;
			
			// 유닛(건물 아님)인 경우 계급과 킬 수 표시
			bool is_building = (selected_unit->unit_type->flags & unit_type_t::flag_building) != 0;
			if (!is_building) {
				// 종족 확인 (테란만 계급 표시)
				race_t unit_race_type = unit_race(selected_unit);
				bool is_terran = (unit_race_type == race_t::terran);
				
				int line_y = info_text_y;
				
				// 계급 정보 (테란만 표시)
				if (is_terran) {
					const char* rank_name = get_unit_rank_name(selected_unit->rank_increase);
					char rank_text[64];
					std::snprintf(rank_text, sizeof(rank_text), "Rank: %s", rank_name);
					
					// 계급 텍스트 렌더링 (노란색)
					int cx = info_text_x;
					for (int ci = 0; rank_text[ci] != '\0'; ++ci) {
						const uint8_t* g = glyph5x7(rank_text[ci]);
						for (int i = 0; i < 5; ++i) {
							uint8_t bits = g[i];
							for (int j = 0; j < 7; ++j) {
								if (bits & (1u << j)) {
									for (int sx = 0; sx < info_scale; ++sx) {
										for (int sy = 0; sy < info_scale; ++sy) {
											int px = cx + i * info_scale + sx;
											int py = line_y + j * info_scale + sy;
											if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
												data[py * pitch + px] = 155;  // 노란색 팔레트 인덱스
											}
										}
									}
								}
							}
						}
						cx += 6 * info_scale;
					}
					line_y += 7 * info_scale + 3;
				}
				
				// 킬 수 정보 (모든 종족 표시)
				char kills_text[64];
				std::snprintf(kills_text, sizeof(kills_text), "Kills: %d", selected_unit->kill_count);
				
				// 킬 수 텍스트 렌더링 (흰색)
				int cx = info_text_x;
				for (int ci = 0; kills_text[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(kills_text[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < info_scale; ++sx) {
									for (int sy = 0; sy < info_scale; ++sy) {
										int px = cx + i * info_scale + sx;
										int py = line_y + j * info_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = 255;  // 흰색 팔레트 인덱스
										}
									}
								}
							}
						}
					}
					cx += 6 * info_scale;
				}
			}
			
			// 프로토스 유닛 아이콘은 rgba_surface에서 그림 (draw_protoss_icons_rgba 함수)
			// 여기서는 플래그만 설정
			g_draw_protoss_icons = (unit_race_type == race_t::protoss && !is_building);
			
			// 마우스가 가운데 아이콘(Armor) 또는 오른쪽 아이콘(Shield) 위에 있는지 체크
			if (g_draw_protoss_icons) {
				const int icon_x = 595;
				const int icon_y = 700;
				const int icon_h = 80;
				const int icon_total_w = (int)(406 * ((float)icon_h / 157));  // 스케일된 전체 너비
				const int single_icon_w = icon_total_w / 3;  // 3개 아이콘이므로
				
				// 가운데 아이콘 영역 (두 번째 아이콘 - Armor)
				const int center_icon_x = icon_x + single_icon_w;
				const int center_icon_y = icon_y;
				const int center_icon_w = single_icon_w;
				const int center_icon_h = icon_h;
				
				// 오른쪽 아이콘 영역 (세 번째 아이콘 - Shield)
				const int right_icon_x = icon_x + single_icon_w * 2;
				const int right_icon_y = icon_y;
				const int right_icon_w = single_icon_w;
				const int right_icon_h = icon_h;
				
				// 마우스가 가운데 아이콘(Armor) 영역 안에 있는지 체크
				if (g_ui_mouse_x >= center_icon_x && g_ui_mouse_x < center_icon_x + center_icon_w &&
					g_ui_mouse_y >= center_icon_y && g_ui_mouse_y < center_icon_y + center_icon_h) {
					g_draw_armor_tooltip = true;
					g_armor_value = selected_unit->unit_type->armor;
					g_draw_shield_tooltip = false;
				}
				// 마우스가 오른쪽 아이콘(Shield) 영역 안에 있는지 체크
				else if (g_ui_mouse_x >= right_icon_x && g_ui_mouse_x < right_icon_x + right_icon_w &&
					g_ui_mouse_y >= right_icon_y && g_ui_mouse_y < right_icon_y + right_icon_h) {
					g_draw_shield_tooltip = true;
					// Plasma Shields 업그레이드 레벨 표시 (0, 1, 2, 3)
					g_shield_value = player_upgrade_level(selected_unit->owner, UpgradeTypes::Protoss_Plasma_Shields);
					g_draw_armor_tooltip = false;
				}
				else {
					g_draw_armor_tooltip = false;
					g_draw_shield_tooltip = false;
				}
			} else {
				g_draw_armor_tooltip = false;
				g_draw_shield_tooltip = false;
			}
			
			// 인구수 제공 건물 확인
			bool is_supply_building = false;
			int supply_provided = 0;
			
			// Nexus, Pylon, Overlord, Supply Depot 등
			if (unit_is(selected_unit, UnitTypes::Protoss_Nexus)) {
				is_supply_building = true;
				supply_provided = 10;
			} else if (unit_is(selected_unit, UnitTypes::Protoss_Pylon)) {
				is_supply_building = true;
				supply_provided = 8;
			} else if (unit_is(selected_unit, UnitTypes::Zerg_Overlord)) {
				is_supply_building = true;
				supply_provided = 8;
			} else if (unit_is(selected_unit, UnitTypes::Terran_Supply_Depot)) {
				is_supply_building = true;
				supply_provided = 8;
			} else if (unit_is(selected_unit, UnitTypes::Terran_Command_Center)) {
				is_supply_building = true;
				supply_provided = 10;
			} else if (unit_is(selected_unit, UnitTypes::Zerg_Hatchery) || 
			           unit_is(selected_unit, UnitTypes::Zerg_Lair) || 
			           unit_is(selected_unit, UnitTypes::Zerg_Hive)) {
				is_supply_building = true;
				supply_provided = 1;
			}
			
			if (is_supply_building) {
				// 현재 플레이어의 인구수 정보
				race_t player_race = unit_race(selected_unit);
				int supplies_used = 0;
				int supplies_available = 0;
				
				// 종족별 인구수 계산
				for (unit_t* u : ptr(st.player_units[selected_unit->owner])) {
					if (unit_race(u) == player_race) {
						if (u->unit_type->space_required > 0) {
							supplies_used += u->unit_type->space_required;
						}
						if (u->unit_type->space_provided > 0) {
							supplies_available += u->unit_type->space_provided;
						}
					}
				}
				supplies_used /= 2;
				supplies_available /= 2;
				int supplies_max = 200;
				
				// 팔레트 인덱스 방식으로 텍스트 렌더링
				const uint8_t text_color = 255;  // 흰색
				char info_line[64];
				int line_y = info_text_y;
				
				// 각 줄 렌더링
				const char* lines[4];
				std::snprintf(info_line, sizeof(info_line), "Supplies Used: %d", supplies_used);
				lines[0] = info_line;
				
				char line2[64], line3[64], line4[64];
				std::snprintf(line2, sizeof(line2), "Supplies Provided: %d", supply_provided);
				std::snprintf(line3, sizeof(line3), "Total Supplies: %d", supplies_available);
				std::snprintf(line4, sizeof(line4), "Supplies Max: %d", supplies_max);
				
				// 첫 번째 줄 (2배 크기)
				int cx = info_text_x;
				for (int ci = 0; info_line[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(info_line[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < info_scale; ++sx) {
									for (int sy = 0; sy < info_scale; ++sy) {
										int px = cx + i * info_scale + sx;
										int py = line_y + j * info_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = text_color;
										}
									}
								}
							}
						}
					}
					cx += 6 * info_scale;
				}
				line_y += 7 * info_scale + 3;
				
				// 두 번째 줄 (2배 크기)
				cx = info_text_x;
				for (int ci = 0; line2[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(line2[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < info_scale; ++sx) {
									for (int sy = 0; sy < info_scale; ++sy) {
										int px = cx + i * info_scale + sx;
										int py = line_y + j * info_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = text_color;
										}
									}
								}
							}
						}
					}
					cx += 6 * info_scale;
				}
				line_y += 7 * info_scale + 3;
				
				// 세 번째 줄 (2배 크기)
				cx = info_text_x;
				for (int ci = 0; line3[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(line3[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < info_scale; ++sx) {
									for (int sy = 0; sy < info_scale; ++sy) {
										int px = cx + i * info_scale + sx;
										int py = line_y + j * info_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = text_color;
										}
									}
								}
							}
						}
					}
					cx += 6 * info_scale;
				}
				line_y += 7 * info_scale + 3;
				
				// 네 번째 줄 (2배 크기)
				cx = info_text_x;
				for (int ci = 0; line4[ci] != '\0'; ++ci) {
					const uint8_t* g = glyph5x7(line4[ci]);
					for (int i = 0; i < 5; ++i) {
						uint8_t bits = g[i];
						for (int j = 0; j < 7; ++j) {
							if (bits & (1u << j)) {
								for (int sx = 0; sx < info_scale; ++sx) {
									for (int sy = 0; sy < info_scale; ++sy) {
										int px = cx + i * info_scale + sx;
										int py = line_y + j * info_scale + sy;
										if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
											data[py * pitch + px] = text_color;
										}
									}
								}
							}
						}
					}
					cx += 6 * info_scale;
				}
			}
			
			// ★ 생산 대기열 (Build Queue) - 생산 건물이 생산 중일 때만 표시
			// 생산 중인지 확인: build_queue가 비어있지 않아야 함
			bool is_producing = !selected_unit->build_queue.empty();
			
			if ((selected_unit->unit_type->flags & unit_type_t::flag_building) && is_producing) {
				// 생산 중인 유닛 확인
				a_vector<const unit_type_t*> queue;
				a_vector<float> progress_list;  // 각 유닛의 진행도
				
				// build_queue만 사용 (current_build_unit은 build_queue[0]에 포함됨)
				// 스타크래프트에서는 build_queue에 현재 생산 중인 유닛도 포함됨
				for (size_t i = 0; i < selected_unit->build_queue.size(); ++i) {
					auto queued = selected_unit->build_queue[i];
					if (queued) {
						queue.push_back(queued);
						
						// 첫 번째 유닛만 진행도 계산 (현재 생산 중)
						if (i == 0 && selected_unit->current_build_unit) {
							int build_time = selected_unit->current_build_unit->unit_type->build_time;
							int remaining = selected_unit->current_build_unit->remaining_build_time;
							float progress = (build_time > 0) ? (1.0f - ((float)remaining / (float)build_time)) : 0.0f;
							progress_list.push_back(progress);
						} else {
							progress_list.push_back(0.0f);  // 대기 중인 유닛은 진행도 0
						}
					}
				}
				
				// 대기열 5칸 반대 ㄴ자 배치 (1번 위, 2345 아래) - 맨 위부터 생산
				// 크기: 50x50, 초상화 패널 2칸 남기고 오른쪽 배치 + 1칸 더 오른쪽
				const int queue_icon_size = 50;
				const int queue_gap = 3;
				const int queue_bottom_y = info_y + info_h - queue_icon_size - 6;  // 하단 줄
				const int queue_top_y = queue_bottom_y - queue_icon_size - queue_gap;  // 상단 1번 칸
				
				// 초상화 패널 2칸 남기고 오른쪽 배치 + 1칸(53픽셀) 더 오른쪽
				const int portrait_margin = 2 * (queue_icon_size + queue_gap);  // 2칸 여백
				const int queue_start_x = portrait_x - portrait_margin - (4 * queue_icon_size + 3 * queue_gap) + (queue_icon_size + queue_gap);
				
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
					
					if (i < (int)queue.size()) {
						// 대기열에 유닛이 있음
						const unit_type_t* queued_type = queue[i];
						float progress = progress_list[i];
						
						// 아이콘 외곽 테두리 (검은색)
						fill_rectangle(data, pitch, rect{{qx - 1, qy - 1}, {qx + queue_icon_size + 1, qy + queue_icon_size + 1}}, 0);
						
						// 아이콘 배경 (어두운 회색)
						fill_rectangle(data, pitch, rect{{qx, qy}, {qx + queue_icon_size, qy + queue_icon_size}}, 24);
						
						// 유닛 아이콘 영역 (밝은 회색)
						fill_rectangle(data, pitch, rect{{qx + 2, qy + 2}, {qx + queue_icon_size - 2, qy + queue_icon_size - 2}}, 112);
						
						// TODO: 실제 유닛 아이콘 이미지 렌더링 (현재는 색상만 표시)
						
						// 진행도 바 (하단, 파란색 - 스타크래프트 스타일)
						if (progress > 0.0f) {
							const int prog_bar_h = 4;
							const int prog_bar_y = qy + queue_icon_size - prog_bar_h - 2;
							int prog_bar_w = (int)((queue_icon_size - 4) * progress);
							
							// 진행도 바 배경 (검은색)
							fill_rectangle(data, pitch, rect{{qx + 2, prog_bar_y}, {qx + queue_icon_size - 2, prog_bar_y + prog_bar_h}}, 0);
							
							// 진행도 바 (파란색)
							if (prog_bar_w > 0) {
								fill_rectangle(data, pitch, 
									rect{{qx + 2, prog_bar_y}, 
									     {qx + 2 + prog_bar_w, prog_bar_y + prog_bar_h}}, 
									29);  // 파란색 진행도
							}
						}
						
						// 아이콘 테두리 (밝은 회색)
						line_rectangle(data, pitch, rect{{qx, qy}, {qx + queue_icon_size, qy + queue_icon_size}}, 156);
					} else {
						// 빈 칸 (매우 어두운 회색)
						fill_rectangle(data, pitch, rect{{qx - 1, qy - 1}, {qx + queue_icon_size + 1, qy + queue_icon_size + 1}}, 0);
						fill_rectangle(data, pitch, rect{{qx, qy}, {qx + queue_icon_size, qy + queue_icon_size}}, 8);
						line_rectangle(data, pitch, rect{{qx, qy}, {qx + queue_icon_size, qy + queue_icon_size}}, 12);
					}
				}
			}
			
		} else {
			// ★ 2개 이상 선택: Wireframe 그리드 (8x3 = 24개)
			// 정보 패널 영역에 wireframe 그리드 표시
			fill_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 0);
			line_rectangle(data, pitch, rect{{info_x, info_y}, {info_x + info_w, info_y + info_h}}, 12);
			
			// Wireframe 그리드를 정보 패널 내부 중앙에 배치
			const int grid_start_x = info_x + (info_w - wireframe_w) / 2;  // 가로 중앙 정렬
			const int grid_start_y = info_y + (info_h - wireframe_h) / 2;  // 세로 중앙 정렬
			
			// 각 wireframe 칸 렌더링
			for (int row = 0; row < wireframe_rows; ++row) {
				for (int col = 0; col < wireframe_cols; ++col) {
					int idx = row * wireframe_cols + col;
					int wx = grid_start_x + col * (wireframe_size + wireframe_gap);
					int wy = grid_start_y + row * (wireframe_size + wireframe_gap);
					
					if (idx < (int)selected_units.size()) {
						unit_t* u = selected_units[idx];
						
						// 유닛 타입에 따른 색상
						uint8_t unit_color = 159;
						race_t unit_race_type = unit_race(u);
						if (unit_race_type == race_t::protoss) unit_color = 159;
						else if (unit_race_type == race_t::zerg) unit_color = 185;
						else if (unit_race_type == race_t::terran) unit_color = 112;
						
						// Wireframe 배경
						fill_rectangle(data, pitch, rect{{wx, wy}, {wx + wireframe_size, wy + wireframe_size}}, unit_color);
						
						// HP 바
						float hp_max = (float)u->unit_type->hitpoints.integer_part();
						float hp_cur = (float)u->hp.integer_part();
						if (hp_max < 1.f) hp_max = 1.f;
						float hp_ratio = hp_cur / hp_max;
						
						const int hp_bar_h = 3;
						const int hp_bar_y = wy + wireframe_size - hp_bar_h - 1;
						
						// 쉴드가 있는지 확인 (프로토스만)
						bool has_shield = (unit_race_type == race_t::protoss && u->unit_type->shield_points > 0);
						
						// Shield 바 (프로토스만 HP 바 위에 표시)
						if (has_shield) {
							float sh_max = (float)u->unit_type->shield_points;
							float sh_cur = (float)u->shield_points.integer_part();
							float sh_ratio = sh_cur / std::max(1.f, sh_max);
							
							const int sh_bar_y = hp_bar_y - hp_bar_h - 1;
							fill_rectangle(data, pitch, rect{{wx + 1, sh_bar_y}, {wx + wireframe_size - 1, sh_bar_y + hp_bar_h}}, 0);
							fill_rectangle(data, pitch, rect{{wx + 1, sh_bar_y}, {wx + 1 + (int)((wireframe_size - 2) * sh_ratio), sh_bar_y + hp_bar_h}}, 165);  // 165=파란색
						}
						
						// HP 바 (빨간색)
						fill_rectangle(data, pitch, rect{{wx + 1, hp_bar_y}, {wx + wireframe_size - 1, hp_bar_y + hp_bar_h}}, 0);
						fill_rectangle(data, pitch, rect{{wx + 1, hp_bar_y}, {wx + 1 + (int)((wireframe_size - 2) * hp_ratio), hp_bar_y + hp_bar_h}}, 176);  // 176=빨간색
						
						// 테두리
						line_rectangle(data, pitch, rect{{wx, wy}, {wx + wireframe_size, wy + wireframe_size}}, 255);
					} else {
						// 빈 칸
						fill_rectangle(data, pitch, rect{{wx, wy}, {wx + wireframe_size, wy + wireframe_size}}, 8);
						line_rectangle(data, pitch, rect{{wx, wy}, {wx + wireframe_size, wy + wireframe_size}}, 12);
					}
				}
			}
		}
		
		// === 중앙: 초상화 (항상 표시) ===
		fill_rectangle(data, pitch, rect{{portrait_x, portrait_y}, {portrait_x + portrait_w, portrait_y + portrait_h}}, 0);
		line_rectangle(data, pitch, rect{{portrait_x, portrait_y}, {portrait_x + portrait_w, portrait_y + portrait_h}}, 12);
		
		if (selected_units.size() > 0) {
			// 첫 번째 선택된 유닛의 초상화 표시
			unit_t* u = selected_units[0];
			race_t unit_race_type = unit_race(u);
			uint8_t portrait_color = 159;
			if (unit_race_type == race_t::protoss) portrait_color = 159;
			else if (unit_race_type == race_t::zerg) portrait_color = 185;
			else if (unit_race_type == race_t::terran) portrait_color = 112;
			
			// TODO: 실제 초상화 이미지 렌더링 (현재는 임시로 색상만 표시)
			fill_rectangle(data, pitch, rect{{portrait_x + 10, portrait_y + 10}, {portrait_x + portrait_w - 10, portrait_y + portrait_h - 10}}, portrait_color);
		}
		
		// 명령 패널 배경 (위에서 정의한 cmd_x, cmd_y 사용)
		const int cmd_bg_h = btn_size * 3 + btn_gap * 2 + cmd_bg_margin;
		fill_rectangle(data, pitch, rect{{cmd_x - 5, cmd_y - 5}, {cmd_x + cmd_bg_w - 5, cmd_y + cmd_bg_h}}, 0);
		line_rectangle(data, pitch, rect{{cmd_x - 5, cmd_y - 5}, {cmd_x + cmd_bg_w - 5, cmd_y + cmd_bg_h}}, 12);
		
		// 버튼 레이블 정의
		struct CommandButton {
			const char* label;
			uint8_t color;
			bool active;
		};
		
		CommandButton buttons[9] = {
			{"", 8, false}, {"", 8, false}, {"", 8, false},
			{"", 8, false}, {"", 8, false}, {"", 8, false},
			{"", 8, false}, {"", 8, false}, {"", 8, false}
		};
		
		// 글로벌 스코프의 g_ui_selected_category 변수 참조
		int ui_cat = ::g_ui_selected_category;
		
		// 선택된 유닛에 따라 버튼 활성화
		if (selected_units.size() > 0) {
			unit_t* selected_unit = selected_units[0];  // 첫 번째 유닛 기준
			
			// ★ 카테고리 8번 우선 체크 (건물+유닛 혼합)
			if (ui_cat == 8) {
				// 건물 + 유닛 혼합 선택: 유닛 기본 명령만 (M,S,A,P,H)
				buttons[0] = {"M", 112, true};   // 이동 (Move)
				buttons[1] = {"S", 112, true};   // 정지 (Stop)
				buttons[2] = {"A", 112, true};   // 공격 (Attack)
				buttons[3] = {"P", 112, true};   // 정찰 (Patrol)
				buttons[4] = {"H", 112, true};   // 홀드 (Hold)
				// G, B, V 버튼 없음 (사용자 요구: 카테고리 8에는 G 금지)
			}
			// ★ 카테고리 9: 프로브 건설 모드 전용 (B/V)
			else if (ui_cat == 9) {
				// current_command_mode: 5=Build, 7=AdvancedBuild (player_game_complete에서 전달)
				if (current_command_mode == 5) {
					// B 모드: 0~8 = N, P, A, G, F, C, Y, B, ESC
					buttons[0] = {"N", 112, true};
					buttons[1] = {"P", 112, true};
					buttons[2] = {"A", 112, true};
					buttons[3] = {"G", 112, true};
					buttons[4] = {"F", 112, true};
					buttons[5] = {"C", 112, true};
					buttons[6] = {"Y", 112, true};
					buttons[7] = {"B", 112, true};
					buttons[8] = {"ESC", 112, true};
				} else if (current_command_mode == 7) {
					// V 모드: 0~8 = R, S, C, B, F, T, O, A, ESC
					buttons[0] = {"R", 112, true};
					buttons[1] = {"S", 112, true};
					buttons[2] = {"C", 112, true};
					buttons[3] = {"B", 112, true};
					buttons[4] = {"F", 112, true};
					buttons[5] = {"T", 112, true};
					buttons[6] = {"O", 112, true};
					buttons[7] = {"A", 112, true};
					buttons[8] = {"ESC", 112, true};
				} else {
					// 예외적으로 Build 모드가 아닐 때는 아무 버튼도 표시하지 않음 (안전)
				}
			}
			// 자원인지 체크
			else if (unit_is_mineral_field(selected_unit) || 
			         unit_is(selected_unit, UnitTypes::Resource_Vespene_Geyser) ||
			         unit_is_refinery(selected_unit)) {
				// 자원 선택 시: 모든 버튼 비활성화 (기본값 유지)
				// 아무것도 하지 않음
			}
			// 건물인지 확인
			else if (selected_unit->unit_type->flags & unit_type_t::flag_building) {
				// 건물 선택 시: g_ui_selected_category에 따라 버튼 표시
				
				// 건설 중인 건물인지 확인
				bool is_under_construction = (selected_unit->remaining_build_time > 0);
				
				// 카테고리 7: 서로 다른 건물 여러 개 또는 건설 상태가 섞인 경우 - 항상 버튼 비활성
				// (ESC 포함 어떤 버튼도 표시하지 않음)
				if (ui_cat == 7) {
					// 아무 버튼도 설정하지 않음 (기본값 유지)
				}
				// 단일/동일 건물 선택 시 건설 중인 건물이면 ESC만 표시
				else if (is_under_construction) {
					// 건설 중인 건물: 8번 버튼만 표시 (취소)
					buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
				} else if (ui_cat == 1) {
					// Nexus: P=Probe, R=Rally, O=AutoRally
					buttons[0] = {"P", 112, true};   // 프로브 생산 (0키)
					buttons[5] = {"R", 112, true};   // 렐리 포인트 (R키)
					buttons[6] = {"O", 112, true};   // 자동 렐리 포인트 (O키)
					// 생산 중이거나 명령 모드 활성화 시 ESC 버튼 표시
					bool is_training = !selected_unit->build_queue.empty();
					if (is_training || ::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (ui_cat == 2) {
					// Gateway: Z=Zealot, D=Dragoon, R=Rally
					buttons[0] = {"Z", 112, true};   // 질럿 생산 (Z키)
					buttons[1] = {"D", 112, true};   // 드라군 생산 (D키)
					buttons[5] = {"R", 112, true};   // 렐리 포인트 (R키)
					// 생산 중이거나 명령 모드 활성화 시 ESC 버튼 표시
					bool is_training = !selected_unit->build_queue.empty();
					if (is_training || ::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (ui_cat == 3) {
					// Hatchery/Lair/Hive: D=Drone, R=Rally, O=AutoRally
					buttons[0] = {"D", 112, true};   // 드론 생산 (0키)
					buttons[5] = {"R", 112, true};   // 렐리 포인트 (R키)
					buttons[6] = {"O", 112, true};   // 자동 렐리 포인트 (O키)
					// 생산 중이거나 명령 모드 활성화 시 ESC 버튼 표시
					bool is_training = !selected_unit->build_queue.empty();
					if (is_training || ::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (ui_cat == 4) {
					// Command Center: S=SCV, R=Rally, O=AutoRally
					buttons[0] = {"S", 112, true};   // SCV 생산 (0키)
					buttons[5] = {"R", 112, true};   // 렐리 포인트 (R키)
					buttons[6] = {"O", 112, true};   // 자동 렐리 포인트 (O키)
					// 생산 중이거나 명령 모드 활성화 시 ESC 버튼 표시
					bool is_training = !selected_unit->build_queue.empty();
					if (is_training || ::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (ui_cat == 5) {
					// Attack Building (Photon Cannon, Sunken, Spore, Bunker, Turret): A=Attack, S=Stop
					buttons[0] = {"A", 112, true};   // 공격
					buttons[1] = {"S", 112, true};   // 정지
				} else if (ui_cat == 6) {
					// Resource (Mineral, Gas): 모든 버튼 비활성화
					// 버튼을 설정하지 않음 (기본값 유지: active=false)
				} else if (ui_cat == 7) {
					// Multiple different buildings: 모든 버튼 비활성화
					// 버튼을 설정하지 않음 (기본값 유지: active=false)
				}
				// 다른 건물은 버튼 없음 (기본값 유지: active=false)
			} else {
				// 유닛 선택 시
				// 선택된 유닛 개수 및 타입 확인
				int selected_count = 0;
				int worker_count = 0;
				int non_worker_count = 0;
				
				for (unit_id_t uid : current_selection) {
					unit_t* u = get_unit(uid);
					if (!u) continue;
					selected_count++;
					bool is_worker = unit_is(u, UnitTypes::Protoss_Probe) || unit_is(u, UnitTypes::Zerg_Drone);
					if (is_worker) worker_count++;
					else non_worker_count++;
				}
				
				bool is_probe = unit_is(selected_unit, UnitTypes::Protoss_Probe);
				bool is_drone = unit_is(selected_unit, UnitTypes::Zerg_Drone);
				bool is_single_worker = (is_probe || is_drone) && selected_count == 1;
				bool is_multiple_workers_only = worker_count >= 2 && non_worker_count == 0;
				bool is_mixed = worker_count > 0 && non_worker_count > 0;
				
				if (is_single_worker) {
					// 일꾼 1기만 선택: 모든 버튼 표시
					buttons[0] = {"M", 112, true};   // 이동 (Move)
					buttons[1] = {"S", 112, true};   // 정지 (Stop)
					buttons[2] = {"A", 112, true};   // 공격 (Attack)
					buttons[3] = {"P", 112, true};   // 정찰 (Patrol)
					buttons[4] = {"H", 112, true};   // 홀드 (Hold)
					buttons[5] = {"G", 112, true};   // 광물채취 (Gather)
					buttons[6] = {"B", 112, true};   // 건설모드1 (Build Basic)
					buttons[7] = {"V", 112, true};   // 건설모드2 (Build Advanced)
					// 명령 모드 활성화 시 ESC 버튼 표시
					if (::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (is_multiple_workers_only) {
					// 일꾼 2기 이상만 선택: M,S,A,P,H,G 표시
					buttons[0] = {"M", 112, true};   // 이동 (Move)
					buttons[1] = {"S", 112, true};   // 정지 (Stop)
					buttons[2] = {"A", 112, true};   // 공격 (Attack)
					buttons[3] = {"P", 112, true};   // 정찰 (Patrol)
					buttons[4] = {"H", 112, true};   // 홀드 (Hold)
					buttons[5] = {"G", 112, true};   // 광물채취 (Gather)
					// 명령 모드 활성화 시 ESC 버튼 표시
					if (::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else if (is_mixed) {
					// 일꾼 + 다른 유닛 혼합 선택: M,S,A,P,H만 표시
					buttons[0] = {"M", 112, true};   // 이동 (Move)
					buttons[1] = {"S", 112, true};   // 정지 (Stop)
					buttons[2] = {"A", 112, true};   // 공격 (Attack)
					buttons[3] = {"P", 112, true};   // 정찰 (Patrol)
					buttons[4] = {"H", 112, true};   // 홀드 (Hold)
					// 명령 모드 활성화 시 ESC 버튼 표시
					if (::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				} else {
					// 일반 유닛 공통 버튼
					buttons[0] = {"M", 112, true};   // 이동 (Move)
					buttons[1] = {"S", 112, true};   // 정지 (Stop)
					buttons[2] = {"A", 112, true};   // 공격 (Attack)
					buttons[3] = {"P", 112, true};   // 정찰 (Patrol)
					buttons[4] = {"H", 112, true};   // 홀드 (Hold)
					// 명령 모드 활성화 시 ESC 버튼 표시
					if (::g_ui_command_mode_active) {
						buttons[8] = {"ESC", 112, true};  // 취소 (ESC)
					}
				}
			}
		}
		
		// 버튼 렌더링
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				int idx = row * 3 + col;
				int bx = cmd_x + col * (btn_size + btn_gap);
				int by = cmd_y + row * (btn_size + btn_gap);
				
				// g_ui_active_button을 사용하여 하이라이트 결정
				// 버튼 인덱스: 0=M, 1=S, 2=A, 3=P, 4=H, 5=G, 6=B, 7=V
				bool is_highlighted = false;
				bool has_button = buttons[idx].active;  // 버튼이 존재하는가?
				bool is_button_clickable = has_button;  // 버튼을 클릭할 수 있는가?
				
				// 생산 건물(카테고리 1-4)의 렐리 버튼(5,6)만 명령 모드에서만 클릭 가능
				if (::g_ui_selected_category >= 1 && ::g_ui_selected_category <= 4) {
					// 5번(렐리), 6번(오토 렐리) 버튼만 명령 모드 체크
					if ((idx == 5 || idx == 6) && !::g_ui_command_mode_active) {
						is_button_clickable = false;  // 명령 모드 아니면 클릭 불가
					}
					// 0번, 1번(생산) 버튼은 항상 클릭 가능
				}
				
				// 하이라이트 조건: 버튼이 존재하고, g_ui_active_button과 일치하고, -1이 아니면 하이라이트
				// 단, 생산 건물(카테고리 1-4)에서는 명령 모드일 때만 5,6번 버튼 하이라이트
				if (has_button && ::g_ui_active_button >= 0 && idx == ::g_ui_active_button) {
					// 생산 건물의 렐리 버튼(5,6)은 명령 모드일 때만 하이라이트
					if (::g_ui_selected_category >= 1 && ::g_ui_selected_category <= 4) {
						if (idx == 5 || idx == 6) {
							// 렐리 버튼: 명령 모드일 때만 하이라이트
							is_highlighted = ::g_ui_command_mode_active;
						} else {
							// 생산 버튼(0,1): 항상 하이라이트 가능
							is_highlighted = true;
						}
					} else {
						// 일반 유닛이나 다른 카테고리: 항상 하이라이트
						is_highlighted = true;
					}
				}
				
				// 배경 색상 결정
				uint8_t bg_color;
				if (!has_button) {
					bg_color = 8;  // 버튼 없음: 분홍색
				} else if (is_highlighted) {
					bg_color = 159;  // 하이라이트: 청록색
				} else if (is_button_clickable) {
					bg_color = buttons[idx].color;  // 클릭 가능: 버튼 색상 (112=초록, 159=청록, 185=보라)
				} else {
					// 버튼 있지만 비활성화: 유닛 명령 버튼과 같은 갈색
					bg_color = 112;  // 초록색 (유닛 명령 버튼과 동일)
				}
				
				fill_rectangle(data, pitch, rect{{bx, by}, {bx + btn_size, by + btn_size}}, bg_color);
				
				// 하이라이트된 버튼은 밝은 테두리 (노란색)
				uint8_t border_color = is_highlighted ? 155 : 12;  // 155 = 노란색
				line_rectangle(data, pitch, rect{{bx, by}, {bx + btn_size, by + btn_size}}, border_color);
				
				// 버튼 레이블 표시 (5x7 폰트 사용)
				if (buttons[idx].active && buttons[idx].label[0] != '\0') {
					const char* label = buttons[idx].label;
					int label_len = strlen(label);
					
					// 2배 확대된 5x7 폰트 사용
					const int scale = 2;
					int text_w = label_len * 6 * scale;  // 5 + 1 간격
					int text_h = 7 * scale;
					int tx = bx + (btn_size - text_w) / 2;
					int ty = by + (btn_size - text_h) / 2;
					
					// 각 문자를 5x7 폰트로 렌더링
					for (int ci = 0; ci < label_len; ++ci) {
						const uint8_t* g = glyph5x7(label[ci]);
						int cx = tx + ci * 6 * scale;
						
						// 5x7 글리프를 2배 확대하여 그리기
						// LSB부터 읽기 (비트 0이 위)
						for (int i = 0; i < 5; ++i) {
							uint8_t bits = g[i];
							for (int j = 0; j < 7; ++j) {
								if (bits & (1u << j)) {  // LSB부터 읽기
									// 2x2 픽셀 블록 그리기
									for (int sx = 0; sx < scale; ++sx) {
										for (int sy = 0; sy < scale; ++sy) {
											int px = cx + i * scale + sx;
											int py = ty + j * scale + sy;
											if (px >= bx && px < bx + btn_size && py >= by && py < by + btn_size) {
												data[py * pitch + px] = 255;  // 흰색
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		
		// MENU 버튼 제거 (보라색 박스)
		
		// 리플레이 슬라이더 (있는 경우)
		if (replay_st.end_frame > 0) {
			auto area = get_replay_slider_area();
			if (area.from.x >= 0 && area.from.y >= 0) {
				fill_rectangle(data, pitch, area, 1);
				line_rectangle(data, pitch, area, 12);

				int button_w = 16;
				int button_h = 32;
				int ow = (area.to.x - area.from.x) - button_w;
				int ox = replay_frame * ow / replay_st.end_frame;

				if (st.current_frame != replay_frame) {
					int cox = st.current_frame * ow / replay_st.end_frame;
					line_rectangle(data, pitch, rect{area.from + xy(cox + button_w / 2, 0), area.from + xy(cox + button_w / 2 + 1, button_h)}, 50);
				}

				fill_rectangle(data, pitch, rect{area.from + xy(ox, 0), area.from + xy(ox, 0) + xy(button_w, button_h)}, 10);
				line_rectangle(data, pitch, rect{area.from + xy(ox, 0), area.from + xy(ox, 0) + xy(button_w, button_h)}, 51);
			}
		}
		
		// === 마우스 커서 위치에 UI 좌표 실시간 표시 (모든 UI 위에 표시, F9로 토글) ===
		if (g_ui_show_coordinates) {
			// 전역 변수에서 마우스 좌표 가져오기
			int mouse_ui_x = g_ui_mouse_x;
			int mouse_ui_y = g_ui_mouse_y;
			
			// 마우스 커서 옆에 UI 좌표 표시 (오른쪽 아래에 표시)
			int text_x = mouse_ui_x + 15;  // 커서 오른쪽
			int text_y = mouse_ui_y + 15;  // 커서 아래
			
			// 화면 범위 체크 (하단 패널 포함 전체 화면)
			if (text_x >= 0 && text_x + 200 < (int)screen_width && text_y >= 0 && text_y + 30 < (int)screen_height) {
			// UI 좌표 텍스트 생성 (예: "UI: 335, 165")
			char ui_coord_text[64];
			std::snprintf(ui_coord_text, sizeof(ui_coord_text), "UI: %d, %d", mouse_ui_x, mouse_ui_y);
			
			const int scale = 3;  // FPS 크기 (3배)
			const uint8_t color = 255;  // 흰색
			
			// 각 문자를 5x7 폰트로 렌더링 (팔레트 인덱스 방식)
			int cx = text_x;
			for (int ci = 0; ui_coord_text[ci] != '\0'; ++ci) {
				const uint8_t* g = glyph5x7(ui_coord_text[ci]);
				
				// 5x7 글리프를 scale배 확대하여 그리기
				for (int i = 0; i < 5; ++i) {
					uint8_t bits = g[i];
					for (int j = 0; j < 7; ++j) {
						if (bits & (1u << j)) {
							// scale x scale 픽셀 블록 그리기
							for (int sx = 0; sx < scale; ++sx) {
								for (int sy = 0; sy < scale; ++sy) {
									int px = cx + i * scale + sx;
									int py = text_y + j * scale + sy;
									if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
										data[py * pitch + px] = color;
									}
								}
							}
						}
					}
				}
				cx += 6 * scale;  // 다음 문자 위치 (5 + 1 간격)
			}
		}
		}  // g_ui_show_coordinates
	}

	virtual void draw_callback(uint8_t* data, size_t pitch) {
	}

	a_vector<const image_t*> image_draw_queue;

	bool use_new_images = false;

	bool new_images_index_loaded = false;

	a_vector<char> grp_new_image_state = a_vector<char>((size_t)ImageTypes::None);
	a_unordered_set<a_string> new_images_index;
	string_table_data images_tbl;

	a_vector<a_vector<std::unique_ptr<native_window_drawing::surface>>> new_images;
	a_vector<a_vector<std::unique_ptr<native_window_drawing::surface>>> new_images_flipped;

	bool is_new_image(const image_t* image) {
		if (!use_new_images) return false;
		if (!new_images_index_loaded) {
			new_images_index_loaded = true;
			async_read_file("index.txt", [this](const uint8_t* data, size_t len) {
				if (!data) {
					ui::log("failed to load index.txt :(\n");
					return;
				}
				char* c = (char*)data;
				char* e = (char*)data + len;
				while (c != e && (*c == '\r' || *c =='\n' || *c == ' ')) ++c;
				while (c != e) {
					char* s = c;
					while (c != e && *c != '\r' && *c !='\n' && *c != ' ') ++c;
					a_string fn(s, c - s);
					while (c != e && (*c == '\r' || *c =='\n' || *c == ' ')) ++c;
					for (char& c : fn) {
						if (c == '\\') c = '/';
					}
					if (!fn.empty() && fn.front() == '/') fn.erase(fn.begin());
					if (!fn.empty() && fn.back() == '/') fn.erase(std::prev(fn.end()));
					ui::log("index entry '%s'\n", fn);
					new_images_index.insert(std::move(fn));
				}
			});
		}
		if (new_images_index.empty()) return false;
		size_t index = image->grp - global_st.grps.data();
		auto& state = grp_new_image_state.at(index);
		if (state == 0) {
			state = 2;
			a_string fn = images_tbl.at(image->image_type->grp_filename_index);
			for (char& c : fn) {
				if (c == '\\') c = '/';
			}
			for (auto i = fn.rbegin(); i != fn.rend(); ++i) {
				if (*i == '.') {
					fn.erase(std::prev(i.base()), fn.end());
				}
				if (*i == '/') break;
			}
			if (!fn.empty() && fn.front() == '/') fn.erase(fn.begin());
			fn = "unit/" + fn;
			ui::log("checking '%s' (image %d, grp %u)\n", fn, (int)image->image_type->id, index);
			if (new_images_index.count(fn)) {
				size_t frames = image->grp->frames.size();
				if (new_images.size() <= index) new_images.resize(index + 1);
				new_images[index].resize(frames);
				if (new_images_flipped.size() <= index) new_images_flipped.resize(index + 1);
				new_images_flipped[index].resize(frames);
				ui::log("loading %d frames...\n", frames);
				auto frames_left = std::make_shared<size_t>(frames);
				for (size_t i = 0; i != frames; ++i) {
					a_string frame_fn = format("%s/%02u.png", fn, i);
					async_read_file(frame_fn, [this, frame_fn, index, i, frames_left](const uint8_t* data, size_t len) {
						if (!data) {
							ui::log("failed to load '%s'\n", frame_fn);
							return;
						}
						new_images.at(index).at(i) = native_window_drawing::load_image(data, len);
						--*frames_left;
						if (*frames_left == 0) {
							grp_new_image_state.at(index) = 1;

							ui::log("grp %d successfully loaded %d frames\n", index, new_images.at(index).size());
						}
					});
				}
			}
		}
		//ui::log("index %d state %d\n", index, state);
		return state == 1;
	}

	native_window_drawing::surface* get_new_image_surface(const image_t* image, bool flipped) {
		size_t index = image->grp - global_st.grps.data();
		size_t frame = image->frame_index;
		if (flipped) {
			auto& r = new_images_flipped.at(index).at(frame);
			if (!r) {
				auto* s = new_images.at(index).at(frame).get();
				if (!s) return nullptr;
				r = flip_image(s);
			}
			return r.get();
		} else {
			return new_images.at(index).at(frame).get();
		}
	}

	std::unique_ptr<native_window_drawing::surface> tmp_surface;

	void draw_new_image(const image_t* image) {
		xy map_pos = get_image_center_map_position(image);

		int screen_x = map_pos.x - screen_pos.x;
		int screen_y = map_pos.y - screen_pos.y;

		auto* surface = get_new_image_surface(image, i_flag(image, image_t::flag_horizontally_flipped));
		if (!surface) {
			ui::log("ERROR: new image %d (grp %d) frame %d does not exist\n", (int)image->image_type->id, image->grp - global_st.grps.data(), image->frame_index);
			return;
		}

		auto scale = 114_fp8;

		size_t w = (fp8::integer(surface->w) * scale).integer_part();
		size_t h = (fp8::integer(surface->w) * scale).integer_part();

		size_t orig_w = w;
		size_t orig_h = h;

		screen_x -= w / 2;
		screen_y -= w / 2;

		if (screen_x >= (int)screen_width || screen_y >= (int)screen_height) return;
		if (screen_x + (int)w <= 0 || screen_y + (int)h <= 0) return;

		size_t offset_x = 0;
		size_t offset_y = 0;
		if (screen_x < 0) {
			offset_x = -screen_x;
			w += screen_x;
			screen_x = 0;
		}
		if (screen_y < 0) {
			offset_y = -screen_y;
			h += screen_y;
			screen_y = 0;
		}

		w = std::min(w, screen_width - screen_x);
		h = std::min(h, screen_height - screen_y);

		if (image->modifier == 10) {
			if (!tmp_surface || (size_t)tmp_surface->w < orig_w || (size_t)tmp_surface->h < orig_h) {
				tmp_surface = native_window_drawing::create_rgba_surface(orig_w, orig_h);
			}
			surface->set_blend_mode(native_window_drawing::blend_mode::none);
			surface->blit_scaled(&*tmp_surface, 0, 0, orig_w, orig_h);

			size_t src_pitch = tmp_surface->pitch / 4;
			size_t dst_pitch = rgba_surface->pitch / 4;
			uint32_t* src = (uint32_t*)tmp_surface->lock();
			uint32_t* dst = (uint32_t*)rgba_surface->lock();

			src += src_pitch * offset_y + offset_x;
			dst += dst_pitch * screen_y + screen_x;

			size_t src_skip = src_pitch - w;
			size_t dst_skip = dst_pitch - w;

			for (size_t y = h; y--;) {

				for (size_t x = w; x--;) {
					uint32_t s = *src;
					uint32_t d = *dst;
					if (s >> 24 >= 16) *dst = (d & 0xfefefe) / 2 | 0xff000000;
					++src;
					++dst;
				}

				src += src_skip;
				dst += dst_skip;
			}

			tmp_surface->unlock();
			rgba_surface->unlock();

		} else {
			surface->set_blend_mode(native_window_drawing::blend_mode::alpha);
			surface->blit_scaled(&*rgba_surface, screen_x - offset_x, screen_y - offset_y, orig_w, orig_h);
		}
	}

	void draw_image_queue() {
		for (auto* image : image_draw_queue) {
			draw_new_image(image);
		}
	}

	fp8 game_speed = fp8::integer(1);

	std::unique_ptr<native_window_drawing::surface> window_surface;
	std::unique_ptr<native_window_drawing::surface> indexed_surface;
	std::unique_ptr<native_window_drawing::surface> rgba_surface;
	native_window_drawing::palette* palette = nullptr;
	std::chrono::high_resolution_clock clock;
	std::chrono::high_resolution_clock::time_point last_draw;
	std::chrono::high_resolution_clock::time_point last_input_poll;
	std::chrono::high_resolution_clock::time_point last_fps;
	int fps_counter = 0;
	size_t scroll_speed_n = 0;

	void resize(int width, int height) {
		if (!wnd && create_window) wnd.create("OpenBW", 0, 0, width, height);
		screen_width = width;
		screen_height = height;
		//view_scale = fp16::integer(1) - (fp16::integer(1) / 4);
		view_scale = fp16::integer(1);
		view_width = (fp16::integer(screen_width) / view_scale).integer_part();
		view_height = (fp16::integer(screen_height) / view_scale).integer_part();
		view_scale = (ufp16::integer(screen_width) / view_width).as_signed();
		window_surface.reset();
		indexed_surface.reset();
		rgba_surface.reset();
	}

	a_vector<unit_id> current_selection;

	bool current_selection_is_selected(unit_t* u) {
		auto uid = get_unit_id(u);
		return std::find(current_selection.begin(), current_selection.end(), uid) != current_selection.end();
	}

	void current_selection_add(unit_t* u) {
		auto uid = get_unit_id(u);
		if (std::find(current_selection.begin(), current_selection.end(), uid) != current_selection.end()) return;
		current_selection.push_back(uid);
	}

	void current_selection_clear() {
		current_selection.clear();
	}

	void current_selection_remove(const unit_t* u) {
		auto uid = get_unit_id(u);
		auto i = std::find(current_selection.begin(), current_selection.end(), uid);
		if (i != current_selection.end()) current_selection.erase(i);
	}

	bool is_moving_minimap = false;
	bool is_moving_replay_slider = false;
	bool is_paused = false;
	bool is_drag_selecting = false;
	bool is_dragging_screen = false;
	int drag_select_from_x = 0;
	int drag_select_from_y = 0;
	int drag_select_to_x = 0;
	int drag_select_to_y = 0;
	xy drag_screen_pos;
	bool is_clicking_ui_panel = false;

	// ★ 커스텀 더블클릭 판정용 상태 변수
	unit_t* last_clicked_unit_for_dblclick = nullptr;
	std::chrono::high_resolution_clock::time_point last_click_time_for_dblclick;
	static constexpr int DOUBLE_CLICK_THRESHOLD_MS = 300;

	// Bottom panels areas (for input gating and drawing)
	rect bottom_info_area{};
	rect bottom_portrait_area{};
	rect bottom_cmd_area{};

	bool selection_suppressed = false;
	void set_selection_suppressed(bool v) { selection_suppressed = v; }

	// Build preview overlay
	bool build_preview_active = false;
	xy build_preview_pos;     // map coords center
	xy build_preview_size;    // placement_size in pixels
	bool build_preview_valid = false;

	// Mouse click indicator state
	int click_indicator_frames = 0; // frames left to display
	xy click_indicator_pos;         // map coords

	// Rally point visualization (단일 - 하위 호환)
	bool rally_visual_active = false;
	xy rally_building_pos;
	xy rally_point_pos;
	bool rally_is_auto = false;  // true=yellow, false=green
	
	// Rally point visualization (다중 - 여러 건물 선택 시)
	struct MultiRallyPoint {
		xy building_pos;
		xy rally_pos;
		bool is_auto_rally;
	};
	a_vector<MultiRallyPoint> multi_rally_points;

	a_string toast_text;
	int toast_frames_left = 0;

    // HUD fields
    bool hud_show = true;
    int hud_fps = 0;
    xy hud_cursor_pos{0,0};
    int hud_scale = 3;
    uint32_t hud_text_color = 0xFFFFFFFFu;
    uint32_t hud_outline_color = 0xC0000000u;
    uint32_t hud_bg_alpha = 0x60u; // alpha 0..255 for semi-transparent black background

	void show_toast(const a_string& text, int frames) {
		toast_text = text;
		toast_frames_left = frames;
	}

	void update() {
		auto now = clock.now();

		if (now - last_fps >= std::chrono::seconds(1)) {
			double secs = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_fps).count();
			hud_fps = (int)(fps_counter / (secs > 0 ? secs : 1));
			last_fps = now;
			fps_counter = 0;
		}
		++fps_counter;

		auto minimap_area = get_minimap_area();
		auto replay_slider_area = get_replay_slider_area();

		auto move_minimap = [&](int mouse_x, int mouse_y) {
			printf("[미니맵] move_minimap 호출 - 마우스=(%d,%d), 미니맵영역=(%d,%d)-(%d,%d)\n",
				mouse_x, mouse_y, minimap_area.from.x, minimap_area.from.y, minimap_area.to.x, minimap_area.to.y);
			
			if (mouse_x < minimap_area.from.x) mouse_x = minimap_area.from.x;
			else if (mouse_x >= minimap_area.to.x) mouse_x = minimap_area.to.x - 1;
			if (mouse_y < minimap_area.from.y) mouse_y = minimap_area.from.y;
			else if (mouse_y >= minimap_area.to.y) mouse_y = minimap_area.to.y - 1;
			int x = mouse_x - minimap_area.from.x;
			int y = mouse_y - minimap_area.from.y;
			x = x * game_st.map_tile_width / (minimap_area.to.x - minimap_area.from.x);
			y = y * game_st.map_tile_height / (minimap_area.to.y - minimap_area.from.y);
			
			xy old_screen_pos = screen_pos;
			screen_pos = xy(32 * x - view_width / 2, 32 * y - view_height / 2);
			
			printf("[미니맵] 화면 이동: (%d,%d) -> (%d,%d)\n",
				old_screen_pos.x, old_screen_pos.y, screen_pos.x, screen_pos.y);
		};

		auto check_move_minimap = [&](auto& e) {
			if (e.mouse_x >= minimap_area.from.x && e.mouse_x < minimap_area.to.x) {
				if (e.mouse_y >= minimap_area.from.y && e.mouse_y < minimap_area.to.y) {
					printf("[미니맵] 미니맵 클릭 감지! 마우스=(%d,%d)\n", e.mouse_x, e.mouse_y);
					is_moving_minimap = true;
					move_minimap(e.mouse_x, e.mouse_y);
				}
			}
		};

		auto move_replay_slider = [&](int mouse_x, int mouse_y) {
			(void)mouse_y;
			int x = mouse_x - replay_slider_area.from.x;
			int button_w = 16;
			x -= button_w / 2;
			int ow = (replay_slider_area.to.x - replay_slider_area.from.x) - button_w;
			if (x < 0) x = 0;
			if (x >= ow) x = ow - 1;
			replay_frame = x * replay_st.end_frame / ow;
		};

		auto check_move_replay_slider = [&](auto& e) {
			if (e.mouse_x >= replay_slider_area.from.x && e.mouse_x < replay_slider_area.to.x) {
				if (e.mouse_y >= replay_slider_area.from.y && e.mouse_y < replay_slider_area.to.y) {
					is_moving_replay_slider = true;
					move_replay_slider(e.mouse_x, e.mouse_y);
				}
			}
		};

		auto end_drag_select = [&](bool double_clicked) {
			bool shift = wnd.get_key_state(225) || wnd.get_key_state(229);
			
			// 드래그 좌표를 view_height 내로 제한 (크래시 방지)
			if (drag_select_from_y >= (int)view_height) drag_select_from_y = view_height - 1;
			if (drag_select_to_y >= (int)view_height) drag_select_to_y = view_height - 1;
			if (drag_select_from_x < 0) drag_select_from_x = 0;
			if (drag_select_to_x < 0) drag_select_to_x = 0;
			
			if (drag_select_from_x > drag_select_to_x) std::swap(drag_select_from_x, drag_select_to_x);
			if (drag_select_from_y > drag_select_to_y) std::swap(drag_select_from_y, drag_select_to_y);
			if (drag_select_to_x - drag_select_from_x <= 4 || drag_select_to_y - drag_select_from_y <= 4) {
				unit_t* u = select_get_unit_at(screen_pos + xy(drag_select_from_x, drag_select_from_y));
				if (u && !selection_suppressed) {
					bool ctrl = wnd.get_key_state(224) || wnd.get_key_state(228);
					if (double_clicked || ctrl) {
						if (!shift) current_selection_clear();
						auto is_tank = [&](unit_t* a) {
							return unit_is(a, UnitTypes::Terran_Siege_Tank_Siege_Mode) || unit_is(a, UnitTypes::Terran_Siege_Tank_Tank_Mode);
						};
						auto is_same_type = [&](unit_t* a, unit_t* b) {
							if (unit_is_mineral_field(a) && unit_is_mineral_field(b)) return true;
							if (is_tank(a) && is_tank(b)) return true;
							return a->unit_type == b->unit_type;
						};
						for (unit_t* u2 : find_units({screen_pos, screen_pos + xy(view_width, view_height)})) {
							if (u2->owner != u->owner) continue;
							if (!is_same_type(u, u2)) continue;
							current_selection_add(u2);
						}
					} else {
						if (shift) {
							if (current_selection_is_selected(u)) current_selection_remove(u);
							else current_selection_add(u);
						} else {
							current_selection_clear();
							current_selection_add(u);
						}
					}
				}
			} else {
				if (!selection_suppressed) {
					if (!shift) current_selection_clear();
					auto r = rect{{drag_select_from_x, drag_select_from_y}, {drag_select_to_x, drag_select_to_y}};
					if (r.from.x > r.to.x) std::swap(r.from.x, r.to.x);
					if (r.from.y > r.to.y) std::swap(r.from.y, r.to.y);
					a_vector<unit_t*> new_units;
					bool any_non_neutrals = false;
					for (unit_t* u : find_units(translate_rect(r, screen_pos))) {
						if (!unit_can_be_selected(u)) continue;
						new_units.push_back(u);
						if (u->owner != 11) any_non_neutrals = true;
					}
					for (unit_t* u : new_units) {
						if (u->owner == 11 && any_non_neutrals) continue;
						current_selection_add(u);
					}
				}
			}
			is_drag_selecting = false;
		};

		if (wnd) {
			native_window::event_t e;
			while (wnd.peek_message(e)) {
				switch (e.type) {
				case native_window::event_t::type_quit:
					if (exit_on_close) std::exit(0);
					else window_closed = true;
					break;
				case native_window::event_t::type_resize:
					resize(e.width, e.height);
					break;
				case native_window::event_t::type_mouse_button_down:
					if (e.button == 1) {
						// Input gating: if inside bottom panels, do not forward to game (no selection/slider/minimap)
						auto in_rect = [&](const rect& r){ return e.mouse_x >= r.from.x && e.mouse_x < r.to.x && e.mouse_y >= r.from.y && e.mouse_y < r.to.y; };
						if (in_rect(bottom_info_area) || in_rect(bottom_portrait_area) || in_rect(bottom_cmd_area)) {
							is_clicking_ui_panel = true;
							break;
						}
						check_move_minimap(e);
						check_move_replay_slider(e);
						if (!is_moving_minimap && !is_moving_replay_slider && !selection_suppressed) {
							// 드래그 선택 시작 (view_height 경계 체크)
							if (e.mouse_y < (int)view_height) {
								is_drag_selecting = true;
								drag_select_from_x = e.mouse_x;
								drag_select_from_y = e.mouse_y;
								drag_select_to_x = e.mouse_x;
								drag_select_to_y = e.mouse_y;
							}
						}
					} else if (e.button == 3) {
						// ★ 우클릭 드래그 비활성화 - 플레이어 명령용으로 사용
						// is_dragging_screen = true;
						// drag_screen_pos = screen_pos + xy((fp16::integer(e.mouse_x) / view_scale).integer_part(), (fp16::integer(e.mouse_y) / view_scale).integer_part());
					}
					break;
				case native_window::event_t::type_mouse_motion:
					if (e.button_state & 1) {
						if (is_clicking_ui_panel) break; // swallow drags over panel
						if (is_moving_minimap) check_move_minimap(e);
						if (is_moving_replay_slider) check_move_replay_slider(e);
						if (is_drag_selecting) {
							// 드래그 좌표 업데이트 (view_height 경계 체크)
							drag_select_to_x = e.mouse_x;
							drag_select_to_y = std::min(e.mouse_y, (int)view_height - 1);
						}
					} else if (e.button_state & 4) {
						// ★ 우클릭 드래그 비활성화
						// if (is_dragging_screen) {
						// 	screen_pos = drag_screen_pos - xy((fp16::integer(e.mouse_x) / view_scale).integer_part(), (fp16::integer(e.mouse_y) / view_scale).integer_part());
						// }
					}

					if (is_drag_selecting && ~e.button_state & 1 && !selection_suppressed) end_drag_select(false);
					break;
				case native_window::event_t::type_mouse_button_up:
					if (e.button == 1) {
						if (is_clicking_ui_panel) { is_clicking_ui_panel = false; break; }
						if (is_moving_minimap) is_moving_minimap = false;
						if (is_moving_replay_slider) is_moving_replay_slider = false;
						if (is_drag_selecting && !selection_suppressed) {
							// ★ 커스텀 더블클릭 판정
							unit_t* clicked_unit = select_get_unit_at(screen_pos + xy(drag_select_from_x, drag_select_from_y));
							bool is_valid_double_click = false;
							
							if (clicked_unit != nullptr) {
								auto now = std::chrono::high_resolution_clock::now();
								auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time_for_dblclick).count();
								
								// 더블클릭 조건:
								// 1) 같은 유닛 포인터를 연속 클릭 (a→a는 OK, a→b는 NO)
								// 2) 시간 내 (300ms)
								// 3) SDL도 더블클릭 인식 (e.clicks >= 2)
								if (clicked_unit == last_clicked_unit_for_dblclick && 
								    elapsed <= DOUBLE_CLICK_THRESHOLD_MS &&
								    e.clicks >= 2) {
									is_valid_double_click = true;
								}
								
								// 상태 업데이트 (다음 클릭을 위해)
								last_clicked_unit_for_dblclick = clicked_unit;
								last_click_time_for_dblclick = now;
							} else {
								// 빈 곳 클릭 시 초기화
								last_clicked_unit_for_dblclick = nullptr;
							}
							
							end_drag_select(is_valid_double_click);
						}
					} else if (e.button == 3) {
						// ★ 우클릭 드래그 비활성화
						// is_dragging_screen = false;
					}
					break;
				case native_window::event_t::type_key_down:
					//if (e.sym == 'q') {
					//	use_new_images = !use_new_images;
					//}
#ifndef EMSCRIPTEN
					if (e.sym == ' ' || e.sym == 'p') {
						is_paused = !is_paused;
					}
					if (e.sym == 'a' || e.sym == 'u') {
						if (game_speed < fp8::integer(128)) game_speed *= 2;
					}
					if (e.sym == 'd') {
						if (game_speed > 2_fp8) game_speed /= 2;
					}
					if (e.sym == '\b') {
						int t = 5 * 42 / 1000;
						if (replay_frame < t) replay_frame = 0;
						else replay_frame -= t;
					}
#endif
					break;
				}
			}
		}

		if (!indexed_surface) {
			if (wnd) {
				window_surface = native_window_drawing::get_window_surface(&wnd);
				rgba_surface = native_window_drawing::create_rgba_surface(window_surface->w, window_surface->h);
			} else {
				rgba_surface = native_window_drawing::create_rgba_surface(screen_width, screen_height);
			}
			indexed_surface = native_window_drawing::convert_to_8_bit_indexed(&*rgba_surface);
			if (!palette) set_image_data();
			indexed_surface->set_palette(palette);

			indexed_surface->set_blend_mode(native_window_drawing::blend_mode::none);
			rgba_surface->set_blend_mode(native_window_drawing::blend_mode::none);
			rgba_surface->set_alpha(0);

			if (window_surface) {
				window_surface->set_blend_mode(native_window_drawing::blend_mode::none);
				window_surface->set_alpha(0);
			}
		}

		if (wnd) {
			auto input_poll_speed = std::chrono::milliseconds(12);

			auto input_poll_t = now - last_input_poll;
			while (input_poll_t >= input_poll_speed) {
				if (input_poll_t >= input_poll_speed * 20) last_input_poll = now - input_poll_speed;
				else last_input_poll += input_poll_speed;
				std::array<int, 6> scroll_speeds = {2, 2, 4, 6, 6, 8};

				if (!is_drag_selecting) {
					int scroll_speed = scroll_speeds[scroll_speed_n];
					auto prev_screen_pos = screen_pos;
					if (wnd.get_key_state(81)) screen_pos.y += scroll_speed;
					else if (wnd.get_key_state(82)) screen_pos.y -= scroll_speed;
					if (wnd.get_key_state(79)) screen_pos.x += scroll_speed;
					else if (wnd.get_key_state(80)) screen_pos.x -= scroll_speed;
					if (screen_pos != prev_screen_pos) {
						if (scroll_speed_n != scroll_speeds.size() - 1) ++scroll_speed_n;
					} else scroll_speed_n = 0;
				}

				input_poll_t = now - last_input_poll;
			}

			if (is_moving_minimap) {
				int x = -1;
				int y = -1;
				wnd.get_cursor_pos(&x, &y);
				if (x != -1) move_minimap(x, y);
			}
			if (is_moving_replay_slider) {
				int x = -1;
				int y = -1;
				wnd.get_cursor_pos(&x, &y);
				if (x != -1) move_replay_slider(x, y);
			}
		}

		if (screen_pos.y + view_height > game_st.map_height) screen_pos.y = game_st.map_height - view_height;
		if (screen_pos.y < 0) screen_pos.y = 0;
		if (screen_pos.x + view_width > game_st.map_width) screen_pos.x = game_st.map_width - view_width;
		if (screen_pos.x < 0) screen_pos.x = 0;

		uint8_t* data = (uint8_t*)indexed_surface->lock();
		draw_tiles(data, indexed_surface->pitch);
		draw_sprites(data, indexed_surface->pitch);

		draw_callback(data, indexed_surface->pitch);

		if (draw_ui_elements) {
			draw_minimap(data, indexed_surface->pitch);
			draw_ui(data, indexed_surface->pitch, current_command_mode);
		}
		indexed_surface->unlock();

		rgba_surface->fill(0, 0, 0, 255);
		indexed_surface->blit(&*rgba_surface, 0, 0);

		draw_image_queue();

		// 프로토스 아이콘 RGBA 원본 렌더링 (640, 700) 위치, 높이 80에 맞춤
		if (g_draw_protoss_icons && g_protoss_icons_loaded && !g_protoss_icon_rgba.empty()) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			
			// 목표 위치 및 크기 (높이 80 기준, 비율 유지)
			const int dest_x = 595;
			const int dest_y = 700;
			const int dest_h = 80;
			
			// 원본 이미지 전체 사용 (3개 아이콘 가로 나열)
			const int icon_src_w = g_protoss_icon_width;   // 406
			const int icon_src_h = g_protoss_icon_height;  // 157
			
			// 높이 80에 맞춰 스케일 계산
			float scale = (float)dest_h / icon_src_h;
			int scaled_w = (int)(icon_src_w * scale);
			int scaled_h = dest_h;
			int offset_x = dest_x;
			int offset_y = dest_y;
			
			// 스케일링하여 렌더링
			for (int dy = 0; dy < scaled_h; ++dy) {
				int src_y = (int)(dy / scale);
				if (src_y >= g_protoss_icon_height) continue;
				
				for (int dx = 0; dx < scaled_w; ++dx) {
					int src_x = (int)(dx / scale);
					if (src_x >= g_protoss_icon_width) continue;
					
					// g_protoss_icon_rgba는 ARGB 형식 (0xAARRGGBB)
					uint32_t pixel = g_protoss_icon_rgba[src_y * g_protoss_icon_width + src_x];
					uint8_t a = (pixel >> 24) & 0xFF;
					uint8_t r = (pixel >> 16) & 0xFF;
					uint8_t g = (pixel >> 8) & 0xFF;
					uint8_t b = (pixel >> 0) & 0xFF;
					
					// 투명 픽셀 스킵
					if (a < 10) continue;
					
					int px = offset_x + dx;
					int py = offset_y + dy;
					
					if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
						// SDL surface는 ABGR 형식 (Little Endian) - R과 B 스왑 필요!
						uint32_t sdl_pixel = (a << 24) | (b << 16) | (g << 8) | r;
						if (a >= 250) {
							data[py * pitchw + px] = sdl_pixel;
						} else {
							data[py * pitchw + px] = alpha_blend_over(data[py * pitchw + px], sdl_pixel);
						}
					}
				}
			}
			rgba_surface->unlock();
			g_draw_protoss_icons = false;
		}

		// Armor 툴팁 렌더링 (크롭된 이미지 사용)
		if (g_draw_armor_tooltip && g_armor_tooltip_loaded && !g_armor_tooltip_rgba.empty()) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			
			// ★ 기준 위치 (여기만 수정하면 툴팁 전체가 함께 이동)
			const int base_x = 733;
			const int base_y = 700;
			
			// 스케일 설정 (크기 축소)
			const float scale = 0.6f;  // 60% 크기로 축소
			const int scaled_w = (int)(g_armor_tooltip_width * scale);
			const int scaled_h = (int)(g_armor_tooltip_height * scale);
			
			// 툴팁 이미지 렌더링 (스케일링 + 흰색 배경 완전 제거)
			for (int dy = 0; dy < scaled_h; ++dy) {
				int src_y = (int)(dy / scale);
				if (src_y >= g_armor_tooltip_height) continue;
				
				for (int dx = 0; dx < scaled_w; ++dx) {
					int src_x = (int)(dx / scale);
					if (src_x >= g_armor_tooltip_width) continue;
					
					uint32_t pixel = g_armor_tooltip_rgba[src_y * g_armor_tooltip_width + src_x];
					uint8_t a = (pixel >> 24) & 0xFF;
					uint8_t r = (pixel >> 16) & 0xFF;
					uint8_t g = (pixel >> 8) & 0xFF;
					uint8_t b = (pixel >> 0) & 0xFF;
					
					// 투명 픽셀 제거
					if (a < 250) continue;
					// 흰색 및 밝은 회색 배경 완전 제거 (더 엄격한 필터)
					if (r > 150 && g > 150 && b > 150) continue;
					
					int px = base_x + dx;
					int py = base_y + dy;
					
					if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
						// SDL surface는 ABGR 형식 - R과 B 스왑
						uint32_t sdl_pixel = (a << 24) | (b << 16) | (g << 8) | r;
						data[py * pitchw + px] = sdl_pixel;
					}
				}
			}
			
			// 방어력 수치 표시 (기준 위치 기준 상대 좌표)
			char armor_text[16];
			snprintf(armor_text, sizeof(armor_text), "%d", g_armor_value);
			int text_x = base_x + 70;  // 기준점에서 오른쪽으로 70px
			int text_y = base_y + 31;  // 기준점에서 아래로 31px
			uint32_t text_color = pack(255, 180, 180, 180);  // Protoss Armor 글씨체와 동일한 밝은 회색
			draw_text8x13_scaled(data, pitchw, text_x, text_y, std::string(armor_text), text_color, 1);
			
			rgba_surface->unlock();
			g_draw_armor_tooltip = false;
		}

		// Shield 툴팁 렌더링 (크롭된 이미지 사용)
		if (g_draw_shield_tooltip && g_shield_tooltip_loaded && !g_shield_tooltip_rgba.empty()) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			
			// ★ 기준 위치 (여기만 수정하면 툴팁 전체가 함께 이동)
			const int base_x = 802;
			const int base_y = 700;
			
			// 스케일 설정 (Armor와 동일한 크기로 축소)
			const float scale = 0.25f;  // 25% 크기로 축소 (Armor와 동일하게)
			const int scaled_w = (int)(g_shield_tooltip_width * scale);
			const int scaled_h = (int)(g_shield_tooltip_height * scale);
			
			// 툴팁 이미지 렌더링 (스케일링 + 흰색 배경 완전 제거)
			for (int dy = 0; dy < scaled_h; ++dy) {
				int src_y = (int)(dy / scale);
				if (src_y >= g_shield_tooltip_height) continue;
				
				for (int dx = 0; dx < scaled_w; ++dx) {
					int src_x = (int)(dx / scale);
					if (src_x >= g_shield_tooltip_width) continue;
					
					uint32_t pixel = g_shield_tooltip_rgba[src_y * g_shield_tooltip_width + src_x];
					uint8_t a = (pixel >> 24) & 0xFF;
					uint8_t r = (pixel >> 16) & 0xFF;
					uint8_t g = (pixel >> 8) & 0xFF;
					uint8_t b = (pixel >> 0) & 0xFF;
					
					// 투명 픽셀 제거
					if (a < 250) continue;
					// 흰색 및 밝은 회색 배경 완전 제거 (더 엄격한 필터)
					if (r > 150 && g > 150 && b > 150) continue;
					
					int px = base_x + dx;
					int py = base_y + dy;
					
					if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
						// SDL surface는 ABGR 형식 - R과 B 스왑
						uint32_t sdl_pixel = (a << 24) | (b << 16) | (g << 8) | r;
						data[py * pitchw + px] = sdl_pixel;
					}
				}
			}
			
			// 쉬드 수치 표시 (기준 위치 기준 상대 좌표)
			char shield_text[16];
			snprintf(shield_text, sizeof(shield_text), "%d", g_shield_value);
			int text_x = base_x + 75;  // 기준점에서 오른쪽으로 75px (877 - 802)
			int text_y = base_y + 31;  // 기준점에서 아래로 31px (731 - 700)
			uint32_t text_color = pack(255, 180, 180, 180);  // 밝은 회색
			draw_text8x13_scaled(data, pitchw, text_x, text_y, std::string(shield_text), text_color, 1);
			
			rgba_surface->unlock();
			g_draw_shield_tooltip = false;
		}

		if (is_drag_selecting) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			if (is_drag_selecting) {
				auto r = rect{{drag_select_from_x, drag_select_from_y}, {drag_select_to_x, drag_select_to_y}};
				if (r.from.x > r.to.x) std::swap(r.from.x, r.to.x);
				if (r.from.y > r.to.y) std::swap(r.from.y, r.to.y);

				line_rectangle_rgba(data, rgba_surface->pitch / 4, r, 0xff18fc10);
			}
			rgba_surface->unlock();
		}

		// Build preview rectangle
		if (build_preview_active) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			
			// build_preview_pos는 이제 타일 좌상단 좌표
			xy screen_topleft = build_preview_pos - screen_pos;
			int w = build_preview_size.x;
			int h = build_preview_size.y;
			int left = screen_topleft.x;
			int top = screen_topleft.y;
			int right = left + w;
			int bottom = top + h;
			
			uint32_t col = build_preview_valid ? 0xff18fc10u : 0xfff01010u; // green/red
			
			// clamp to screen
			left = std::max(left, 0); 
			top = std::max(top, 0);
			right = std::min(right, (int)screen_width - 1);
			bottom = std::min(bottom, (int)screen_height - 1);
			
			// draw rectangle border (두께 2픽셀)
			for (int x = left; x <= right; ++x) {
				data[top * pitchw + x] = col;
				if (top + 1 < (int)screen_height) data[(top + 1) * pitchw + x] = col;
				data[bottom * pitchw + x] = col;
				if (bottom - 1 >= 0) data[(bottom - 1) * pitchw + x] = col;
			}
			for (int y = top; y <= bottom; ++y) {
				data[y * pitchw + left] = col;
				if (left + 1 < (int)screen_width) data[y * pitchw + (left + 1)] = col;
				data[y * pitchw + right] = col;
				if (right - 1 >= 0) data[y * pitchw + (right - 1)] = col;
			}
			rgba_surface->unlock();
		}

		// Mouse click indicator (simple crosshair + square), fades out
		if (click_indicator_frames > 0) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			xy sp = click_indicator_pos - screen_pos;
			int cx = sp.x, cy = sp.y;
			uint32_t col = 0xffffffffu;
			// small square 10x10
			int left = std::max(0, cx - 5);
			int top = std::max(0, cy - 5);
			int right = std::min<int>(screen_width - 1, cx + 5);
			int bottom = std::min<int>(screen_height - 1, cy + 5);
			for (int x = left; x <= right; ++x) {
				data[top * pitchw + x] = col;
				data[bottom * pitchw + x] = col;
			}
			for (int y = top; y <= bottom; ++y) {
				data[y * pitchw + left] = col;
				data[y * pitchw + right] = col;
			}
			// crosshair lines length 6
			for (int dx = -6; dx <= 6; ++dx) {
				int xx = cx + dx;
				if (xx >= 0 && xx < (int)screen_width && cy >= 0 && cy < (int)screen_height) data[cy * pitchw + xx] = col;
			}
			for (int dy = -6; dy <= 6; ++dy) {
				int yy = cy + dy;
				if (cx >= 0 && cx < (int)screen_width && yy >= 0 && yy < (int)screen_height) data[yy * pitchw + cx] = col;
			}
			rgba_surface->unlock();
			--click_indicator_frames;
		}

		// Draw toast on top-left (8x13 폰트 사용)
		if (toast_frames_left > 0 && !toast_text.empty()) {
			uint32_t* data = (uint32_t*)rgba_surface->lock();
			int pitchw = rgba_surface->pitch / 4;
			int x0 = 10, y0 = 10;
			// a_string을 std::string으로 변환
			std::string toast_str(toast_text.data(), toast_text.size());
			// 2배 스케일 사용
			int scale = 2;
			int char_width = 9 * scale;  // 8 + 1 spacing
			int char_height = 13 * scale;
			int w = (int)toast_str.size() * char_width + 16;
			int h = char_height + 12;
			// bright background for readability
			uint32_t bg = 0xFFFFFF00u; // 밝은 노란색 (불투명)
			for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) put_pixel_rgba(data, pitchw, x0 + xx, y0 + yy, bg);
			// border (두껍게)
			uint32_t border = 0xFF000000u;
			for (int t = 0; t < 2; ++t) {
				for (int xx = 0; xx < w; ++xx) { 
					put_pixel_rgba(data, pitchw, x0 + xx, y0 + t, border); 
					put_pixel_rgba(data, pitchw, x0 + xx, y0 + h - 1 - t, border); 
				}
				for (int yy = 0; yy < h; ++yy) { 
					put_pixel_rgba(data, pitchw, x0 + t, y0 + yy, border); 
					put_pixel_rgba(data, pitchw, x0 + w - 1 - t, y0 + yy, border); 
				}
			}
			// text (8x13 폰트, 2배 크기, 검은색)
			draw_text8x13_scaled(data, pitchw, x0 + 8, y0 + 6, toast_str, 0xFF000000u, scale);
			rgba_surface->unlock();
			--toast_frames_left;
		}

        // Draw HUD at top-center (12 o'clock)
        if (hud_show) {
            uint32_t* data = (uint32_t*)rgba_surface->lock();
            if (data) {
                int pitchw = rgba_surface->pitch / 4;
                char buf[128];
                std::snprintf(buf, sizeof(buf), "FPS: %d   X:%d Y:%d", hud_fps, hud_cursor_pos.x, hud_cursor_pos.y);
                std::string s(buf);
                int scale = hud_scale;
                int tw = (int)s.size() * (8 + 1) * scale;
                int x0 = (int)screen_width / 2 - tw / 2;
                int y0 = 6 + 18 * scale; // push below HUD row to avoid overlap
                int bg_h = 16 * scale; // 13 + padding
                uint32_t bg = (hud_bg_alpha << 24); // semi-transparent black with configurable alpha
                for (int yy = 0; yy < bg_h; ++yy) for (int xx = 0; xx < tw; ++xx) put_pixel_rgba_blend(data, pitchw, x0 + xx, y0 + yy, bg);
                // outline (8-connected)
                uint32_t outline = hud_outline_color;
                draw_text8x13_scaled(data, pitchw, x0-1, y0,   s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0+1, y0,   s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0,   y0-1, s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0,   y0+1, s, outline, scale);
                // main text
                draw_text8x13_scaled(data, pitchw, x0, y0, s, hud_text_color, scale);
                rgba_surface->unlock();
            }
        }

        // ★ 기존 하단 UI 패널 비활성화 (새로운 스타크래프트 스타일 UI 사용)
        /*
        // Bottom UI panels scaffold (Info, Portrait, Command) next to minimap
        {
            // Heights and widths (placeholder layout similar to SC ratio)
            const int panel_h = 160;
            int y0 = (int)screen_height - panel_h;
            if (y0 < 0) y0 = 0;

            // Base background strip across bottom (under the world view) excluding minimap area
            auto mma = get_minimap_area();
            {
                uint32_t* data = (uint32_t*)rgba_surface->lock();
                if (data) {
                    int pitchw = rgba_surface->pitch / 4;
                    uint32_t bg = (uint32_t)(std::min<int>(0x80, hud_bg_alpha + 0x20)) << 24; // a bit stronger
                    for (int yy = y0; yy < (int)screen_height; ++yy) {
                        for (int xx = 0; xx < (int)screen_width; ++xx) {
                            bool in_minimap = (xx >= mma.from.x && xx < mma.to.x && yy >= mma.from.y && yy < mma.to.y);
                            if (!in_minimap) put_pixel_rgba_blend(data, pitchw, xx, yy, bg);
                        }
                    }
                    rgba_surface->unlock();
                }
            }

            // Minimap area (existing); place panels to its right
            int x_left = std::max(mma.to.x + 6, 6);

            // Info panel (left of center)
            int info_w = 280;
            int info_x = x_left;
            int info_y = y0 + 8;
            // Portrait panel (center)
            int portrait_w = 128;
            int portrait_h = 128;
            int portrait_x = info_x + info_w + 8;
            int portrait_y = y0 + panel_h - portrait_h - 8;
            // Command panel (right)
            int cmd_x = portrait_x + portrait_w + 8;
            int cmd_y = y0 + 8;
            int cmd_w = (int)screen_width - cmd_x - 8;
            int cmd_h = panel_h - 16;

            // Draw panels boxes and labels
            auto draw_panel_box = [&](int x, int y, int w, int h, const char* label) {
                uint32_t* data = (uint32_t*)rgba_surface->lock();
                if (!data) return;
                int pitchw = rgba_surface->pitch / 4;
                // panel bg
                uint32_t bg = (uint32_t)(std::min<int>(0xA0, hud_bg_alpha + 0x40)) << 24;
                for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) put_pixel_rgba_blend(data, pitchw, x + xx, y + yy, bg);
                // border via text outline color
                uint32_t col = hud_outline_color | 0xFF000000u;
                int right = x + w - 1, bottom = y + h - 1;
                for (int xx = x; xx <= right; ++xx) {
                    if (y >= 0 && y < (int)screen_height) data[y * pitchw + xx] = col;
                    if (bottom >= 0 && bottom < (int)screen_height) data[bottom * pitchw + xx] = col;
                }
                for (int yy = y; yy <= bottom; ++yy) {
                    if (x >= 0 && x < (int)screen_width) data[yy * pitchw + x] = col;
                    if (right >= 0 && right < (int)screen_width) data[yy * pitchw + right] = col;
                }
                // label
                std::string s(label);
                draw_text8x13_scaled(data, pitchw, x + 8, y + 8, s, hud_text_color, std::max(1, hud_scale - 1));
                rgba_surface->unlock();
            };

            draw_panel_box(info_x, info_y, info_w, panel_h - 16, "INFO");
            draw_panel_box(portrait_x, portrait_y, portrait_w, portrait_h, "PORTRAIT");
            draw_panel_box(cmd_x, cmd_y, cmd_w, cmd_h, "COMMANDS");
        */  // ★ 기존 UI 비활성화 끝

        /*  // ★ 기존 UI 비활성화 시작
            // INFO panel content: selection count + first selected unit bars (HP/Shield/Energy)
            {
                uint32_t* data = (uint32_t*)rgba_surface->lock();
                if (data) {
                    int pitchw = rgba_surface->pitch / 4;
                    int scale_s = std::max(1, hud_scale - 1);
                    int count = (int)current_selection.size();
                    std::string s = std::string("Selected: ") + std::to_string(count);
                    draw_text8x13_scaled(data, pitchw, info_x + 8, info_y + 28, s, hud_text_color, scale_s);

                    // Find first selected unit
                    unit_t* sel = nullptr;
                    for (unit_t* u : ptr(st.visible_units)) {
                        if (!u || !u->sprite) continue;
                        if (current_selection_is_selected(u)) { sel = u; break; }
                    }
                    if (sel) {
                        // Bars layout
                        int bx = info_x + 8;
                        int by = info_y + 48;
                        int bw = info_w - 16;
                        int bh = 10;

                        auto draw_bar = [&](int x, int y, int w, int h, float ratio, uint32_t fg_col, uint32_t bg_col) {
                            // background
                            for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) put_pixel_rgba_blend(data, pitchw, x + xx, y + yy, bg_col);
                            int fw = (int)(w * std::max(0.f, std::min(1.f, ratio)));
                            for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < fw; ++xx) put_pixel_rgba_blend(data, pitchw, x + xx, y + yy, fg_col);
                            // border
                            int right = x + w - 1, bottom = y + h - 1;
                            uint32_t border = 0xFF000000u;
                            for (int xx = x; xx <= right; ++xx) { data[y * pitchw + xx] = border; data[bottom * pitchw + xx] = border; }
                            for (int yy = y; yy <= bottom; ++yy) { data[yy * pitchw + x] = border; data[yy * pitchw + right] = border; }
                        };

                        // HP
                        float hp_max = (float)sel->unit_type->hitpoints.integer_part();
                        float hp_cur = (float)sel->hp.integer_part();
                        if (hp_max < 1.f) hp_max = 1.f;
                        draw_bar(bx, by, bw, bh, hp_cur / hp_max, 0xFF18FC10u, (uint32_t)(std::min<int>(0x50, hud_bg_alpha) << 24));
                        draw_text8x13_scaled(data, pitchw, bx + 4, by - 14, std::string("HP"), hud_text_color, scale_s);
                        by += bh + 8;

                        // Shield (if any)
                        if (sel->unit_type->shield_points > 0) {
                            float sh_max = (float)sel->unit_type->shield_points;
                            float sh_cur = (float)sel->shield_points.integer_part();
                            draw_bar(bx, by, bw, bh, sh_cur / std::max(1.f, sh_max), 0xFF00FFFFu, (uint32_t)(std::min<int>(0x50, hud_bg_alpha) << 24));
                            draw_text8x13_scaled(data, pitchw, bx + 4, by - 14, std::string("SHIELD"), hud_text_color, scale_s);
                            by += bh + 8;
                        }

                        // Energy (if has energy flag)
                        if (sel->unit_type->flags & unit_type_t::flag_has_energy) {
                            int max_e = (sel->unit_type->flags & unit_type_t::flag_hero) ? 250 : 200;
                            float e_cur = (float)sel->energy.integer_part();
                            draw_bar(bx, by, bw, bh, e_cur / (float)max_e, 0xFF4060FFu, (uint32_t)(std::min<int>(0x50, hud_bg_alpha) << 24));
                            draw_text8x13_scaled(data, pitchw, bx + 4, by - 14, std::string("ENERGY"), hud_text_color, scale_s);
                            by += bh + 8;
                        }
                    }
                    rgba_surface->unlock();
                }
            }

            // COMMANDS panel 3x3 placeholders with hotkey letters
            int cmd_grid_cols = 3, cmd_grid_rows = 3;
            int cmd_grid_pad = 6;
            int cell_w = 0, cell_h = 0;
            {
                uint32_t* data = (uint32_t*)rgba_surface->lock();
                if (data) {
                    int pitchw = rgba_surface->pitch / 4;
                    cell_w = (cmd_w - cmd_grid_pad * (cmd_grid_cols + 1)) / cmd_grid_cols;
                    cell_h = (cmd_h - cmd_grid_pad * (cmd_grid_rows + 1)) / cmd_grid_rows;
                    const char* labels[9] = {"A","M","H","S","P","B","G","T","C"};
                    for (int r = 0; r < cmd_grid_rows; ++r) {
                        for (int c = 0; c < cmd_grid_cols; ++c) {
                            int idx = r*cmd_grid_cols + c;
                            int bx = cmd_x + cmd_grid_pad + c * (cell_w + cmd_grid_pad);
                            int by = cmd_y + cmd_grid_pad + r * (cell_h + cmd_grid_pad);
                            // button bg
                            uint32_t bg = (uint32_t)(std::min<int>(0x90, hud_bg_alpha + 0x30)) << 24;
                            for (int yy = 0; yy < cell_h; ++yy) for (int xx = 0; xx < cell_w; ++xx) put_pixel_rgba_blend(data, pitchw, bx + xx, by + yy, bg);
                            // border
                            uint32_t col = hud_outline_color | 0xFF000000u;
                            int right = bx + cell_w - 1, bottom = by + cell_h - 1;
                            for (int xx = bx; xx <= right; ++xx) {
                                if (by >= 0 && by < (int)screen_height) data[by * pitchw + xx] = col;
                                if (bottom >= 0 && bottom < (int)screen_height) data[bottom * pitchw + xx] = col;
                            }
                            for (int yy = by; yy <= bottom; ++yy) {
                                if (bx >= 0 && bx < (int)screen_width) data[yy * pitchw + bx] = col;
                                if (right >= 0 && right < (int)screen_width) data[yy * pitchw + right] = col;
                            }
                            // label letter centered approximately
                            std::string s(labels[idx]);
                            int sx = bx + cell_w/2 - (4 * std::max(1, hud_scale - 1));
                            int sy = by + cell_h/2 - (6 * std::max(1, hud_scale - 1));
                            draw_text8x13_scaled(data, pitchw, sx, sy, s, hud_text_color, std::max(1, hud_scale - 1));
                        }
                    }
                    rgba_surface->unlock();
                }
            }

            // Hover highlight and tooltip over command cells
            {
                int mx = 0, my = 0;
                wnd.get_cursor_pos(&mx, &my);
                if (mx >= cmd_x && mx < cmd_x + cmd_w && my >= cmd_y && my < cmd_y + cmd_h) {
                    int cx = (mx - cmd_x - cmd_grid_pad) / (cell_w + cmd_grid_pad);
                    int cy = (my - cmd_y - cmd_grid_pad) / (cell_h + cmd_grid_pad);
                    if (cx >= 0 && cx < cmd_grid_cols && cy >= 0 && cy < cmd_grid_rows) {
                        int bx = cmd_x + cmd_grid_pad + cx * (cell_w + cmd_grid_pad);
                        int by = cmd_y + cmd_grid_pad + cy * (cell_h + cmd_grid_pad);
                        // highlight border
                        uint32_t* data = (uint32_t*)rgba_surface->lock();
                        if (data) {
                            int pitchw = rgba_surface->pitch / 4;
                            uint32_t hl = 0xFF18FC10u;
                            int right = bx + cell_w - 1, bottom = by + cell_h - 1;
                            for (int xx = bx; xx <= right; ++xx) { data[by * pitchw + xx] = hl; data[bottom * pitchw + xx] = hl; }
                            for (int yy = by; yy <= bottom; ++yy) { data[yy * pitchw + bx] = hl; data[yy * pitchw + right] = hl; }
                            rgba_surface->unlock();
                        }

                        // tooltip text
                        const char* tips[9] = {"A: 공격", "M: 이동", "H: 홀드", "S: 정지", "P: 정찰", "B: 건설", "G: 채취", "T: 귀환", "C: 취소"};
                        int idx = cy * cmd_grid_cols + cx;
                        a_string tip = tips[idx];
                        show_toast(tip, 45);
                    }
                }
            }
        }
        */  // ★ 기존 UI 비활성화 끝

        // ★ 렐리 포인트 시각화 (초록색/노란색 선) - 게임 화면에 직접 그리기
        // 다중 렐리 포인트 렌더링 (여러 건물 선택 시)
        if (!multi_rally_points.empty()) {
            uint32_t* data = (uint32_t*)rgba_surface->lock();
            if (data) {
                int pitchw = rgba_surface->pitch / 4;
                const int game_viewport_height = (int)screen_height - 220;  // 220 = 패널 높이
                
                // 모든 렐리 포인트 그리기
                for (const auto& mrp : multi_rally_points) {
                    // 화면 좌표로 변환
                    xy building_screen = {mrp.building_pos.x - screen_pos.x, 
                                         mrp.building_pos.y - screen_pos.y};
                    xy rally_screen = {mrp.rally_pos.x - screen_pos.x, 
                                      mrp.rally_pos.y - screen_pos.y};
                    
                    // 색상 선택 (초록색 또는 노란색)
                    uint32_t line_color = mrp.is_auto_rally ? 
                        0xFFFFFF00u :  // 노란색 (6번 자동 렐리)
                        0xFF00FF00u;   // 초록색 (5번 일반 렐리)
                    
                    // 선 그리기 (Bresenham 알고리즘)
                    int x0 = building_screen.x, y0 = building_screen.y;
                    int x1 = rally_screen.x, y1 = rally_screen.y;
                    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
                    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
                    int err = dx + dy, e2;
                    
                    while (true) {
                        // 게임 뷰포트 범위 체크 (하단 패널 제외)
                        if (x0 >= 0 && x0 < (int)screen_width && y0 >= 0 && y0 < game_viewport_height) {
                            data[y0 * pitchw + x0] = line_color;
                            // 두께 3픽셀 (더 잘 보이도록)
                            if (x0 + 1 < (int)screen_width) data[y0 * pitchw + x0 + 1] = line_color;
                            if (x0 + 2 < (int)screen_width) data[y0 * pitchw + x0 + 2] = line_color;
                            if (y0 + 1 < game_viewport_height) data[(y0 + 1) * pitchw + x0] = line_color;
                            if (y0 + 2 < game_viewport_height) data[(y0 + 2) * pitchw + x0] = line_color;
                        }
                        if (x0 == x1 && y0 == y1) break;
                        e2 = 2 * err;
                        if (e2 >= dy) { err += dy; x0 += sx; }
                        if (e2 <= dx) { err += dx; y0 += sy; }
                    }
                }
                
                rgba_surface->unlock();
            }
        }
        // 단일 렐리 포인트 렌더링 (하위 호환 - multi_rally_points가 비어있을 때만)
        else if (rally_visual_active) {
            uint32_t* data = (uint32_t*)rgba_surface->lock();
            if (data) {
                int pitchw = rgba_surface->pitch / 4;
                
                // 화면 좌표로 변환
                xy building_screen = {rally_building_pos.x - screen_pos.x, 
                                     rally_building_pos.y - screen_pos.y};
                xy rally_screen = {rally_point_pos.x - screen_pos.x, 
                                  rally_point_pos.y - screen_pos.y};
                
                // 색상 선택 (초록색 또는 노란색)
                uint32_t line_color = rally_is_auto ? 
                    0xFFFFFF00u :  // 노란색 (6번 자동 렐리)
                    0xFF00FF00u;   // 초록색 (5번 일반 렐리)
                
                // 선 그리기 (Bresenham 알고리즘)
                int x0 = building_screen.x, y0 = building_screen.y;
                int x1 = rally_screen.x, y1 = rally_screen.y;
                int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
                int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
                int err = dx + dy, e2;
                
                // 게임 뷰포트 영역 (하단 패널 제외)
                const int game_viewport_height = (int)screen_height - 220;  // 220 = 패널 높이
                
                while (true) {
                    // 게임 뷰포트 범위 체크 (하단 패널 제외)
                    if (x0 >= 0 && x0 < (int)screen_width && y0 >= 0 && y0 < game_viewport_height) {
                        data[y0 * pitchw + x0] = line_color;
                        // 두께 3픽셀 (더 잘 보이도록)
                        if (x0 + 1 < (int)screen_width) data[y0 * pitchw + x0 + 1] = line_color;
                        if (x0 + 2 < (int)screen_width) data[y0 * pitchw + x0 + 2] = line_color;
                        if (y0 + 1 < game_viewport_height) data[(y0 + 1) * pitchw + x0] = line_color;
                        if (y0 + 2 < game_viewport_height) data[(y0 + 2) * pitchw + x0] = line_color;
                    }
                    if (x0 == x1 && y0 == y1) break;
                    e2 = 2 * err;
                    if (e2 >= dy) { err += dy; x0 += sx; }
                    if (e2 <= dx) { err += dx; y0 += sy; }
                }
                
                rgba_surface->unlock();
            }
        }
        
        // Draw top-right resources bar: Minerals, Gas, Supply (player 0)
        {
            int pid = 0;
            int minerals = st.current_minerals[pid];
            int gas = st.current_gas[pid];
            int race_id = (int)st.players[pid].race;
            int supply_used = st.supply_used[pid][race_id].integer_part() / 2;
            int supply_max  = st.supply_available[pid][race_id].integer_part() / 2;

            char buf[160];
            std::snprintf(buf, sizeof(buf), "M:%d   G:%d   %d/%d", minerals, gas, supply_used, supply_max);
            std::string s(buf);

            uint32_t* data = (uint32_t*)rgba_surface->lock();
            if (data) {
                int pitchw = rgba_surface->pitch / 4;
                int scale = hud_scale;
                int tw = (int)s.size() * (8 + 1) * scale;
                int x0 = (int)screen_width - tw - 12;
                if (x0 < 8) x0 = 8;
                int y0 = 6;
                int bg_h = 16 * scale;
                uint32_t bg = (hud_bg_alpha << 24);
                for (int yy = 0; yy < bg_h; ++yy) for (int xx = 0; xx < tw; ++xx) put_pixel_rgba_blend(data, pitchw, x0 + xx, y0 + yy, bg);
                uint32_t outline = hud_outline_color;
                draw_text8x13_scaled(data, pitchw, x0-1, y0,   s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0+1, y0,   s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0,   y0-1, s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0,   y0+1, s, outline, scale);
                draw_text8x13_scaled(data, pitchw, x0, y0, s, hud_text_color, scale);
                rgba_surface->unlock();
            }
        }

		if (wnd) {
			rgba_surface->blit(&*window_surface, 0, 0);
			wnd.update_surface();
		}
	}

	std::tuple<int, int, uint32_t*> get_rgba_buffer() {
		void* r = rgba_surface->lock();
		rgba_surface->unlock();
		return std::make_tuple(rgba_surface->pitch / 4, rgba_surface->h, (uint32_t*)r);
	}

	template<typename cb_F>
	void async_read_file(a_string filename, cb_F cb) {
#ifdef EMSCRIPTEN
		auto uptr = std::make_unique<cb_F>(std::move(cb));
		auto f = [](void* ptr, uint8_t* data, size_t size) {
			cb_F* cb_p = (cb_F*)ptr;
			std::unique_ptr<cb_F> uptr(cb_p);
			(*cb_p)(data, size);
		};
		auto* a = filename.c_str();
		auto* b = (void(*)(void*, uint8_t*, size_t))f;
		auto* c = uptr.release();
		EM_ASM({js_download_file($0, $1, $2);}, a, b, c);
#else
		filename = "data/" + filename;
		FILE* f = fopen(filename.c_str(), "rb");
		if (!f) {
			cb(nullptr, 0);
		} else {
			a_vector<uint8_t> data;
			fseek(f, 0, SEEK_END);
			data.resize(ftell(f));
			fseek(f, 0, SEEK_SET);
			data.resize(fread(data.data(), 1, data.size(), f));
			fclose(f);
			cb(data.data(), data.size());
		}
#endif
	}

	std::array<tileset_image_data, 8> all_tileset_img;

	template<typename load_data_file_F>
	void load_all_image_data(load_data_file_F&& load_data_file) {
		load_image_data(img, load_data_file);
		for (size_t i = 0; i != 8; ++i) {
			load_tileset_image_data(all_tileset_img[i], i, load_data_file);
		}
	}

	void set_image_data() {
		tileset_img = all_tileset_img.at(game_st.tileset_index);

		if (!palette) palette = native_window_drawing::new_palette();

		native_window_drawing::color palette_colors[256];
		if (tileset_img.wpe.size() != 256 * 4) error("wpe size invalid (%d)", tileset_img.wpe.size());
		for (size_t i = 0; i != 256; ++i) {
			palette_colors[i].r = tileset_img.wpe[4 * i + 0];
			palette_colors[i].g = tileset_img.wpe[4 * i + 1];
			palette_colors[i].b = tileset_img.wpe[4 * i + 2];
			palette_colors[i].a = tileset_img.wpe[4 * i + 3];
		}
		palette->set_colors(palette_colors);
	}

	void reset() {
		apm = {};
		replay_frame = 0;
		auto& game = *st.game;
		st = state();
		game = game_state();
		replay_st = replay_state();
		action_st = action_state();

		st.global = &global_st;
		st.game = &game;
		replay_st = replay_state();
		action_st = action_state();

		st.global = &global_st;
		st.game = &game;
	}
};

}

#endif
