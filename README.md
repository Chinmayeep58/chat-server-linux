# TCP Client with Real-Time Chat & File Transfer
This is a C-based interactive TCP client that connects to a server at `127.0.0.1:9999` and supports:

- Live, character-by-character chat
- Backspace and prompt handling
- Sending and receiving files using Base64 encoding
- Text preview of received files

---

## Features

-  Real-time chat input/output
-  Custom line-based protocol
-  File sending (`/send <path>`)
-  File receiving with preview if it's a text file
-  Terminal raw mode input (non-blocking, no echo)

---

## 🔧 Compilation

Make sure you have `gcc` installed.

```bash
gcc client.c -o client -Wall -Wextra
````

---

## 🖥️ Running the Client

Start a compatible server **first** (must listen on `127.0.0.1:9999`).

Then run the client:

```bash
./client
```

If successful, you'll see:

```
Connected to server.
[HH:MM] Client:
```

---

## 📤 Sending Files

Inside the client, type:

```
/send path/to/your/file.txt
```

Then press **Enter** to send the file to the server.

---

## 📥 Receiving Files

When a file is received:

* It is saved as `received_<filename>`.
* If it's mostly printable text, a preview will be shown in the terminal.


