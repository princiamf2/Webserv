*This project has been created as part of the 42 curriculum by mm-furi, nicolsan and malapoug.*

---

# Webserv

## Description

Webserv project is about writing our own HTTP server, handling multiple clients on multiple servers at the same time.
All built by a `Makefile` and simply launched by a command with a configuration file

---

## Instructions

### Prerequisites

- `make`
- `c++ 98`

### Setup

1. Clone the repository:

```bash
git clone https://github.com/Loufok0/Webserv.git
cd Webserv
```

2. Configure your config file (full details bellow)

3. Build and start the infrastructure:

```bash
make
```

4. Access the site at `http://localhost:PORT` (if you want to have access by an URL like `https://webserv.com`, you may want to add `127.0.0.1  webserv.com` to `/etc/hosts`).

### Makefile Targets
| Target | Description |
|--------|-------------|
| `make` | Create the executable webserv compiled with `-Wall -Wextra -Werror -std=c++98` |
| `make clean` | Remove all the objects (files.o) |
| `make fclean` | `clean` + remove executable |
| `make re` | `fclean` + full rebuild |
---

## Overview

The Webserv project is a lightweight and efficient web server designed to serve static and dynamic content with high performance and low resource consumption. The primary goals of the project include:

- **Performance:** Optimized for speed and responsiveness, ensuring quick load times for users.
- **Simplicity:** A straightforward configuration and deployment process, catering to both developers and system administrators.
- **Flexibility:** Capable of handling various content types and configurable to meet different operational needs.

### Key Features:
- Static file serving with support for caching.
- Dynamic request handling using server-side includes or scripts.
- Modular architecture allowing for easy extension and integration of additional features.

### Language Composition

This project is built with multiple technologies:

| Language | Percentage |
|----------|-----------|
| C++ | 44.8% |
| HTML | 28.4% |
| CSS | 15.5% |
| JavaScript | 8.9% |
| Makefile | 1.2% |
| Shell | 0.6% |
| Other | 0.6% |

The core server logic is implemented in C++98 standard, with a web-based interface built using HTML, CSS, and JavaScript.

### Design Choices

**Multiplexing with poll()** — We chose `poll()` over `select()` for better scalability and resource management when handling multiple client connections simultaneously.

**C++98 Compliance** — Strict adherence to C++98 standard ensures maximum compatibility and aligns with 42 school curriculum requirements.

**Configuration-Driven Server** — The server reads a configuration file at startup, allowing easy customization of ports, server blocks, routes, and CGI settings without recompilation.

**Non-Blocking I/O** — All socket operations are non-blocking to prevent thread stalls and ensure responsive handling of multiple clients.

**Modular Architecture** — Core networking logic is separated from HTTP request/response handling, making the codebase maintainable and testable.

## Screenshots
<img src="https://github.com/Loufok0/Webserv/blob/main/ressources/Welcome.png" width="400">
<img src="https://github.com/Loufok0/Webserv/blob/main/ressources/Webserv_home.png" width="400">


## Resources

### Documentation
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/)
- [poll](https://man7.org/linux/man-pages/man2/poll.2.html)
- [bind](https://cplusplus.com/reference/functional/bind/)
- [fcntl](https://man7.org/linux/man-pages/man2/fcntl.2.html)
- [HTTP guides](https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides)
- Hypertext Transfer Protocol (HTTP/1.1)
  - [Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
  - [ Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [HTTP Semantics](https://datatracker.ietf.org/doc/html/rfc9110)
- [The Common Gateway Interface (CGI) Version 1.1](https://datatracker.ietf.org/doc/html/rfc3875)
- [Parsing inspo](https://nginx.org/en/docs/beginners_guide.html)

### AI Usage
AI (Claude by Anthropic) was used during this project for the following tasks:

- **Debugging issues** — as learning new thing, we've run into some issues, that we sometimes did not found response online.
- **Poll configuration** — same, as before, while learning, we sometime had to ask for explanations as we didn't undestand the documentation really well.
- **Comparisons and explanations** — clarifying socket, bind, cgis concepts.
- **HTML, JavaScript and CSS** — As the goal of this 42 projects was not to learn frontend tools, we used ai for a faster creations of this part.

AI was not used to write the the project from scratch — all configuration decisions were made and validated manually.

