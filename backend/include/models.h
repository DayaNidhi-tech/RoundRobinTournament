#ifndef MODELS_H
#define MODELS_H

#include <stddef.h>
#define NAME_LEN 96
#define TEXT_LEN 256

typedef enum { TOURNAMENT_DRAFT, TOURNAMENT_ACTIVE, TOURNAMENT_COMPLETED } TournamentStatus;
typedef enum { MATCH_UPCOMING, MATCH_COMPLETED, MATCH_BYE } MatchStatus;
typedef struct { int id; char name[NAME_LEN]; char city[NAME_LEN]; char logo[TEXT_LEN]; } Team;
typedef struct { int id, round, home_id, away_id, home_score, away_score; MatchStatus status; } Match;
typedef struct { int team_id, played, wins, draws, losses, gf, ga, gd, points; char name[NAME_LEN]; } Standing;
typedef struct {
    int id; char name[NAME_LEN]; char description[TEXT_LEN]; TournamentStatus status;
    Team *teams; size_t team_count, team_capacity;
    Match *matches; size_t match_count, match_capacity;
    int next_team_id, next_match_id, rounds;
} Tournament;
typedef struct TournamentNode { Tournament tournament; struct TournamentNode *next; } TournamentNode;
typedef struct { TournamentNode *head; int next_tournament_id; } TournamentStore;
#endif
