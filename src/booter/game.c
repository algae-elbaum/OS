/* This is the entry-point for the game! */
#include "video.h"
#define TIMER_START_TIME 18000 // Half hour of play is on the timer
#include "keyboard.h"

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

typedef struct 
{
    int x;
    int y;
} location;

piece board[8][8]; // row, col
int timer_1 = TIMER_START_TIME;
int timer_2 = TIMER_START_TIME;
typedef enum{ONE, TWO, OFF} timer_state;

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

void move_piece(location start, location stop)
{
    /* Actually does the movement. Doesn't check for anything
    does handle promotion properly though.
    // all pawns just become queens
    */
    int x_1 = start.x;
    int x_2 = stop.x;
    int y_1 = start.y;
    int y_2 = stop.y;

    board[y_2][x_2] = board[y_1][x_1];
    if (board[y_1][x_1].symbol=='p' && (x_1 == 8 || x_1 == 0))
    {
        board[y_2][x_2].symbol = 'Q';
        board[y_2][x_2].class = QUEEN;
    }
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

void print_prompt(char color)
{
    int i;
    char *temp;
    if (color == LIGHT_BLUE)
    {
        temp = "Player 1";
        /* code */
    }
    else
    {
        temp = "Player 2";
    }
    write_string(CYAN, BLACK, temp, 0, 11);
    write_string(CYAN, BLACK, "Start Row:", 0, 12);
    write_string(CYAN, BLACK, "Start Col:", 0, 13);
    write_string(CYAN, BLACK, "Stop Row:", 0, 14);
    write_string(CYAN, BLACK, "Sttop Col:", 0, 15);
}

void bishop_path(location start, location stop, location * ans)
{
    // determines which tiles a bishop would have had to go through 
    // in order to get here
    // assert that the move is always correct, so it should only be
    // called if the relative positions are correct. basically
    // we are checking if there is something in the way.
    // ans should be an array of size seven because its the longest
    // possible value
    int i = start.x;
    int j = start.y;
    int x_dir = stop.x - start.x;
    int y_dir = stop.y - start.y;
    location temp;
    int k = 0; //iterator
    //assert that these directions cannot be 0.
    while(i != stop.x && j != stop.y)
    {
        if (x_dir > 0)
        {
            i ++;
        }
        else
        {
            i --;
        }
        if (y_dir > 0)
        {
            j ++;
        }
        else
        {
            j--;
        }
        temp.x = i;
        temp.y = j;
        ans[k] = temp;
        k ++;
    }
}
void rook_path(location start, location stop, location * ans)
{
    // determines which tiles a rook would have had to go through 
    // in order to get here
    // assert that the move is always correct, so it should only be
    // called if the relative positions are correct. basically
    // we are checking if there is something in the way.
    // ans should be an array of size seven because its the longest
    // possible value
    int i = start.x;
    int j = start.y;
    int x_dir = stop.x - start.x;
    int y_dir = stop.y - start.y;
    location temp;
    int k = 0; //iterator
    if(x_dir != 0)
    {
        while(i != stop.x && j != stop.y)
        {
            if (x_dir > 0)
            {
                i ++;
            }
            else
            {
                i --;
                temp.x = i;
                temp.y = j;
                ans[k] = temp;
                k ++;
            }
        }
    }
    else
    {
        while(i != stop.x && j != stop.y)
        {
            if (y_dir > 0)
            {
                j ++;
            }
            else
            {
                j --;
                temp.x = i;
                temp.y = j;
                ans[k] = temp;
                k ++;
            }
        }
    }
}

_Bool check_check(char color)
{
    // This checks if color is CHECKING the other color.
    int i,j;
    _Bool ans= 0;
    location king_pos;
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            if (board[i][j].class == KING && board[i][j].color != color)
            {
                king_pos.x = i;
                king_pos.y = j;
            }
        }
    }
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            if (board[i][j].color == color)
            {
                // If valid move from [i][j] to kingpos
                // then set ans to 1
                ans = 0; // REMOVE THIS. just wanted to put something in this if block
            }
        }
    }
    return ans;
}

void print_timers()
{
    /* Print out the timer labels */
    write_string(CYAN, BLACK, "Player One Timer:", 20, 0);
    write_string(CYAN, BLACK, "Player Two Timer:", 20, 5);
}

/* Not sure how to do anything having to do with printing the numbers for the timer
and making it decrease */

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

    // Somewhere after move one, we need to say that the timer is now 
    // belgoning to Green. Use the timer_state enum
    // there is an off state provded for when the first move hasn't been played yet
    int proposed_move[4];
    int i;
    for (i = 0; i < 4; ++i)
    {
        proposed_move[i] = -1;
    }
    i = 0;

    while (1) // while both of the timers are above 0
    {
        // print the board
        // pull from keyboard
        char curr_key = pop_queue();
        
        // want to know what type of key do you get?
        switch(curr_key)
        {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '0':
                proposed_move[i] = curr_key;
                i ++;
                write_char(CYAN, BLACK, curr_key, 12, 12+i);
            case '\b':
                proposed_move[i] = -1;
                i --;
                write_char(BLACK, BLACK, '_', 12, 12+i);
            case '\n':
                for (i = 0; i < 4; ++i)
                {
                    write_char(BLACK, BLACK, '_', 12, 12+i);
                }
                i = 0;
                // check for valid mvoe and make the move
                // every time we press enter we try to run the move. if we fail to 
                    // make that move then we stay on the same turn. otherwise we switch turns
            case '\0':
                // do nothing
                break;
        }

        // print the timer numbers. we get the updated numbers from the interrupt.
        // check if its down to 0. If so, display that the player is out 
        // of time and turn red or something
        // but that is something related to the timer


    }
}

