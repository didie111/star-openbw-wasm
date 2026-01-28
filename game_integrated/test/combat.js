// 전투 시스템 (bwgame.h combat 구현)
class CombatSystem {
    static isInRange(attacker, target) {
        const dx = attacker.x - target.x;
        const dy = attacker.y - target.y;
        const distance = Math.sqrt(dx * dx + dy * dy);
        return distance <= attacker.attackRange;
    }

    static calculateDamage(attacker, target) {
        let damage = attacker.attackPower - target.armor;
        if (damage < 1) damage = 1;
        return damage;
    }

    static attack(attacker, target) {
        if (!this.isInRange(attacker, target)) return false;
        if (attacker.attackCooldown > 0) return false;

        // 쉴드 먼저 감소
        if (target.shield > 0) {
            const damage = this.calculateDamage(attacker, target);
            target.shield -= damage;
            if (target.shield < 0) {
                target.hp += target.shield; // 남은 데미지는 HP로
                target.shield = 0;
            }
        } else {
            const damage = this.calculateDamage(attacker, target);
            target.hp -= damage;
        }

        // 공격 쿨다운 설정
        attacker.attackCooldown = attacker.maxAttackCooldown;

        return true;
    }

    static findNearestEnemy(unit, allUnits) {
        let nearest = null;
        let minDistance = Infinity;

        for (const other of allUnits) {
            if (other.owner === unit.owner) continue;
            if (other.hp <= 0) continue;

            const dx = unit.x - other.x;
            const dy = unit.y - other.y;
            const distance = Math.sqrt(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                nearest = other;
            }
        }

        return nearest;
    }

    static autoAttack(unit, allUnits) {
        if (unit.currentOrder && unit.currentOrder.type === 'ATTACK') {
            // 명령된 타겟 공격
            if (unit.currentOrder.target && unit.currentOrder.target.hp > 0) {
                if (this.isInRange(unit, unit.currentOrder.target)) {
                    this.attack(unit, unit.currentOrder.target);
                }
            }
        } else {
            // 자동 공격 (범위 내 적 찾기)
            const enemy = this.findNearestEnemy(unit, allUnits);
            if (enemy && this.isInRange(unit, enemy)) {
                this.attack(unit, enemy);
            }
        }
    }
}
