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

## Project Description

### Docker Usage and Included Sources

---

### Design Choices

---

### Virtual Machines vs Docker

---

### Docker Network vs Host Network

---

### Docker Volumes vs Bind Mounts

---

## Resources

### Documentation
- [Core](https://github.com/Loufok0/Webserv/tree/main/Core)
    - [poll](https://man7.org/linux/man-pages/man2/poll.2.html)
    - [bind](https://cplusplus.com/reference/functional/bind/)
    - [fcntl](https://man7.org/linux/man-pages/man2/fcntl.2.html)

### AI Usage
AI (Claude by Anthropic) was used during this project for the following tasks:

- **Debugging issues** — as learning new thing, we've run into some issues, that we sometimes did not found response online.
- **Poll configuration** — same, as before, while learning, we sometime had to ask for explanations as we didn't undestand the documentation really well.
- **Comparisons and explanations** — clarifying socket, bind, cgis concepts.

AI was not used to write the the project from scratch — all configuration decisions were made and validated manually.

