# API

## Authentication and user access

- `GET /api/auth/status` reports whether the first administrator must be created.
- `POST /api/auth/setup` creates the first administrator and signs them in.
- `POST /api/auth/login` signs in with a username and password.
- `GET /api/auth/me` returns the signed-in account.
- `POST /api/auth/logout` invalidates the current session.
- `DELETE /api/auth/reset` deletes the signed-in account and clears all saved tournaments and users so a fresh setup can begin.
- `GET/POST /api/users` lists accounts or creates an account (administrator only).
- `PUT/DELETE /api/users/{id}` updates or removes an account (administrator only).

Send the returned session token as `Authorization: Bearer <token>` on protected API requests. Administrators can change tournament data and manage accounts. View-only users may access `GET` tournament routes but cannot change data or access the user-management API. At least one administrator account must remain.

- `GET/POST /api/tournaments`
- `GET/PUT/DELETE /api/tournaments/{id}`
- `GET/POST /api/tournaments/{id}/teams`
- `PUT/DELETE /api/tournaments/{id}/teams/{teamId}`
- `POST /api/tournaments/{id}/schedule/generate`
- `GET /api/tournaments/{id}/schedule`, `/matches`, `/standings`
- `PUT /api/tournaments/{id}/matches/{matchId}/result`
- `POST /api/tournaments/{id}/save` and `/load`

Bodies and responses are JSON. Successful responses include `success: true`. Account passwords are stored as salted PBKDF2-SHA256 hashes in `data/users.bin`.
