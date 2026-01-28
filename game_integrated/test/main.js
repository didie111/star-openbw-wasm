// 메인 게임 루프
class Game {
    constructor() {
        this.canvas = document.getElementById('gameCanvas');
        this.renderer = new Renderer(this.canvas);
        this.selectionManager = new SelectionManager();
        this.commandSystem = new CommandSystem();
        
        this.units = [];
        this.playerOwner = 0;
        this.lastTime = 0;
        this.fps = 60;
        
        this.init();
        this.setupEventListeners();
    }

    init() {
        // 아군 15개 생성 (마린)
        for (let i = 0; i < 15; i++) {
            const x = 100 + (i % 5) * 50;
            const y = 100 + Math.floor(i / 5) * 50;
            this.units.push(new Unit('marine', 0, x, y));
        }

        // 적군 2개 생성 (질럿)
        this.units.push(new Unit('zealot', 1, 900, 300));
        this.units.push(new Unit('zealot', 1, 950, 350));

        console.log('게임 초기화 완료!');
        console.log('아군:', this.units.filter(u => u.owner === 0).length);
        console.log('적군:', this.units.filter(u => u.owner === 1).length);
    }

    setupEventListeners() {
        // 마우스 이벤트
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            this.onRightClick(e);
        });

        // 키보드 이벤트
        document.addEventListener('keydown', (e) => this.onKeyDown(e));
    }

    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        const clickedUnit = this.selectionManager.findUnitAt(x, y, this.units);
        
        if (clickedUnit && clickedUnit.owner === this.playerOwner) {
            this.selectionManager.selectUnit(clickedUnit, e.shiftKey);
        } else {
            this.selectionManager.startDrag(x, y);
        }
    }

    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        this.selectionManager.updateDrag(x, y);
    }

    onMouseUp(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        this.selectionManager.endDrag(this.units, this.playerOwner);
        this.updateStats();
    }

    onRightClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        const targetUnit = this.selectionManager.findUnitAt(x, y, this.units);
        this.commandSystem.handleRightClick(
            this.selectionManager.selectedUnits,
            x, y, targetUnit
        );
    }

    onKeyDown(e) {
        this.commandSystem.handleKeyPress(e.key, this.selectionManager.selectedUnits);
    }

    update(deltaTime) {
        // 유닛 업데이트
        for (const unit of this.units) {
            if (unit.hp > 0) {
                unit.update(deltaTime, this.units);
            }
        }

        // 충돌 감지
        const aliveUnits = this.units.filter(u => u.hp > 0);
        CollisionSystem.checkAllCollisions(aliveUnits);

        // 죽은 유닛 제거
        this.units = this.units.filter(u => u.hp > 0);
    }

    render() {
        this.renderer.clear();

        // 유닛 그리기
        for (const unit of this.units) {
            this.renderer.drawUnit(unit);
        }

        // 경로 그리기 (선택된 유닛)
        for (const unit of this.selectionManager.selectedUnits) {
            if (unit.path.length > 0) {
                this.drawPath(unit);
            }
        }

        // 드래그 박스 그리기
        const dragRect = this.selectionManager.getDragRect();
        if (dragRect) {
            this.renderer.ctx.strokeStyle = '#00ff00';
            this.renderer.ctx.lineWidth = 1;
            this.renderer.ctx.strokeRect(dragRect.x, dragRect.y, dragRect.width, dragRect.height);
        }
    }

    drawPath(unit) {
        const ctx = this.renderer.ctx;
        ctx.strokeStyle = '#00ff0044';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(unit.x, unit.y);
        
        for (const point of unit.path) {
            ctx.lineTo(point.x, point.y);
        }
        
        ctx.stroke();
    }

    gameLoop(currentTime) {
        const deltaTime = (currentTime - this.lastTime) / 1000;
        this.lastTime = currentTime;

        if (deltaTime < 1) {
            this.update(deltaTime);
            this.render();
            
            // FPS 계산
            this.fps = Math.round(1 / deltaTime);
            if (Math.random() < 0.1) {
                this.updateStats();
            }
        }

        requestAnimationFrame((t) => this.gameLoop(t));
    }

    updateStats() {
        document.getElementById('selected-count').textContent = this.selectionManager.selectedUnits.length;
        document.getElementById('ally-count').textContent = this.units.filter(u => u.owner === 0).length;
        document.getElementById('enemy-count').textContent = this.units.filter(u => u.owner === 1).length;
        document.getElementById('fps').textContent = this.fps;
    }

    start() {
        console.log('게임 시작!');
        this.lastTime = performance.now();
        this.gameLoop(this.lastTime);
    }
}

// 게임 시작
window.addEventListener('load', () => {
    const game = new Game();
    game.start();
});
