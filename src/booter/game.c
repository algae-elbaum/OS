/* This is the entry-point for the game! */
#include "video.h"
#include "keyboard.h"
#include "interrupts.h"

#define TIMER_START_TIME 18000 // Half hour of play is on the timer

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

void bishop_path(location start, location stop, location * ans);
void rook_path(location start, location stop, location * ans);
_Bool check_check(char color);

piece board[8][8]; // row, col
int timer_1 = TIMER_START_TIME;
int timer_2 = TIMER_START_TIME;
typedef enum{ONE, TWO, OFF} timer_state;
timer_state global_timer_state = OFF;

int get_timer1()
{
	disable_interrupts();
	int x = timer_1;
	enable_interrupts();
	return x;
}

int get_timer2()
{
	disable_interrupts();
	int x = timer_2;
	enable_interrupts();
	return x;
}

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
    board[7][4].class = KING;
    board[7][4].color = GREEN;
    board[7][4].symbol = 'K';
    board[7][3].class = QUEEN;
    board[7][3].color = GREEN;
    board[7][3].symbol = 'Q';
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

    board[x_2][y_2] = board[x_1][y_1];
    if (board[x_1][y_1].class == PAWN && (x_2 == 7 || x_2 == 0))
    {
        board[x_2][y_2].symbol = 'Q';
        board[x_2][y_2].class = QUEEN;
    }
    board[x_1][y_1].color = -1;
    board[x_1][y_1].symbol='_';
    board[x_1][y_1].class = NONE;
}
void clear_error()
{
write_string(BLACK, BLACK, "________________________________________________", 0, 20);
}
// checks if a move is legal, given a starting and ending position
// note: blue side always moves first
int is_legal_move(int x1, int y1, int x2, int y2, char turn_color, _Bool print_errors)
{
    // check if piece color is same as turn color (so you can't move the 
    // otwrite_string(board[x1][y1].color, GREEN, "this is the color on th eboard", 0, 21);

    if (board[x1][y1].color != turn_color)
    {
        if(print_errors){
	       write_string(RED, BLACK, "cannot move this piece, it's not your turn", 0, 20);}
	        return 0;
    }
    // if end move is start move, print error and return
    if (x1 == x2 && y1 == y2)
    {
        if(print_errors){
        write_string(RED, BLACK, "invalid input, end location is same as start", 0, 20);}
        return 0;
    }
    // if end move is off the board, print error and return
    if ((x1 < 0 || x1 > 8) || (x2 < 0 || x2 > 8) || (y1 < 0 || y1 > 8) || (y2 < 0 || y2 > 8))
    {
        if(print_errors){
        write_string(RED, BLACK, "invalid input, end location is off the board", 0, 20);}
        return 0;
    }
    if (board[x2][y2].color == board[x1][y1].color)
    {
        if(print_errors){
         write_string(RED, BLACK, "you can't attack your ally!", 0, 20);  }
         return 0; 
    }
    // "Disable" the destination piece in case capturing it would take us out of check
    classtype temp = board[x2][y2].class;
    board[x2][y2].class = NONE;
    if (check_check(board[x1][y1].color))
    {
         write_string(RED, BLACK, "CHECK your privilege", 0, 20);  
         board[x2][y2].class = temp; // reenable the piece
         return 0; 
    }
    board[x2][y2].class = temp; // reenable

    switch(board[x1][y1].class)
    {
        case PAWN:
            // if in starting row, can move 2 or 1 spaces
        // if forward diagonals 1 unit away are occupied, can move there
        // else can move 1 space fwd
            if (board[x1][y1].color == LIGHT_BLUE)
            {

                if (x1 == 1 && (x2 == 2 || x2 == 3) && y2 == y1) // if in starting row
                {
                    return 1;
                }
                else if (x2 == x1 + 1 && y2 == y1) // normal case
                {
                    return 1;
                }
                // if attacking
                else if (board[x2][y2].class != NONE && 
                    x2 == x1 + 1 && (y2 == y1 + 1 || y2 == y1 - 1))
                {
                    return 1;
                }
                // all other cases (invalid move) fall through
            }
            // if green, things go the other direction
            else if (board[x1][y1].color == GREEN)
            {
                if (x1 == 6 && (x2 == 5 || x2 == 4)) // if in starting row
                {
                    return 1;
                }
                else if (x2 == x1 - 1 && y2 == y1) // normal case
                {
                    return 1;
                }
                // if attacking
                else if (board[x2][y2].class != NONE && 
                    x2 == x1 - 1 && (y2 == y1 + 1 || y2 == y1 - 1))
                {
                    return 1;
                }
                // all other cases (invalid move) fall through
            }
            break;

        case BISHOP:
        // if position is on either diagonal from starting pos, is legal
        // if slope between desired and start is 1, is on diagonal
            if ((y2 - y1) == (x2 - x1) || (y2 - y1) == -(x2 - x1))
            {
                // check if it passes through any occupied spaces
                location start;
                location end;
                location thru[8];
                start.x = x1;
                start.y = y1;
                end.x = x2;
                end.y = y2;
                bishop_path(start, end, thru);
                int i;
                for (i = 0; thru[i].x != -1; i++)
                {
                    if (board[thru[i].x][thru[i].y].class != NONE)
                    {
                        if(print_errors){
                        write_string(RED, BLACK, "invalid move, piece in the way", 0, 20);}
                        return 0;
                    }
                }

                return 1;
            }
            break;
            // all other cases (invalid move) fall through

        case KNIGHT:
        // moves in Ls, valid locs just listed
            if ((y2 == y1 + 2 && (x2 == x1 + 1 || x2 == x1 - 1)) || 
                (y2 == y1 - 2 && (x2 == x1 + 1 || x2 == x1 - 1)) || 
                (x2 == x1 + 2 && (y2 == y1 + 1 || y2 == y1 - 1)) ||
                (x2 == x1 - 2 && (y2 == y1 + 1 || y2 == y1 - 1)))
            {
                return 1;
            }
            break;
            // all other cases (invalid move) fall through

        case ROOK:
        // if position is in straight line from starting position, is legal
        // i.e. if same x or y value
            if (x2 == x1 || y2 == y1)
            {
                // check if it passes through any occupied spaces
                location start;
                location end;
                location * thru;
                start.x = x1;
                start.y = y1;
                end.x = x2;
                end.y = y2;
                rook_path(start, end, thru);
                int i;
                for (i = 0; thru[i].x != -1; i++)
                {
                    if (board[thru[i].x][thru[i].y].class != NONE)
                    {
                        if(print_errors){
                        write_string(RED, BLACK, "invalid move, piece in the way", 0, 20);}
                        return 0;
                    }
                }

                return 1;
            }
            break;
            // all other cases (invalid move) fall through

        case QUEEN:
        // can move on diagonals or straight lines (bishops + rook movement)
            if ((y2 - y1) == (x2 - x1) || (y2 - y1) == -(x2 - x1) || (x2 == x1 || y2 == y1))
            {
                location start;
                location end;
                location * thru;
                start.x = x1;
                start.y = y1;
                end.x = x2;
                end.y = y2;
                rook_path(start, end, thru);
                int i;
                for (i = 0; thru[i].x != -1; i++)
                {
                    if (board[thru[i].x][thru[i].y].class != NONE)
                    {
                        if(print_errors){
                        write_string(RED, BLACK, "invalid move, piece in the way", 0, 20);}
                        return 0;
                    }
                }
                bishop_path(start, end, thru);
                for (i = 0; thru[i].x != -1; i++)
                {
                    if (board[thru[i].x][thru[i].y].class != NONE)
                    {
                        if(print_errors){
                        write_string(RED, BLACK, "invalid move, piece in the way", 0, 20);}
                        return 0;
                    }
                }

                return 1;
            }
            break;
            
        case KING:
        // 1 square in any direction, including diagonals
            if ((y1 - y2 == -1 || y1 - y2 == 1) && (x1 - x2 == -1 || x1 - x2 == 1 || x1 - x2 == 0)
                || (y1 - y2 == -1 || y1 - y2 == 1 || y1 - y2 == 0) && (x1 - x2 == -1 || x1 - x2 == 1))
            {
                return 1;
            }
            break;
            // all other cases (invalid move) fall through
        case NONE:
                        if(print_errors){
            write_string(RED, BLACK, 
                "this piece does not have a valid class or does not exist", 0, 20);}
            return 0;
    }
                        if(print_errors){
    write_string(RED, BLACK, "invalid move", 0, 20);}
    return 0;
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

void print_prompt()
{
    int i;
    char *temp;
    if (global_timer_state != TWO)
    {
        temp = "Player 1";
    }
    else
    {
        temp = "Player 2";
    }
    write_string(YELLOW, BLACK, temp, 0, 11);
    write_string(YELLOW, BLACK, "Start Row:", 0, 13);
    write_string(YELLOW, BLACK, "Start Col:", 0, 12);
    write_string(YELLOW, BLACK, "Stop Row:", 0, 15);
    write_string(YELLOW, BLACK, "Stop Col:", 0, 14);
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
    temp.x = -1;
    temp.y = -1;
    ans[k] = temp; // to know where it terminates
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
    temp.x = -1;
    temp.y = -1;
    ans[k] = temp; // to know where it terminates
}

_Bool check_check(char color)
{
    // Simple janky lock to prevent mutual recursion
    static _Bool currently_checking = 0;
    if (currently_checking)
    {
        return 0;
    }
    currently_checking = 1;
    // This checks if color is in check
    int i,j;
    location king_pos;
    for (i = 0; i < 8; ++i)
    {
        for (j = 0; j < 8; ++j)
        {
            if (board[i][j].class == KING && board[i][j].color == color)
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
            if (board[i][j].color != color && board[i][j].color != -1)
            {
                if (is_legal_move(i, j, king_pos.x, king_pos.y, board[i][j].color, 0))
                {
                    currently_checking = 0;
                    return 1;
                }
            }
        }
    }
    currently_checking = 0;
    return 0;
}

void print_timers()
{
    /* Print out the timer labels */
    write_string(CYAN, BLACK, "Player One Timer:", 20, 0);
    write_int(CYAN, BLACK, get_timer1(), 20, 2, 7);
    write_string(CYAN, BLACK, "Player Two Timer:", 20, 5);
    write_int(CYAN, BLACK, get_timer2(), 20, 7, 7);
}

void switch_turn()
{
    if (global_timer_state == TWO)
    {
        global_timer_state = ONE;
    }
    else
    {
        global_timer_state = TWO;
    }
}


void decrement_timer()
{
    if (global_timer_state == ONE)
    {
        timer_1 --;
    }
    if (global_timer_state == TWO)
    {
        timer_2 --;
    }
}

/* Not sure how to do anything having to do with printing the numbers for the timer
and making it decrease */

void c_start(void) 
{
    init_interrupts();
    init_keyboard();
    init_timer();
    enable_interrupts();
    init_video();
    init_board();

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
    print_timers();
                char curr_color;
    while (1) // while both of the timers are above 0
    {
 print_board();
    print_prompt();
print_timers();

                if(global_timer_state == TWO)
                {
                    curr_color = GREEN;
                }
                else
                {
                    curr_color = LIGHT_BLUE;
                }

        // print the board
        // pull from keyboard
        char curr_key = pop_queue();
	if(curr_key != '\0'){
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
		if(i<4){
                proposed_move[i] = curr_key - '0';
                write_char(CYAN, BLACK, curr_key, 12, 12+i);
                i ++;}
                clear_error();
		break;
            case '\b':
                proposed_move[i] = -1;
                i --;
                write_char(BLACK, BLACK, '_', 12, 12+i);
		if (i < 0){i = 0;}
		break;
            case '\n':
                for (i = 0; i < 4; ++i)
                {
                    write_char(BLACK, BLACK, '_', 12, 12+i);
                }
                i = 0;
                if (is_legal_move(proposed_move[0], proposed_move[1], proposed_move[2], proposed_move[3], curr_color, 1))
                {
                    location loc1;
                    loc1.x = proposed_move[0];
                    loc1.y = proposed_move[1];
                    location loc2;
                    loc2.x = proposed_move[2];
                    loc2.y = proposed_move[3];
                    move_piece(loc1, loc2);
                    switch_turn();
		
                }    
            for(i=0;i<4;i++)
			{
			     proposed_move[i] = -1;
			}
            i = 0;
            case '\0':
                // do nothing
                break;
	    default:
                write_char(RED, BLACK, curr_key, 30, 30);
        }

        // print the timer numbers. we get the updated numbers from the interrupt.
        // check if its down to 0. If so, display that the player is out 
        // of time and turn red or something
        // but that is something related to the timer


   }} print_board();
    print_prompt();


}

