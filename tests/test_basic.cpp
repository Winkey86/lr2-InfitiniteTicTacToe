#include <cassert>
#include "Board.hpp"

int main(){
    Board b;
    assert(b.place(0,0, Cell::X));
    assert(!b.place(0,0, Cell::O)); // occupied
    // win horizontal
    b.place(1,0, Cell::X);
    b.place(2,0, Cell::X);
    b.place(3,0, Cell::X);
    assert(b.is_win_from(3,0, Cell::X, 4));
    // undo and check non-win
    b.undo(3,0);
    assert(!b.is_win_from(2,0, Cell::X, 4));
    return 0;
}
