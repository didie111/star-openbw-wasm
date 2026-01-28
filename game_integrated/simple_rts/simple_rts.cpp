// 간단한 RTS 게임 - OpenBW 핵심 로직만 추출
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>

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
    ZEALOT,
    ZERGLING
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
};

const UnitData UNIT_STATS[] = {
    {"Marine", 40.0f, 0.0f, 3.5f, 6.0f, 120.0f, 15.0f, 16.0f},
    {"Zealot", 100.0f, 60.0f, 3.0f, 16.0f, 20.0f, 22.0f, 18.0f},
    {"Zergling", 35.0f, 0.0f, 4.5f, 5.0f, 15.0f, 12.0f, 14.0f}
};

// 유닛 클래스
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
    
    Unit(int id, UnitType type, int owner, Vec2 pos) 
        : id(id), type(type), owner(owner), position(pos), 
          attackCooldown(0), isSelected(false), attackTarget(nullptr) {
        const UnitData& data = UNIT_STATS[(int)type];
        hp = data.maxHp;
        shield = data.maxShield;
        targetPos = pos;
    }
    
    const UnitData& getData() const { return UNIT_STATS[(int)type]; }
    
    bool isDead() const { return hp <= 0; }
    
    // 이동 (bwgame.h 기반)
    void moveTo(Vec2 target) {
        targetPos = target;
        attackTarget = nullptr;
    }
    
    // 공격 명령
    void attack(Unit* target) {
        attackTarget = target;
        if (target) {
            targetPos = target->position;
        }
    }
    
    // 업데이트
    void update(float deltaTime, std::vector<std::unique_ptr<Unit>>& allUnits) {
        // 쿨다운 감소
        if (attackCooldown > 0) {
            attackCooldown -= deltaTime * 60.0f;
        }
        
        // 이동
        updateMovement(deltaTime);
        
        // 전투
        updateCombat(allUnits);
    }
    
private:
    void updateMovement(float deltaTime) {
        Vec2 diff = targetPos - position;
        float distance = diff.length();
        
        if (distance > 3.0f) {
            Vec2 direction = diff.normalize();
            float moveSpeed = getData().speed * deltaTime * 60.0f;
            position = position + direction * moveSpeed;
        }
    }
    
    void updateCombat(std::vector<std::unique_ptr<Unit>>& allUnits) {
        // 타겟 추적
        if (attackTarget && !attackTarget->isDead()) {
            float dist = (attackTarget->position - position).length();
            
            if (dist > getData().attackRange) {
                // 범위 밖 - 추적
                targetPos = attackTarget->position;
            } else {
                // 범위 안 - 공격
                if (attackCooldown <= 0) {
                    performAttack(attackTarget);
                    attackCooldown = getData().attackCooldown;
                }
            }
        } else {
            // 자동 공격 - 범위 내 적 찾기
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
    
    void performAttack(Unit* target) {
        float damage = getData().attackPower;
        
        // 쉴드 먼저 감소
        if (target->shield > 0) {
            target->shield -= damage;
            if (target->shield < 0) {
                target->hp += target->shield;
                target->shield = 0;
            }
        } else {
            target->hp -= damage;
        }
        
        std::cout << getData().name << " attacks " << target->getData().name 
                  << " (HP: " << target->hp << ")" << std::endl;
    }
    
    Unit* findNearestEnemy(std::vector<std::unique_ptr<Unit>>& allUnits) {
        Unit* nearest = nullptr;
        float minDist = 999999.0f;
        
        for (auto& unit : allUnits) {
            if (unit->owner == owner || unit->isDead()) continue;
            
            float dist = (unit->position - position).length();
            if (dist < minDist) {
                minDist = dist;
                nearest = unit.get();
            }
        }
        
        return nearest;
    }
};

// 충돌 감지 시스템
class CollisionSystem {
public:
    static void resolveCollisions(std::vector<std::unique_ptr<Unit>>& units) {
        for (size_t i = 0; i < units.size(); i++) {
            for (size_t j = i + 1; j < units.size(); j++) {
                if (units[i]->isDead() || units[j]->isDead()) continue;
                
                Vec2 diff = units[i]->position - units[j]->position;
                float distance = diff.length();
                float minDist = units[i]->getData().size + units[j]->getData().size;
                
                if (distance < minDist && distance > 0) {
                    // 충돌 - 밀어내기
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
    std::vector<std::unique_ptr<Unit>> units;
    int nextUnitId;
    
public:
    Game() : nextUnitId(0) {}
    
    void init() {
        std::cout << "=== Simple RTS Game ===" << std::endl;
        std::cout << "OpenBW 핵심 로직 구현" << std::endl << std::endl;
        
        // 아군 15개 (마린)
        for (int i = 0; i < 15; i++) {
            float x = 100.0f + (i % 5) * 50.0f;
            float y = 100.0f + (i / 5) * 50.0f;
            units.push_back(std::unique_ptr<Unit>(new Unit(nextUnitId++, UnitType::MARINE, 0, Vec2(x, y))));
        }
        
        // 적군 2개 (질럿)
        units.push_back(std::unique_ptr<Unit>(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(900, 300))));
        units.push_back(std::unique_ptr<Unit>(new Unit(nextUnitId++, UnitType::ZEALOT, 1, Vec2(950, 350))));
        
        std::cout << "아군: " << countUnits(0) << "개" << std::endl;
        std::cout << "적군: " << countUnits(1) << "개" << std::endl << std::endl;
    }
    
    void update(float deltaTime) {
        // 유닛 업데이트
        for (auto& unit : units) {
            if (!unit->isDead()) {
                unit->update(deltaTime, units);
            }
        }
        
        // 충돌 처리
        CollisionSystem::resolveCollisions(units);
        
        // 죽은 유닛 제거
        units.erase(
            std::remove_if(units.begin(), units.end(), 
                [](const std::unique_ptr<Unit>& u) { return u->isDead(); }),
            units.end()
        );
    }
    
    void commandAttack() {
        // 모든 아군 유닛에게 적 공격 명령
        for (auto& unit : units) {
            if (unit->owner == 0 && !unit->isDead()) {
                // 가장 가까운 적 찾기
                Unit* nearestEnemy = nullptr;
                float minDist = 999999.0f;
                
                for (auto& enemy : units) {
                    if (enemy->owner == 1 && !enemy->isDead()) {
                        float dist = (enemy->position - unit->position).length();
                        if (dist < minDist) {
                            minDist = dist;
                            nearestEnemy = enemy.get();
                        }
                    }
                }
                
                if (nearestEnemy) {
                    unit->attack(nearestEnemy);
                }
            }
        }
        std::cout << ">>> 전체 공격 명령!" << std::endl;
    }
    
    int countUnits(int owner) const {
        int count = 0;
        for (const auto& unit : units) {
            if (unit->owner == owner && !unit->isDead()) count++;
        }
        return count;
    }
    
    bool isGameOver() const {
        return countUnits(0) == 0 || countUnits(1) == 0;
    }
    
    void printStatus() const {
        std::cout << "아군: " << countUnits(0) << " | 적군: " << countUnits(1) << std::endl;
    }
};

int main() {
    Game game;
    game.init();
    
    // 공격 명령
    game.commandAttack();
    
    std::cout << std::endl << "=== 전투 시작 ===" << std::endl << std::endl;
    
    // 게임 루프 (시뮬레이션)
    float deltaTime = 1.0f / 60.0f; // 60 FPS
    int frame = 0;
    
    while (!game.isGameOver() && frame < 600) { // 최대 10초
        game.update(deltaTime);
        
        // 1초마다 상태 출력
        if (frame % 60 == 0) {
            std::cout << "Frame " << frame << " - ";
            game.printStatus();
        }
        
        frame++;
    }
    
    std::cout << std::endl << "=== 전투 종료 ===" << std::endl;
    game.printStatus();
    
    if (game.countUnits(0) > 0) {
        std::cout << "아군 승리!" << std::endl;
    } else {
        std::cout << "적군 승리!" << std::endl;
    }
    
    return 0;
}
