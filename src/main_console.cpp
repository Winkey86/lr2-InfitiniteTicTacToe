#include <iostream>
#include <sstream>
#include "Board.hpp"
#include "AI.hpp"

int main(){
    std::cout << "TTT4Infinite (console). Human plays X, AI (O) needs 4 in a row.\n";
    std::cout << "Enter moves as: x y\n";
    Board board;
    AI ai;
    ai.set_mode(AI::ALPHABETA);
    ai.set_depth(4);

    while (true){
        // print bbox
        int pad = 2;
        int x0 = board.min_x()-pad, x1 = board.max_x()+pad;
        int y0 = board.min_y()-pad, y1 = board.max_y()+pad;
        if (board.empty()){ x0=y0=-2; x1=y1=2; }
        for (int y=y1; y>=y0; --y){
            for (int x=x0; x<=x1; ++x){
                Cell c = board.at(x,y);
                char ch='.';
                if (c==Cell::X) ch='X';
                else if (c==Cell::O) ch='O';
                else if (x==0 && y==0) ch='+';
                std::cout << ch << ' ';
            }
            std::cout << '\n';
        }
        std::cout << "Your move (x y): ";
        std::string line; if (!std::getline(std::cin, line)) break;
        std::stringstream ss(line);
        int x,y; if (!(ss>>x>>y)){ std::cout<<"bad input\n"; continue;}
        if (!board.place(x,y, Cell::X)){ std::cout<<"occupied!\n"; continue; }
        if (board.is_win_from(x,y, Cell::X, 4)){ std::cout<<"You (X) win!\n"; break; }
        Move m = ai.choose_move(board);
        board.place(m.x,m.y, Cell::O);
        std::cout << "AI plays O at ("<<m.x<<","<<m.y<<")\n";
        if (board.is_win_from(m.x,m.y, Cell::O, 4)){ std::cout<<"AI (O) wins!\n"; break; }
    }
    return 0;
}
