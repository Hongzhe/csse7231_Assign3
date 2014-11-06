#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <wait.h>
#include <string.h>
#include <signal.h>

#define WRITE_END 1
#define READ_END 0

#define FALSE 0
#define TRUE 1

/* A deck */
struct Deck {
    int card[16];
    struct Deck *next; 
    struct Deck *prev;
};
/* A pair of file descriptors */
struct PipePair {
    int write[2];
    int read[2];
};
/* A player */
struct Player {
    char name;
    int isOut;
    int isProtected;
    int scores; 
    int onHand[2];
};
/* A struct which stores every round result */
struct RoundResult {
    int max;
    char* winners;
};

/* store up to 4 child processes pid */
pid_t pids[4];
/* pipes used to read message from child processes */
FILE* readFiles[4];
/* pipes used to write message to child processes */
FILE* writeFiles[4];
/* numbers of players */
int numPlayers;
/* a flag to indicate if game is over */
int isGameover;
/* a flag to indicate if round is end */
int isRoundEnd;
/* indicate how many rounds played */
int roundNo; 
/* point to next card in a deck */
int indexOfCard; 
/* store thishappened message. */
char stateMesg[10]; 
/* array of all active players */
struct Player* players[4];
/* first deck in a linked list. */
struct Deck *decks; 
/* the deck is being used for this round */
struct Deck *currentDeck; 
/* array of all player names */
const char* names[4] = {"A", "B", "C", "D"};
/* numbers of each card */
int cardCounts[9] = {0, 5, 2, 2, 2, 2, 1, 1, 1};

/* enum of exit code */
typedef enum {
    OVER = 0,
        NUM = 1,
        DECKFILE = 2,
        DECKCONTENT = 3,
        STARTPROCESS = 4,
        PLAYERQUIT = 5,
        PLAYERMSG = 6,
        SIGNAL = 7
} ExitCode;

/* Insert a new deck into a linked list */
void insert(struct Deck **first, struct Deck *node);
/* Fork child processes */
void fork_children(int num, char* argc[], struct PipePair* pipes[]);
/* Open pipes */
void open_pipes (struct PipePair* pipes[], int num);
/* Read players' message */
void read_player(int whichPlayer, char* message);
/* Hub make respond to different cards */
void respond_to_card(int card, char target, int guess, int whichPlayer);
/* Return the index of player */
int index_of_player(char name);

/**
 * Exit hub according to corresponding sitution 
*/
void exit_hub(ExitCode e) {
    switch(e) {
        case OVER:
            exit(0);
        case NUM:
            fprintf(stderr, "Usage: hub deckfile prog1 prog2" 
                    " [prog3 [prog4]]\n");
            exit(1);
        case DECKFILE:
            fprintf(stderr, "Unable to access deckfile\n");
            exit(2);
        case DECKCONTENT:
            fprintf(stderr, "Error reading deck\n");
            exit(3);
        case STARTPROCESS:
            fprintf(stderr, "Unable to start subprocess\n");
            exit(4);
        case PLAYERQUIT:
            fprintf(stderr, "Player quit\n");
            exit(5);
        case PLAYERMSG:
            fprintf(stderr, "Invalid message received from player\n");
            exit(6);
        case SIGNAL:
            fprintf(stderr, "SIGINT caught\n");
            exit(7);
    }
}
/**
 * Validate the parameter
 * takes numbers of parameters and paramters as parameters
 * If the parameters are invalida, then print erro message to
 * stderr and exit hub
 * return nothing,
 */
void validate_parameters(int argv, char* argc[]) {
    if(argv < 4) {
        exit_hub(NUM);
    }       
}

/* Validate the player mesage
 * take string as a parameter
 * If found out message is invalide, then exit hub
 * */
void validate_player_message(char* message) {
    int i, card1, card2, targetIndex;
    char target = message[1];
    card1 = message[0] - '0';
    card2 = message[2] - '0';
    int isPlayer = FALSE;
    if(target != '-') { 
        targetIndex = index_of_player(target);
        struct Player *player = players[targetIndex];
        if (player->isOut == TRUE || player->isProtected == TRUE) {
            exit_hub(PLAYERMSG);
        }
    }
    if(strlen(message) != 4) {
        exit_hub(PLAYERMSG);
    }
    /*invalid card number */
    if (message[2] != '-') {
        if(1 > card1 || card1 > 8 || 1 > card2 || (card2 > 8)) {
            exit_hub(PLAYERMSG);
        }
    }
    
    for(i = 0; i < numPlayers; i++) {
        if(names[i][0] == message[1] || message[1] == '-') {
            isPlayer = TRUE;
        }
    }
    /*Invalid player name */
    if (!isPlayer) {
        exit_hub(PLAYERMSG);
    }
}
/* Validate deck content */
void validate_deck(struct Deck *deck) {
    int i;
    while(deck != NULL) {
        int sum = 0;
        for(i = 0; i < 16; i++) {
            sum += deck->card[i];
        }
        if (sum != 54) {
            exit_hub(DECKCONTENT);
        }
        deck = deck->prev;
    }
}

/**
 * Read the deck file and add each row into a linked list 
 * If can't open deck file, print the error message and exit
 * If the content of deck is invalide, print error messaga and exit
 */
struct Deck* read_deck(char* fileName) {
    struct Deck *first, *tail;
    int line[16], t;
    int i, isFirst;
    FILE *f;
    char c;
    f = fopen(fileName, "r");
    if(f == NULL) {
        exit_hub(DECKFILE);
    }
    isFirst = TRUE; 
    first = tail = malloc(sizeof(*first));
    first = tail = NULL;
    
    i = 0;
    while(1) {
        c = fgetc(f);
        if(c == EOF) {
            break;
        }
        int ch = c - '0';
        if(c != '\n') {
            line[i] = c - '0';
            if(ch < 1 || ch > 8) { /*character is not a valide digit */
                exit_hub(DECKCONTENT);
            }
            if(i > 15) { /*has more than 16 cards in one deck */
                exit_hub(DECKCONTENT);
            }
            if(i == 15) {
                struct Deck* node = malloc(sizeof(*node));
                for(t = 0; t < 16; t++) {
                    node->card[t] = line[t];
                }
                insert(&first, node);
                if(isFirst) {
                    tail = node;
                    isFirst = FALSE;
                }
                i = 0;
                continue;
            }
        } else { // c is '\n'
            i = 0;
            continue;
        }
        i++;
    }
    return tail;
}

/**
 * Insert a deck into a linked list of deck.
 * take pointer to pointer to struct Deck and pointer to struct Deck 
 * as parameters.
 * return nothing.
*/
void insert(struct Deck **first, struct Deck *node) {
    if(!(*first == NULL)) {
        node->next = *first;
        (*first)->prev = node;
        *first = node;
        (*first)->prev = NULL;
    } else {
        *first = node;
    }
}

/**
 * Find the index of a player
 * take name of a player as parameter
 * return the player index 
*/
int index_of_player(char name) {
    int i, index;
    for(i = 0; i < numPlayers; i++) {
        if(names[i][0] == name) {
            index = i;
            break;
        }
    }
    return index; 
}

/**
 * Translate thishappened message to readable format.
 * It takes no parameters.
 * return readable formate message
 */
char* translate_state_message(void) {
    char base[22], aim[26], force[27], out[10];
    char* whole = malloc(sizeof(char) * 90);   // need free
    strcpy(whole, "Player ");
    if(stateMesg[2] == '-' && stateMesg[3] == '-') {
        sprintf(base, "%c discarded %c.", stateMesg[0], stateMesg[1]);
    } else {
        sprintf(base, "%c discarded %c ", stateMesg[0], stateMesg[1]);
    }
    
    if(stateMesg[2] != '-' && stateMesg[3] == '-') {
        sprintf(aim, "aimed at %c.", stateMesg[2]);
    } else if (stateMesg[3] != '-') {
        sprintf(aim, "aimed at %c guessing %c.", stateMesg[2], stateMesg[3]);
    } else {
        aim[0] = '\0';
    }
    
    if(stateMesg[5] != '-') {
        sprintf(force, " This forced %c to discard %c.", 
                stateMesg[5], stateMesg[6]);
    } else {
        force[0] = '\0';
    } 
    if(stateMesg[7] != '-') {
        sprintf(out, " %c was out.", stateMesg[7]);
    } else {
        out[0] = '\0';
    }

    strcat(whole, base);
    strcat(whole, aim);
    strcat(whole, force);
    strcat(whole, out);
    strcat(whole, "\n");
    return whole;
}

/**
 * Send thishappened message to all players 
 * And print message to stdout
 * take no paramater
 * return nothing
 */
void print_this_happened(void) {
    int i;
    char* state = translate_state_message();
    char* hubMesg = malloc(sizeof(char) * 40);
    strcpy(hubMesg, "thishappened ");
    strcat(hubMesg, stateMesg);
    for(i = 0; i < numPlayers; i++) {
        fprintf(writeFiles[i], hubMesg, 23);
        fflush(writeFiles[i]);
    }
    fprintf(stdout, state);
    fflush(stdout);
    free(state);
    free(hubMesg);
}

/**
 * Once user discard card 5, enbale target user get a new card from deck
 * It takes target player name and current player index as parameters. 
 * return nothing.
 */
void process_new_card(char target, char whichPlayer) {
    int index = index_of_player(target);
    struct Player *player = players[index];
    int newCard = currentDeck->card[indexOfCard + 1];
    stateMesg[5] = target;
    stateMesg[6] = player->onHand[0] + '0'; //discard 
    if(player->onHand[0] == 8) {
        player->isOut = TRUE;
        stateMesg[7] = target;
    }
    if(player->onHand[0] == 4) {
        player->isProtected = TRUE;
    }
    if(indexOfCard != 16) {
        player->onHand[0] = currentDeck->card[indexOfCard + 1];
        indexOfCard++;
    } else { // get card which put aside at beginning
        player->onHand[0] = currentDeck->card[0];
    }
    fprintf(writeFiles[index], "replace %d\n", newCard);
    fflush(writeFiles[index]);
}

/**
 * Send replace message to players
 * take current player index and target player name as parameter
 * return nothing.
 */
void process_swap(char target, int whichPlayer) {
    int targetIndex = index_of_player(target);
    struct Player *player = players[whichPlayer];
    struct Player *other = players[targetIndex];

    int card = player->onHand[0];
    int targetCard = other->onHand[0];
    fprintf(writeFiles[whichPlayer], "replace %d\n", targetCard);
    fprintf(writeFiles[targetIndex], "replace %d\n", card);
    fflush(writeFiles[whichPlayer]);
    fflush(writeFiles[targetIndex]); 
    player->onHand[0] = targetCard;
    other->onHand[0] = card;   
}

/**
 *Once user discard card 3, start comparing cards with target
 *take target player name and current player index
 *return nothing
 */
void process_compare(char target, int whichPlayer) {
    int out;
    int index = index_of_player(target);
    int card = players[whichPlayer]->onHand[0];
    int targetCard = players[index]->onHand[0];
    int less;
    if (card > targetCard) {
        out = index;
        less = targetCard;
    } else {
        out = whichPlayer;
        less = card;
    }
    players[out]->isOut = TRUE;
    stateMesg[5] = names[out][0];
    stateMesg[6] = less + '0';
    stateMesg[7] = names[out][0];
}

/**
 *Once user discard card 6, realize the swap 
 *take target player name, guess card number 
 *and current player index as parameters
 *return noting
 */
void process_guess(char target, int guess, int whichPlayer) {
    int index = index_of_player(target); 
    int targetCard = players[index]->onHand[0];
    if(targetCard == guess) {
        players[index]->isOut = TRUE;
        stateMesg[5] = names[index][0];
        stateMesg[6] = targetCard + '0';
        stateMesg[7] = names[index][0];
    }
}

/**
 *Respond to card that a player discard
 *take the card number, taget player, guess card number 
 and current playr as parameters
 return nothing
 */
void respond_to_card(int card, char target, int guess, int whichPlayer) {
    switch(card) {
        case 8: //out of round
            players[whichPlayer]->isOut = TRUE;
            stateMesg[7] = names[whichPlayer][0];
            break;
        case 6: // swap card
            if(target != '-') {
                process_swap(target, whichPlayer);
            }
            break;
        case 5: //get a new card
            process_new_card(target, whichPlayer);
            break;
        case 4:
            players[whichPlayer]->isProtected = TRUE;
        case 3: //compare card
            if(target != '-') {
                process_compare(target, whichPlayer);
            }
            break;
        case 1: //guess
            if(target != '-') {
                process_guess(target, guess, whichPlayer);
            }
            break;
    }
}

/**
 * Update player card on hand after they dicard card
 * take the card number and current playr index as parameters
 * return nothing
 */
void update_player_card(int card, int whichPlayer) {
    int i, index;
    struct Player *player = players[whichPlayer];
    for(i = 0; i < 2; i++) {
        if(player->onHand[i] == card) {
            index = i;
            break;
        }
    }
    player->onHand[0] = player->onHand[1 - index];
    player->onHand[1] = 0;
}

/**
 *Parse the message send by player
 *take player message and current player index as parameters
 *return nothing
 */
void parse_player_message(char* message, int whichPlayer) {
    validate_player_message(message);
    int discard = message[0] - '0';
    char target = message[1];
    int guess = message[2] - '0';
    stateMesg[0] = names[whichPlayer][0];
    stateMesg[1] = message[0];
    stateMesg[2] = target;
    stateMesg[3] = message[2];
    update_player_card(discard, whichPlayer);
    respond_to_card(discard, target, guess, whichPlayer);
}

/**
 * Read player message
 * If hub can't read message from player, then print error message and exit
 * take and current player index and player message as parameters
 * return noting.
 */
void read_player(int whichPlayer, char* message) {
    if (fgets(message, 5, readFiles[whichPlayer]) == NULL) {
        exit_hub(PLAYERQUIT);
    }
}

/**
 * Send yourturn message to current player
 * takes the current player index as only parameter
 * return nothing
 */
void your_turn(int whichPlayer) {
    int card = currentDeck->card[indexOfCard + 1];
    struct Player *player = players[whichPlayer];
    indexOfCard++; 
    player->isProtected = FALSE;
    player->onHand[1] = card; 
    fprintf(writeFiles[whichPlayer], "yourturn %d\n", card);
    fflush(writeFiles[whichPlayer]);
    cardCounts[card] -= 1;
    stateMesg[5] = '-';
    stateMesg[6] = '-';
    stateMesg[7] = '-';
}

/**
 * Detect if game is over
 * If the game is over, change global variable isGameOver to TRUE
 * take no parameters.
 * return nothing.
 */
void is_game_over(void) {
    int i, t, numWinners;
    int winners[4];
    char winnerNames[5];
    numWinners = 0;
    for(i = 0, t = 0; i < numPlayers; i++) {
        if(players[i]->scores == 4) {
            winners[t] = i;
            numWinners++;
            t++;
        }
    }
    if (numWinners > 0) {
        for(i = 0; i < numWinners; i++) {
            winnerNames[i] = names[winners[i]][0];
        }   
        winnerNames[numWinners] = '\0';
        fprintf(stdout, "Winner(s): %s\n", winnerNames);
        fflush(stdout);
    /* send winners to all players*/
        for(i = 0; i < numPlayers; i++) {
            fprintf(writeFiles[i], "gameover\n");
            fflush(writeFiles[i]);
        }   
        exit_hub(OVER);
    }
}

/**
 * Find the round winners 
 * take no parameter
 * return struct RouondResult
 */
struct RoundResult* find_round_winners(void) {
    int i, numSurvivors, onHandMax, max, numWinners;
    int survivorCards[4], survivors[4];
    char* winners = malloc(sizeof(char) * 8);
    struct RoundResult *result = malloc(sizeof(*result));

    for(i = 0, numSurvivors = 0; i < numPlayers; i++) {
        struct Player *player = players[i];
        if(player->isOut == TRUE) {
            continue;
        }

        if(player->onHand[0] > player->onHand[1]) {
            onHandMax = player->onHand[0];
        } else {
            onHandMax = player->onHand[1];
        }
        survivorCards[numSurvivors] = onHandMax;
        survivors[numSurvivors] = i;
        numSurvivors++;
    }
    
    max = survivorCards[0];
    for(i = 0; i < numSurvivors; i++) {
        if(survivorCards[i] > max) {
            max = survivorCards[i];
        }
    }
    result->max = max;
    for(i = 0, numWinners = 0; i < numSurvivors; i++) {
        struct Player *player = players[survivors[i]];
        if(player->onHand[0] == max || player->onHand[1] == max) {
            numWinners++;
            winners[i] = player->name;
        }
    }
    winners[numWinners] = '\0';
    result->winners = winners;
    return result;
}

/**
 *Print the round winner message to stdout
 *take struct RoundResult as a parameter
 *return nothing
*/
void print_round_winner(struct RoundResult* result) {
    int i, len, max;
    char* names = malloc(sizeof(char) * 7);
    char winnerName[2];
    len = strlen(result->winners);
    for(i = 0; i < len; i++) {
        winnerName[0] = result->winners[i];
        if(i == 0) {
            strcpy(names, " ");
        } else {
            strcat(names, " ");
        }
        strcat(names, winnerName);   
    }
    names[len + 1] = '\0';
    max = result->max;
    fprintf(stdout, "Round winner(s) holding %d:%s\n", max, names);
    fflush(stdout); 
    
    free(names);
    free(result);
}

/**
 * To detect if the round is end 
 * If round is end, change global variable isRoundEnd to TRUE
 * It takes no parameter
 * return nothing. 
 */
void is_round_end(void) {
    int i, count;
    if(indexOfCard + 1 == 17) {
        isRoundEnd = TRUE;
    }
    for(i = 0, count = 0; i < numPlayers; i++) {
        if(players[i]->isOut == TRUE) {
            count++;
        }
    }
    if(count == (numPlayers - 1)) {
        isRoundEnd = TRUE;
    }
}
/**
 * Calculate the scores of each players
 * and send scores i j.. message to all players
 * take struct RoundResult as parameter
 * return nothing
 */
void evaluate_scores(struct RoundResult* result) {
    int i, index; 
    int len = strlen(result->winners);
    char* scoresMesg = malloc(sizeof(char) * 14);
    strcpy(scoresMesg, "scores");
    char digit[2];
    digit[1] = '\0';
    for(i = 0; i < len; i++) {
        char name = result->winners[i];
        index = index_of_player(name);
        players[index]->scores += 1;
    }
    
    for(i = 0; i < numPlayers; i++) {
        digit[0] = players[i]->scores + '0';
        strcat(scoresMesg, " ");
        strcat(scoresMesg, digit); 
    }
    strcat(scoresMesg, "\n");
    for(i = 0; i < numPlayers; i++) {
        fprintf(writeFiles[i], scoresMesg);
        fflush(writeFiles[i]);
    }
    free(scoresMesg);
}

/**
 * caller to play game
 * take the indexo of current player as parameter
 * return nothing
 */
void play_game(int whichPlayer) {
    char* message = malloc(sizeof(char) * 5);
    your_turn(whichPlayer);
    read_player(whichPlayer, message);
    parse_player_message(message, whichPlayer);
    print_this_happened();
    
    is_round_end();
    if(isRoundEnd) {
        struct RoundResult *result = find_round_winners();
        print_round_winner(result);
        evaluate_scores(result);
        is_game_over();
    }
    free(message);
}

/**
 * Choose deck from decks
 * take no parameters
 * return struct Deck*
 */
struct Deck* chose_deck (void) {
    int i = 0;
    struct Deck* deck = decks;
    while(1) {
        if(i == roundNo) {
            return deck;
        } 
        deck = deck->prev;
        if(deck == NULL) {
            deck = decks;
        }
        i++; 
    }
}

/*start new round 
 * send newround message to child processes
 * takes no paramter
 * return nothing
*/
void start_new_round(void) {
    int i;
    roundNo++; 
    indexOfCard = 0;
    isRoundEnd = FALSE;
    currentDeck = chose_deck(); 
    stateMesg[8] = '\n';
    stateMesg[9] = '\0';
    for(i = 0; i < 8; i++) {
        if(i == 4) {
            stateMesg[i] = '/';
        } else {
            stateMesg[i] = '-';
        }
    }
    for(i = 0; i < numPlayers; i++) {
        int card = (currentDeck->card)[i + 1];
        players[i]->onHand[0] = 0;
        players[i]->onHand[1] = 0;
        players[i]->isOut = FALSE;
        players[i]->onHand[0] = card; // update player's on hand card
        fprintf(writeFiles[i], "newround %d\n", card);
        fflush(writeFiles[i]);
    }
    indexOfCard = numPlayers;
}

/**
 * Initalize few variables
 * takes no parameters
 * return nothing.
 */
void init_game(void) {
    int i;
    roundNo = -1;
    isGameover = FALSE;
    isRoundEnd = TRUE;
    stateMesg[8] = '\n';
    stateMesg[9] = '\0';
    for(i = 0; i < 8; i++) {
        if(i == 4) {
            stateMesg[i] = '/';
        } else {
            stateMesg[i] = '-';
        }
    }
    /*init players*/
    for(i = 0; i < numPlayers; i++) {
        struct Player *player = malloc(sizeof(*player));
        player->name = names[i][0];
        player->scores = 0;
        player->isOut = FALSE;
        player->isProtected = FALSE;   
        players[i] = player;
    }
}


/**
 * Close all files opened for pipe communication
 * It takes numbers of players as parameter
 * return nothing
 */
void close_files(int num) {
    int i;
    for(i = 0; i < num; i++) {
        fclose(writeFiles[i]);
        fclose(readFiles[i]);
    }
}
/**
 * When hub received SIGINT, kill all child processes and then exit
 * take the singal number as paramters
 * return nothing
 */
void handle_sigint(int signum) {
    int i;
    for(i = 0; i < numPlayers; i++) {
        kill(pids[i], SIGKILL);
    }
    exit_hub(SIGNAL);
}

int main(int argv, char*argc[])
{
    validate_parameters(argv, argc);
    numPlayers = argv - 2;
    int whichPlayer;
    decks = read_deck(argc[1]);
    init_game(); 
    struct PipePair* pipes[numPlayers]; 
    struct sigaction sa;
    struct sigaction saPipe; 
    memset(&sa, 0, sizeof(sa));
    memset(&saPipe, 0, sizeof(saPipe));
    sa.sa_handler = handle_sigint; 
    saPipe.sa_handler = SIG_IGN;
    
    sigaction(SIGPIPE, &saPipe, NULL);
    
    whichPlayer = 0;
    open_pipes(pipes, numPlayers);
    fork_children(numPlayers, argc, pipes);  
    sigaction(SIGINT, &sa, NULL);
    int i;
    for(i = 0; i < numPlayers; i++) {
        if(fgetc(readFiles[i]) != '-') {
            exit_hub(STARTPROCESS);
        }
    }
    
    while(!isGameover) {
        struct Player* player = players[whichPlayer];
        if(isRoundEnd) {
            start_new_round();
            whichPlayer = 0;
        }
        if(player->isOut == TRUE) {
            whichPlayer++;
            if(whichPlayer == numPlayers) {
                whichPlayer = 0;
            }
            continue;
        }
        play_game(whichPlayer);
        whichPlayer++;
        if(whichPlayer == numPlayers) {
            whichPlayer = 0;
        }
    } 
    close_files(numPlayers);
    return 0;
}

/**
 * create muliple pipes for IPC
 * take two parameeters. One is array of struct PipePair.
 * The second parameter is number of players
 */
void open_pipes(struct PipePair* pipes[], int num) {
    int i;
    for(i = 0; i < num; i++) {
        struct PipePair *pair = malloc(sizeof(*pair));
        if(pipe(pair->write) == -1) {
            exit_hub(STARTPROCESS);
        }
        if(pipe(pair->read) == -1) {
            exit_hub(STARTPROCESS);
        }
        pipes[i] = pair;
    } 
}

/**
 * Fork muliple child processes
 * Take three parameters.The first one is number of players
 * The second parameter is 
 * The third parameter is array of struct PipePair
 * If fork fail, print error message to stderr and exit.
 * If exec fail, print error message to stder and exit.
 */
void fork_children(int num, char* argc[], struct PipePair* pipes[]) {
    int i, t;
    char numchar[2];
    pid_t pid;
    FILE* nullFile = fopen("/dev/null", "w"); 
    int nullNo = fileno(nullFile);
    for(i = 0; i < num; i++) {
        pid = fork();
        if(pid < 0) {
            exit_hub(STARTPROCESS);
        }
        if(pid == 0) { //child process
            for(t = 0; t < num; t++) {
                if(t == i) {
                    continue;
                }
                close(pipes[t]->write[READ_END]);
                close(pipes[t]->write[WRITE_END]);
                close(pipes[t]->read[READ_END]);
                close(pipes[t]->read[WRITE_END]);
            }
            close(pipes[i]->read[WRITE_END]);
            dup2(pipes[i]->read[READ_END], STDIN_FILENO);
            close(pipes[i]->read[READ_END]);

            close(pipes[i]->write[READ_END]);
            dup2(pipes[i]->write[WRITE_END], STDOUT_FILENO);
            close(pipes[i]->write[WRITE_END]);
            
            dup2(nullNo, STDERR_FILENO);
            fclose(nullFile);
            snprintf(numchar, 2, "%d", num);
            numchar[1] = '\0';
            execl(argc[2 + i], argc[2 + i], numchar, names[i], NULL);
            exit_hub(STARTPROCESS);
        } else {
            pids[i] = pid;
            close(pipes[i]->write[WRITE_END]);
            close(pipes[i]->read[READ_END]);
            FILE *f = fdopen(pipes[i]->read[WRITE_END], "w");
            writeFiles[i] = f;
            FILE *f2 = fdopen(pipes[i]->write[READ_END], "r");
            readFiles[i] = f2;
        }
    }
}

