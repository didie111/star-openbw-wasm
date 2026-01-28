#include <SDL2/SDL.h>
#include <iostream>
int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL 초기화 실패: " << SDL_GetError() << std::endl;
        return 1;
    }
    std::cout << "SDL 초기화 성공!" << std::endl;
    SDL_Quit();
    return 0;
}
