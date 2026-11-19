# Rubik

`rubik` résout un cube 3×3×3 à partir d'un mélange fourni en argument. Il accepte la notation standard des faces et affiche une séquence de résolution sur la sortie standard.

## Compilation

Prérequis : un compilateur C compatible C99 et GNU Make.

```sh
make
```

Cette commande construit l'exécutable `./rubik` ainsi que la bibliothèque statique `build/librubik.a`.

## Utilisation

Passez le mélange comme un seul argument, entre guillemets :

```sh
./rubik "F R U2 B' L' D'"
```

La solution est écrite sur `stdout`, avec des mouvements séparés par des espaces et un saut de ligne final. Le programme n'ajoute pas de texte explicatif à la solution, ce qui permet par exemple de compter ses mouvements :

```sh
./rubik "F R U2 B' L' D'" | wc -w
```

Un mélange vide représente le cube déjà résolu et produit une ligne vide. En cas d'erreur d'argument ou de notation, un message est écrit sur `stderr` et le programme se termine avec un code non nul.

Pour afficher la solution, son nombre de mouvements et le temps de calcul :

```sh
./rubik --stats "F R U2 B' L' D'"
```

Le nombre utilise la métrique HTM : un quart de tour ou un demi-tour compte pour un mouvement. Le temps est mesuré en secondes, de l'analyse du mélange à la fin de la résolution, sans compter l'affichage ni le démarrage du processus.

Le mode temporel conserve le solveur en deux phases et explore plusieurs possibilités, avec un budget de 1,5 seconde pour chaque phase. Il conserve aussi la solution du solveur standard et renvoie la plus courte solution trouvée parmi les candidats conservés ; l'optimalité n'est pas garantie :

```sh
./rubik --timed "F R U2 B' L' D'"
./rubik --stats --timed "F R U2 B' L' D'"
```

Le mode 2x2 accepte la même notation de mélange. Il construit un cube avec le moteur 3x3 existant, ignore les arêtes et recherche une solution optimale sur les huit coins avec un parcours IDA* :

```sh
./rubik --2x2 "R U R' U R U2 R' F2"
```

La sortie du mode `--2x2` résout les huit coins ; les centres et les arêtes du modèle interne ne sont pas pris en compte.

## Générer des mélanges

Le générateur produit par défaut un mélange de 25 mouvements. `--length` définit le nombre de mouvements par mélange et `--count` le nombre de mélanges à générer. Chaque mélange est écrit sur une ligne :

```sh
./rubik --generate
./rubik --generate --length 20
./rubik --generate --count 5
./rubik --generate --length 30 --count 5
```

Les options peuvent être utilisées indépendamment ou ensemble. Les mouvements sont tirés aléatoirement depuis `/dev/urandom` ; deux mouvements consécutifs ne tournent jamais la même face. Un quart de tour ou un demi-tour compte chacun comme un mouvement.

Ajoutez `--solve` pour résoudre chaque mélange généré. La sortie présente chaque mélange et sa solution sur deux lignes étiquetées, avec une ligne vide entre chaque paire :

```sh
./rubik --generate --solve --count 5
./rubik --generate --solve --length 20 --count 5
```

Pour utiliser le solveur temporel avec les mélanges générés, ajoutez `--timed` à `--solve` :

```sh
./rubik --generate --timed --solve --length 25 --count 5
```

Ajoutez `--stats` pour afficher le nombre de mouvements et le temps de résolution de chaque mélange :

```sh
./rubik --generate --stats --timed --solve --length 25 --count 20
```

Le temps inclut la préparation du cube et la résolution, mais pas la génération du mélange ni l'affichage. La première résolution peut inclure la construction des tables, réutilisées pour les mélanges suivants.

## Visualisation 3D (bonus)

Le mode graphique est optionnel et se compile séparément, sans ajouter de dépendances à `rubik`. Il nécessite les bibliothèques de développement SDL2 et OpenGL, ainsi qu'un environnement graphique ; sous Ubuntu, elles sont fournies par les paquets `libsdl2-dev` et `libgl-dev`.

```sh
make visual
./rubik-visual --visual "F R U2 B' L' D'"
./rubik-visual --visual --full "F R U2 B' L' D'"
```

Par défaut, la fenêtre commence sur le cube mélangé et anime la solution. Avec `--full`, elle anime d'abord le mélange à partir d'un cube résolu, puis se met en pause avant la solution. Appuyez sur Espace pour poursuivre, ou sur la flèche droite pour avancer pas à pas. Le titre indique la phase et la progression.

- Glisser avec le bouton gauche de la souris : tourner la vue.
- Espace : mettre en pause ou reprendre.
- Flèche droite : avancer d'un mouvement lorsque la lecture est en pause.
- Flèche gauche : mettre en pause et revenir à l'étape précédente ; pendant un mouvement, revenir d'abord au début de ce mouvement.
- R : revenir au début de l'animation en conservant l'état de pause.
- Échap ou fermeture de la fenêtre : quitter.

## Notation acceptée

- `F`, `R`, `U`, `B`, `L`, `D` : quart de tour horaire de la face.
- `F'` (ou `F’`) : quart de tour antihoraire.
- `F2` : demi-tour.
- Les mouvements doivent être séparés par un ou plusieurs espaces.

Les rotations de cube (`x`, `y`, `z`) et les mouvements de tranche (`M`, `E`, `S`) ne sont pas acceptés.

## Fonctionnement

Le programme applique le mélange à un cube résolu, convertit son état en permutations et orientations de coins et d'arêtes, puis recherche une solution avec un solveur en deux phases. La première phase amène le cube dans un sous-groupe adapté ; la seconde termine la résolution. Des tables de mouvements et de distances guident la recherche.

La recherche 3x3 standard est limitée à 12 mouvements en phase 1 et 18 en phase 2. Le mode temporel explore jusqu'à 20 mouvements en phase 1 et 18 en phase 2, avec un maximum de 30 mouvements avant simplification et un budget de 1,5 seconde par phase. Une phase peut finir avant son délai si les possibilités considérées sont épuisées ; la construction initiale des tables et un éventuel repli ajoutent du temps. Le solveur 2x2 utilise des tables de distance séparées pour les permutations et orientations des coins et une recherche itérative jusqu'à 14 mouvements. Les mouvements consécutifs de la même face sont fusionnés dans les solutions renvoyées.

## Tests et nettoyage

```sh
make test
```

Cette commande exécute les tests du parseur, du moteur du cube, du solveur et de l'interface en ligne de commande. Pour supprimer l'exécutable et les fichiers de compilation :

```sh
make clean
```

## Organisation du code

- `src/main.c` : interface en ligne de commande.
- `src/parser.c` : validation et analyse du mélange.
- `src/cube.c` : représentation des 54 autocollants et mouvements de face.
- `src/cubie.c` : conversion et opérations sur les coins et les arêtes.
- `src/solver.c` : tables de recherche et solveur en deux phases.
- `src/playback.c` et `src/visual.c` : lecture des mouvements et affichage 3D optionnel.
- `tests/` : tests unitaires et tests de l'exécutable.

## Comparaison avec l'inverse du mélange

Lors d'un essai, 50 mélanges aléatoires ont été générés pour chaque longueur. Le tableau compare la séquence calculée à l'inverse exact du mélange : ces chiffres sont indicatifs et varient d'un essai à l'autre.

| Longueur du mélange | Solution identique à l'inverse | Solution différente | Dont plus courte que l'inverse |
|---:|---:|---:|---:|
| 1 | 50/50 (100 %) | 0/50 | 0/50 |
| 2 | 44/50 (88 %) | 6/50 | 0/50 |
| 3 | 41/50 (82 %) | 9/50 | 1/50 |
| 4 | 30/50 (60 %) | 20/50 | 1/50 |
| 5 | 24/50 (48 %) | 26/50 | 6/50 |
| 6 | 21/50 (42 %) | 29/50 | 4/50 |
| 7 | 15/50 (30 %) | 35/50 | 8/50 |
| 8 | 11/50 (22 %) | 39/50 | 8/50 |
| 9 | 7/50 (14 %) | 43/50 | 3/50 |
| 10 | 5/50 (10 %) | 45/50 | 4/50 |
| 11 | 2/50 (4 %) | 48/50 | 5/50 |
| 12 | 0/50 (0 %) | 50/50 | 2/50 |

Une solution différente n'est pas forcément plus courte : le solveur ne garantit pas une solution optimale. Le générateur évite deux mouvements consécutifs sur une même face ; ces résultats ne couvrent donc pas tous les mélanges possibles.
