# Testing

`backend/tests/test_all.c` runs scheduler checks for 2–8 teams, including expected rounds/matches, duplicate prevention, same-round conflicts and BYE behavior through `schedule_validate`. It also checks invalid match IDs, standings points, and binary save/load.

Run: `mingw32-make test`.
