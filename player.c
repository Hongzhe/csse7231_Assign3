#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define NORMAL 2
#define OUT 3
#define PROTECTED 4

/*number of players*/
int numOfPlayer; 
/*the index of player names */
int indexOfPlayer;
/*to indicate if is in your turn */

/* predefined exitcode*/
typedef enum {
    OVER = 0,
        numOfPlayerARGU = 1,
        numOfPlayerPLAYER = 2,
        PLAYERID = 3,
        PIPECLOSE = 4,
        HUBMESG = 5
} ExitCode;

/* A struct contianing a hub message */
struct Command {
    char* command;
    char* value;
};

/* A player of the game */
struct Player {
    char name;
    int status; // 3- out of round 3- protected 2-normal  
    int size;  //number of card a player have played;
    int cards[8]; //cards which have been played; 
};

/* array used to store all players */
struct Player* allPlayers[4];
/* names of all players */
const char playerNames[4] = {'A', 'B', 'C', 'D'};
/* all validate command sent from hub */
const char* hubCommand[6] = {"newround",
        "yourturn", "thishappened", "replace", "scores", "gameover"
};
/* symbols used to represents status of a player */
char statusSymbol[3] = {' ', '-', '*'};
/* a flag used to indicate if a round is end */
int isRoundEnd = TRUE;
/* a flag used to indicate if a player in its trun */
int isYourTurn;
/* store numbers of each card that hasn't been played */
int cardCounts[9] = {0, 5, 2, 2, 2, 2, 1, 1, 1}; 
/* store cards on a player hand */
int onHand[2];

/* exit programe */
void exit_player(ExitCode e);
/* validate program parameters */
void check_parameter(int argc, char*argv[]);
/* react to different cards a player discarded */
char trigger(int card);
/* record a player move */
void record_move(int value);
/* return the index of player */
int index_of_player(char name);
/* print player state to stderr */
void print_player_state(void);


/**
 *Check if parameters are validae
 *takes numbers of parameters and parameters as parameters
 *if parameters are invalide, print error message to stderr and exit.
 *return nothing.
 */
void check_parameter(int argc, char*argv[]) {
    if(argc < 3) {
        exit_player(numOfPlayerARGU);
    }
    char name = argv[2][0];
    char max = playerNames[atoi(argv[1]) - 1];
    if(atoi(argv[1]) < 2 || atoi(argv[1]) > 4) {
        exit_player(numOfPlayerPLAYER);
    }
    if((char)(*argv[2]) < 65 || (char)(*argv[2]) > 68) {
        exit_player(PLAYERID);
    }
    if (name > max) {
        exit_player(PLAYERID);
    } 
}

/**
 * Exit player accorind to exitcode
 * take Exitcode as parameters
 * return nothing.
 */
void exit_player(ExitCode e) {
    switch(e) {
        case OVER: 
            exit(0);
        case numOfPlayerARGU: 
            fprintf(stderr, "Usage: player number_of_players myid\n");
            exit(1);
        case numOfPlayerPLAYER: 
            fprintf(stderr, "Invalid player count\n");
            exit(2);
        case PLAYERID:
            fprintf(stderr, "Invalid player ID\n");
            exit(3);
        case PIPECLOSE: 
            fprintf(stderr, "Unexpected loss of hub\n");
            exit(4);
        case HUBMESG: 
            fprintf(stderr, "Bad message from hub\n");
            exit(5);
    }
}
/**
 * find the index number of given command
 * take the hub message as parameter
 * return index number.
 */
int indexof (char* command) {
    int index = -1;
    int i;
    for(i = 0; i < 6; i++) {
        if(strcmp(command, hubCommand[i]) == 0) {
            index = i;
            break;
        }
    }
    return index;
}

/*just for test purpose*/
void print_remaining_card () {
    int i;
    fprintf(stderr, "Numbers of each card: ");
    for(i = 1; i < 9; i++) {
        fprintf(stderr, "%d ", cardCounts[i]);
    }
    fprintf(stderr, "\n");
}
    
/**
 * Validate card
 * take a string of card number as paramter
 * if card is invalide, then call exit_player
 * return nothing
 */
void validate_card (char* value) {
    if(strlen(value) != 1) {
        exit_player(HUBMESG);
    }
    int card = atoi(value); 
    if(card == 0) {  //string doesn't contain int 
        exit_player(HUBMESG);
    }
    if(cardCounts[card] < 1) {
        exit_player(HUBMESG);
    }
    if(atoi(value) < 1 || atoi(value) > 8) {
        exit_player(HUBMESG);
    }
    if(cardCounts[atoi(value)] < 1) {
        exit_player(HUBMESG);
    }
}

/**
 * Validate the player names
 * takes a array of player names as parameter
 * return int. If names are valide then return 1.Otherwise return 0
 */
int is_active_player(char name[4]) {
    int i, t, isContains;
    isContains = FALSE;
    for(i = 0; i < 4; i++) {
        for(t = 0; t < numOfPlayer; t++) {
            if(name[i] == playerNames[t]) {
                isContains = TRUE;
            }
        }
    }
    return isContains;
}

/**
 * To find out if a player name is in the valide name list
 * take a player name as parameter
 * return TRUE if name in the list otherwise return false
 */
int in_name_list(char name) {
    int in = FALSE;
    int i;
    for(i = 0; i < numOfPlayer; i++) {
        if(name == playerNames[i]) {
            in = TRUE;
        }
    }
    return in;
}

/**
 * Test if a player name is valide 
 * take array of char as paramters
 * if the name is valide then return 1 otherwise
 * return 0
 */ 
int is_player(char names[4]) {
    int isName = FALSE;
    int i;
    for(i = 0; i < 4; i++) {
        if(names[i] == '-' || in_name_list(names[i])) {
            isName = TRUE;
        }
    }
    return isName;
}

/**
 * validate thishappened message 
 * It takes message as paramter
 * If message is invalide then call exit_player() 
 * return nothing
 */
void validate_this_happen(char* value) {
    char players[4] = {value[0], value[2], value[5], value[7]};
    
    if(strlen(value) != 8) {
        exit_player(HUBMESG);
    }
    if(value[4] != '/') {
        exit_player(HUBMESG);
    }
    if(!is_active_player(players)) {
        exit_player(HUBMESG);
    }
    if(!is_player(players)) {
        exit_player(HUBMESG);
    }
}

/*validate scores
 * It takes scores message as parameters
 * If the scores is invalide then call exit_player
 * return nothing
 */
void validate_scores(char* value) {
    char* token, *copy;
    if(strlen(value) != (numOfPlayer + numOfPlayer - 1)) {
        exit_player(HUBMESG);
    }
    copy = malloc(sizeof(char) * (strlen(value) + 1));
    strcpy(copy, value);
    token = strtok(copy, " ");
    if(atoi(token) < 0 || atoi(token) > 4) {
        exit_player(HUBMESG);
    }
    while(1) {
        token = strtok(NULL, " ");
        if(token == NULL) {
            break;
        }
        if(atoi(token) < 0 || atoi(token) > 4) {
          //  fprintf(stderr, "wrong scrores\n");
            exit_player(HUBMESG);
        }
    }
}
/** 
 * Based on type of command, this function will call corresponding validation
 * function
 * It takes command and value after
 * return nothing
 */
void validate_command(char* command, char* value) {
    int isContain;
    int index, i;
    isContain = FALSE;
    for(i = 0; i < 6; i++) {
        if(strcmp(command, hubCommand[i]) == 0) {
            isContain = TRUE;
        }
    }
    if (isContain == FALSE) {
        //fprintf(stderr, "not contains\n");
        exit_player(HUBMESG);
    }
    index = indexof(command);
    switch(index) {
        case 0: //newround 
            validate_card(value);
            break;
        case 1: //yourturn
            validate_card(value);
            break;
        case 2: //thishappened
            validate_this_happen(value);
            break;
        case 3: //replace
            validate_card(value);
            break;
        case 4: //scores
            validate_scores(value);
            break;
        case 5: //gamover
            if(strcmp(value, "")) {
                exit_player(HUBMESG);
            }
            break;
    }
}
/**
 * Split hub message to two parts, command and value
 * It takes the hub message as parameter
 * return struct Command
 */
struct Command* split_hub_message(char hub[]) {
    char* command, *value, *copy;
    struct Command* instruction;
    value = malloc(sizeof(char) * 9);
    copy = malloc(sizeof(char) * (strlen(hub) + 1));
    strncpy(copy, hub, strlen(hub) + 3);
    command = strtok(copy, " "); 
    //fprintf(stderr, "command is %s\n", command);
    value = strtok(NULL, "\n");
    if(value == NULL) { //handle gameover
        command[strlen(command) - 1] = '\0'; //no need to minus 1
        value = ""; 
    }
    validate_command(command, value);
    instruction = malloc(sizeof(*instruction));
    instruction->command = command;
    instruction->value = value;

    return instruction;
}
/**
 * Pick a player for targeting users 
 * It takes card number as parameters
 * return the target name
 */
char pick_target(int card) {
    int i;
    char name = '-';
    if(indexOfPlayer == numOfPlayer - 1) {
        i = 0;
    } else {
        i = indexOfPlayer + 1;
    }
    while(1) {
        if(i == indexOfPlayer) {
            break;
        }
        struct Player *player = allPlayers[i];
        if (player->status == NORMAL) {
            name = player->name;
            break;
        } 
        if(++i == numOfPlayer) {
            i = 0;
        }
    }
    if (name == '-' && card == 5) {
        name = playerNames[indexOfPlayer];
    }
    //fprintf(stderr, "target name is %c\n", name);
    return name;
}

/**
 * Update multiple  players states based on thishappened mesage *
 * It takes a array of player index and hub message as parameters
 * return nothing.
 */
void update_multiple_players (int names[2], char* value) {
    int i, index, size, card;
    struct Player *player;
    for(i = 0; i < 2; i++) {
        if(value[names[i]] == playerNames[indexOfPlayer] && i == 0) {
            continue;
        }

        if(value[names[i]] == '-') {
            continue;
        }
        if(value[names[i] + 1] == '-') {
            continue;
        }
        
        index = index_of_player(value[names[i]]);
        player = allPlayers[index];
        size = player->size;
        if(value[names[i] + 1] != '-') {
            card = value[names[i] + 1] - '0';
            cardCounts[card] -= 1;
            player->cards[size] = value[names[i] + 1] - '0';
        }
        if(player->status != OUT) {
            player->status = NORMAL;
        }
        if(value[names[i] + 1] == '4') {
            player->status = PROTECTED;
        }
        player->size += 1;
    }
}

/**
 * process hub message: thishappened ----/---
 * It takes hub message as parameters
 * return nothing,
 */
void process_this_happened(char* value) {
    int indexsOfPlayers[2] = {0, 5};
    int index;
    struct Player *player;
    
    if(value[7] != '-') {
        index = index_of_player(value[7]);
        player = allPlayers[index];
        player->status = OUT;
        if(value[7] == playerNames[indexOfPlayer]) {
            cardCounts[onHand[0]] -= 1;
            if(onHand[1] != 0) {
                cardCounts[onHand[1]] -= 1;
            }
            onHand[0] = 0;
            onHand[1] = 0; 
        } 
    }
    /*update player status*/
    update_multiple_players(indexsOfPlayers, value);
}

/**
 * respond to hub message replace 
 * replace onHand card with the card hub sent over
 * take a string as only parameter
 * return nothing
 */
void replace_card(char* value) {
    int cardNo = atoi(value);
    onHand[0] = cardNo;  // replace 
}

/**
 * Send player move to hub
 * It takes discarded card number, target player names
 * adn guess card number as parameters
 * return nothing,
 */
void print_discard_msg(int value, char target, char guess) {
    fprintf(stdout, "%d%c%c\n", value, target, guess);
    fflush(stdout);
    fprintf(stderr, "To hub:%d%c%c\n", value, target, guess);
    isYourTurn = FALSE;
}

/**
 * Find out apporiate card for guessing
 * It takes no parameters
 * return guess card number
 */
int guess_card(void) {  // need to considering if all cards been played
    int i, guess;
    for(i = 8; i > 1; i--) { //not allow to guess others hold 1
        if(cardCounts[i] > 0) {
            guess = i;
            break;
        }
    }
    return guess; 
}
/**
 * Enable player to make a guess and print out message to hub.
 * It takes two parameters. One is card player discard.The
 * second is that player's onHand card
 * It return nothing
 */
void process_guess(int card, char cardNo) {
    char target = pick_target(card);
    if(target != '-') {
        cardNo = '0' + guess_card();
    } else {
        cardNo = '-';
    }
    print_discard_msg(card, target, cardNo);
}
/**
 * Depends on what card player discarded, trigger corresponding
 * function to repond.
 * Then record player's move.
 * It takes card number as paramters
 * return target player name.
 */
char trigger(int card) {
    char target;
    char cardNo = '0' + onHand[0];//initial value is the first carad on hand
    struct Player *myself = allPlayers[indexOfPlayer];
    switch(card) {
        case 1:  // guess  card
            if(isYourTurn) {
                process_guess(card, cardNo);
            }
            break;
        case 2:
            print_discard_msg(card, '-', '-');
            break;
        case 3:  // two player compare cards
            if(isYourTurn) {
                target = pick_target(card);
                print_discard_msg(card, target, '-');
            }
            break;
        case 4: // player become protected
            myself->status = PROTECTED;
            print_discard_msg(card, '-', '-');
            break;
        case 5: //get new one from deck
            if(isYourTurn) {
                target = pick_target(card);
                print_discard_msg(card, target, '-');
            }
            break;
        case 6: // swap card
            if(isYourTurn) {
                target = pick_target(card);
                print_discard_msg(card, target, '-');
            }
            break;
        case 7:
            print_discard_msg(card, '-', '-');
            break;
        case 8: // out of round immediately
            myself->status = OUT;       
            onHand[0] = 0;
            onHand[1] = 0; 
            break;
    } 
    record_move(card);
    return target;
}

/**
 * To decide which card should be discrded
 * It taks no parameters
 * return discard card number,
 */
int discard_which(void) {
    int discard;
    int one = onHand[0];
    int second = onHand[1];

    if(one == 7) {
        if(second == 5 || second == 6) {
            discard = one;
            onHand[0] = second;
            onHand[1] = 0;
            return discard;
        }   
    } 
    if(second == 7) {
        if(one == 5 || one == 6) {
            discard = second;
            onHand[0] = one;
            onHand[1] = 0;
            return discard;
        }   
    } 
    if(one < second) {
        discard = one;
        onHand[0] = second;
        onHand[1] = 0;
    } else {
        discard = second;
        onHand[0] = one;
        onHand[1] = 0;
    } 
    return discard;

}

/** 
 * To process hub message yourturn
 * It takes hub message as parameter
 * return nothing,
 */
void my_turn(char* value) {
    int c = atoi(value);
    onHand[1] = c;
    print_player_state(); //yourturn print addition state info
    isYourTurn = TRUE;
    struct Player *myself = allPlayers[indexOfPlayer];
    myself->status = NORMAL;
    int discard = discard_which(); 
    trigger(discard);
}

/**
 * print each player state to stderr
 * It takes no parameters
 * return nothing.
 */
void print_player_state(void) {
    int i, t; 
    char cardOnHand[3], card;
    for(i = 0; i < numOfPlayer; i++) {
        struct Player *player;
        player = allPlayers[i];
        char name = player->name;
        char symbol = statusSymbol[(player->status) - 2];
        int size = player->size;
        char cards[size + 1];
        for(t = 0; t < size; t++) {
            cards[t] = player->cards[t] + '0';
        } 
        cards[size] = '\0';
        fprintf(stderr, "%c%c:%s\n", name, symbol, cards);
    }
    
    for(i = 0; i < 2; i++) {
        if(onHand[i] != 0) {
            card = onHand[i] + '0';
        } else {
            card = '-';
        }
        cardOnHand[i] = card;
    }
    cardOnHand[2] = '\0';
    fprintf(stderr, "You are holding:%s\n", cardOnHand);
}

/**
 * Once receive message newround, update player cards onhand and update/reset
 * states of players
 * It takes hub message as parameters
 * return nohting.
 */
void new_round(char* value) {
    isRoundEnd = FALSE;
    int c = atoi(value);
    int i, t;
    for(i = 0; i < numOfPlayer; i++) {
        for(t = 0; t < 8; t++) {
            allPlayers[i]->cards[t] = 0;
            allPlayers[i]->status = NORMAL;
        }
        allPlayers[i]->size = 0;
    }
    onHand[0] = c;
}

/**
 * Initalize some global variables and create struct Player for 
 * each player
 * It takes no parameters,
 * return nonthing.
 */
void init_game(void) {
    int i;
    isYourTurn = FALSE;
    for(i = 0; i < numOfPlayer; i++) {
        struct Player *player = malloc(sizeof(*player));
        player->name = playerNames[i];
        player->status = NORMAL;
        player->size = 0;
        allPlayers[i] = player;
    }
}

/**
 * Record every card a player discard 
 * and update the remaining numbers of card 
 * It takes discarded card number as paramters
 * return nothing.
 */
void record_move(int card) {
    struct Player *myself = allPlayers[indexOfPlayer];
    int size = myself->size;
    myself->cards[size] = card;
    myself->size = size + 1;
    
    cardCounts[card] -= 1;

}

/**
 * process hub message gameover
 * exit the hub
 */
void process_gameover() {
    exit(0);
}

/**
 * process hub message score 
 * reset few variables and player's state
 * It takes hub message as paramters
 * return nonthing.
 */
void process_score(char* value) {
    int copy[9] = {0, 5, 2, 2, 2, 2, 1, 1, 1};
    int i;
    isRoundEnd = TRUE;
    for(i = 0; i < 9; i++) {
        cardCounts[i] = copy[i];
    }
}
/**
 * Based on received hub message, call corresponding functions to
 * process.
 * It takes hub message as paramters
 * return nothings,
 */
void process_hub(char hub[]) {
    int index;
    struct Command* move = malloc(sizeof(*move));
    fprintf(stderr, "From hub:%s", hub); 
    print_player_state();
    move = split_hub_message(hub);
    index = indexof(move->command);
    switch(index) {
        case 0: // new round
            new_round(move->value);
            break;
        case 1: // your turn
            my_turn(move->value);
            break;
        case 2: // this happened
            process_this_happened(move->value);
            break;
        case 3: // replace
            replace_card(move->value);
            break;
        case 4: // scores
            process_score(move->value);
            break;
        case 5: // gameover
            process_gameover();
            break;
    }
    print_player_state();
    free(move);
}

/**
 * Find out index of the player 
 * It takes player name as paramters
 * return thh index.
 */
int index_of_player(char name) {
    int index, i;
    for(i = 0; i < numOfPlayer; i++) {
        if(name == playerNames[i]) {
            index = i;
            break;
        }
    }
    return index;
}

int main(int argc, char* argv[])
{
    fprintf(stdout, "-");
    fflush(stdout);
    check_parameter(argc, argv);    
    char hub[23];  //21+2 '\n''\0'
    hub[22] = '\0';
    numOfPlayer = atoi(argv[1]);
    indexOfPlayer = index_of_player(argv[2][0]);
    init_game(); 
    while(1) {
        if(fgets(hub, 23, stdin) == NULL) {
            exit_player(PIPECLOSE);
            break;
        }
        process_hub(hub);
    }
    return 0;
}
