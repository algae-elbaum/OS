/* This is the entry-point for the game! */
#include "video.h"

// Gotta write Chess or something
// so we need an 8x8 board that needs to store data, so maybe store 
// the data in an array of structs.
typedef enum{PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING, NONE} classtype;

typedef struct 
{
    classtype class;
    char color; // Let's make the board black and white, and the pieces red and green
                // that way we can have pieces be the foreground color and the background always be black
                // or white
    char symbol;
} piece;
piece board[8][8]; // row, col

void init_board()
{
    //We can set up things here
    int i,j;
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            board[i][j].class = NONE;
            board[i][j].color = -1;
            board[i][j].symbol = '_';
        }
    }
    for (i = 0; i < 8; ++i)
    {
        board[1][i].class = PAWN;
        board[1][i].color = LIGHT_BLUE;
        board[1][i].symbol = 'P';
    }
    for (i = 0; i < 8; ++i)
    {
        board[6][i].class = PAWN;
        board[6][i].color = GREEN;
        board[6][i].symbol = 'P';
    }
    // Set up non-pawns
    board[0][0].class = ROOK;
    board[0][0].color = LIGHT_BLUE;
    board[0][0].symbol = 'R';
    board[0][7].class = ROOK;
    board[0][7].color = LIGHT_BLUE;
    board[0][7].symbol = 'R';
    board[8][0].class = ROOK;
    board[7][0].color = GREEN;
    board[7][0].symbol = 'R';
    board[7][7].class = ROOK;
    board[7][7].color = GREEN;
    board[7][7].symbol = 'R';

    board[0][1].class = KNIGHT;
    board[0][1].color = LIGHT_BLUE;
    board[0][1].symbol = 'N';
    board[0][6].class = KNIGHT;
    board[0][6].color = LIGHT_BLUE;
    board[0][6].symbol = 'N';
    board[7][1].class = KNIGHT;
    board[7][1].color = GREEN;
    board[7][1].symbol = 'N';
    board[7][6].class = KNIGHT;
    board[7][6].color = GREEN;
    board[7][6].symbol = 'N'; 

    board[0][2].class = BISHOP;
    board[0][2].color = LIGHT_BLUE;
    board[0][2].symbol = 'B';
    board[0][5].class = BISHOP;
    board[0][5].color = LIGHT_BLUE;
    board[0][5].symbol = 'B';
    board[7][2].class = BISHOP;
    board[7][2].color = GREEN;
    board[7][2].symbol = 'B';
    board[7][5].class = BISHOP;
    board[7][5].color = GREEN;
    board[7][5].symbol = 'B';

    board[0][4].class = KING;
    board[0][4].color = LIGHT_BLUE;
    board[0][4].symbol = 'K';
    board[0][3].class = QUEEN;
    board[0][3].color = LIGHT_BLUE;
    board[0][3].symbol = 'Q';
    board[7][3].class = KING;
    board[7][3].color = GREEN;
    board[7][3].symbol = 'K';
    board[7][4].class = QUEEN;
    board[7][4].color = GREEN;
    board[7][4].symbol = 'Q';
}

void move_piece(int x_1, int y_1, int x_2, int y_2)
{
    /* Actually does the movement. Doesn't check for anything
    */
    board[y_2][x_2] = board[y_1][x_1];
    board[y_1][x_1].color = -1;
    board[y_1][x_1].symbol='_';
    board[x_1][y_1].class = NONE;
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
    
    // 48 is ascii 0
    for (i = 0; i < 8; ++i)
    {
        write_char(CYAN, DARK_GRAY, i+48, i+1, 0);
        write_char(CYAN, DARK_GRAY, i+48, 0, i+1);
        write_char(CYAN, DARK_GRAY, i+48, 9, i+1);
        write_char(CYAN, DARK_GRAY, i+48, i+1, 9);
    }

    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            char back_color = ((i+j) % 2 == 0) ? BLACK : WHITE;
            char char_color = (board[i][j].color == -1) ? back_color : board[i][j].color;
            write_char(char_color, back_color, board[i][j].symbol, i+1, j+1);                
        }
    }
}

void print_prompt(_Bool color)
{
    int i;
    char *temp;
    if (color)
    {
        temp = "Player 1";
        /* code */
    }
    else
    {
        temp = "Player 2";
    }
    for (i = 0; i < 8; ++i)
    {
        write_char(CYAN, BLACK, temp[i], i, 11);
    }
    temp = "Start Row:";
    for (i = 0; i <10; ++i)
    {
        write_char(CYAN, BLACK, temp[i], i, 12);
    }
    temp = "Start Col:";
    for (i = 0; i < 10; ++i)
    {
        write_char(CYAN, BLACK, temp[i], i, 13);
    }
    temp = "Stop Row:";
    for (i = 0; i <10; ++i)
    {
        write_char(CYAN, BLACK, temp[i], i, 14);
    }
    temp = "Stop Col:";
    for (i = 0; i < 10; ++i)
    {
        write_char(CYAN, BLACK, temp[i], i, 15);
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
    init_video();
    init_board();
    print_board();
    print_prompt(0);

    while (1) 
    {
//        write_string(LIGHT_BLUE, "Hello World!");
    }
}

