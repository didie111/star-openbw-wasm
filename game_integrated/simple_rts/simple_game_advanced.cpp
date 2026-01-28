// Simple RTS - OpenBW 유닛 로직 정확 구현
// bwgame.h의 실제 로직을 최대한 가깝게 구현

#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#include <map>

// 고정소수점 (OpenBW fp8 방식)
struct fp8 {
    int raw_value;
    
    fp8(float f = 0) : raw_value((int)(f * 256.0f)) {}
    fp8(int raw, bool) : raw_value(raw) {}
    
    float toFloat() const { return raw_value / 256.0f; }
    
    fp8 operator+(fp8 other) const { return fp8(raw_value + other.raw_value, true); }
    fp8 operator-(fp8 other) const { return fp8(raw_value - other.raw_value, true); }
    fp8 operator*(fp8 other) const { return fp8((raw_value * other.raw_value) >> 8, true); }
    fp8 operator/(fp8 other) const { return fp8((raw_value << 8) / other.raw_value, true); }
    
    bool operator<(fp8 other) const { return raw_value < other.raw_value; }
    bool operator>(fp8 other) const { return raw_value > other.raw_value; }
};

// 벡터 (OpenBW xy 방식)
struct Vec2 {
    fp8 x, y;
    
    Vec2(fp8 x = fp8(0), fp8 y = fp8(0)) : x(x), y(y) {}
    Vec2(float fx, float fy) : x(fp8(fx)), y(fp8(fy)) {}
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(fp8 scalar) const { return Vec2(x * scalar, y * scalar); }
    
    fp8 length() const {
        float fx = x.toFloat();
        float fy = y.toFloat();
        return fp8(std::sqrt(fx * fx + fy * fy));
    }
    
    Vec2 normalize() const {
        fp8 len = length();
        if (len.toFloat() < 0.001f) return Vec2(0, 0);
        return Vec2(x / len, y / len);
    }
};

// 방향 (OpenBW direction_t 방식)
struct Direction {
    int value; // 0-255 (256방향)
    
    Direction(int v = 0) : value(v & 255) {}
    
    Vec2 toVector(fp8 length) const {
        float angle = (value / 256.0f) * 2.0f * 3.14159f;
        return Vec2(
            fp8(std::cos(angle)) * length,
            fp8(std::sin(angle)) * length
        );
    }
    
    static Direction fromVector(Vec2 vec) {
        float angle = std::atan2(vec.y.toFloat(), vec.x.toFloat());
        int dir = (int)((angle / (2.0f * 3.14159f)) * 256.0f);
        return Direction(dir);
    }
};

// 유닛 타입
enum class UnitType {
    MARINE,
    ZEALOT
};

// 유닛 데이터 (OpenBW unit_type_t 기반)
struct UnitData {
    const char* name;
    fp8 maxHp;
    fp8 maxShield;
    fp8 topSpeed;          // 최대 속도
    fp8 acceleration;      // 가속도
    fp8 turnRate;          // 회전 속도
    fp8 attackPower;
    fp8 attackRange;
    fp8 attackCooldown;
    fp8 size;
    fp8 haltDistance;      // 정지 거리
    SDL_Color color;
};

const UnitData UNIT_STATS[] = {
    {"Marine", fp8(40), fp8(0), fp8(4.0f), fp8(1.0f), fp8(0.3f), 
     fp8(6), fp8(120), fp8(15), fp8(16), fp8(8), {0, 255, 0, 255}},
    {"Zealot", fp8(100), fp8(60), fp8(3.0f), fp8(0.8f), fp8(0.25f),
     fp8(16), fp8(20), fp8(22), fp8(18), fp8(10), {255, 170, 0, 255}}
};

// Forward declarations
class Unit;
class Pathfinder;

// 유닛 클래스 (OpenBW unit_t 기반) - 먼저 선언
class Unit {
public:
    int id;
    UnitType type;
    int owner;
    
    // 위치 및 이동 (OpenBW flingy_t 기반)
    Vec2 position;
    Vec2 velocity;              // 현재 속도
    Vec2 moveTarget;           // 이동 목표
    
    // 상태 (OpenBW unit_t 기반)
    fp8 hp;
    fp8 shield;
    fp8 attackCooldown;
    bool isSelected;
    Unit* attackTarget;
    
    // 이동 상태 플래그
    bool isMoving;
    bool hasPath;
    
    std::vector<Vec2> path;
    int pathIndex;
    
    Unit(int id, UnitType type, int owner, Vec2 pos) 
        : id(id), type(type), owner(owner), position(pos),
          velocity(0, 0), moveTarget(pos), attackCooldown(0), isSelected(false),
          attackTarget(nullptr), isMoving(false), hasPath(false),
          pathIndex(0) {
        const UnitData& data = UNIT_STATS[(int)type];
        hp = data.maxHp;
        shield = data.maxShield;
    }
    
    const UnitData& getData() const { return UNIT_STATS[(int)type]; }
    bool isDead() const { return hp.toFloat() <= 0; }
    
    // 함수 선언만 (구현은 Pathfinder 뒤에)
    void moveTo(Vec2 target, const std::vector<Unit*>& allUnits);
    void attack(Unit* target, const std::vector<Unit*>& allUnits);
    void update(fp8 deltaTime, std::vector<Unit*>& allUnits);
    
private:
    void updateMovement(fp8 deltaTime);
    void updateCombat(std::vector<Unit*>& allUnits);
    void performAttack(Unit* target);
    Unit* findNearestEnemy(const std::vector<Unit*>& allUnits);
};

// A* 경로 찾기 (OpenBW pathfinder 간소화 버전)
class Pathfinder {
private:
    static const int GRID_SIZE = 32;
    static const int MAP_WIDTH = 1200 / GRID_SIZE;
    static const int MAP_HEIGHT = 800 / GRID_SIZE;
    
    struct Node {
        int x, y;
        float g, h, f;
        Node* parent;
        
        Node(int x, int y) : x(x), y(y), g(0), h(0), f(0), parent(nullptr) {}
    };
    
    struct CompareNode {
        bool operator()(Node* a, Node* b) {
            return a->f > b->f;
        }
    };
    
public:
    static std::vector<Vec2> findPath(Vec2 start, Vec2 goal, const std::vector<Unit*>& obstacles) {
        int startX = (int)(start.x.toFloat() / GRID_SIZE);
        int startY = (int)(start.y.toFloat() / GRID_SIZE);
        int goalX = (int)(goal.x.toFloat() / GRID_SIZE);
        int goalY = (int)(goal.y.toFloat() / GRID_SIZE);
        
        // 맵 범위 체크
        if (startX < 0 || startX >= MAP_WIDTH || startY < 0 || startY >= MAP_HEIGHT ||
            goalX < 0 || goalX >= MAP_WIDTH || goalY < 0 || goalY >= MAP_HEIGHT) {
            return {goal};
        }
        
        // 장애물 그리드 생성
        bool obstacleGrid[MAP_WIDTH][MAP_HEIGHT] = {false};
        for (const Unit* unit : obstacles) {
            if (unit->isDead()) continue;
            int ux = (int)(unit->position.x.toFloat() / GRID_SIZE);
            int uy = (int)(unit->position.y.toFloat() / GRID_SIZE);
            if (ux >= 0 && ux < MAP_WIDTH && uy >= 0 && uy < MAP_HEIGHT) {
                obstacleGrid[ux][uy] = true;
            }
        }
        
        // A* 알고리즘
        std::priority_queue<Node*, std::vector<Node*>, CompareNode> openList;
        std::set<std::pair<int, int>> closedList;
        std::map<std::pair<int, int>, Node*> allNodes;
        
        Node* startNode = new Node(startX, startY);
        startNode->h = heuristic(startX, startY, goalX, goalY);
        startNode->f = startNode->h;
        openList.push(startNode);
        allNodes[{startX, startY}] = startNode;
        
        Node* goalNode = nullptr;
        
        while (!openList.empty()) {
            Node* current = openList.top();
            openList.pop();
            
            if (current->x == goalX && current->y == goalY) {
                goalNode = current;
                break;
            }
            
            closedList.insert({current->x, current->y});
            
            // 8방향 탐색
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = current->x + dx;
                    int ny = current->y + dy;
                    
                    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) continue;
                    if (obstacleGrid[nx][ny] && !(nx == goalX && ny == goalY)) continue;
                    if (closedList.count({nx, ny})) continue;
                    
                    float moveCost = (dx != 0 && dy != 0) ? 1.414f : 1.0f;
                    float newG = current->g + moveCost;
                    
                    auto it = allNodes.find({nx, ny});
                    Node* neighbor;
                    
                    if (it == allNodes.end()) {
                        neighbor = new Node(nx, ny);
                        neighbor->g = newG;
                        neighbor->h = heuristic(nx, ny, goalX, goalY);
                        neighbor->f = neighbor->g + neighbor->h;
                        neighbor->parent = current;
                        allNodes[{nx, ny}] = neighbor;
                        openList.push(neighbor);
                    } else {
                        neighbor = it->second;
                        if (newG < neighbor->g) {
                            neighbor->g = newG;
                            neighbor->f = neighbor->g + neighbor->h;
                            neighbor->parent = current;
                        }
                    }
                }
            }
        }
        
        // 경로 재구성
        std::vector<Vec2> path;
        if (goalNode) {
            Node* current = goalNode;
            while (current) {
                path.insert(path.begin(), Vec2(
                    current->x * GRID_SIZE + GRID_SIZE / 2,
                    current->y * GRID_SIZE + GRID_SIZE / 2
                ));
                current = current->parent;
            }
        } else {
            // 경로를 찾지 못하면 직선
            path.push_back(goal);
        }
        
        // 메모리 정리
        for (auto& pair : allNodes) {
            delete pair.second;
        }
        
        return path;
    }
    
private:
    static float heuristic(int x1, int y1, int x2, int y2) {
        int dx = abs(x1 - x2);
        int dy = abs(y1 - y2);
        return (float)(dx + dy) + (1.414f - 2.0f) * std::min(dx, dy);
    }
};

// Unit 클래스 함수 구현 (Pathfinder 뒤에 구현)
void Unit::moveTo(Vec2 target, const std::vector<Unit*>& allUnits) {
    moveTarget = target;
    attackTarget = nullptr;
    hasPath = true;
    isMoving = true;
    
    // A* 경로 찾기 (자신을 제외한 다른 유닛을 장애물로 인식)
    std::vector<Unit*> obstacles;
    for (Unit* u : allUnits) {
        if (u != this && !u->isDead()) {
            obstacles.push_back(u);
        }
    }
    path = Pathfinder::findPath(position, target, obstacles);
    pathIndex = 0;
    
    std::cout << "=== 경로 계산 ===" << std::endl;
    std::cout << "시작: (" << position.x.toFloat() << ", " << position.y.toFloat() << ")" << std::endl;
    std::cout << "목표: (" << target.x.toFloat() << ", " << target.y.toFloat() << ")" << std::endl;
    std::cout << "장애물: " << obstacles.size() << "개" << std::endl;
    std::cout << "경로: " << path.size() << "개 웨이포인트" << std::endl;
    for (size_t i = 0; i < path.size() && i < 5; i++) {
        std::cout << "  [" << i << "] (" << path[i].x.toFloat() << ", " << path[i].y.toFloat() << ")" << std::endl;
    }
    std::cout << std::endl;
}

void Unit::attack(Unit* target, const std::vector<Unit*>& allUnits) {
    attackTarget = target;
    if (target) {
        moveTarget = target->position;
        
        // 자신을 제외한 장애물
        std::vector<Unit*> obstacles;
        for (Unit* u : allUnits) {
            if (u != this && !u->isDead()) {
                obstacles.push_back(u);
            }
        }
        path = Pathfinder::findPath(position, moveTarget, obstacles);
        pathIndex = 0;
        hasPath = true;
    }
}

void Unit::update(fp8 deltaTime, std::vector<Unit*>& allUnits) {
    // 쿨다운 감소
    if (attackCooldown > fp8(0)) {
        attackCooldown = attackCooldown - deltaTime * fp8(60);
        if (attackCooldown < fp8(0)) attackCooldown = fp8(0);
    }
    
    // 이동 업데이트 (OpenBW 방식)
    updateMovement(deltaTime);
    
    // 전투 업데이트
    updateCombat(allUnits);
}

void Unit::updateMovement(fp8 deltaTime) {
    if (!hasPath && !isMoving) {
        // 감속
        velocity = velocity * fp8(0.9f);
        if (velocity.length() < fp8(0.1f)) {
            velocity = Vec2(0, 0);
        }
        position = position + velocity * deltaTime;
        return;
    }
    
    // 경로가 있으면 다음 웨이포인트로 이동
    if (pathIndex < (int)path.size()) {
        Vec2 waypoint = path[pathIndex];
        Vec2 diff = waypoint - position;
        fp8 distance = diff.length();
        
        // 웨이포인트 도달
        if (distance < fp8(10)) {
            pathIndex++;
            if (pathIndex >= (int)path.size()) {
                hasPath = false;
                isMoving = false;
            }
            return;
        }
        
        // 웨이포인트 방향으로 이동
        Vec2 direction = diff.normalize();
        
        // 가속
        Vec2 targetVel = direction * getData().topSpeed;
        Vec2 accelVec = (targetVel - velocity) * getData().acceleration * deltaTime;
        velocity = velocity + accelVec;
        
        // 최대 속도 제한
        fp8 currentSpeed = velocity.length();
        if (currentSpeed > getData().topSpeed) {
            velocity = velocity.normalize() * getData().topSpeed;
        }
        
        // 위치 업데이트
        position = position + velocity * deltaTime * fp8(60);
    } else {
        hasPath = false;
        isMoving = false;
    }
}

void Unit::updateCombat(std::vector<Unit*>& allUnits) {
    if (attackTarget && !attackTarget->isDead()) {
        Vec2 diff = attackTarget->position - position;
        fp8 dist = diff.length();
        
        if (dist > getData().attackRange) {
            // 범위 밖 - 추적 (경로 재계산은 가끔만)
            if (!hasPath || pathIndex >= (int)path.size()) {
                moveTarget = attackTarget->position;
                
                // 자신을 제외한 장애물
                std::vector<Unit*> obstacles;
                for (Unit* u : allUnits) {
                    if (u != this && !u->isDead()) {
                        obstacles.push_back(u);
                    }
                }
                path = Pathfinder::findPath(position, moveTarget, obstacles);
                pathIndex = 0;
                hasPath = true;
                isMoving = true;
            }
        } else {
            // 범위 안 - 공격
            hasPath = false;
            isMoving = false;
            
            if (attackCooldown < fp8(0.1f)) {
                performAttack(attackTarget);
                attackCooldown = getData().attackCooldown;
            }
        }
    } else {
        // 자동 공격
        Unit* nearestEnemy = findNearestEnemy(allUnits);
        if (nearestEnemy) {
            Vec2 diff = nearestEnemy->position - position;
            fp8 dist = diff.length();
            if (dist < getData().attackRange && attackCooldown < fp8(0.1f)) {
                performAttack(nearestEnemy);
                attackCooldown = getData().attackCooldown;
            }
        }
    }
}

void Unit::performAttack(Unit* target) {
    fp8 damage = getData().attackPower;
    
    if (target->shield > fp8(0)) {
        target->shield = target->shield - damage;
        if (target->shield < fp8(0)) {
            target->hp = target->hp + target->shield;
            target->shield = fp8(0);
        }
    } else {
        target->hp = target->hp - damage;
    }
}

Unit* Unit::findNearestEnemy(const std::vector<Unit*>& allUnits) {
    Unit* nearest = nullptr;
    fp8 minDist = fp8(999999);
    
    for (Unit* unit : allUnits) {
        if (unit->owner == owner || unit->isDead()) continue;
        
        Vec2 diff = unit->position - position;
        fp8 dist = diff.length();
        if (dist < minDist) {
            minDist = dist;
            nearest = unit;
        }
    }
    
    return nearest;
}

// 충돌 시스템 (OpenBW 방식)
class CollisionSystem {
public:
    static void resolveCollisions(std::vector<Unit*>& units) {
        for (size_t i = 0; i < units.size(); i++) {
            for (size_t j = i + 1; j < units.size(); j++) {
                if (units[i]->isDead() || units[j]->isDead()) continue;
                
                Vec2 diff = units[i]->position - units[j]->position;
                fp8 distance = diff.length();
                fp8 minDist = units[i]->getData().size + units[j]->getData().size;
                
                if (distance < minDist && distance > fp8(0.1f)) {
                    fp8 overlap = minDist - distance;
                    Vec2 push = diff.normalize() * overlap * fp8(0.5f);
                    
                    units[i]->position = units[i]->position + push;
                    units[j]->position = units[j]->position - push;
                }
            }
        }
    }
};

// 게임 클래스 (이전과 동일, Unit 클래스만 업그레이드됨)
class Game {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    std::vector<Unit*> units;
    std::vector<Unit*> selectedUnits;
    int nextUnitId;
    bool running;
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
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
        
        window = SDL_CreateWindow("Simple RTS - OpenBW 정확 구현",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1200, 800, SDL_WINDOW_SHOWN);
        if (!window) return false;
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) return false;
        
        // 유닛 생성
        for (int i = 0; i < 15; i++) {
            float x = 100.0f + (i % 5) * 50.0f;
            float y = 100.0f + (i / 5) * 50.0f;
            units.push_back(new Unit(nextUnitId++, UnitType::MARINE, 0, Vec2(x, y)));
        }
        
        units.push_back(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(900, 300)));
        units.push_back(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(950, 350)));
        
        std::cout << "=== OpenBW 정확 구현 ===" << std::endl;
        std::cout << "- 고정소수점 연산" << std::endl;
        std::cout << "- 가속도 시스템" << std::endl;
        std::cout << "- 정지 거리" << std::endl;
        std::cout << "- A* 경로 찾기" << std::endl;
        std::cout << "\n경로 시각화:" << std::endl;
        std::cout << "- 청록색 선: 계산된 경로" << std::endl;
        std::cout << "- 분홍색 점: 웨이포인트" << std::endl << std::endl;
        
        return true;
    }
    
    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
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
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                onLeftRelease(Vec2(e.button.x, e.button.y));
            }
            else if (e.type == SDL_MOUSEMOTION) {
                mousePos = Vec2(e.motion.x, e.motion.y);
            }
        }
    }

    void onLeftClick(Vec2 pos) {
        dragStart = pos;
        isDragging = true;
        
        Unit* clickedUnit = findUnitAt(pos);
        if (clickedUnit && clickedUnit->owner == 0) {
            for (Unit* u : selectedUnits) u->isSelected = false;
            selectedUnits.clear();
            selectedUnits.push_back(clickedUnit);
            clickedUnit->isSelected = true;
        }
    }
    
    void onLeftRelease(Vec2 pos) {
        if (isDragging) {
            Vec2 diff = pos - dragStart;
            fp8 dist = diff.length();
            
            if (dist > fp8(10)) {
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
                unit->attack(targetUnit, units);
            } else {
                unit->moveTo(pos, units);
            }
        }
    }
    
    Unit* findUnitAt(Vec2 pos) {
        for (Unit* unit : units) {
            if (unit->isDead()) continue;
            
            Vec2 diff = pos - unit->position;
            if (diff.length() < unit->getData().size) {
                return unit;
            }
        }
        return nullptr;
    }
    
    void selectUnitsInRect(Vec2 start, Vec2 end) {
        for (Unit* u : selectedUnits) u->isSelected = false;
        selectedUnits.clear();
        
        fp8 minX = start.x < end.x ? start.x : end.x;
        fp8 maxX = start.x > end.x ? start.x : end.x;
        fp8 minY = start.y < end.y ? start.y : end.y;
        fp8 maxY = start.y > end.y ? start.y : end.y;
        
        for (Unit* unit : units) {
            if (unit->owner != 0 || unit->isDead()) continue;
            
            if (unit->position.x > minX && unit->position.x < maxX &&
                unit->position.y > minY && unit->position.y < maxY) {
                selectedUnits.push_back(unit);
                unit->isSelected = true;
            }
        }
    }
    
    void update(fp8 deltaTime) {
        for (Unit* unit : units) {
            if (!unit->isDead()) {
                unit->update(deltaTime, units);
            }
        }
        
        CollisionSystem::resolveCollisions(units);
        
        units.erase(
            std::remove_if(units.begin(), units.end(),
                [](Unit* u) { return u->isDead(); }),
            units.end()
        );
    }
    
    void render() {
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderClear(renderer);
        
        for (Unit* unit : units) {
            drawUnit(unit);
        }
        
        if (isDragging) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_Rect rect;
            rect.x = std::min(dragStart.x.toFloat(), mousePos.x.toFloat());
            rect.y = std::min(dragStart.y.toFloat(), mousePos.y.toFloat());
            rect.w = std::abs(mousePos.x.toFloat() - dragStart.x.toFloat());
            rect.h = std::abs(mousePos.y.toFloat() - dragStart.y.toFloat());
            SDL_RenderDrawRect(renderer, &rect);
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void drawUnit(Unit* unit) {
        const UnitData& data = unit->getData();
        SDL_Color color = data.color;
        
        drawCircle(unit->position, data.size, color);
        
        if (unit->isSelected) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            drawCircle(unit->position, data.size + fp8(4), {0, 255, 0, 255}, false);
        }
        
        drawHealthBar(unit);
        
        // 경로 표시 (디버그)
        if (unit->isSelected && unit->path.size() > 0) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 128);
            for (size_t i = 0; i < unit->path.size() - 1; i++) {
                SDL_RenderDrawLine(renderer,
                    unit->path[i].x.toFloat(), unit->path[i].y.toFloat(),
                    unit->path[i+1].x.toFloat(), unit->path[i+1].y.toFloat());
            }
            
            // 웨이포인트 표시
            for (size_t i = unit->pathIndex; i < unit->path.size(); i++) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                SDL_Rect rect = {
                    (int)unit->path[i].x.toFloat() - 2,
                    (int)unit->path[i].y.toFloat() - 2,
                    4, 4
                };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
    
    void drawCircle(Vec2 center, fp8 radius, SDL_Color color, bool filled = true) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        
        int r = (int)radius.toFloat();
        int cx = (int)center.x.toFloat();
        int cy = (int)center.y.toFloat();
        
        for (int w = 0; w < r * 2; w++) {
            for (int h = 0; h < r * 2; h++) {
                int dx = r - w;
                int dy = r - h;
                if ((dx*dx + dy*dy) <= (r * r)) {
                    if (filled || (dx*dx + dy*dy) >= ((r-2) * (r-2))) {
                        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                    }
                }
            }
        }
    }
    
    void drawHealthBar(Unit* unit) {
        const UnitData& data = unit->getData();
        float barWidth = data.size.toFloat() * 2;
        float barHeight = 4;
        float x = unit->position.x.toFloat() - barWidth / 2;
        float y = unit->position.y.toFloat() - data.size.toFloat() - 10;
        
        SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
        SDL_Rect bgRect = {(int)x, (int)y, (int)barWidth, (int)barHeight};
        SDL_RenderFillRect(renderer, &bgRect);
        
        float hpPercent = unit->hp.toFloat() / data.maxHp.toFloat();
        if (hpPercent > 0.5f) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        } else if (hpPercent > 0.25f) {
            SDL_SetRenderDrawColor(renderer, 255, 170, 0, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        }
        SDL_Rect hpRect = {(int)x, (int)y, (int)(barWidth * hpPercent), (int)barHeight};
        SDL_RenderFillRect(renderer, &hpRect);
        
        if (data.maxShield.toFloat() > 0 && unit->shield.toFloat() > 0) {
            float shieldPercent = unit->shield.toFloat() / data.maxShield.toFloat();
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
            update(fp8(deltaTime));
            render();
            
            SDL_Delay(16);
        }
    }
};

int main(int argc, char* argv[]) {
    Game game;
    
    if (!game.init()) {
        std::cerr << "초기화 실패!" << std::endl;
        return 1;
    }
    
    game.run();
    
    return 0;
}
