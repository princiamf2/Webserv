#include "webserv.hpp"


int main(int ac, char **av)
{
	if (ac != 2)
	{
		std::cerr << "Usage: " << av[0] << " <config>" << std::endl;
		return (1);
	}

	std::vector<ServerConfig> configs = parseConfig(av[1]);
	Core C(configs);
	C.debug();

	// loop(servers); //will need to fork to handle multiple servers
	return (0);
}

// TODO (CORE / SEMANTIQUE + GESTION D'ERREUR):
// La structure générale du parsing existe, mais il manque encore toute la partie
// validation sémantique et remontée propre des erreurs.
//
// Problèmes à corriger:
//
// 1. Validité des directives:
//    - aucune vérification qu'un mot-clé inconnu est une erreur
//    - actuellement, une directive inconnue peut être ignorée silencieusement
//
// 2. Validité des valeurs:
//    - listen: vérifier que le port est numérique et dans [1, 65535]
//    - client_max_body_size: vérifier que la valeur est numérique et non négative
//    - error_page / redirect_page: vérifier que le code HTTP est valide
//    - methods: vérifier que seules les méthodes autorisées sont acceptées
//    - show_directory: refuser toute valeur autre que true/false
//    - cgi_extensions: vérifier le format des extensions
//
// 3. Présence des arguments obligatoires:
//    - root sans chemin
//    - index sans nom de fichier
//    - error_page sans code ou sans chemin
//    - location sans path
//    - methods vide
//
// 4. Cohérence sémantique:
//    - vérifier qu'un server a au moins un port listen
//    - vérifier qu'une location a un path valide
//    - éviter les doublons inutiles ou incohérents
//    - vérifier les redéfinitions de directives si vous voulez les interdire
//
// 5. Valeurs invalides acceptées silencieusement:
//    - conversions avec stringstream non vérifiées
//    - si la conversion échoue, l'objet est quand même rempli avec une valeur incorrecte
//
// 6. Gestion des erreurs:
//    - aujourd'hui, les fonctions retournent souvent true même si les données sont invalides
//    - il faut remonter false ou lancer une exception avec un message clair
//    - idéalement préciser:
//         - la directive fautive
//         - la ligne concernée
//         - la raison de l'erreur
//
// 7. Fichier de configuration invalide:
//    - aucune erreur claire pour:
//         - bloc server vide
//         - bloc location vide
//         - directive manquante
//         - argument en trop
//         - argument manquant
//
// 8. Contraintes métier du projet:
//    - vérifier ce que le sujet impose réellement:
//         - directives obligatoires ou non
//         - méthodes HTTP autorisées
//         - format attendu pour redirect_page
//         - comportement si plusieurs server partagent un port
//
// Conclusion:
// Le parsing "syntaxique" existe déjà, mais le coeur du travail qu'il reste à faire
// dans ta partie est:
//   - valider chaque directive
//   - valider chaque valeur
//   - détecter les incohérences
//   - remonter des erreurs propres et explicites
