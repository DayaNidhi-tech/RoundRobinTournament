# Round-robin algorithm

`scheduler.c` uses the circle method. Team IDs are placed in an array; an odd team count receives an internal ID `0` as a virtual BYE. Each round pairs opposite entries, then rotates every entry except the first. BYE pairings are retained as display-only matches. The validator checks round count, expected pair count, duplicate pairings, and same-round conflicts.
