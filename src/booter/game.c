/* This is the entry-point for the game! */
#include "video.h"

// Gotta write Chess or something
// so we need an 8x8 board that needs to store data, so maybe store 
// the data in an array of structs.
typedef enum{PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING} classtype;

typedef struct 
{
    classtype class;
    _Bool color; // Let's make the board black and white, and the pieces red and green
    // that way we can have pieces be the foreground color and the background always be black
    // or white
    char symbol;
} piece;
piece board[8][8]; // row, col

piece black_rook = {ROOK, 1, "R"};

void init_board()
{
    //We can set up things here
    int i,j;
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            board[i][j] = (void *) 0;
        }
    }
    for (i = 0; i < 8; ++i)
    {
        board[1][i]->class = PAWN;
        board[1][i]->color = 0;
        board[1][i]->symbol = 'p';
    }
    for (i = 0; i < 8; ++i)
    {
        board[7][i]->class = PAWN;
        board[7][i]->color = 1;
        board[7][i]->symbol = 'p';
    }
    // Set up non-pawns
    board[0][0]->class = ROOK;
    board[0][0]->color = 0;
    board[0][0]->symbol = 'R';
    board[0][7]->class = ROOK;
    board[0][7]->color = 0;
    board[0][7]->symbol = 'R';
    board[8][0]->class = ROOK;
    board[8][0]->color = 1;
    board[8][0]->symbol = 'R';
    board[8][7]->class = ROOK;
    board[8][7]->color = 1;
    board[8][7]->symbol = 'R';

    board[0][1]->class = KNIGHT;
    board[0][1]->color = 0;
    board[0][1]->symbol = 'N';
    board[0][6]->class = KNIGHT;
    board[0][6]->color = 0;
    board[0][6]->symbol = 'N';
    board[8][1]->class = KNIGHT;
    board[8][1]->color = 1;
    board[8][1]->symbol = 'N';
    board[8][6]->class = KNIGHT;
    board[8][6]->color = 1;
    board[8][6]->symbol = 'N'; 

    board[0][2]->class = BISHOP;
    board[0][2]->color = 0;
    board[0][2]->symbol = 'B';
    board[0][5]->class = BISHOP;
    board[0][5]->color = 0;
    board[0][5]->symbol = 'B';
    board[8][2]->class = BISHOP;
    board[8][2]->color = 1;
    board[8][2]->symbol = 'B';
    board[8][5]->class = BISHOP;
    board[8][5]->color = 1;
    board[8][5]->symbol = 'B';

    board[0][4]->class = KING;
    board[0][4]->color = 0;
    board[0][4]->symbol = 'K';
    board[0][3]->class = QUEEN;
    board[0][3]->color = 0;
    board[0][3]->symbol = 'Q';
    board[8][3]->class = KING;
    board[8][3]->color = 1;
    board[8][3]->symbol = 'K';
    board[8][4]->class = QUEEN;
    board[8][4]->color = 1;
    board[8][4]->symbol = 'Q';
}

void move_piece(int x_1, int y_1, int x_2, int y_2)
{
    /* Actually does the movement. Doesn't check for anything
    */
    board[y_2][x_2] = board[y_1][x_1];
    board[y_1][x_1] = (void *) 0;
}

void print_board()
{
    /* Prints the board to screen 
    Takes up the first 10x10 parts of the screen*/
    /* 01234567
      0RBNKQBNR0
      ...
    */
    int i,j;
    for (int i = 0; i < 8; ++i)
    {
        write_char(CYAN, i, i, 0)
        write_char(CYAN, i, 0, i)
        write_char(CYAN, i, 9, i)
        write_char(CYAN, i, i, 9)
    }
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            if (board[i][j]->color)
            {
                write_char(WHITE, board[i][j]->symbol, i+1, j+1);
            }
            else
            {
                write_char(BLACK, board[i][j]->symbol, i+1, j+1);                
            }
        }
    }
}

void c_start(void) {
    /* TODO:  You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */

    /* Loop forever, so that we don't fall back into the bootloader code. */
    unsigned char c = 'a';
    while (1) 
    {
        write_string(RED, "Hello World!");
        init_board();
        print_board();

    }
}

