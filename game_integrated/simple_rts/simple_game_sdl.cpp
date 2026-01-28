// Simple RTS - C++ SDL2 버전 (HTML과 동일한 로직)
// HTML 버전과 비교하기 위한 간단한 구현

#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// 기본 구조체
struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalize() const {
        float len = length();
        return len > 0 ? Vec2(x / len, y / len) : Vec2(0, 0);
    }
};

// 유닛 타입
enum class UnitType {
    MARINE,
    ZEALOT
};

// 유닛 데이터
struct UnitData {
    const char* name;
    float maxHp;
    float maxShield;
    float speed;
    float attackPower;
    float attackRange;
    float attackCooldown;
    float size;
    SDL_Color color;
};

const UnitData UNIT_STATS[] = {
    {"Marine", 40.0f, 0.0f, 3.5f, 6.0f, 120.0f, 15.0f, 16.0f, {0, 255, 0, 255}},
    {"Zealot", 100.0f, 60.0f, 3.0f, 16.0f, 20.0f, 22.0f, 18.0f, {255, 170, 0, 255}}
};

// 유닛 클래스 (HTML unit.js와 동일한 로직)
class Unit {
public:
    int id;
    UnitType type;
    int owner;
    Vec2 position;
    float hp;
    float shield;
    float attackCooldown;
    bool isSelected;
    Vec2 targetPos;
    Unit* attackTarget;
    std::vector<Vec2> path;
    int pathIndex;
    
    Unit(int id, UnitType type, int owner, Vec2 pos) 
        : id(id), type(type), owner(owner), position(pos), 
          attackCooldown(0), isSelected(false), attackTarget(nullptr),
          pathIndex(0) {
        const UnitData& data = UNIT_STATS[(int)type];
        hp = data.maxHp;
        shield = data.maxShield;
        targetPos = pos;
    }
    
    const UnitData& getData() const { return UNIT_STATS[(int)type]; }
    bool isDead() const { return hp <= 0; }
    
    // HTML unit.js의 moveTo()와 동일
    void moveTo(Vec2 target) {
        targetPos = target;
        attackTarget = nullptr;
        
        // 간단한 직선 경로 (HTML pathfinding.js 간소화 버전)
        path.clear();
        path.push_back(target);
        pathIndex = 0;
    }
    
    // HTML unit.js의 attackTarget()와 동일
    void attack(Unit* target) {
        attackTarget = target;
        if (target) {
            targetPos = target->position;
        }
    }
    
    // HTML unit.js의 update()와 동일
    void update(float deltaTime, std::vector<Unit*>& allUnits) {
        // 쿨다운 감소
        if (attackCooldown > 0) {
            attackCooldown -= deltaTime * 60.0f;
        }
        
        // 이동 처리 (HTML unit.js updateMovement()와 동일)
        updateMovement(deltaTime);
        
        // 전투 처리 (HTML combat.js와 동일)
        updateCombat(allUnits);
    }
    
private:
    // HTML unit.js updateMovement()와 동일한 로직
    void updateMovement(float deltaTime) {
        if (path.empty()) return;
        
        Vec2 target = path[pathIndex];
        Vec2 diff = target - position;
        float distance = diff.length();
        
        if (distance < 3.0f) {
            pathIndex++;
            if (pathIndex >= (int)path.size()) {
                path.clear();
                pathIndex = 0;
            }
        } else {
            Vec2 direction = diff.normalize();
            float moveSpeed = getData().speed * deltaTime * 60.0f;
            position = position + direction * moveSpeed;
        }
    }
    
    // HTML combat.js와 동일한 로직
    void updateCombat(std::vector<Unit*>& allUnits) {
        if (attackTarget && !attackTarget->isDead()) {
            float dist = (attackTarget->position - position).length();
            
            if (dist > getData().attackRange) {
                targetPos = attackTarget->position;
                moveTo(targetPos);
            } else {
                if (attackCooldown <= 0) {
                    performAttack(attackTarget);
                    attackCooldown = getData().attackCooldown;
                }
            }
        } else {
            // 자동 공격
            Unit* nearestEnemy = findNearestEnemy(allUnits);
            if (nearestEnemy) {
                float dist = (nearestEnemy->position - position).length();
                if (dist <= getData().attackRange && attackCooldown <= 0) {
                    performAttack(nearestEnemy);
                    attackCooldown = getData().attackCooldown;
                }
            }
        }
    }
    
    // HTML combat.js calculateDamage()와 동일
    void performAttack(Unit* target) {
        float damage = getData().attackPower;
        
        if (target->shield > 0) {
            target->shield -= damage;
            if (target->shield < 0) {
                target->hp += target->shield;
                target->shield = 0;
            }
        } else {
            target->hp -= damage;
        }
    }
    
    Unit* findNearestEnemy(std::vector<Unit*>& allUnits) {
        Unit* nearest = nullptr;
        float minDist = 999999.0f;
        
        for (Unit* unit : allUnits) {
            if (unit->owner == owner || unit->isDead()) continue;
            
            float dist = (unit->position - position).length();
            if (dist < minDist) {
                minDist = dist;
                nearest = unit;
            }
        }
        
        return nearest;
    }
};

// 충돌 시스템 (HTML collision.js와 동일)
class CollisionSystem {
public:
    static void resolveCollisions(std::vector<Unit*>& units) {
        for (size_t i = 0; i < units.size(); i++) {
            for (size_t j = i + 1; j < units.size(); j++) {
                if (units[i]->isDead() || units[j]->isDead()) continue;
                
                Vec2 diff = units[i]->position - units[j]->position;
                float distance = diff.length();
                float minDist = units[i]->getData().size + units[j]->getData().size;
                
                if (distance < minDist && distance > 0) {
                    float overlap = minDist - distance;
                    Vec2 push = diff.normalize() * (overlap * 0.5f);
                    
                    units[i]->position = units[i]->position + push;
                    units[j]->position = units[j]->position - push;
                }
            }
        }
    }
};

// 게임 클래스
class Game {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    std::vector<Unit*> units;
    std::vector<Unit*> selectedUnits;
    int nextUnitId;
    bool running;
    
    // 마우스 상태
    Vec2 mousePos;
    bool isDragging;
    Vec2 dragStart;
    
public:
    Game() : window(nullptr), renderer(nullptr), nextUnitId(0), 
             running(true), isDragging(false) {}
    
    ~Game() {
        for (Unit* unit : units) delete unit;
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL 초기화 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        window = SDL_CreateWindow("Simple RTS - C++ vs HTML 비교",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1200, 800, SDL_WINDOW_SHOWN);
        
        if (!window) {
            std::cerr << "윈도우 생성 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "렌더러 생성 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // 유닛 생성 (HTML과 동일)
        for (int i = 0; i < 15; i++) {
            float x = 100.0f + (i % 5) * 50.0f;
            float y = 100.0f + (i / 5) * 50.0f;
            units.push_back(new Unit(nextUnitId++, UnitType::MARINE, 0, Vec2(x, y)));
        }
        
        units.push_back(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(900, 300)));
        units.push_back(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(950, 350)));
        
        std::cout << "=== Simple RTS C++ 버전 ===" << std::endl;
        std::cout << "아군: 15개 (마린)" << std::endl;
        std::cout << "적군: 2개 (질럿)" << std::endl;
        std::cout << "\n조작법:" << std::endl;
        std::cout << "- 좌클릭: 유닛 선택" << std::endl;
        std::cout << "- 드래그: 다중 선택" << std::endl;
        std::cout << "- 우클릭: 이동/공격" << std::endl;
        std::cout << "- ESC: 종료" << std::endl << std::endl;
        
        return true;
    }
    
    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    onLeftClick(Vec2(e.button.x, e.button.y));
                }
                else if (e.button.button == SDL_BUTTON_RIGHT) {
                    onRightClick(Vec2(e.button.x, e.button.y));
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    onLeftRelease(Vec2(e.button.x, e.button.y));
                }
            }
            else if (e.type == SDL_MOUSEMOTION) {
                mousePos = Vec2(e.motion.x, e.motion.y);
            }
        }
    }

    void onLeftClick(Vec2 pos) {
        dragStart = pos;
        isDragging = true;
        
        // 유닛 클릭 확인
        Unit* clickedUnit = findUnitAt(pos);
        if (clickedUnit && clickedUnit->owner == 0) {
            selectedUnits.clear();
            selectedUnits.push_back(clickedUnit);
            clickedUnit->isSelected = true;
        }
    }
    
    void onLeftRelease(Vec2 pos) {
        if (isDragging) {
            float dx = pos.x - dragStart.x;
            float dy = pos.y - dragStart.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist > 10) {
                // 드래그 선택
                selectUnitsInRect(dragStart, pos);
            }
        }
        isDragging = false;
    }
    
    void onRightClick(Vec2 pos) {
        if (selectedUnits.empty()) return;
        
        Unit* targetUnit = findUnitAt(pos);
        
        for (Unit* unit : selectedUnits) {
            if (targetUnit && targetUnit->owner != unit->owner) {
                // 적 유닛 - 공격
                unit->attack(targetUnit);
            } else {
                // 빈 공간 - 이동
                unit->moveTo(pos);
            }
        }
    }
    
    Unit* findUnitAt(Vec2 pos) {
        for (Unit* unit : units) {
            if (unit->isDead()) continue;
            
            Vec2 diff = pos - unit->position;
            if (diff.length() <= unit->getData().size) {
                return unit;
            }
        }
        return nullptr;
    }
    
    void selectUnitsInRect(Vec2 start, Vec2 end) {
        // 선택 해제
        for (Unit* unit : selectedUnits) {
            unit->isSelected = false;
        }
        selectedUnits.clear();
        
        float minX = std::min(start.x, end.x);
        float maxX = std::max(start.x, end.x);
        float minY = std::min(start.y, end.y);
        float maxY = std::max(start.y, end.y);
        
        for (Unit* unit : units) {
            if (unit->owner != 0 || unit->isDead()) continue;
            
            if (unit->position.x >= minX && unit->position.x <= maxX &&
                unit->position.y >= minY && unit->position.y <= maxY) {
                selectedUnits.push_back(unit);
                unit->isSelected = true;
            }
        }
    }
    
    void update(float deltaTime) {
        // 유닛 업데이트
        for (Unit* unit : units) {
            if (!unit->isDead()) {
                unit->update(deltaTime, units);
            }
        }
        
        // 충돌 처리
        CollisionSystem::resolveCollisions(units);
        
        // 죽은 유닛 제거
        units.erase(
            std::remove_if(units.begin(), units.end(),
                [](Unit* u) { return u->isDead(); }),
            units.end()
        );
    }
    
    void render() {
        // 배경
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderClear(renderer);
        
        // 유닛 그리기
        for (Unit* unit : units) {
            drawUnit(unit);
        }
        
        // 드래그 박스
        if (isDragging) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_Rect rect;
            rect.x = std::min(dragStart.x, mousePos.x);
            rect.y = std::min(dragStart.y, mousePos.y);
            rect.w = std::abs(mousePos.x - dragStart.x);
            rect.h = std::abs(mousePos.y - dragStart.y);
            SDL_RenderDrawRect(renderer, &rect);
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void drawUnit(Unit* unit) {
        const UnitData& data = unit->getData();
        SDL_Color color = data.color;
        
        // 유닛 원
        drawCircle(unit->position, data.size, color);
        
        // 선택 표시
        if (unit->isSelected) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            drawCircle(unit->position, data.size + 4, {0, 255, 0, 255}, false);
        }
        
        // HP 바
        drawHealthBar(unit);
    }
    
    void drawCircle(Vec2 center, float radius, SDL_Color color, bool filled = true) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        
        for (int w = 0; w < radius * 2; w++) {
            for (int h = 0; h < radius * 2; h++) {
                int dx = radius - w;
                int dy = radius - h;
                if ((dx*dx + dy*dy) <= (radius * radius)) {
                    if (filled || (dx*dx + dy*dy) >= ((radius-2) * (radius-2))) {
                        SDL_RenderDrawPoint(renderer, center.x + dx, center.y + dy);
                    }
                }
            }
        }
    }
    
    void drawHealthBar(Unit* unit) {
        const UnitData& data = unit->getData();
        float barWidth = data.size * 2;
        float barHeight = 4;
        float x = unit->position.x - barWidth / 2;
        float y = unit->position.y - data.size - 10;
        
        // 배경
        SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
        SDL_Rect bgRect = {(int)x, (int)y, (int)barWidth, (int)barHeight};
        SDL_RenderFillRect(renderer, &bgRect);
        
        // HP
        float hpPercent = unit->hp / data.maxHp;
        if (hpPercent > 0.5f) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        } else if (hpPercent > 0.25f) {
            SDL_SetRenderDrawColor(renderer, 255, 170, 0, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        }
        SDL_Rect hpRect = {(int)x, (int)y, (int)(barWidth * hpPercent), (int)barHeight};
        SDL_RenderFillRect(renderer, &hpRect);
        
        // 쉴드
        if (data.maxShield > 0 && unit->shield > 0) {
            float shieldPercent = unit->shield / data.maxShield;
            SDL_SetRenderDrawColor(renderer, 0, 170, 255, 255);
            SDL_Rect shieldRect = {(int)x, (int)(y - 6), (int)(barWidth * shieldPercent), 3};
            SDL_RenderFillRect(renderer, &shieldRect);
        }
    }
    
    void run() {
        Uint32 lastTime = SDL_GetTicks();
        
        while (running) {
            Uint32 currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            
            handleEvents();
            update(deltaTime);
            render();
            
            SDL_Delay(16); // ~60 FPS
        }
    }
};

int main(int argc, char* argv[]) {
    Game game;
    
    if (!game.init()) {
        return 1;
    }
    
    game.run();
    
    return 0;
}
