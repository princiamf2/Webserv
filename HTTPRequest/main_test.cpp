#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include "../Parsing/ParseConfig.hpp"
#include <iostream>
#include <vector>

static const Location* findLocation(ServerConfig const& server, std::string const& uri)
{
	const Location* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const Location& loc = server.locations[i];

		if (uri.find(loc.path) == 0 && loc.path.size() > bestLen)
		{
			best = &server.locations[i];
			bestLen = loc.path.size();
		}
	}
	return best;
}
// TODO:
// Cette fonction matche actuellement les locations en utilisant request.uri brut,
// ce qui est incorrect car l'URI peut contenir une query string (ex: /images/logo.png?x=1).
//
// Il faut:
//   1. utiliser uniquement le path HTTP (sans la partie ?query)
//   2. éviter les faux positifs:
//        exemple: "/images2" ne doit PAS matcher la location "/images"
//
// Solution:
//   - travailler avec request.path (à ajouter dans HttpRequest)
//   - vérifier que le match est soit exact, soit suivi d'un '/'.
//
// Sinon, le routing HTTP sera incorrect dans plusieurs cas.
//nico

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserv_test_http config_file" << std::endl;
		return 1;
	}

	std::vector<ServerConfig> servers = parseConfig(argv[1]);
	if (servers.empty())
	{
		std::cerr << "Error: no server config loaded" << std::endl;
		return 1;
	}

	ServerConfig const& server = servers[0];

	std::vector<std::string> rawRequests;
	rawRequests.push_back(
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"GET /images/logo.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 11\r\n"
		"\r\n"
		"hello world"
	);
	rawRequests.push_back(
		"DELETE /images/logo.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"DELETE /upload/upload.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);

	for (size_t i = 0; i < rawRequests.size(); ++i)
	{
		std::cout << "===== TEST " << i + 1 << " =====" << std::endl;

		try
		{
			HttpRequest request = HttpParser::parseRequest(rawRequests[i]);
			const Location* location = findLocation(server, request.uri);

			HttpResponse response = RequestHandler::handleRequest(request, server, location);
			std::string rawResponse = HttpResponseBuilder::buildResponse(response);

			std::cout << rawResponse << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
		}

		std::cout << std::endl;
	}

	return 0;
}

// TODO:
// Actuellement, on utilise toujours servers[0], ce qui est incorrect dans un vrai serveur HTTP.
// Il faut sélectionner dynamiquement le bon ServerConfig en fonction :
//   - du port d'écoute de la socket (listen_ports)
//   - du header "Host" présent dans la requête HTTP (domain_names)
//
// Exemple attendu :
//   - récupérer le port sur lequel la requête est reçue
//   - récupérer le header Host depuis request.headers["Host"]
//   - parcourir tous les ServerConfig et trouver celui qui correspond
//
// Sans ça, toute la configuration multi-serveurs est inutile et ignorée.
//nico





// TODO GLOBAL pour tout les TODO tous les fichiers:
//
// La configuration (ServerConfig / Location) est bien définie,
// mais elle n'est pas encore pleinement utilisée dans la logique HTTP.
//
// Les points critiques à corriger sont:
//   - sélection dynamique du serveur (Host + port)
//   - utilisation correcte du path (sans query)
//   - parsing HTTP complet (headers + body)
//   - application réelle de toutes les options de config
//
// Actuellement, le code fonctionne pour des tests simples,
// mais ne respecte pas encore le comportement attendu d'un serveur HTTP.
//nico