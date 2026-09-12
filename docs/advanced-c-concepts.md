# Advanced C concepts used

- **Structures/enums:** model tournaments, teams, matches, statuses and standings.
- **Pointers/dynamic memory:** dynamically sized arrays use `realloc`; standings use temporary heap memory.
- **Linked list:** `TournamentNode` supports insertion, traversal, searching, deletion and cleanup for a changing tournament collection.
- **Modular programming:** each `.c` module owns one responsibility behind a header.
- **File handling:** `storage.c` reads/writes documented binary records with magic validation.
- **Sorting:** `qsort` applies points, goal difference, goals scored, and name tie-breakers.
- **Sockets:** Winsock2 serves HTTP and static frontend files on localhost.
