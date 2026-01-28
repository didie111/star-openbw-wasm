// 명령 시스템 (actions.h 구현)
class CommandSystem {
    constructor() {
        this.commandMode = 'NORMAL'; // NORMAL, ATTACK, MOVE, PATROL
    }

    issueOrder(units, orderType, target, targetUnit = null) {
        for (const unit of units) {
            switch (orderType) {
                case 'MOVE':
                    unit.moveTo(target.x, target.y);
                    unit.currentOrder = { type: 'MOVE' };
                    break;

                case 'ATTACK':
                    if (targetUnit) {
                        unit.attackTarget(targetUnit);
                        unit.moveTo(targetUnit.x, targetUnit.y);
                    } else {
                        unit.moveTo(target.x, target.y);
                        unit.currentOrder = { type: 'MOVE' };
                    }
                    break;

                case 'STOP':
                    unit.stop();
                    break;

                case 'HOLD':
                    unit.hold();
                    break;
            }
        }
    }

    handleRightClick(selectedUnits, x, y, targetUnit) {
        if (selectedUnits.length === 0) return;

        if (this.commandMode === 'ATTACK') {
            // 강제 공격
            this.issueOrder(selectedUnits, 'ATTACK', { x, y }, targetUnit);
            this.commandMode = 'NORMAL';
        } else {
            // 스마트 명령
            if (targetUnit) {
                if (targetUnit.owner !== selectedUnits[0].owner) {
                    // 적 유닛 -> 공격
                    this.issueOrder(selectedUnits, 'ATTACK', { x, y }, targetUnit);
                } else {
                    // 아군 유닛 -> 이동
                    this.issueOrder(selectedUnits, 'MOVE', { x, y });
                }
            } else {
                // 빈 공간 -> 이동
                this.issueOrder(selectedUnits, 'MOVE', { x, y });
            }
        }
    }

    handleKeyPress(key, selectedUnits) {
        switch (key.toLowerCase()) {
            case 'a':
                this.commandMode = 'ATTACK';
                console.log('공격 모드 활성화');
                break;

            case 's':
                this.issueOrder(selectedUnits, 'STOP', {});
                console.log('정지 명령');
                break;

            case 'h':
                this.issueOrder(selectedUnits, 'HOLD', {});
                console.log('홀드 명령');
                break;

            case 'escape':
                this.commandMode = 'NORMAL';
                break;
        }
    }
}
