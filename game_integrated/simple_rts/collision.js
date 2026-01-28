// 충돌 감지 (간단하고 작동하는 버전)
class CollisionSystem {
    static checkCollision(unit1, unit2) {
        if (unit1 === unit2) return false;
        
        const dx = unit1.x - unit2.x;
        const dy = unit1.y - unit2.y;
        const distance = Math.sqrt(dx * dx + dy * dy);
        const minDistance = unit1.size + unit2.size;

        return distance < minDistance;
    }

    static resolveCollision(unit1, unit2) {
        const dx = unit1.x - unit2.x;
        const dy = unit1.y - unit2.y;
        const distance = Math.sqrt(dx * dx + dy * dy);
        
        if (distance === 0) return; // 완전히 같은 위치
        
        const minDistance = unit1.size + unit2.size;

        if (distance < minDistance) {
            const overlap = minDistance - distance;
            const pushX = (dx / distance) * overlap * 0.5;
            const pushY = (dy / distance) * overlap * 0.5;

            unit1.x += pushX;
            unit1.y += pushY;
            unit2.x -= pushX;
            unit2.y -= pushY;
        }
    }

    static checkAllCollisions(units) {
        for (let i = 0; i < units.length; i++) {
            for (let j = i + 1; j < units.length; j++) {
                if (this.checkCollision(units[i], units[j])) {
                    this.resolveCollision(units[i], units[j]);
                }
            }
        }
    }
}
