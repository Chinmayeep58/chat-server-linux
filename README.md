# chat-server

## Overview

This is a simple **TCP chat program in C** using sockets and `poll()`.
It consists of a **server** and a **client** that can exchange messages interactively.

---

## Files

* `server.c` – waits for a client and relays messages.
* `client.c` – connects to the server and exchanges messages.

---

## Build

```bash
gcc server.c -o server
gcc client.c -o client
```

---

## Run

1. Start the server:

   ```bash
   ./server
   ```
2. In another terminal, start the client:

   ```bash
   ./client
   ```

Type messages in either terminal — they will be sent to the other side.

---

## Notes

* Runs on port **9999**.
* Handles **one client** at a time.
* Messages are up to 255 bytes.

