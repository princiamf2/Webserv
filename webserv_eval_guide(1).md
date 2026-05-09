# 🖥️ WEBSERV — Guide d'Évaluation

## Architecture générale

```
socket() → bind() → listen()
    ↓
poll() ← boucle principale (Core::runPoll)
    ↓
POLLIN  (serveur)     → acceptClient()
POLLIN  (client)      → readClient() → parse → traite → writeBuf
POLLOUT (client)      → writeClient() → send()
POLLOUT (CGI stdin)   → CgiManager::writeInput()
POLLIN  (CGI stdout)  → CgiManager::readOutput()
POLLHUP (CGI stdout)  → finalizeCgi() → réponse HTTP
```

| Fichier | Rôle |
|---|---|
| `srcs/Core/Core.cpp` | Boucle `poll()`, dispatch |
| `srcs/Core/Server.cpp` | `readClient`, `writeClient`, `finalizeCgi` |
| `srcs/HTTPRequest/CgiManager.cpp` | fork, pipes, execve, chdir |
| `srcs/Parsing/` | Config + requêtes HTTP |
| `configs/Core.config` | Config principale |

```bash
make && ./webserv configs/Core.config
```

---

## Vérifications de code

### Q1 — Bases d'un serveur HTTP

1. `socket` + `bind` + `listen` → `Server::init()`
2. `accept` → `Core::acceptClient()`
3. `recv` → `Server::readClient()` → `HttpParser::parseRequest`
4. `RequestHandler::resolveAction` → `ActionRequest`
5. `send` → `Server::writeClient()`

```bash
./webserv configs/Core.config && curl -i http://localhost:8080/
```

---

### Q2 — Mécanisme d'événements : `poll()`

```bash
grep -n "poll(" srcs/Core/Core.cpp
# → poll(_pollFds.data(), _pollFds.size(), 1);
```

- `poll()` (pas `select` ni `epoll`), une seule boucle `runPoll()`
- `_pollFds` contient : sockets serveur, sockets clients, pipes CGI stdin/stdout
- Timeout 1 ms pour vérifier les timeouts entre chaque itération

---

### Q3 — Comment fonctionne `poll()`

```c
int ret = poll(_pollFds.data(), _pollFds.size(), 1);
if (ret == -1) { /* errno == EINTR → continue */ }
if (ret == 0)  { continue; }  // timeout
// revents & POLLIN          → lire / accepter
// revents & POLLOUT         → écrire
// revents & (POLLERR|POLLNVAL) → fermer
// revents & POLLHUP         → CGI terminé → finalizeCgi
```

---

### Q4 — Un seul `poll()` pour tout gérer ⚠️

```bash
grep -rn "poll(" srcs/
# → une seule occurrence dans srcs/Core/Core.cpp
```

Depuis cette boucle unique :
- **POLLIN serveur** → `acceptClient()`
- **POLLIN client** → `readClient()`
- **POLLOUT client** → `writeClient()` (activé dynamiquement si `writeBuf` non vide)
- **POLLOUT CGI stdin** → `CgiManager::writeInput()`
- **POLLIN CGI stdout** → `CgiManager::readOutput()`
- **POLLHUP CGI stdout** → `finalizeCgi()`

---

### Q5 — `poll()` dans la main loop, POLLIN + POLLOUT simultanés ⚠️ note 0 si absent

```bash
grep -n "poll(" srcs/Core/Core.cpp
grep -n "POLLIN\|POLLOUT" srcs/Core/Core.cpp
curl -v http://localhost:8080/
```

---

### Q6 — I/O uniquement après `revents`

```bash
grep -n "revents" srcs/Core/Core.cpp
```

Tout `recv/send/accept/read/write` est gardé par `if (revents & POLLIN)` ou `if (revents & POLLOUT)`.

---

### Q7 — Erreur sur socket → client supprimé

```c
// readClient()  : recv(...) <= 0 → toClose = true
// writeClient() : send(...) <= 0 → toClose = true
// runPoll()     : POLLERR/POLLNVAL → closeClient(fd)
```

```bash
./webserv configs/Core.config
telnet localhost 8080   # fermer brutalement (Ctrl+]) → quit
curl -i http://localhost:8080/   # → 200 OK (serveur toujours actif)
```

---

### Q8 — Retour `-1` ET `0` gérés ⚠️

```bash
grep -n -A3 "recv(" srcs/Core/Server.cpp   # → bytes <= 0
grep -n -A3 "send(" srcs/Core/Server.cpp   # → bytes <= 0
```

- `recv ≤ 0` et `send ≤ 0` : les deux couverts
- `writeBuf.erase(0, bytes)` — seulement les octets réellement envoyés

---

### Q9 — `errno` interdit après I/O ⚠️ note 0 si violation

```bash
grep -rn "errno" srcs/          # → 1 seule ligne : errno == EINTR après poll()
grep -rn "EAGAIN\|EWOULDBLOCK" srcs/   # → aucun résultat
```

`errno` utilisé uniquement après `poll()` lui-même pour gérer `EINTR`. Jamais après `recv/send`.

---

### Q10 — Tout I/O déclenché depuis `poll()` ⚠️

```bash
grep -rn "recv\|send" srcs/Core/Server.cpp
grep -rn "CgiManager::writeInput\|CgiManager::readOutput" srcs/Core/Core.cpp
```

Chemin unique : `poll() → revents → handler`. Aucun I/O en dehors de ce chemin.

---

### Q11 — Fichiers disque synchrones OK

Config lue avant `runPoll()` : `main() → parseConfig() → Core::init() → runPoll()`.
Fichiers statiques petits, pas de blocage notable.

---

### Q12 — Compilation sans relinking

```bash
make && make          # → "Nothing to be done"
touch srcs/Core/Core.cpp && make   # → recompile seulement Core.cpp
```

---

## Configuration

### Q13 — Status codes HTTP

| Code | Cas |
|---|---|
| 200 | GET `/` |
| 201 | POST `/upload/` |
| 204 | DELETE `/upload/file` |
| 301 | `/redirect/` |
| 404 | URL inexistante |
| 405 | méthode interdite sur une route |
| 413 | body > `client_max_body_size` |
| 501 | méthode inconnue |

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/notfound
curl -i -X UNKNOWN http://localhost:8080/
curl -i -X DELETE http://localhost:8080/readonly/index.html
```

> Pour tester 413 : utiliser `configs/test.config` (limite 1 Mo) au lieu de `Core.config` (1 Go).

---

### Q14 — Page d'erreur personnalisée (404)

Config : `error_page 404 /errors/404.html;`

```bash
./webserv configs/Core.config
curl -i http://localhost:8080/doesnotexist
# → 404 + contenu de configs/www/errors/404.html
```

Pages définies : 400, 403, 404, 405, 413, 500, 501, 505.

---

### Q15 — `client_max_body_size`

Vérification en deux temps dans `readClient()` : sur `Content-Length` reçu, puis sur body accumulé.

```bash
# Avec configs/test.config (limite 1 Mo)
curl -i -X POST --data "hello" http://localhost:8080/upload/small.txt   # → 201
python3 -c "print('A'*2000000)" | curl -i -X POST --data-binary @- http://localhost:8080/upload/big.txt  # → 413
```

---

### Q16 — Routes vers différents dossiers

Blocs `location` dans `configs/Core.config` : `/`, `/browse/`, `/upload/`, `/cgi-bin/`, `/readonly/`, `/redirect/`.

```bash
curl -i http://localhost:8080/        # → configs/www/index.html
curl -i http://localhost:8080/browse/ # → listing configs/www/browse
curl -i http://localhost:8080/upload/ # → configs/www/upload/
```

---

### Q17 — Index par défaut

`index index.html;` dans le bloc `server`. Résolu dans `RequestHandler` : location → server → défaut `"index.html"`.

```bash
curl -i http://localhost:8080/       # → index.html
curl -i http://localhost:8080/docs/  # → docs/index.html
```

---

### Q18 — Méthodes autorisées par route

```bash
curl -X POST --data "test" http://localhost:8080/upload/test.txt          # → 201
curl -i -X DELETE http://localhost:8080/upload/test.txt                   # → 204
curl -i -X DELETE http://localhost:8080/readonly/index.html               # → 405
```

---

## Tests basiques HTTP

### Q19 — GET, POST, DELETE

```bash
curl -i http://localhost:8080/                                  # 200
curl -i -X POST --data "hello" http://localhost:8080/upload/test.txt   # 201
curl -i -X DELETE http://localhost:8080/upload/test.txt         # 204
```

---

### Q20 — Méthode inconnue → pas de crash

```bash
curl -i -X UNKNOWN http://localhost:8080/   # → 501
curl -i http://localhost:8080/              # → 200 (serveur toujours actif)
```

---

### Q21 — Status codes corrects

```bash
curl -i http://localhost:8080/                                  # 200
curl -i http://localhost:8080/notfound                         # 404
curl -i -X UNKNOWN http://localhost:8080/                      # 501
curl -i -X POST --data "x" http://localhost:8080/upload/x.txt  # 201
curl -i -X DELETE http://localhost:8080/upload/x.txt           # 204
```

---

### Q22 — Upload et récupération de fichiers

```bash
curl -i -X POST --data-binary @/etc/hostname http://localhost:8080/upload/test.txt  # → 201
curl -i http://localhost:8080/upload/test.txt   # → 200 + contenu
```

---

## CGI

### Q23 — CGI fonctionnel

Flux : `resolveAction → ACTION_START_CGI → startCgiForClient → CgiManager::startProcess`  
→ `fork` + pipes non bloquants → `registerCgi` → ajout à `_pollFds`  
→ `writeInput` (POLLOUT) → `readOutput` (POLLIN) → `finalizeCgi` (POLLHUP)

```bash
./webserv configs/Core.config
curl -i http://localhost:8080/cgi-bin/test.py   # → 200 + sortie script
```

---

### Q24 — CGI lancé dans son propre dossier (`chdir`)

Dans l'enfant du `fork()` : `chdir(scriptDir)` avant `execve`. → le script peut ouvrir ses fichiers relatifs.

```bash
grep -n "chdir" srcs/HTTPRequest/CgiManager.cpp
```

---

### Q25 — CGI GET et POST

```bash
curl -i http://localhost:8080/cgi-bin/test.py                          # GET → 200
curl -i -X POST --data "name=test" http://localhost:8080/cgi-bin/test.py  # POST → 200
```

---

### Q26 — CGI erreur → 500, pas de crash

`finalizeCgi()` → si `CgiManager::checkChild()` détecte une erreur → réponse 500.

```bash
curl -i http://localhost:8080/cgi-bin/slow.py   # → 500 si erreur
curl -i http://localhost:8080/                  # → 200 (serveur OK)
```

---

## Navigateur

### Q27 — Connexion navigateur

```
http://localhost:8080  →  F12 → Network
GET / → 200 OK
```

---

### Q28 — Headers requête et réponse

F12 → Network → cliquer la requête → Headers.  
Request : `Host`, `User-Agent`, `Connection`.  
Response : `HTTP/1.1 200 OK`, `Content-Type`, `Content-Length`.

---

### Q29 — Site statique complet

F12 → Network : `index.html`, `styles.css`, `script.js` → tous 200 OK.

---

### Q30 — URL incorrecte → 404

```bash
curl -i http://localhost:8080/thispagedoesnotexist   # → 404 personnalisé
```

---

### Q31 — Listing de dossier

```bash
curl -i http://localhost:8080/browse/    # → 200 + liste fichiers (show_directory true)
curl -i http://localhost:8080/noindex/   # → 403 (show_directory false, pas d'index)
```

---

### Q32 — Redirection

Config : `redirect_page 301 http://www.youtube.com/...;` dans `location /redirect/`.

```bash
curl -i http://localhost:8080/redirect/
# → 301 + Location: http://www.youtube.com/...
```

---

### Q33 — Tests libres

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/notfound
curl -i -X UNKNOWN http://localhost:8080/
curl -i http://localhost:8080/browse/
curl -i http://localhost:8080/cgi-bin/test.py
for i in {1..20}; do curl -s http://localhost:8080/ > /dev/null & done
```

---

## Ports et multi-serveurs

### Q34 — Plusieurs ports/interfaces

`configs/multi.config` : deux serveurs sur 8080 et 8081.

```bash
./webserv configs/multi.config
curl http://localhost:8080/   # → serveur 1
curl http://localhost:8081/   # → serveur 2
```

---

### Q35 — Même port, deux serveurs

Deux blocs `server` sur le même port → le second `bind()` échoue avec `"Bind returned -1"`.

```bash
# Terminal 1
./webserv configs/Core.config

# Terminal 2
./webserv configs/Core.config
# → "Bind returned -1"

curl -i http://localhost:8080/   # Terminal 1 toujours actif → 200
```

---

### Q36 — Deux processus webserv sur même port

```bash
# Terminal 1 : ./webserv configs/Core.config
# Terminal 2 : ./webserv configs/Core.config  → "Bind returned -1"
curl -i http://localhost:8080/   # Terminal 1 toujours actif → 200
```

---

## Stress test Siege

### Q37 — Test de charge

```bash
./webserv configs/Core.config
siege -b -c 20 -r 100 http://localhost:8080/
curl -i http://localhost:8080/   # → 200, aucun crash
```

---

### Q38 — Disponibilité > 99.5%

```bash
siege -b -c 50 -t 30s http://localhost:8080/
# Availability: 99.XX %   ← doit être > 99.5%
```

---

### Q39 — Pas de fuite mémoire

```bash
ps -o pid,rss,comm -p $(pgrep webserv)   # RSS stable sous charge
siege -b -c 20 -t 30s http://localhost:8080/
```

---

### Q40 — Pas de connexions pendantes

`clientTimedOut()` appelé dans `runPoll()` → ferme les clients inactifs via `closeClient()`.

```bash
./webserv configs/Core.config
telnet localhost 8080   # ne rien envoyer → attendre timeout
curl -i http://localhost:8080/   # → 200 OK
```

---

### Q41 — Siege indéfiniment sans redémarrage

```bash
siege -b -c 20 -t 5M http://localhost:8080/
curl -i http://localhost:8080/   # → 200, pas de redémarrage
```

---

### Q42 — Paramètres siege équilibrés

```bash
siege -c 20 -r 50 -d 1 http://localhost:8080/
```

Éviter `-c 500` sans `-d` (saturation artificielle).

---

## Bonus

### Q43 — Conditions pour évaluer les bonus

> Évalués uniquement si **tout le mandatory est validé** sans exception.

- **Cookies & sessions** : `Set-Cookie` dans les headers de réponse, état persistant côté serveur
- **Multi-CGI** : plusieurs `cgi_interpreter` par extension (`.py`, `.php` — voir `configs/test.config`)

---

## Checklist finale

### Bloquants (note 0 si échec)

- [ ] `poll()` dans main loop, POLLIN **et** POLLOUT simultanément
- [ ] I/O uniquement après `revents`
- [ ] `errno` jamais après `recv/send` ✅ (`EINTR` après `poll()` seulement)
- [ ] `-1` **et** `0` gérés pour `recv/send` ✅
- [ ] Pas de segfault / crash
- [ ] Pas de memory leaks

### Mandatory — Points importants

- [ ] Un seul `poll()` dans une boucle unique (`Core::runPoll`)
- [ ] `acceptClient()` → nouveaux clients via POLLIN sur socket serveur
- [ ] Erreur socket → client supprimé (`closeClient` + retrait de `_pollFds`)
- [ ] Timeout clients géré (connexions pendantes fermées automatiquement)
- [ ] Compilation propre sans relinking
- [ ] GET / POST / DELETE fonctionnels
- [ ] Méthode inconnue → 501, pas de crash
- [ ] Status codes HTTP corrects
- [ ] Page 404 personnalisée (`configs/www/errors/404.html`)
- [ ] `client_max_body_size` → 413 si dépassé (vérifié sur `Content-Length` et body reçu)
- [ ] Routes vers dossiers différents (via `location` blocks)
- [ ] Fichier index par défaut sur dossier (`index index.html`)
- [ ] Méthodes autorisées par route → 405 si interdit
- [ ] Upload fichier + récupération
- [ ] CGI fonctionnel (GET + POST) via pipes non bloquants dans `_pollFds`
- [ ] CGI lancé dans son dossier (`chdir` avant `execve` dans processus enfant)
- [ ] CGI en erreur → 500, pas de crash
- [ ] Site statique complet en navigateur
- [ ] URL incorrecte → 404 contrôlé
- [ ] Listing dossier configurable (`show_directory true/false`)
- [ ] Redirections 3xx avec header `Location` (`redirect_page` dans config)
- [ ] Multi-ports/interfaces
- [ ] Deux webserv sur même port → `"Bind returned -1"` cohérent
- [ ] Siege : disponibilité > 99.5%
- [ ] Mémoire stable sous charge
- [ ] Connexions pendantes fermées (timeout via `clientTimedOut`)
- [ ] Siege indéfiniment sans redémarrage

### README requis

- [ ] Première ligne italique : `*This project has been created as part of the 42 curriculum by your_42_login.*`
- [ ] Section "Description"
- [ ] Section "Instructions" (compilation, installation, exécution)
- [ ] Section "Resources" (docs + usage IA)
