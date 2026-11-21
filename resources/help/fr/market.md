# Marchés

L'onglet Marchés suit les prix et les interprète. On y trouve un tableau en direct de 127 marchés avec une fiche complète par instrument, la saisonnalité sur plusieurs années, une note fondamentale des huit grandes devises, un scan technique de chaque marché et la carte des pools de liquidité d'un instrument. Chaque score et chaque signal est calculé par Deepwater à partir de données publiques ; les règles figurent ci-dessous et dans les infobulles des pages.

## Vue des marchés

La Vue des marchés est le tableau de bord en direct : tous les marchés du catalogue sur un seul écran, rangés par catégorie, avec leur dernier prix et leur variation. Le titre **MARCHÉS** donne le nombre de marchés affichés et la fréquence de mise à jour (**COTATIONS MISES À JOUR TOUTES LES 20 S**). Viennent ensuite un champ de recherche et les pastilles de catégorie, de **TOUS** et **FOREX** jusqu'à **OBLIGATIONS**, chacune avec son nombre de marchés, qui suit la recherche.

| Catégorie | Marchés |
|---|---|
| Forex | 42 paires (majeures, croisées, exotiques) |
| Indices | 23 (S&P 500, Nasdaq 100, Dow, DAX, FTSE, VIX, Nikkei, Hang Seng, DXY…) |
| Matières premières | 18 (métaux, énergie, agriculture) |
| Actions | 28 grandes capitalisations américaines |
| Crypto | 12 |
| Obligations | 4 taux américains (13 semaines, 5, 10 et 30 ans) |

### Lire le tableau

Quatre cartes résument les marchés affichés, catégorie et recherche appliquées. **AMPLEUR** compte les marchés en hausse, en baisse et inchangés, une variation arrondie à 0.00 % comptant comme inchangée ; son infobulle donne les trois comptes. **VARIATION MOYENNE** est la moyenne des variations, et **MEILLEUR** et **PIRE** désignent les marchés à la meilleure et à la pire variation.

Le sélecteur **VARIATION SUR** fixe la période : **1J**, **1S** ou **1M**, les cartes indiquant alors **AUJOURD'HUI**, **1 SEMAINE** ou **1 MOIS**. Le sélecteur **ORDRE** trie les lignes : **DÉFAUT** place les majeures en tête, puis les croisées et le reste ; **PERFORMANCE** met la meilleure variation d'abord et les marchés sans variation à la fin ; **A-Z** suit l'ordre alphabétique.

Chaque catégorie a son panneau, avec son titre, son nombre de marchés et la variation **moy** de ses lignes, puis les colonnes **MARCHÉ**, **DERNIER** et la variation. Les actions affichent le nom de la société sous le ticker. Les panneaux se répartissent sur 3 à 8 colonnes selon la largeur de la fenêtre ; si vous ne choisissez qu'une catégorie, son panneau occupe toute la largeur.

### Calcul des variations

La variation 1J est celle que donne Yahoo Finance, par rapport à la clôture précédente. Les deux périodes plus longues sont mesurées par Deepwater par rapport à une clôture passée :

1S = dernier prix ÷ clôture de la dernière séance au plus tard 7 jours avant la date de la cotation − 1

La variation 1M suit la même règle, avec la dernière séance au plus tard un mois plus tôt. Au survol d'une variation 1S ou 1M, l'infobulle donne la clôture de référence et sa date ; ailleurs sur la ligne, elle donne le nom complet du marché.

### Chercher et ouvrir un marché

La touche `/` place le curseur dans le champ de recherche, quelle que soit la disposition de votre clavier ; Échap l'efface et Entrée ouvre le premier résultat. La recherche porte sur le libellé (EUR/USD), le nom (Or, Nvidia), le symbole et le ticker Yahoo. Un panneau où elle ne trouve rien affiche **Aucun résultat**.

Un clic sur une ligne ouvre la fiche de l'instrument, décrite plus bas. La liste défile en douceur à la molette ou au trackpad, et la catégorie, la période de variation et l'ordre choisis sont mémorisés.

### La fiche instrument

La fiche s'ouvre par-dessus la liste et rassemble ce que Deepwater sait d'un marché. Son en-tête nomme le marché, et la société pour une action ; × ou Échap la ferme et vous ramène à la liste. Chaque carte se charge seule et, quand elle n'a rien à montrer, explique pourquoi.

Le graphique de prix occupe le coin supérieur gauche (voir la section suivante). À côté, **Position retail** donne la part des traders de la communauté Myfxbook à l'achat et à la vente. Elle demande une connexion Myfxbook, à saisir dans Positionnement › Sentiment retail ou dans l'onglet [Sources](help:sources) ; sans elle la carte affiche **SOURCE DE DONNÉES NON CONNECTÉE.**, et les marchés que Myfxbook ne couvre pas l'indiquent. Son infobulle donne la source et l'heure de l'instantané.

**Position COT** montre comment se placent les grands spéculateurs (non commerciaux) du rapport CFTC legacy sur la dernière semaine, sous la forme long ÷ (long + short). Une paire de devises affiche un camembert par future de devise ; les autres marchés utilisent leur propre future (S&P 500 → E-mini, DXY → indice du dollar…). L'infobulle donne les contrats long et short, la variation de la semaine et la date du rapport, et les marchés sans contrat CFTC l'indiquent.

**Comparaison macro** met côte à côte le taux d'intérêt, la croissance trimestrielle du PIB, l'inflation annuelle, le taux de chômage et la confiance des entreprises de l'OCDE (100 = sa moyenne de long terme), soit pour les deux devises d'une paire, soit pour l'économie d'origine de tout autre marché. Les chiffres viennent des services [Économie](help:economy), et le survol d'une valeur en donne la période et la source. Les six tuiles **Heatmap technique** (tendance, momentum, volatilité, volume, structure, liquidité) reprennent les signaux de la page Heatmap technique, calculés sur 2 ans de bougies journalières ; leurs infobulles donnent les chiffres derrière chaque tuile.

**Performance des indices** suit deux courbes depuis janvier 2017, toutes deux ramenées à 0 % à leur premier jour commun ; le survol donne la date et les deux valeurs. Pour une paire de deux devises majeures, chaque courbe est un indice d'une devise : sa variation logarithmique moyenne face aux autres majeures. Tout autre marché est comparé à une référence :

| Marché | Référence |
|---|---|
| Action | S&P 500 |
| Indice | S&P 500 (Nasdaq 100 pour le S&P 500 lui-même) |
| Crypto | Bitcoin (Nasdaq 100 pour le Bitcoin) |
| Matière première | Or (DXY pour l'or) |
| Autre paire | DXY |
| Obligation | S&P 500 |

**Comparaison des réunions** donne, pour chaque banque centrale, les probabilités que les futures de taux attribuent à sa prochaine décision, en cinq tranches : **-50+bps**, **-25bps**, **Statu quo**, **+25bps** et **+50+bps**. L'infobulle indique la date de la réunion et la source ; les banques sans pricing de marché le signalent (voir [Économie](help:economy) › Taux).

Deux cartes de saisonnalité terminent la fiche. **Saisonnalité · Par mois** montre le rendement moyen de chaque mois sur 16 ans au plus, le mois en cours encadré ; le survol d'une barre donne sa valeur et le nombre d'années. **Saisonnalité · Trajectoire annuelle** trace la trajectoire moyenne des années terminées face à l'année en cours.

### Le graphique de la fiche

Le graphique s'ouvre en 4h. Sa barre d'outils propose quatre unités de temps favorites (1m, 30m, 1h, 4h) et un menu qui les réunit toutes : 1m, 5m, 15m, 30m, 1h, 4h, **1J**, **1S** et **1M**. Suivent des boutons pour les barres, les bougies et la zone, avec un menu complet : **Barres**, **Bougies**, **Bougies creuses**, **Ligne**, **Zone** et **Heikin Ashi**. Le + cerclé superpose un autre marché, choisi dans une liste adaptée à la catégorie, jusqu'à ce que vous choisissiez **Retirer la comparaison**. Le menu **Indicateurs** offre **Volume**, SMA 20, SMA 50, SMA 200, EMA 21 et **Bandes de Bollinger 20, 2**, et l'appareil photo copie l'image du graphique dans le presse-papiers.

La profondeur d'historique dépend de l'unité de temps, et la dernière bougie suit toujours la cotation en direct :

| Unité de temps | Historique chargé |
|---|---|
| 1m | 5 jours |
| 5m à 30m | 1 mois |
| 1h | 6 mois |
| 4h | 1 an, construit à partir des bougies horaires |
| 1J | 5 ans |
| 1S, 1M | tout l'historique disponible |

Sous le graphique, les boutons de plage sautent à une durée avec une taille de bougie adaptée : **1J** (bougies 1m), **5J** (5m), **1M** (30m), **3M** (1h), **6M** (4h), **YTD** et **1A** (journalier), **5A** (hebdomadaire) et **Tout** (mensuel). À côté, les bascules **auto**, **log** et **%** règlent l'échelle, et une horloge affiche votre heure locale avec le décalage UTC.

La barre de dessin, à gauche, contient un réticule, un outil emoji et les outils de tracé suivants :

| Outil | Mode d'emploi |
|---|---|
| Ligne de tendance | 2 clics |
| Retracement de Fibonacci | 2 clics ; niveaux 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1 |
| Figure XABCD | 5 clics |
| Boîte de position acheteuse | Un clic ; objectif et stop à 2:1 |
| Pinceau | Glisser |
| Texte | Demande le texte |
| Mesure | Glisser : variation de prix, % et nombre de bougies |
| Zoom | Glisser sur les bougies à afficher |
| Aimant | Accroche l'ouverture, le plus haut, le plus bas ou la clôture la plus proche |

À la souris, glissez pour vous déplacer, utilisez la molette pour zoomer autour du pointeur ou balayez horizontalement pour faire défiler. Glisser l'axe des prix étire l'échelle, et un double-clic dessus revient à l'échelle automatique ; un double-clic sur le graphique réinitialise la vue. Près du bas du graphique apparaissent des boutons de zoom avant, zoom arrière, défilement et réinitialisation, et un clic sur la légende OHLC la replie. Le clic droit propose **Ajouter une ligne horizontale à** au prix sous le pointeur, **Masquer les dessins** / **Afficher les dessins**, **Supprimer tous les dessins** et **Réinitialiser la vue**.

### D'où viennent les données

Les cotations viennent de Yahoo Finance, interrogé toutes les 20 s tant que la liste est visible ; la liste cesse de l'interroger quand une fiche est ouverte ou que la page est masquée. Les références 1S et 1M sont les clôtures journalières Yahoo des 3 derniers mois, gardées 30 minutes et redemandées après un échec. Un marché fermé affiche sa dernière transaction et sa variation par rapport à la clôture précédente, et un marché sans cotation affiche **—**. Aucune clé n'est nécessaire, sauf la connexion Myfxbook pour la carte retail.

Les dessins et réglages du graphique de la fiche ne sont pas enregistrés : ils disparaissent quand vous ouvrez un autre marché.

## Saisonnalité

La Saisonnalité répond à une question simple : comment cet actif s'est-il comporté, d'habitude, à cette période de l'année ? Sous le titre **Labo Saisonnalité**, une barre d'outils choisit l'actif, les années et la période. La partie **SYNTHÈSE** réunit le graphique de tendance saisonnière et la carte **TAUX DE RÉUSSITE** ; la partie **AVANCÉ** montre le rendement moyen par jour de la semaine et par mois, le rendement de chaque année sur la période et les **MESURES DE RISQUE**.

### Choisir ce que vous étudiez

La barre d'outils commence par le menu de l'actif, intitulé **Saisonnalité :** suivi du nom de l'actif. Il liste tout le catalogue, majeures du forex en tête, avec un champ de recherche : les flèches du clavier déplacent la sélection, Entrée choisit et Échap ferme. Les flèches voisines passent à l'actif précédent ou suivant, et le ⓘ explique la page. Le menu des années propose **10 ans**, **15 ans** ou **20 ans** (20 par défaut), et le menu **Filtre** choisit entre **Toutes les années** et **Exclure les années atypiques (>1,5σ)** ; le filtre ne touche que les moyennes mensuelles.

Les boutons de période **Année**, **S1**, **S2** et **T1** à **T4** choisissent l'année entière, un semestre ou un trimestre, et chaque carte est recalculée pour cette période. **Comparer** superpose la moyenne saisonnière d'un autre actif (**Aucun** la retire). La bascule **réel**, précédée de l'année en cours, superpose la trajectoire réelle de cette année, et **Indice (100)** affiche la trajectoire en base 100 plutôt qu'en rendement %. Le bouton de rechargement se trouve à droite. L'actif, les années et le filtre sont mémorisés.

### Le graphique de tendance saisonnière

Le titre du graphique donne l'actif, le nombre d'années réellement utilisées et la plage de dates. La courbe est la trajectoire moyenne, lissée par une moyenne mobile gaussienne sur 3 jours pour la lisibilité ; ses plus haut et plus bas sont étiquetés, et **AUJOURD’HUI** marque la date du jour. Le survol donne la date et la valeur, ainsi que les valeurs comparée et réelle quand elles sont affichées.

Pour étudier n'importe quel moment de l'année, glissez sur le graphique sur 2 jours au moins : un badge donne la période et la variation de la trajectoire moyenne sur celle-ci, et **× Effacer** revient à l'année entière.

### Calcul des statistiques

Les clôtures journalières sont placées sur un calendrier de 366 jours, et un jour sans clôture reprend la précédente. Une année est retenue si elle a une clôture dans les 10 jours qui précèdent le 1er janvier, qui devient sa base, ou dans ses 10 premiers jours ; les années sans historique sont écartées. Chaque année trace alors une trajectoire :

Trajectoire d'une année = clôture du jour ÷ base − 1

La trajectoire moyenne est la moyenne des trajectoires des années terminées. L'année en cours en est exclue : elle apparaît à part, en superposition **réel**. Pour les statistiques, le rendement d'une année sur la période choisie vaut :

Rendement de la période = clôture de fin ÷ clôture de la veille du début − 1

Une année compte dès qu'elle couvre toute la période ; l'année en cours ne compte donc qu'une fois la période passée.

| Chiffre | Règle |
|---|---|
| Taux de réussite | Part des années comptées dont le rendement sur la période est positif |
| **Rendement annuel** / **Rendement de la période** | Variation de la trajectoire moyenne sur la période : (1 + p(fin)) ÷ (1 + p(veille du début)) − 1 |
| **Meilleur mois** / **Pire mois** | Mois moyen le plus haut et le plus bas dans la période |
| **RATIO DE VOLATILITÉ** | Écart-type des rendements journaliers dans la période ÷ écart-type de tous les rendements journaliers ; sous 0.95 plus calme, au-dessus de 1.05 plus volatile, sinon normal |
| Rendement moyen par jour | Rendement journalier moyen par jour de la semaine, sur les jours de la période (année en cours comprise) ; infobulle avec le nombre de jours |
| Rendement moyen par mois | Rendement mensuel moyen des années comptées ; avec le filtre, les années à plus de 1.5 écart-type de la moyenne du mois sont écartées |
| Rendements par année | Rendement de chaque année sur la période ; l'année en cours est grisée (**à date, non comptée**) |
| **GAIN CUMULÉ** | (1 + r₁) × (1 + r₂) × … − 1 sur les années comptées |
| **RENDEMENT MOYEN** / **VOLATILITÉ** | Moyenne et écart-type des rendements de la période |
| **DRAWDOWN MAX** | Plus forte baisse du sommet au creux de la trajectoire moyenne dans la période |
| **RENDEMENT / RISQUE** | Moyenne ÷ écart-type (un simple ratio, pas un ratio de Sharpe) |
| **MEILLEURE ANNÉE** / **PIRE ANNÉE** | Meilleure et pire année sur la période |

### D'où viennent les données

La page lit les clôtures journalières Yahoo Finance des années choisies plus un mois, les garde 30 minutes et les redemande quand vous appuyez sur le bouton de rechargement. Si l'historique manque ou est trop court, le graphique affiche **Pas d’historique quotidien pour…** ou **Historique quotidien insuffisant pour une moyenne saisonnière** ; un actif comparé sans historique est nommé dans le sous-titre du graphique.

## Heatmap fondamentale

La Heatmap fondamentale cherche laquelle des huit grandes devises son économie soutient le mieux. Le bandeau du haut, **FONDAMENTAUX — DOSSIER MACRO PAR DEVISE**, porte à droite un ⓘ et un badge dont l'infobulle donne les règles de notation. En dessous, huit cartes de score (USD, EUR, GBP, JPY, CHF, CAD, AUD, NZD) sont classées de la plus forte à la plus faible, chacune avec son rang, son drapeau, son score, une barre centrée sur zéro et un biais : **HAUSSIER**, **NEUTRE** ou **BAISSIER**.

Le reste de la page est le **DOSSIER MACRO** de l'économie de la devise choisie : la carte de politique monétaire, la lecture automatique, le comparateur de paire, la heatmap elle-même, le détail des publications, une note de bas de page et la méthode. La devise la plus forte reste choisie tant que vous n'en sélectionnez pas une autre, et votre choix est mémorisé.

### La carte de politique monétaire

La carte s'ouvre sur le **TAUX DIRECTEUR** (ou **TAUX DIRECTEUR · HAUT DE** une fourchette), le taux précédent et la date de la dernière décision. **DERNIER MOUVEMENT** indique **HAUSSE**, **STATU QUO** ou **BAISSE**, et **PROCHAINE RÉUNION** fait le compte à rebours (**DANS** n jours, **AUJOURD’HUI**, **PASSÉE**). Une jauge d'inflation situe l'IPC global et l'IPC sous-jacent par rapport à la cible de la banque.

Deux graphiques d'historique suivent, le taux directeur et l'inflation face à la cible, sur **5A**, **10A** (par défaut), **20A** ou **MAX**. Le survol donne la valeur, avec la variation sur le mois pour le taux ou l'écart à la cible pour l'inflation.

### La notation d'une devise

Chaque devise reçoit six signaux compris entre −1 et +1, une valeur positive soutenant la devise. Deux outils reviennent dans les règles : tanh ramène un écart dans l'intervalle ±1, et la tendance vaut +1, −1 ou 0 selon que la dernière publication est au-dessus, en dessous ou égale à la précédente.

| Colonne | Signal | Mot |
|---|---|---|
| **TAUX** | 0.7 × dernier mouvement (hausse +1, statu quo 0, baisse −1) + 0.3 × tanh(taux réel ÷ 2), taux réel = directeur − IPC | HAUSSE / STATU QUO / BAISSE |
| **INFLATION** | 0.7 × tanh(IPC − milieu de la cible) + 0.3 × tendance ; au-dessus de la cible soutient (banque hawkish) | AU-DESSUS CIBLE / PROCHE CIBLE / SOUS LA CIBLE (bande ± 0.5 pt) |
| **CROISSANCE** | 0.7 × tanh(PIB t/t ÷ 0.4) + 0.3 × tendance | EN EXPANSION (> 0.1 %) / STABLE / EN REPLI (< −0.1 %) |
| **EMPLOI** | −0.7 × tanh(Δ chômage ÷ 0.15) − 0.3 × tanh(z face à la moyenne sur 5 ans) | EN MIEUX / STABLE / DÉGRADATION (± 0.1 pt) |
| **ACTIVITÉ** | 0.7 × tanh((indice − neutre) ÷ échelle) + 0.3 × tendance ; PMI : neutre 50, échelle 3 ; climat des affaires : neutre 100, échelle 1 | EXPANSION / CONTRACTION |
| **SENTIMENT** | 0.6 × tanh(Δ confiance des ménages ÷ (2 × variation médiane)) + 0.4 × tanh(z sur 5 ans) | EN MIEUX / STABLE / EN BAISSE |

Les cibles d'inflation sont de 2 % pour la plupart des banques (au-dessus de la cible à partir de 2.5, en dessous à partir de 1.5), de 2–3 % pour la RBA (au-dessus de 3.0, en dessous de 2.0) et de 0–2 % pour la BNS (au-dessus de 2.5, en dessous de 0.0).

Le niveau d'une devise vaut 100 × la moyenne de ses signaux disponibles, et elle n'a pas de score avec moins de 3 signaux. Comme une devise ne bouge jamais que face aux autres, le score retranche du niveau la moyenne des huit :

Score = niveau − niveau moyen des huit devises

Un score de +8 ou plus se lit **HAUSSIER**, de −8 ou moins **BAISSIER**, et tout ce qui est entre les deux **NEUTRE**. Dans la heatmap, les cellules sont turquoise vif à partir de 0.5, turquoise sombre à partir de 0.15, grises entre −0.15 et 0.15, rouge sombre à partir de −0.15 vers le bas et rouges à partir de −0.5 ; un chiffre manquant s'affiche **—**.

### La heatmap, la lecture et le comparateur

La **HEATMAP FONDAMENTALE** compte une ligne par devise : la **DEVISE** avec son score, les six colonnes de signaux et le **BIAIS**. Le survol d'une cellule donne la règle appliquée et la valeur du signal, celui du score ou du biais montre comment le score s'est construit. Un clic sur une ligne choisit la devise, comme le font les cartes.

La **LECTURE AUTOMATIQUE**, marquée **RÈGLES**, met le dossier en mots : un titre sur la politique monétaire, l'inflation et le plus fort ou le plus faible des autres signaux, un paragraphe sur les dernières publications et l'explication des trois signaux les plus forts. Elle est construite par des règles, sans modèle de langage.

Le **COMPARATEUR DE PAIRE** oppose la devise choisie à l'une des sept autres, sélectionnée par onglet. Il compare sept lignes : taux directeur, IPC global, IPC sous-jacent, PIB t/t, chômage, indice d'activité et ventes au détail g.a. Plus haut vaut mieux sur chaque ligne sauf le chômage, et le verdict nomme la devise en tête sur le plus de lignes (**Les fondamentaux favorisent…**, ou **Fondamentaux équilibrés**), avec les deux scores. Une ligne qui manque d'un côté ne compte pas, et deux indices d'activité différents ne sont pas comparés.

Enfin, le **DÉTAIL DES PUBLICATIONS** de la devise se déplie d'un clic en un tableau aux colonnes **INDICATEUR**, **DERNIER**, **PRÉCÉDENT**, **PÉRIODE**, **PUBLIÉ** et **SOURCE** ; un chiffre manquant en donne la raison.

### D'où viennent les données

Les taux directeurs, les décisions et les réunions viennent des flux des banques centrales ; l'historique des taux et l'IPC, de la BRI (mensuel) ; le PIB, le chômage, le climat des affaires et la confiance des ménages, de l'OCDE, avec le chômage annuel de la Banque mondiale en secours, marqué **(an)**. La zone euro est l'agrégat. Ces services sont décrits dans [Économie](help:economy). La page se recalcule à chacune de leurs mises à jour, et aucune clé n'est nécessaire. Pendant le chargement, les cartes affichent **CHARGEMENT** et le dossier **En attente des données macro…**.

Aucun PMI gratuit n'est utilisé : l'activité est donc l'indice de climat des affaires de l'OCDE, affiché **ICA**. L'IPC sous-jacent et les ventes au détail manquent pour certaines économies, qui affichent alors **—** avec la raison.

## Heatmap technique

La Heatmap technique passe au crible tous les marchés du catalogue sur bougies journalières et vous montre d'un coup d'œil lesquels sont en tendance, en excès ou près d'un pool de liquidité. Le bandeau du haut, **SURVEILLANCE TECHNIQUE — SCAN MULTI-SIGNAUX**, porte un badge qui affiche la progression pendant le chargement, puis le nombre d'instruments calculés ; son infobulle liste les instruments sans historique exploitable.

Six cartes comptent, sur tous les instruments chargés, ceux en tendance haussière, en tendance baissière, surachetés, survendus et près d'une liquidité, et **BIAIS DU MARCHÉ** trace en barre haussiers ÷ (haussiers + baissiers). Viennent ensuite les filtres, le tableau, avec **INSTRUMENT** (nom et prix en direct), les six signaux et une légende sous les lignes, et à droite la **FICHE SIGNAL** de l'instrument choisi.

### Les six signaux

Tous les signaux se calculent sur bougies journalières. L'ATR14 % est la moyenne du vrai range sur 14 jours, exprimée en % du prix.

| Colonne | Règle |
|---|---|
| **TENDANCE** | écart = (EMA20 − EMA50) ÷ EMA50 ; ≥ +0.10 % HAUSSIER, ≤ −0.10 % BAISSIER, sinon NEUTRE ; intensité = tanh(\|écart\| ÷ (2 × ATR14 %)) |
| **MOMENTUM** | RSI(14) ≥ 70 SURACHETÉ, ≤ 30 SURVENDU ; ROC(10) en contexte |
| **VOLATILITÉ** | centile de l'ATR14 % du jour parmi ses 252 derniers jours : ≥ 80 ÉLEVÉE, ≤ 20 FAIBLE, sinon NORMALE ; détail = volatilité réalisée sur 20 jours, annualisée (√365 crypto, √252 autres) |
| **VOLUME** | dernier jour complet ÷ moyenne des 20 jours précédents ; ≥ 1.50 > MOYENNE, < 0.75 < MOYENNE, sinon MOYEN ; N/D sans volume |
| **STRUCTURE** | deux derniers sommets et creux (swings sur 5 bougies) : tous deux plus hauts HAUSSIER, plus bas BAISSIER, sinon EN RANGE |
| **LIQUIDITÉ** | plus hauts / plus bas égaux regroupés à max(0.06 %, ¼ ATR14 %) près ; balayé dans les 3 derniers jours BALAYÉ RÉCEMMENT ; pool non testé le plus proche à moins de 1 × ATR POOL PROCHE, sinon POOL AU-DESSUS / POOL EN DESSOUS ; aucun AUCUN POOL |

Beaucoup de marchés n'ont pas de volume propre : Deepwater le lit alors sur un proxy, dont la cellule affiche le ticker. Les jambes forex utilisent des ETF de devises (FXE, FXB, FXY, FXF, FXC, FXA, UUP), une croisée prenant la moyenne de ses deux jambes. Les indices américains, les métaux, le pétrole, le gaz, l'essence, le blé, le soja et le sucre passent par des ETF (SPY, QQQ, DIA, IWM, GLD, SLV, PPLT, PALL, CPER, USO, BNO, UNG, UGA, WEAT, SOYB, CANE). Les taux et le VIX n'en ont pas, et les autres marchés utilisent leur propre volume.

La teinte d'une cellule donne son sens (cyan haussier, rouge baissier, jaune élevé, gris atone, violet près d'un pool), son intensité l'ampleur.

### La fiche signal

La fiche signal affiche l'identifiant, le nom, le prix en direct et la pastille de tendance de l'instrument choisi, puis un radar des six signaux sur une échelle de 0 à 100 où 50 est neutre. La tendance vaut 50 ± 50 × intensité, le momentum est le RSI, la volatilité le centile de l'ATR, le volume 50 × le ratio, la structure 85, 15 ou 50 (haussière, baissière, en range), et la liquidité :

Liquidité = 100 × exp(−|distance| ÷ (2 × ATR14 %))

Sous le radar, les six signaux sont repris avec leurs détails, et le survol du panneau donne les règles complètes.

### Utiliser le tableau

Les filtres resserrent le tableau : pastilles de catégorie (**TOUS**, **FX**, **MATIÈRES PREMIÈRES**, **INDICES**, **ACTIONS**, **CRYPTO**, **TAUX**, avec leurs comptes), pastilles de tendance (**TOUTE TENDANCE**, **HAUSSIER**, **BAISSIER**) et **RECHERCHER…**, qui porte sur le nom ou le symbole. Après un changement de filtre, la première ligne est sélectionnée ; si les filtres ne laissent rien, le tableau affiche **AUCUN INSTRUMENT NE CORRESPOND À CES FILTRES.**

Un clic sur une ligne affiche sa fiche signal. Un clic sur un en-tête de colonne trie par cette colonne, d'abord en ordre décroissant (alphabétique pour **INSTRUMENT**), et un second clic inverse l'ordre ; par défaut le tableau est trié par tendance, la plus forte en tête. Le survol d'une cellule donne les chiffres derrière le signal, celui de l'instrument ses six signaux. La molette fait défiler le tableau. Sur téléphone, toucher une ligne ouvre la fiche signal par-dessus la page, et **✕** la ferme.

### D'où viennent les données

Le scan lit 2 ans de bougies journalières Yahoo Finance pour les 127 marchés du catalogue et les proxys de volume, et se relance toutes les 30 minutes tant que la page est visible ; les prix suivent les cotations en direct, toutes les 20 s. Le premier scan prend un moment, et le tableau se remplit à mesure que les séries arrivent (**CHARGEMENT DES BOUGIES JOURNALIÈRES…**). Un marché sans historique journalier exploitable est écarté et listé dans l'infobulle du badge.

## Liquidité

La page Liquidité repère, sur un instrument, où se logent probablement les ordres stop, ces pools au-dessus des plus hauts égaux et sous les plus bas égaux, et suit le moment où le prix vient les balayer. Le bandeau du haut, **CARTE DE LIQUIDITÉ — POOLS DE STOPS & BALAYAGES**, porte le bouton de l'instrument et le prix en direct. Ce bouton ouvre tout le catalogue, avec des étiquettes de catégorie et un champ de recherche ; Échap le ferme, et votre choix est mémorisé.

Trois cartes résument la carte. **CHASSE AUX STOPS** dit à quel point le prix est proche d'un pool, la deuxième carte compte les **POOLS NON TESTÉS** sur l'ensemble des pools détectés, et la troisième les **BALAYAGES (HISTORIQUE)**, avec le temps écoulé depuis le dernier. Viennent ensuite le graphique horaire et ses pools, un panneau à trois onglets et une note de méthode ; le pied de page donne l'heure du dernier calcul et un badge **DIFFÉRÉ**.

### Le graphique

Le graphique trace des bougies horaires sur une échelle en bougies, si bien que les week-ends ne laissent pas de trous. Il montre les 4 pools non testés les plus proches de chaque côté du prix et les 3 pools les plus récents balayés par une bougie à l'écran. Les pools acheteurs (BSL) sont en pointillé rouge, les pools vendeurs (SSL) en pointillé cyan, et le plus haut et le plus bas de la veille (PDH / PDL) en tirets dorés. Chaque niveau a son étiquette sur l'axe des prix (**BSL ×N** et son prix), à côté de celle du dernier prix.

La molette zoome autour du pointeur et un glisser déplace la vue ; un réticule en tirets donne le prix et la date en UTC. Glissez la poignée en bas à droite pour changer la hauteur du graphique.

### Les onglets du panneau

**POOLS DE LIQUIDITÉ** liste les pools non testés et ceux pris sur les 30 derniers jours, par prix, avec une bande **LAST** au prix actuel. Chaque ligne donne le côté et le niveau, un point par bougie ayant traversé le niveau (+n au-delà de 5), **BALAYÉ** ou **CASSÉ**, et la distance au prix en %.

**ALERTES DE BALAYAGE** liste les pools balayés, du plus récent au plus ancien : **BALAYAGE DE LIQUIDITÉ DÉTECTÉ**, le côté et le niveau, la **Confirmation du retournement :** et le temps écoulé. **EXTRÊMES DE SESSION** donne la **FENÊTRE**, le **HAUT** et le **BAS** des dernières sessions de Sydney, d'Asie (Tokyo), de Londres et de New York (en UTC), de la **Veille** et de la **Semaine précédente**.

### Détection des pools et des balayages

Un swing est une fractale sur 3 bougies : un plus haut au-dessus de ceux des bougies qui l'encadrent, et l'inverse pour les creux. Les sommets sont regroupés tant qu'ils restent à 0.06 % du premier prix du groupe, et un groupe de 2 swings ou plus forme un pool acheteur à son plus haut ; les creux donnent des pools vendeurs à leur plus bas. ×N est le nombre de swings du pool.

La première bougie suivante qui traite au-delà du niveau décide de son état. Si elle clôture revenue de l'autre côté, le pool est **BALAYÉ** ; si elle clôture au-delà, il est **CASSÉ** ; d'ici là, il reste non testé.

Après un balayage, la confirmation du retournement mesure le retour depuis le niveau sur les 6 bougies suivantes, en ATR(14) de la bougie du balayage : 1.5 ou plus donne Forte, 0.75 ou plus Modérée, moins Faible. Elle reste En attente tant que les 6 bougies ne sont pas toutes là et que le retour reste sous 1.5.

La chasse aux stops mesure la distance du prix au pool non testé le plus proche, en ATR(14) horaire : 2 ou moins donne Élevée, 6 ou moins Moyenne, au-delà Faible. C'est une règle publiée, pas une probabilité backtestée, et son infobulle donne le pool et la distance. Les sessions suivent les heures locales, heure d'été comprise : Sydney 07–16, Tokyo 09–18, Londres 08–17, New York 08–17.

### D'où viennent les données

La page lit 2 ans de bougies horaires Yahoo Finance et les recharge toutes les 5 minutes tant qu'elle est visible ; le prix suit les cotations en direct. Sans bougies, le graphique et la première carte affichent **Bougies horaires indisponibles** avec la raison. Un marché dont tous les pools ont été traversés affiche **Aucun pool non testé**. Les seuils sont propres à Deepwater et imprimés sur la page.
