# Compte

L'onglet Compte concerne votre propre trading. Il contient un journal de vos trades clôturés et ses statistiques, alimenté par des relevés MetaTrader 4, MetaTrader 5 ou cTrader, par un pont en direct en lecture seule ou à la main, ainsi qu'un calculateur qui dimensionne une position d'après votre risque avant d'entrer. Tout reste sur cet ordinateur, et aucun mot de passe de courtier n'est jamais demandé.

## Track record

Le Track record est votre journal de trading : il montre comment vous tradez réellement, trade par trade et dans l'ensemble. Sa barre d'outils propose trois vues, **TABLEAU DE BORD**, **JOURNAL** et **DÉTAILS AVANCÉS**, puis **FILTRES**, un sélecteur d'année, la case du compte avec son nom et son solde de départ, la date du dernier import, et les boutons **+ SAISIR UN TRADE** et **IMPORTER**. La dernière vue utilisée est rouverte.

Chaque vue montre les trades du compte choisi, après application de l'année et des filtres. **FILTRES** ouvre une barre avec **Tous instruments**, un choix du sens (**Long et short**, **Long seulement**, **Short seulement**), **Toutes stratégies**, et le compte **n sur m trades**. Le sélecteur d'année propose **Toutes années** et chaque année qui a des trades, d'après la date de clôture en heure locale. Sans compte ou sans trade, un encadré propose **IMPORTER** et **+ SAISIR UN TRADE** ; quand les filtres ne laissent rien, la vue affiche **AUCUN TRADE NE CORRESPOND AUX FILTRES**.

### Comptes

Un clic sur la case du compte ouvre un menu avec vos comptes (nom, solde et devise ; le compte coché est celui affiché), suivis de **Ajouter un compte…**, **Modifier le compte…** et **Supprimer le compte…**. La dernière entrée, **Ajouter un compte démo avec trades d’exemple** (ou **Afficher le compte démo (trades d’exemple)** une fois qu'il existe), charge 60 trades inventés sur un solde de 10,000 USD ; ce compte démo est clairement signalé et n'est jamais réel.

Le formulaire du compte demande un **Nom**, la **Devise du compte** sous forme de code de 3 lettres, le **Solde de départ** et la **Plateforme** (**Manuel**, MT4, MT5, cTrader ou **Autre**). Le solde de départ correspond à vos dépôts moins vos retraits, et tous les chiffres en % du journal en sont calculés.

Supprimer un compte efface le compte et ses trades après confirmation, sans retour possible. Un compte de synchro en direct supprimé ainsi n'est plus recréé tant que vous ne choisissez pas de nouveau le dossier du pont.

### Tableau de bord

Le tableau de bord s'ouvre sur cinq cartes ; le survol d'un titre en donne la définition.

| Carte | Ce qu'elle mesure |
|---|---|
| **P&L NET** | Somme des résultats nets (profit + commission + swap) dans la devise du compte, avec le nombre de trades clôturés |
| **% TRADES GAGNANTS** | Trades gagnants ÷ trades clôturés ; pastilles gagnants · neutres · perdants |
| **PROFIT FACTOR** | Gains bruts ÷ pertes brutes ; l'anneau montre la part des gains bruts (cyan) et des pertes brutes (rouge) |
| **DRAWDOWN MAX** | Plus forte baisse de la courbe d'equity depuis un sommet, en argent et en % de l'equity à ce sommet (solde de départ + P&L cumulé) ; **actuel** donne l'écart actuel au dernier sommet |
| **GAIN / PERTE MOYENS** | Trade gagnant moyen ÷ trade perdant moyen, avec les deux moyennes |

Le **SCORE DE PERFORMANCE** ramène six statistiques sur un radar gradué de 0 à 100, avec des points verts à 70 ou plus, et en donne la moyenne simple. L'échelle est propre à Deepwater, ce n'est pas une mesure standard :

| Axe | Règle 0–100 |
|---|---|
| % gagnants | Le taux de réussite lui-même |
| Profit factor | Profit factor ÷ 3 (3 ou plus = 100) |
| Gain/perte moy. | Gain moyen ÷ perte moyenne ÷ 2.5 (2.5 ou plus = 100) |
| Recovery factor | P&L net ÷ drawdown max ÷ 5 (5 ou plus = 100) |
| Drawdown max | 100 sans drawdown, 0 à 50 % de drawdown |
| Régularité | Part des mois gagnants |

En dessous, **P&L NET CUMULÉ DES TRADES** trace le cumul, un point par jour de trading ; le survol donne la date, le P&L du jour et le cumul. **P&L MENSUEL** et **P&L TRIMESTRIEL** montrent le P&L net par mois et par trimestre calendaires (date de clôture, heure locale), et **P&L DES DERNIERS TRADES** les 40 derniers trades, le plus ancien à gauche. Le survol d'une barre donne sa valeur et son nombre de trades ou, pour un trade seul, son instrument, son sens et sa date de clôture.

La **LECTURE AUTOMATIQUE** résume le tout en phrases construites par des règles : totaux et moyennes, drawdown le plus profond et s'il a été rattrapé, meilleur et plus faible instrument, meilleur et plus faible jour, durée moyenne des gagnants face aux perdants, et plus longues séries gagnante et perdante, un trade neutre interrompant une série. Ce n'est pas un conseil, et aucun modèle de langage ne l'écrit.

### Journal

Le Journal est le registre lui-même, une ligne par trade. Ses cartes donnent **TRADES CLÔTURÉS**, **GAGNANTS / PERDANTS**, **TAUX DE RÉUSSITE**, **P&L NET**, **POSITIONS OUVERTES** et **LATENT** ; comme le journal ne contient que des trades clôturés, les deux dernières restent toujours à **—**. Les pastilles de résultat **TOUS**, **GAGNANTS**, **PERDANTS** et **NEUTRES**, et un champ de recherche sur l'instrument, la stratégie et le commentaire, resserrent les lignes.

Les colonnes sont **OUVERT**, **CLÔTURÉ**, **INSTRUMENT**, **SENS**, **TAILLE**, **ENTRÉE**, **SORTIE**, **STOP**, **OBJECTIF**, **DURÉE**, **R**, **STRATÉGIE**, **SESSION** et **P&L**, suivies d'un œil pour voir ou modifier le trade et d'une corbeille pour le supprimer après confirmation. **R** exprime le résultat en multiples du risque pris, signé selon le sens, et reste vide sans stop :

R = (sortie − entrée) ÷ (entrée − stop)

**SESSION** se déduit de l'heure d'ouverture en UTC : Asie 21–07, Londres 07–12, Londres-NY 12–16, New York 16–21.

Un clic sur un en-tête trie par cette colonne (dates, R et P&L d'abord décroissants), un second clic inverse l'ordre ; les valeurs vides passent à la fin, et par défaut la clôture la plus récente vient en tête. Le survol d'une ligne donne les heures d'ouverture et de clôture, la session et le commentaire, celui du P&L le décompose en profit, commission et swap. Les fenêtres étroites retirent des colonnes, d'abord OUVERT et SESSION, puis STOP et OBJECTIF ; le formulaire du trade les garde.

### Détails avancés

Les Détails avancés cherchent des régularités dans vos résultats. L'en-tête, **DÉTAILS AVANCÉS · n TRADES CLÔTURÉS**, porte un interrupteur **$ / % SOLDE** qui affiche des montants ou des % du solde de départ ; le mode % exige un solde de départ, et sans lui une infobulle indique où le saisir.

**DRAWDOWN · MAX** trace l'écart de la courbe d'equity sous son précédent sommet après chaque trade. Deux nuages de points placent un point par trade et confrontent son P&L net à l'heure d'ouverture en heure locale (**PERFORMANCE PAR HEURE**) ou à la durée de détention, sur une échelle logarithmique (**PERFORMANCE PAR DURÉE**). **P&L PAR JOUR** et **NOMBRE DE TRADES PAR JOUR** regroupent les trades par jour de clôture en heure locale, en omettant les jours sans trade.

La dernière rangée liste les **TOP 5 TRADES** et les **PIRES 5 TRADES** par P&L net avec leur date de clôture, puis **P&L TOP 5 SYMBOLES** et **NOMBRE DE TRADES · TOP 5 SYMBOLES**. Le survol d'un point ou d'une barre en donne les détails.

### Saisir un trade

**+ SAISIR UN TRADE** ouvre le formulaire du trade, en créant d'abord un compte s'il n'y en a aucun ; l'œil d'un trade existant ouvre le même formulaire. Ses champs sont :

- **Instrument**, à taper pour chercher dans le catalogue ;
- **Sens** : **↗ LONG** ou **↘ SHORT** ;
- **Ouverture (heure locale)** et **Clôture (heure locale)** ;
- **Prix d’entrée**, **Prix de sortie** et **Taille (lots)** ;
- **P&L** ;
- **Stop loss (facultatif)** et **Take profit (facultatif)** ;
- **Commission** et **Swap** ;
- **Stratégie** : vos stratégies passées, Breakout, Mean reversion, News, Reversal, Trend following, ou une nouvelle ;
- **Commentaire**.

Si vous laissez **P&L** vide, Deepwater l'estime par mouvement de prix × lots × taille du contrat, converti dans la devise du compte au taux en direct du jour, et non à celui du jour du trade ; le formulaire affiche la taille de contrat utilisée. Avant d'enregistrer, il vérifie qu'il y a un instrument, que l'entrée, la sortie et la taille sont positives, que la clôture ne précède pas l'ouverture, qu'il existe un P&L ou une estimation, et que le stop diffère de l'entrée.

### Importer des relevés

**IMPORTER** ouvre une fenêtre en deux parties ; la première, **DEPUIS UN FICHIER DE RELEVÉ**, lit un relevé exporté de votre plateforme :

- MT4 : Historique du compte › clic droit › Tout l’historique, puis Enregistrer comme rapport détaillé (.htm).
- MT5 : Historique › clic droit › Rapport › HTML.
- cTrader : Historique › export CSV.
- Tout CSV avec une ligne d'en-tête (symbole, sens, heures d'ouverture/clôture, prix, taille, profit).

Les relevés sont écrits à l'heure du serveur du courtier : réglez donc **Heures du relevé en** sur son décalage, de UTC−12 à UTC+14. Les serveurs MetaTrader sont souvent en UTC+2 l'hiver et UTC+3 l'été, tandis que les fichiers cTrader donnent leur propre fuseau. Changer le décalage met l'aperçu à jour.

**CHOISIR UN FICHIER…** affiche un aperçu : le format, le nombre de trades clôturés, la première et la dernière date, les dépôts, le numéro de compte, les lignes non lues et les éventuels avertissements. **Dans** désigne un compte existant (affiché avec sa devise) ou **Nouveau compte (depuis le fichier)**, prérempli d'après le fichier. Si le relevé est dans une autre devise que celle du compte, Deepwater demande confirmation avant d'importer, car les gains ne sont pas convertis.

**IMPORTER** n'ajoute que les trades absents du journal et indique combien étaient nouveaux : importer deux fois le même fichier ne crée donc pas de doublons, et le réimporter avec un décalage corrigé corrige les heures des trades déjà présents. Les rapports Excel et PDF ne sont pas lus, et les fichiers de plus de 64 Mo sont refusés.

### Synchro en direct (pont en lecture seule)

La seconde partie de la fenêtre tient le journal à jour sans export. **ENREGISTRER LES FICHIERS DU PONT…** écrit un petit Expert Advisor pour MT4 et MT5 et un cBot pour cTrader, avec un README.txt, à installer dans votre plateforme. Ils se contentent d'écrire vos trades clôturés dans un fichier et n'envoient jamais d'ordre.

**CHOISIR UN DOSSIER…** indique à Deepwater où ces fichiers sont écrits, Common/Files de MetaTrader ou Documents/Deepwater pour cTrader, et liste les dossiers correspondants trouvés sur cet ordinateur. Deepwater surveille ensuite le dossier et ajoute les nouveaux trades à chaque modification d'un fichier. Chaque fichier alimente un compte, créé la première fois que le fichier est vu, et la ligne d'état dit ce qui a été synchronisé. **ARRÊTER** met fin à la synchro.

Un trade que vous supprimez d'un compte du pont n'est pas ajouté de nouveau, et un solde de départ que vous avez saisi est conservé.

### Où sont gardées les données

Le journal est un fichier local, jamais envoyé nulle part. S'il ne peut pas être lu, il est mis de côté, le journal repart à vide et un message indique où se trouve l'ancien fichier. Toutes les statistiques utilisent le P&L net (profit + commission + swap) dans la devise du compte, avec des dates en heure locale. Les tailles de contrat suivent les conventions CFD habituelles, détaillées dans le Calculateur de risque ; celles de votre courtier peuvent différer.

## Calculateur de risque

Le Calculateur de risque répond à la question à se poser avant d'entrer : combien de lots prendre pour que le stop, s'il est touché, coûte exactement ce que vous avez décidé de risquer ? Le titre, **CALCULATEUR DE RISQUE — TAILLE DE POSITION**, porte un badge avec l'instrument. Les **PARAMÈTRES DU TRADE** occupent la gauche, chaque erreur s'affichant sous son champ, et les résultats la droite : **POSITION**, **RISQUE**, **GAIN**, **CARTE DU TRADE**, **RATIO RISQUE / GAIN** et une note sur le contrat et la conversion utilisés.

### Saisir le trade

Un clic sur **INSTRUMENT** ouvre une liste avec recherche : majeures du forex, autres paires, métaux, indices, énergie, agriculture, crypto et actions (les obligations n'y figurent pas) ; Entrée choisit le premier résultat et Échap ferme. Le choix d'un instrument préremplit les niveaux d'après le prix en direct : l'entrée au prix en direct, le stop 50 pips en dessous pour le forex ou environ 0.5 % en dessous sinon, et le take profit à deux fois la distance du stop au-dessus. Remplacez-les librement.

**SOLDE DU COMPTE** reprend la dernière valeur saisie, sinon le solde de départ du compte courant du journal, sinon 10,000. **RISQUE PAR TRADE** est un % du solde ou un montant dans la devise du compte, au choix de l'interrupteur à sa droite. La devise du compte est celle du compte courant du journal, ou l'USD sans compte. Viennent ensuite **PRIX D’ENTRÉE**, **STOP LOSS** et **TAKE PROFIT (FACULTATIF)**. Les nombres acceptent la virgule comme séparateur décimal, et le solde, le risque, l'unité et l'instrument sont mémorisés.

### Lire les résultats

Le sens découle du stop : sous l'entrée, le trade est **↗ LONG**, au-dessus **↘ SHORT**. Le calculateur établit d'abord ce que perd un lot au stop, puis divise votre risque par ce montant :

Perte au stop pour 1.00 lot = distance du stop × taille du contrat × unité de prix × taux de conversion

**TAILLE EN LOTS** = risque ÷ cette perte, arrondi **à l'inférieur** au 0.01 lot

Le minimum est de 0.01 lot, affiché en rouge quand même ce minimum risque plus que demandé. **TAILLE DE POSITION** donne lots × taille du contrat, en unités, onces, barils, etc., et **DISTANCE DU STOP** la distance en prix. **RISQUE** est le montant risqué avec la taille arrondie et son % du solde ; quand l'arrondi l'a réduit, l'objectif en % est rappelé à côté. **GAIN** est le montant au take profit, ou indique **aucun take profit** ou **take profit du côté du stop**. La **CARTE DU TRADE** place **STOP**, **ENTRÉE** et **OBJECTIF** sur une même échelle.

Le ratio met le gain en regard du risque :

**RATIO RISQUE / GAIN** = 1 : (|take profit − entrée| ÷ distance du stop)

Il est tracé sur une échelle de 1:0 à 1:4 et noté **FAIBLE** sous 1, **LIMITE** sous 1.5, **ACCEPTABLE** sous 3 et **EXCELLENT** à partir de 3. Le **TAUX DE RÉUSSITE MINIMUM** = 1 ÷ (1 + R) indique à quelle fréquence un tel trade doit gagner rien que pour être à l'équilibre.

### Tailles de contrat (1.00 lot)

| Instrument | Taille |
|---|---|
| Paires de devises | 100,000 unités de la devise de base |
| Or, platine, palladium | 100 oz |
| Argent | 5,000 oz |
| Cuivre | 25,000 lb |
| Pétrole brut | 1,000 barils |
| Gaz naturel | 10,000 MMBtu |
| Fioul, essence | 42,000 gallons |
| Maïs, blé, soja | 5,000 boisseaux (prix en cents) |
| Café | 37,500 lb (prix en cents) |
| Sucre | 112,000 lb (prix en cents) |
| Coton | 50,000 lb (prix en cents) |
| Bovins vivants | 40,000 lb (prix en cents) |
| Cacao | 10 tonnes |
| Indices | 1 unité par point, dans la devise de l'indice |
| Crypto | 1 unité |
| Actions | 1 action |

### D'où viennent les données

Le prix en direct et les taux de conversion viennent des cotations de l'application (Yahoo Finance, toutes les 20 s tant que la page est visible), pris en direct, inversés ou via l'USD. Tant qu'un taux n'est pas arrivé, la page affiche **En attente du taux … en direct.** ; elle demande aussi **Saisissez un prix d’entrée et un stop loss.**, prévient que **Le stop loss doit être différent du prix d’entrée.** ou que **Le take profit est du côté du stop par rapport à l’entrée.**, et exige un solde et un risque positifs.

Le calcul est déterministe et laisse de côté spread, commission, swap et marge. Les lots avancent par pas de 0.01, et les tailles de contrat peuvent différer de celles de votre courtier : c'est pourquoi la page affiche celle qu'elle suppose. Rien n'est envoyé à un courtier : pour garder le trade, saisissez-le dans le Track record une fois clôturé.
