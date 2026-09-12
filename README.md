# Round Robin Tournament Scheduler

A full local tournament-management system for Advanced C Programming. The browser UI is vanilla HTML/CSS/JavaScript; the C11 backend is the source of truth for tournaments, teams, fixtures, results, standings, persistence, JSON, and HTTP.

## Build and run (Windows / MinGW)

```powershell
mingw32-make
.\tournament_hub.exe
```

Open http://localhost:8080. If that port is occupied, run `.\tournament_hub.exe 8091` and open http://localhost:8091. Run automated tests with `mingw32-make test`.

The server is intentionally a compact local-learning server: it handles one request per connection, has basic HTTP/JSON parsing, and binds only to localhost. It is not designed for public deployment.

## Features

- Multiple tournaments stored in a linked list
- Team CRUD with safe protection after fixtures are generated
- Deterministic circle-method schedules for even and odd team counts
- BYE fixtures, result entry, C-calculated football standings
- Binary save/load at `data/tournaments.bin`
- REST-like JSON API and a responsive professional dashboard

See `docs/` for architecture, algorithm, API, testing and Advanced C explanations.
