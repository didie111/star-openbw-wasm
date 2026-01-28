// 유닛 클래스 (C++ game_types.h unit_t 간단 버전)
class Unit {
    constructor(type, owner, x, y) {
        this.id = Math.random().toString(36).substring(2, 9);
        this.type = type;
        this.owner = owner; // 0 = 아군, 1 = 적군
        this.x = x;
        this.y = y;

        const data = UNIT_DATA[type];
        this.maxHp = data.maxHp;
        this.hp = data.maxHp;
        this.maxShield = data.maxShield;
        this.shield = data.maxShield;
        this.speed = data.speed;
        this.attackPower = data.attackPower;
        this.attackRange = data.attackRange;
        this.maxAttackCooldown = data.attackCooldown;
        this.attackCooldown = 0;
        this.armor = data.armor;
        this.size = data.size;
        this.color = data.color;

        this.isSelected = false;
        this.currentOrder = null;
        this.path = [];
        this.pathIndex = 0;
    }

    update(deltaTime, allUnits) {
        // 1. 쿨다운 감소
        if (this.attackCooldown > 0) {
            this.attackCooldown -= deltaTime * 60;
            if (this.attackCooldown < 0) this.attackCooldown = 0;
        }

        // 2. 명령 처리
        if (this.currentOrder) {
            this.executeOrder(allUnits);
        }

        // 3. 이동 처리
        this.updateMovement(deltaTime);

        // 4. 자동 공격 (홀드 상태가 아니면)
        if (!this.currentOrder || this.currentOrder.type !== 'HOLD') {
            CombatSystem.autoAttack(this, allUnits);
        }
    }

    executeOrder(allUnits) {
        switch (this.currentOrder.type) {
            case 'MOVE':
                // 이동 중 (updateMovement에서 처리)
                break;

            case 'ATTACK':
                if (this.currentOrder.target && this.currentOrder.target.hp > 0) {
                    // 타겟이 범위 밖이면 추적
                    if (!CombatSystem.isInRange(this, this.currentOrder.target)) {
                        this.moveTo(this.currentOrder.target.x, this.currentOrder.target.y);
                    }
                } else {
                    // 타겟이 죽으면 명령 취소
                    this.currentOrder = null;
                }
                break;

            case 'STOP':
                this.path = [];
                this.currentOrder = null;
                break;

            case 'HOLD':
                this.path = [];
                // 홀드는 계속 유지
                break;
        }
    }

    updateMovement(deltaTime) {
        if (this.path.length === 0) return;

        const target = this.path[this.pathIndex];
        if (!target) return;

        const dx = target.x - this.x;
        const dy = target.y - this.y;
        const distance = Math.sqrt(dx * dx + dy * dy);

        if (distance < 3) {
            // 다음 경로 지점으로
            this.pathIndex++;
            if (this.pathIndex >= this.path.length) {
                this.path = [];
                this.pathIndex = 0;
                if (this.currentOrder && this.currentOrder.type === 'MOVE') {
                    this.currentOrder = null;
                }
            }
        } else {
            // 이동
            const moveSpeed = this.speed * deltaTime * 60;
            this.x += (dx / distance) * moveSpeed;
            this.y += (dy / distance) * moveSpeed;
        }
    }

    moveTo(x, y) {
        // A* 경로 찾기
        const pathfinder = new Pathfinder(this.x, this.y, x, y, []);
        this.path = pathfinder.findPath();
        this.pathIndex = 0;
    }

    attackTarget(target) {
        this.currentOrder = {
            type: 'ATTACK',
            target: target
        };
    }

    stop() {
        this.currentOrder = { type: 'STOP' };
        this.path = [];
    }

    hold() {
        this.currentOrder = { type: 'HOLD' };
        this.path = [];
    }

    isDead() {
        return this.hp <= 0;
    }
}
