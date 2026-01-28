# Simple RTS - OpenBW 핵심 로직 추출

## 개요

OpenBW의 복잡한 코드에서 **핵심 게임 로직만 추출**한 간단한 C++ RTS 게임입니다.

## 구현된 기능

### 1. 유닛 시스템
- ✅ 유닛 클래스 (HP, 쉴드, 위치, 속도)
- ✅ 3가지 유닛 타입 (마린, 질럿, 저글링)
- ✅ 유닛 데이터 (공격력, 사거리, 쿨다운)

### 2. 이동 시스템
- ✅ 목표 지점으로 이동
- ✅ 정규화된 방향 벡터
- ✅ 속도 기반 이동

### 3. 전투 시스템
- ✅ 공격 범위 확인
- ✅ 데미지 계산
- ✅ 쉴드 시스템
- ✅ 공격 쿨다운
- ✅ 자동 공격 (범위 내 적 탐지)
- ✅ 타겟 추적

### 4. 충돌 감지
- ✅ 유닛 간 충돌 감지
- ✅ 충돌 해결 (밀어내기)
- ✅ 원형 충돌 박스

### 5. 게임 루프
- ✅ 60 FPS 시뮬레이션
- ✅ deltaTime 기반 업데이트
- ✅ 게임 오버 조건

## 빌드 방법

### Windows (MinGW/MSYS2)

```powershell
# PowerShell 스크립트 사용
.\build.ps1

# 또는 직접 컴파일
g++ -std=c++17 -O2 -o simple_rts.exe simple_rts.cpp
```

### Linux/Mac

```bash
g++ -std=c++17 -O2 -o simple_rts simple_rts.cpp
```

## 실행

```powershell
.\simple_rts.exe
```

## 출력 예시

```
=== Simple RTS Game ===
OpenBW 핵심 로직 구현

아군: 15개
적군: 2개

>>> 전체 공격 명령!

=== 전투 시작 ===

Marine attacks Zealot (HP: 84)
Marine attacks Zealot (HP: 68)
...
Frame 0 - 아군: 15 | 적군: 2
Frame 60 - 아군: 15 | 적군: 2
Frame 120 - 아군: 15 | 적군: 1
Frame 180 - 아군: 15 | 적군: 0

=== 전투 종료 ===
아군: 15 | 적군: 0
아군 승리!
```

## 코드 구조

```cpp
// 기본 구조체
struct Vec2 { ... }              // 2D 벡터
enum class UnitType { ... }     // 유닛 타입
struct UnitData { ... }          // 유닛 스탯

// 핵심 클래스
class Unit {
    void moveTo(Vec2 target);           // 이동
    void attack(Unit* target);          // 공격
    void update(float deltaTime);       // 업데이트
    void updateMovement();              // 이동 처리
    void updateCombat();                // 전투 처리
}

class CollisionSystem {
    static void resolveCollisions();    // 충돌 처리
}

class Game {
    void init();                        // 초기화
    void update(float deltaTime);       // 게임 업데이트
    void commandAttack();               // 공격 명령
}
```

## OpenBW 원본과의 차이

### 간소화된 부분
- ❌ 경로 찾기 (A*) → 직선 이동
- ❌ 복잡한 충돌 시스템 → 간단한 원형 충돌
- ❌ 명령 큐 시스템 → 단일 명령
- ❌ 건물, 자원, 업그레이드
- ❌ UI, 렌더링

### 유지된 핵심 로직
- ✅ 유닛 이동 (속도, 방향)
- ✅ 전투 시스템 (공격, 데미지, 쿨다운)
- ✅ 충돌 감지 (밀어내기)
- ✅ 자동 공격 (적 탐지)
- ✅ 게임 루프 (deltaTime)

## 확장 가능성

이 코드를 기반으로 추가할 수 있는 기능:

1. **경로 찾기**: A* 알고리즘 추가
2. **UI**: SDL2 또는 SFML로 렌더링
3. **입력**: 마우스/키보드 제어
4. **건물**: 건설 시스템
5. **자원**: 미네랄/가스 채취
6. **AI**: 적 AI 추가

## 학습 목적

이 코드는 다음을 이해하는 데 도움이 됩니다:

- RTS 게임의 기본 구조
- 유닛 이동 및 전투 로직
- 충돌 감지 시스템
- 게임 루프 및 deltaTime
- C++ 객체 지향 설계

## 라이센스

OpenBW 기반 - 교육 목적
