# Redis-Clone-Cpp ![Redis](https://img.shields.io/badge/Redis-D92B3A?style=for-the-badge&logo=redis&logoColor=white)
 
> A from-scratch Redis implementation in C++ — raw sockets, real threads, zero dependencies on Redis itself.
 
Built over TCP/IPv4 using POSIX sockets and `std::thread`, this project reimplements the core of Redis from the ground up. It speaks the **RESP2 wire protocol**, meaning any standard `redis-cli` or Redis client library connects to it out of the box.
 
---
 
## Features
 
- **RESP2 protocol** — fully compatible with `redis-cli` and standard Redis clients
- **Multi-client** — each connection is handled in a detached `std::thread`
- **Concurrent list operations** — `BLPOP` uses `std::mutex` + `std::condition_variable` for safe blocking across threads
- **Stream support** — append-only streams with monotonic ID validation
- **TTL expiry** — millisecond-precision key expiration via `std::chrono`
---
 
## Getting Started
 
```bash
# Build
g++ -std=c++17 -pthread -o redis-clone main.cpp
 
# Run
./redis-clone
 
# Connect (in another terminal)
redis-cli -p 6379
```
 
---
 
## Supported Commands
 
### General
 
| Command | Syntax | Description |
|---|---|---|
| `PING` | `PING` | Returns `PONG`. Used to test connectivity. |
| `ECHO` | `ECHO <message>` | Returns the given message as a bulk string. |
| `TYPE` | `TYPE <key>` | Returns the type of the value stored at a key: `string`, `list`, `stream`, or `none`. |
 
---
 
### Strings
 
| Command | Syntax | Description |
|---|---|---|
| `SET` | `SET <key> <value> [EX seconds \| PX milliseconds]` | Stores a string value. Optionally set expiry in seconds (`EX`) or milliseconds (`PX`). |
| `GET` | `GET <key>` | Returns the value at key, or `nil` if expired / not found. |
 
**Examples**
```
SET name "alice"
SET session "abc123" EX 60
GET name       → "alice"
GET session    → "abc123" (or nil if expired)
```
 
---
 
### Lists
 
| Command | Syntax | Description |
|---|---|---|
| `RPUSH` | `RPUSH <key> <value> [value ...]` | Appends one or more values to the tail of a list. Returns new list length. |
| `LPUSH` | `LPUSH <key> <value> [value ...]` | Prepends one or more values to the head of a list. Returns new list length. |
| `LPOP` | `LPOP <key> [count]` | Removes and returns element(s) from the head of the list. |
| `LLEN` | `LLEN <key>` | Returns the length of the list. |
| `LRANGE` | `LRANGE <key> <start> <stop>` | Returns a range of elements. Supports negative indices (`-1` = last element). |
| `BLPOP` | `BLPOP <key> <timeout>` | Blocking pop — waits up to `timeout` seconds for an element. `0` blocks indefinitely. Woken up by `RPUSH`/`LPUSH` from another client. |
 
**Examples**
```
RPUSH queue "job1" "job2"   → 2
LPUSH queue "job0"          → 3
LRANGE queue 0 -1           → ["job0", "job1", "job2"]
LPOP queue                  → "job0"
BLPOP queue 5               → blocks up to 5s, returns next pushed element
```
 
---
 
### Streams
 
Streams are append-only logs. Each entry has a unique `<millisecondsTime>-<sequenceNumber>` ID and one or more field-value pairs.
 
| Command | Syntax | Description |
|---|---|---|
| `XADD` | `XADD <key> <id> <field> <value> [field value ...]` | Appends a new entry to a stream. The ID must be strictly greater than the last entry's ID. Returns the entry ID on success. |
 
**ID validation rules:**
- `time` component must be ≥ last entry's `time`
- If `time` is equal, `seq` must be strictly greater
- Violating either returns: `-ERR The ID specified in XADD is equal or smaller than the target stream top item`
**Examples**
```
XADD events 1-1 action "login" user "alice"   → "1-1"
XADD events 1-2 action "click"               → "1-2"
XADD events 1-2 action "duplicate"           → ERR (equal ID)
XADD events 0-9 action "past"               → ERR (smaller ID)
```
 
---
 
## Architecture Notes
 
- ** Boost.Asio** — networking is raw POSIX (`socket`, `bind`, `listen`, `accept`, `recv`, `send`)
- **Parser** — a simple `\r\n`-delimited tokenizer that handles RESP2 arrays
- **Thread model** — one `std::thread` per client, detached; shared state protected by `std::mutex`
- **BLPOP wake-up** — `RPUSH`/`LPUSH` directly checks the waiter list under the same mutex and sends the response to the blocked client's `fd`
---
 
## Limitations
 
- No persistence (RDB/AOF)
- No replication
- No `XREAD`, `XRANGE`, or other stream query commands yet
- Single-buffer recv (max 1023 bytes per command — pipelining not supported)
- No AUTH or ACL
