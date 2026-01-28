// 경로 찾기 (bwgame.h pathfinder 구현)
class Pathfinder {
    constructor(startX, startY, targetX, targetY, obstacles) {
        this.start = { x: Math.floor(startX / 32), y: Math.floor(startY / 32) };
        this.target = { x: Math.floor(targetX / 32), y: Math.floor(targetY / 32) };
        this.obstacles = obstacles || [];
        this.gridWidth = 40;
        this.gridHeight = 30;
    }

    // A* 알고리즘
    findPath() {
        const openList = [];
        const closedList = new Set();
        const cameFrom = new Map();
        const gScore = new Map();
        const fScore = new Map();

        const startKey = `${this.start.x},${this.start.y}`;
        const targetKey = `${this.target.x},${this.target.y}`;

        openList.push(this.start);
        gScore.set(startKey, 0);
        fScore.set(startKey, this.heuristic(this.start, this.target));

        while (openList.length > 0) {
            // 가장 낮은 fScore를 가진 노드 찾기
            openList.sort((a, b) => {
                const aKey = `${a.x},${a.y}`;
                const bKey = `${b.x},${b.y}`;
                return (fScore.get(aKey) || Infinity) - (fScore.get(bKey) || Infinity);
            });

            const current = openList.shift();
            const currentKey = `${current.x},${current.y}`;

            if (currentKey === targetKey) {
                return this.reconstructPath(cameFrom, current);
            }

            closedList.add(currentKey);

            // 이웃 노드 탐색
            const neighbors = this.getNeighbors(current);
            for (const neighbor of neighbors) {
                const neighborKey = `${neighbor.x},${neighbor.y}`;

                if (closedList.has(neighborKey)) continue;
                if (this.isObstacle(neighbor)) continue;

                const tentativeGScore = (gScore.get(currentKey) || Infinity) + 1;

                if (!openList.some(n => n.x === neighbor.x && n.y === neighbor.y)) {
                    openList.push(neighbor);
                } else if (tentativeGScore >= (gScore.get(neighborKey) || Infinity)) {
                    continue;
                }

                cameFrom.set(neighborKey, current);
                gScore.set(neighborKey, tentativeGScore);
                fScore.set(neighborKey, tentativeGScore + this.heuristic(neighbor, this.target));
            }
        }

        // 경로를 찾지 못한 경우 직선 경로 반환
        return [{ x: this.target.x * 32 + 16, y: this.target.y * 32 + 16 }];
    }

    heuristic(a, b) {
        // 맨해튼 거리
        return Math.abs(a.x - b.x) + Math.abs(a.y - b.y);
    }

    getNeighbors(node) {
        const neighbors = [];
        const directions = [
            { x: 0, y: -1 }, { x: 1, y: 0 }, { x: 0, y: 1 }, { x: -1, y: 0 },
            { x: 1, y: -1 }, { x: 1, y: 1 }, { x: -1, y: 1 }, { x: -1, y: -1 }
        ];

        for (const dir of directions) {
            const newX = node.x + dir.x;
            const newY = node.y + dir.y;

            if (newX >= 0 && newX < this.gridWidth && newY >= 0 && newY < this.gridHeight) {
                neighbors.push({ x: newX, y: newY });
            }
        }

        return neighbors;
    }

    isObstacle(node) {
        return this.obstacles.some(obs => 
            Math.abs(obs.x - node.x * 32) < 32 && 
            Math.abs(obs.y - node.y * 32) < 32
        );
    }

    reconstructPath(cameFrom, current) {
        const path = [];
        let currentKey = `${current.x},${current.y}`;

        while (cameFrom.has(currentKey)) {
            const node = current;
            path.unshift({ x: node.x * 32 + 16, y: node.y * 32 + 16 });
            current = cameFrom.get(currentKey);
            currentKey = `${current.x},${current.y}`;
        }

        return path;
    }
}
