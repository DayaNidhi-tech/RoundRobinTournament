CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Ibackend/include
SRC = backend/src/main.c backend/src/tournament.c backend/src/team.c backend/src/scheduler.c backend/src/match.c backend/src/standings.c backend/src/storage.c backend/src/json.c backend/src/server.c
CORE = backend/src/tournament.c backend/src/team.c backend/src/scheduler.c backend/src/match.c backend/src/standings.c backend/src/storage.c backend/src/json.c
all: tournament_hub.exe
tournament_hub.exe: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lws2_32
test: tests.exe
	./tests.exe
tests.exe: backend/tests/test_all.c $(CORE)
	$(CC) $(CFLAGS) -o $@ backend/tests/test_all.c $(CORE)
clean:
	del /Q tournament_hub.exe tests.exe 2>NUL || exit 0
