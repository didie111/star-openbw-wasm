// 선택 시스템 (actions.h 구현)
class SelectionManager {
    constructor() {
        this.selectedUnits = [];
        this.dragStart = null;
        this.dragEnd = null;
        this.isDragging = false;
    }

    selectUnit(unit, shiftPressed = false) {
        if (!shiftPressed) {
            this.clearSelection();
        }

        if (!this.selectedUnits.includes(unit)) {
            this.selectedUnits.push(unit);
            unit.isSelected = true;
        }
    }

    deselectUnit(unit) {
        const index = this.selectedUnits.indexOf(unit);
        if (index > -1) {
            this.selectedUnits.splice(index, 1);
            unit.isSelected = false;
        }
    }

    clearSelection() {
        for (const unit of this.selectedUnits) {
            unit.isSelected = false;
        }
        this.selectedUnits = [];
    }

    selectUnitsInRect(x1, y1, x2, y2, units, playerOwner) {
        this.clearSelection();

        const minX = Math.min(x1, x2);
        const maxX = Math.max(x1, x2);
        const minY = Math.min(y1, y2);
        const maxY = Math.max(y1, y2);

        for (const unit of units) {
            if (unit.owner !== playerOwner) continue;
            if (unit.hp <= 0) continue;

            if (unit.x >= minX && unit.x <= maxX &&
                unit.y >= minY && unit.y <= maxY) {
                this.selectUnit(unit, true);
            }
        }
    }

    findUnitAt(x, y, units) {
        for (let i = units.length - 1; i >= 0; i--) {
            const unit = units[i];
            if (unit.hp <= 0) continue;

            const dx = x - unit.x;
            const dy = y - unit.y;
            const distance = Math.sqrt(dx * dx + dy * dy);

            if (distance <= unit.size) {
                return unit;
            }
        }
        return null;
    }

    startDrag(x, y) {
        this.dragStart = { x, y };
        this.isDragging = true;
    }

    updateDrag(x, y) {
        if (this.isDragging) {
            this.dragEnd = { x, y };
        }
    }

    endDrag(units, playerOwner) {
        if (this.isDragging && this.dragStart && this.dragEnd) {
            const dx = this.dragEnd.x - this.dragStart.x;
            const dy = this.dragEnd.y - this.dragStart.y;
            const distance = Math.sqrt(dx * dx + dy * dy);

            if (distance > 10) {
                this.selectUnitsInRect(
                    this.dragStart.x, this.dragStart.y,
                    this.dragEnd.x, this.dragEnd.y,
                    units, playerOwner
                );
            }
        }

        this.isDragging = false;
        this.dragStart = null;
        this.dragEnd = null;
    }

    getDragRect() {
        if (!this.isDragging || !this.dragStart || !this.dragEnd) {
            return null;
        }

        return {
            x: Math.min(this.dragStart.x, this.dragEnd.x),
            y: Math.min(this.dragStart.y, this.dragEnd.y),
            width: Math.abs(this.dragEnd.x - this.dragStart.x),
            height: Math.abs(this.dragEnd.y - this.dragStart.y)
        };
    }
}
