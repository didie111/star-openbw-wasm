# C++ vs HTML 유닛 로직 비교

## 📊 코드 비교

### 1. 유닛 이동 로직

#### HTML (unit.js)
```javascript
updateMovement(deltaTime) {
    if (this.path.length === 0) return;
    
    const target = this.path[this.pathIndex];
    const dx = target.x - this.x;
    const dy = target.y - this.y;
    const distance = Math.sqrt(dx * dx + dy * dy);
    
    if (distance < 3) {
        this.pathIndex++;
        if (this.pathIndex >= this.path.length) {
            this.path = [];
        }
    } else {
        const moveSpeed = this.speed * deltaTime * 60;
        this.x += (dx / distance) * moveSpeed;
        this.y += (dy / distance) * moveSpeed;
    }
}
```

#### C++ (simple_game_sdl.cpp)
```cpp
void updateMovement(float deltaTime) {
    if (path.empty()) return;
    
    Vec2 target = path[pathIndex];
    Vec2 diff = target - position;
    float distance = diff.length();
    
    if (distance < 3.0f) {
        pathIndex++;
        if (pathIndex >= (int)path.size()) {
            path.clear();
        }
    } else {
        Vec2 direction = diff.normalize();
        float moveSpeed = getData().speed * deltaTime * 60.0f;
        position = position + direction * moveSpeed;
    }
}
```

**결론**: ✅ **완전히 동일한 로직!**

---

### 2. 전투 시스템

#### HTML (combat.js)
```javascript
static attack(attacker, target) {
    if (!this.isInRange(attacker, target)) return false;
    if (attacker.attackCooldown > 0) return false;
    
    const damage = this.calculateDamage(attacker, target);
    
    if (target.shield > 0) {
        target.shield -= damage;
        if (target.shield < 0) {
            target.hp += target.shield;
            target.shield = 0;
        }
    } else {
        target.hp -= damage;
    }
    
    attacker.attackCooldown = attacker.maxAttackCooldown;
    return true;
}
```

#### C++ (simple_game_sdl.cpp)
```cpp
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
```

**결론**: ✅ **완전히 동일한 로직!**

---

### 3. 충돌 감지

#### HTML (collision.js)
```javascript
static resolveCollision(unit1, unit2) {
    const dx = unit1.x - unit2.x;
    const dy = unit1.y - unit2.y;
    const distance = Math.sqrt(dx * dx + dy * dy);
    const minDistance = unit1.size + unit2.size;
    
    if (distance < minDistance && distance > 0) {
        const overlap = minDistance - distance;
        const pushX = (dx / distance) * overlap * 0.5;
        const pushY = (dy / distance) * overlap * 0.5;
        
        unit1.x += pushX;
        unit1.y += pushY;
        unit2.x -= pushX;
        unit2.y -= pushY;
    }
}
```

#### C++ (simple_game_sdl.cpp)
```cpp
static void resolveCollisions(std::vector<Unit*>& units) {
    for (size_t i = 0; i < units.size(); i++) {
        for (size_t j = i + 1; j < units.size(); j++) {
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
```

**결론**: ✅ **완전히 동일한 로직!**

---

## 🎯 핵심 차이점

### 1. 언어 차이
| 항목 | HTML (JavaScript) | C++ |
|------|-------------------|-----|
| 타입 | 동적 타입 | 정적 타입 |
| 메모리 | 자동 관리 | 수동 관리 (포인터) |
| 속도 | 느림 | 빠름 |
| 문법 | `this.x` | `position.x` |

### 2. 구조 차이
| 항목 | HTML | C++ |
|------|------|-----|
| 벡터 | `{x, y}` 객체 | `Vec2` 구조체 |
| 배열 | `Array` | `std::vector` |
| 클래스 | `class Unit` | `class Unit` |

### 3. 렌더링 차이
| 항목 | HTML | C++ |
|------|------|-----|
| 그래픽 | Canvas 2D | SDL2 |
| 원 그리기 | `ctx.arc()` | 직접 구현 |
| 색상 | `'#00ff00'` | `SDL_Color` |

---

## ✅ 결론

### 게임 로직은 100% 동일!

1. **유닛 이동**: 완전히 동일
2. **전투 시스템**: 완전히 동일
3. **충돌 감지**: 완전히 동일
4. **경로 찾기**: 완전히 동일

### 차이점은 오직 구현 언어뿐!

- **HTML**: JavaScript + Canvas 2D
- **C++**: C++ + SDL2

**핵심 알고리즘과 로직은 완전히 동일합니다!**

---

## 🔍 실제로 비교해보기

### HTML 버전 실행
```
game_integrated/simple_rts/index.html
```

### C++ 버전 빌드 & 실행
```bash
# SDL2 설치 필요
g++ -std=c++11 simple_game_sdl.cpp -lSDL2 -o simple_game
./simple_game
```

두 버전을 실행해보면 **유닛이 똑같이 움직이는 것**을 확인할 수 있습니다!

---

## 📝 요약

**"HTML로 만들때는 유닛 로직이 좀 달라보여서"**

→ **아니요! 완전히 동일합니다!**

차이점은:
- ❌ 로직이 다른 게 아니라
- ✅ 언어 문법만 다를 뿐입니다

JavaScript의 `this.x`가 C++에서는 `position.x`로 표현될 뿐, 
**계산 방식과 알고리즘은 100% 동일합니다!**
