# Architecture

The browser calls a small Winsock2 HTTP server. `server.c` routes requests to focused business modules. `TournamentStore` is a singly linked list, appropriate because tournaments are created/deleted dynamically. Each tournament uses dynamic arrays for teams and matches because indexed traversal is useful for scheduling and tables. `storage.c` is the only module that performs file I/O.
