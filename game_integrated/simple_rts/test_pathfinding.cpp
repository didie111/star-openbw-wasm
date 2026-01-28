// 경로 찾기 테스트 (콘솔 전용)
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>

struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
};

// A* 경로 찾기
class Pathfinder {
private:
    static const int GRID_SIZE = 32;
    static const int MAP_WIDTH = 1200 / GRID_SIZE;
    static const int MAP_HEIGHT = 800 / GRID_SIZE;
    
    struct Node {
        int x, y;
        float g, h, f;
        Node* parent;
        
        Node(int x, int y) : x(x), y(y), g(0), h(0), f(0), parent(nullptr) {}
    };
    
    struct CompareNode {
        bool operator()(Node* a, Node* b) {
            return a->f > b->f;
        }
    };
    
    static float heuristic(int x1, int y1, int x2, int y2) {
        int dx = abs(x1 - x2);
        int dy = abs(y1 - y2);
        return (float)(dx + dy) + (1.414f - 2.0f) * std::min(dx, dy);
    }
    
public:
    static std::vector<Vec2> findPath(Vec2 start, Vec2 goal, const std::vector<Vec2>& obstacles) {
        int startX = (int)(start.x / GRID_SIZE);
        int startY = (int)(start.y / GRID_SIZE);
        int goalX = (int)(goal.x / GRID_SIZE);
        int goalY = (int)(goal.y / GRID_SIZE);
        
        std::cout << "그리드 시작: (" << startX << ", " << startY << ")" << std::endl;
        std::cout << "그리드 목표: (" << goalX << ", " << goalY << ")" << std::endl;
        
        // 장애물 그리드
        bool obstacleGrid[MAP_WIDTH][MAP_HEIGHT] = {false};
        for (const Vec2& obs : obstacles) {
            int ox = (int)(obs.x / GRID_SIZE);
            int oy = (int)(obs.y / GRID_SIZE);
            if (ox >= 0 && ox < MAP_WIDTH && oy >= 0 && oy < MAP_HEIGHT) {
                obstacleGrid[ox][oy] = true;
                std::cout << "장애물: (" << ox << ", " << oy << ")" << std::endl;
            }
        }
        
        // A*
        std::priority_queue<Node*, std::vector<Node*>, CompareNode> openList;
        std::set<std::pair<int, int>> closedList;
        std::map<std::pair<int, int>, Node*> allNodes;
        
        Node* startNode = new Node(startX, startY);
        startNode->h = heuristic(startX, startY, goalX, goalY);
        startNode->f = startNode->h;
        openList.push(startNode);
        allNodes[{startX, startY}] = startNode;
        
        Node* goalNode = nullptr;
        int iterations = 0;
        
        while (!openList.empty() && iterations < 1000) {
            iterations++;
            Node* current = openList.top();
            openList.pop();
            
            if (current->x == goalX && current->y == goalY) {
                goalNode = current;
                std::cout << "경로 찾음! (반복: " << iterations << ")" << std::endl;
                break;
            }
            
            closedList.insert({current->x, current->y});
            
            // 8방향
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = current->x + dx;
                    int ny = current->y + dy;
                    
                    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) continue;
                    if (obstacleGrid[nx][ny] && !(nx == goalX && ny == goalY)) continue;
                    if (closedList.count({nx, ny})) continue;
                    
                    float moveCost = (dx != 0 && dy != 0) ? 1.414f : 1.0f;
                    float newG = current->g + moveCost;
                    
                    auto it = allNodes.find({nx, ny});
                    Node* neighbor;
                    
                    if (it == allNodes.end()) {
                        neighbor = new Node(nx, ny);
                        neighbor->g = newG;
                        neighbor->h = heuristic(nx, ny, goalX, goalY);
                        neighbor->f = neighbor->g + neighbor->h;
                        neighbor->parent = current;
                        allNodes[{nx, ny}] = neighbor;
                        openList.push(neighbor);
                    } else {
                        neighbor = it->second;
                        if (newG < neighbor->g) {
                            neighbor->g = newG;
                            neighbor->f = neighbor->g + neighbor->h;
                            neighbor->parent = current;
                        }
                    }
                }
            }
        }
        
        // 경로 재구성
        std::vector<Vec2> path;
        if (goalNode) {
            Node* current = goalNode;
            while (current) {
                path.insert(path.begin(), Vec2(
                    current->x * GRID_SIZE + GRID_SIZE / 2,
                    current->y * GRID_SIZE + GRID_SIZE / 2
                ));
                current = current->parent;
            }
            std::cout << "경로 길이: " << path.size() << std::endl;
        } else {
            std::cout << "경로 못 찾음! 직선으로..." << std::endl;
            path.push_back(goal);
        }
        
        // 메모리 정리
        for (auto& pair : allNodes) {
            delete pair.second;
        }
        
        return path;
    }
};

int main() {
    std::cout << "=== 경로 찾기 테스트 ===" << std::endl << std::endl;
    
    // 테스트 1: 장애물 없음
    std::cout << "테스트 1: 장애물 없음" << std::endl;
    Vec2 start1(100, 100);
    Vec2 goal1(500, 500);
    std::vector<Vec2> obstacles1;
    auto path1 = Pathfinder::findPath(start1, goal1, obstacles1);
    std::cout << "결과: " << path1.size() << "개 웨이포인트" << std::endl << std::endl;
    
    // 테스트 2: 장애물 있음
    std::cout << "테스트 2: 장애물 있음" << std::endl;
    Vec2 start2(100, 100);
    Vec2 goal2(500, 100);
    std::vector<Vec2> obstacles2 = {
        Vec2(300, 100),
        Vec2(300, 120),
        Vec2(300, 80)
    };
    auto path2 = Pathfinder::findPath(start2, goal2, obstacles2);
    std::cout << "결과: " << path2.size() << "개 웨이포인트" << std::endl;
    for (size_t i = 0; i < path2.size(); i++) {
        std::cout << "  [" << i << "] (" << path2[i].x << ", " << path2[i].y << ")" << std::endl;
    }
    
    return 0;
}
