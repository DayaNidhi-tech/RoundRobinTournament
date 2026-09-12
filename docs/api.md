# API

- `GET/POST /api/tournaments`
- `GET/PUT/DELETE /api/tournaments/{id}`
- `GET/POST /api/tournaments/{id}/teams`
- `PUT/DELETE /api/tournaments/{id}/teams/{teamId}`
- `POST /api/tournaments/{id}/schedule/generate`
- `GET /api/tournaments/{id}/schedule`, `/matches`, `/standings`
- `PUT /api/tournaments/{id}/matches/{matchId}/result`
- `POST /api/tournaments/{id}/save` and `/load`

Bodies and responses are JSON. Successful responses include `success: true`.
