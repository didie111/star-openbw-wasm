// 렌더링 시스템
class Renderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
    }

    clear() {
        this.ctx.fillStyle = '#0a0a0a';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }

    drawUnit(unit) {
        if (unit.hp <= 0) return;

        // 유닛 본체
        this.ctx.fillStyle = unit.color;
        this.ctx.beginPath();
        this.ctx.arc(unit.x, unit.y, unit.size, 0, Math.PI * 2);
        this.ctx.fill();

        // 선택 표시
        if (unit.isSelected) {
            this.ctx.strokeStyle = '#00ff00';
            this.ctx.lineWidth = 2;
            this.ctx.beginPath();
            this.ctx.arc(unit.x, unit.y, unit.size + 4, 0, Math.PI * 2);
            this.ctx.stroke();
        }

        // HP 바
        this.drawHealthBar(unit);
    }

    drawHealthBar(unit) {
        const barWidth = unit.size * 2;
        const barHeight = 4;
        const x = unit.x - barWidth / 2;
        const y = unit.y - unit.size - 10;

        // 배경
        this.ctx.fillStyle = '#333';
        this.ctx.fillRect(x, y, barWidth, barHeight);

        // HP
        const hpPercent = unit.hp / unit.maxHp;
        this.ctx.fillStyle = hpPercent > 0.5 ? '#00ff00' : hpPercent > 0.25 ? '#ffaa00' : '#ff0000';
        this.ctx.fillRect(x, y, barWidth * hpPercent, barHeight);

        // 쉴드
        if (unit.maxShield > 0 && unit.shield > 0) {
            const shieldPercent = unit.shield / unit.maxShield;
            this.ctx.fillStyle = '#00aaff';
            this.ctx.fillRect(x, y - 6, barWidth * shieldPercent, 3);
        }
    }
}
