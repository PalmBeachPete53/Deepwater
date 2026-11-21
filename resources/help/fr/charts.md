# Graphiques

Graphiques est un terminal graphique complet. Il trace en direct des bougies issues de sources publiques sur jusqu'à 16 graphiques à la fois, avec 92 indicateurs intégrés plus les vôtres en JavaScript ou en Pine Script, des outils de dessin et le flux d'ordres des marchés Binance. Autour des graphiques, vous trouverez des alertes, un screener, un testeur de stratégie avec contrôles quant, le replay de barres et un compte de paper trading.

## Organisation de la page

### Les zones de la page

La barre du haut traverse la page. De gauche à droite, elle porte le symbole, les unités de temps, le type de graphique, les indicateurs, la comparaison, la disposition, la synchro, le replay et l'alerte ; à droite viennent l'échelle log, l'image, le CSV, les réglages, le passage clair / sombre et le point d'état des données. Quand la fenêtre se resserre, elle retire d'abord le texte des boutons, qui reste dans les infobulles, puis réduit les deux listes, et finit par défiler latéralement.

La barre de dessin, à gauche, réunit les outils de dessin, suivis de trois boutons qui agissent sur le dessin sélectionné. Le centre accueille de 1 à 16 graphiques, chacun avec sa légende en haut à gauche et son navigateur de plage en bas.

Sous les graphiques, le panneau du bas regroupe sept outils en onglets : **Testeur de stratégie**, **Screener**, **Heatmap**, **Saisonnalité**, **Données financières**, **Paper trading** et **Éditeur d'indicateurs**. La flèche à sa droite, **Afficher / masquer le panneau**, le replie sur sa ligne d'onglets. À droite, un panneau latéral de 300 px contient **Liste**, **Alertes**, **Calendrier** et **Infos** ; il disparaît quand la page fait 900 px de large ou moins.

### Boutons de la barre du haut

La plupart des boutons agissent sur le graphique actif, celui que vous avez cliqué en dernier.

| Bouton | Action |
|---|---|
| Nom du symbole | Ouvre la recherche de symbole (taper une lettre ou un chiffre sur la page fait de même) |
| **1m 5m 15m 1h 4h 1D 1W** | Unité de temps du graphique actif |
| Champ à côté | N'importe quelle unité, puis Entrée : `1s`, `7m`, `2h`, `3D`, `2W`, `3M` (m = minutes, M = mois) |
| **Type de graphique** | 16 types, voir Types de graphique |
| Engrenage à côté | **Réglages du footprint** (seulement quand le graphique actif est un footprint) |
| **Indicateurs** | Sélecteur d'indicateurs |
| **Comparer** | Ajoute un autre symbole au graphique actif, en ligne |
| **Disposition des graphiques** | 1 graphique, 2 côte à côte, 2 empilés, 3, 4, 6, 8 ou 16 graphiques |
| **Synchro** | Changer de symbole le change sur tous les graphiques |
| **Replay** | Lance le replay de barres (cliquez ensuite la barre de départ), ou l'arrête |
| **Alerte** | Ouvre **Créer une alerte** pour le graphique actif |
| **Log** | Échelle de prix logarithmique pour le graphique actif |
| Appareil photo | Enregistre une image PNG du graphique actif, titrée avec le symbole et l'unité de temps |
| Téléchargement | Enregistre en CSV les barres du graphique actif et toutes les valeurs de ses indicateurs |
| Engrenage | **Réglages** (généraux et style du graphique) |
| Lune | Rendu clair / sombre de cet onglet |
| Point | **État des données** : vert quand toutes les sources répondent ; l'infobulle nomme celles en échec (des sources de repli sont alors utilisées). Vérifié chaque minute |

## Dispositions et graphiques

### Dispositions et graphique actif

**Disposition des graphiques** affiche de 1 à 16 graphiques ; avec trois, un grand graphique occupe la gauche et deux autres s'empilent à droite. Un nouveau graphique reprend le symbole du premier, démarre sans indicateur et reçoit sa propre unité de temps, dans cet ordre : 1D, 4h, 1h, 15m, 1W, 5m, 30m, 2h. Les graphiques retirés par une disposition plus petite gardent leurs réglages : revenir à une disposition plus grande les rétablit tels quels.

Un clic sur un graphique le rend actif ; il est encadré quand il y en a plusieurs. La barre du haut, la barre de dessin, le testeur, le ticket paper et la fenêtre d'alerte agissent tous sur lui. Chaque graphique garde son symbole, son unité de temps, son type, ses indicateurs, ses symboles comparés, son échelle log et ses réglages de footprint.

### Légende et symboles comparés

La légende, en haut à gauche de chaque graphique, dit ce que vous regardez. Sa première ligne donne ticker · unité de temps · source, puis O, H, L, C et la variation de la barre sous le réticule, ou de la dernière barre quand le réticule est ailleurs. Un clic sur le nom ouvre la recherche de symbole.

Chaque indicateur a ensuite sa ligne, avec son nom, ses réglages et ses valeurs au réticule, et trois boutons : **Réglages** (engrenage), **Afficher/masquer** (œil) et **Retirer**. Un indicateur masqué est grisé, un indicateur encore en calcul affiche « chargement… », et les erreurs apparaissent en rouge. Des notes en orange précisent quelle source est utilisée et pourquoi (repli, données différées, volume absent), ainsi que la couverture du footprint.

**Comparer** trace un autre symbole sur le graphique actif, sous forme de ligne de couleur. L'échelle de prix passe alors en variation en pourcentage pour que les séries partagent une même échelle. Chaque symbole comparé a sa ligne de légende, dont le bouton **Retirer** l'enlève.

### Navigateur de plage

Sous chaque graphique, une bande de 36 px montre une mini-courbe des clôtures sur tout l'historique chargé, avec une fenêtre qui délimite la partie affichée. Glisser dans la fenêtre fait défiler au même zoom. Appuyer en dehors centre la fenêtre à cet endroit, et vous pouvez continuer à glisser pour défiler.

Glisser l'une des poignées de la fenêtre zoome horizontalement ; la fenêtre la plus étroite, 30 px, correspond au zoom maximal, 5 barres. La courbe, elle, ne zoome jamais : seule la fenêtre change.

### Compte à rebours de la bougie

Sous l'étiquette du dernier prix, un compte à rebours indique le temps restant avant la clôture de la barre en cours, en `mm:ss`, `hh:mm:ss`, ou en jours et heures. Il s'affiche pour les cryptos sur toutes les unités de temps, pour le forex en intraday, et pour les autres marchés jusqu'aux barres de 30 minutes. Il n'apparaît ni en replay, ni en Renko, Line break, Kagi, Point & figure ou Barres de range. Si vous préférez vous en passer, désactivez **Compte à rebours de la bougie** dans les réglages.

## Symboles et données

### Trouver un symbole

Pour changer de symbole, cliquez son nom dans la barre du haut ou commencez simplement à taper une lettre ou un chiffre n'importe où sur la page. Saisissez un nom ou un ticker (BTC, AAPL, EURUSD, ^FTSE, VOD.L…) : les résultats donnent ticker, nom, type et place de cotation. ↑ et ↓ parcourent la liste, Entrée ouvre la sélection ; sans résultat, Entrée ouvre le texte tel que vous l'avez tapé. **Comparer** utilise la même recherche.

### Formes de symboles et sources

Un symbole s'écrit sous la forme d'un préfixe et d'un ticker, et c'est le préfixe qui décide d'où viennent les barres. Quand une source échoue, Deepwater passe à la suivante dans la liste, et la légende indique celle qu'il a finalement utilisée.

| Forme | Marché | Sources, dans l'ordre |
|---|---|---|
| `BINANCE:BTCUSDT` | Crypto spot | Binance, puis Coinbase et Kraken (paires en USD), puis Binance.US |
| `BINANCE:BTCUSDT.P` | Perpétuel USD-M Binance | Binance futures, puis Bybit |
| `COINBASE:BTC-USD`, `KRAKEN:…` | Cette plateforme | Cette plateforme (Kraken : 720 dernières barres) |
| `US:AAPL` | Actions et ETF US | Journalier : Nasdaq, puis Yahoo ; intraday : Yahoo, puis Nasdaq (aujourd'hui seulement, sans volume) |
| `FX:EURUSD` | Forex | Yahoo, puis Kraken |
| `FX:XAUUSD`, `FX:XAGUSD` | Or, argent | Yahoo ; l'or suit ensuite PAXG sur Binance ou Coinbase |
| `CME:ES1!` (ES, MES, NQ, MNQ) | Futures sur indices | Yahoo, différé d'environ 10 minutes |
| `YAHOO:^FTSE`, `YAHOO:GC=F` | Indices mondiaux, futures, actions hors US | Yahoo |
| `CSV:NOM` | Vos barres importées | Votre fichier (voir Testeur de stratégie) |
| `BTCUSDT/ETHUSDT` | Spread | Les barres des deux symboles aux mêmes heures ; opérateurs `/`, `*`, ` + `, ` - ` (un par spread) |

Si vous disposez de vos propres clés Alpaca, Polygon ou OANDA, Deepwater les utilise avant les sources publiques, qui servent alors de repli ; les symboles `OANDA:` exigent un jeton OANDA. Ces clés s'inscrivent dans un fichier nommé feeds.json, dans le dossier charts du dossier de données de Deepwater, et une clé ajoutée est prise en compte sans redémarrer.

### Chargement et mises à jour en direct

Chaque graphique charge 3,000 barres par défaut, et **Barres à charger par graphique**, dans les réglages, permet d'aller de 200 à 20,000. Les graphiques Binance se mettent ensuite à jour par le flux en direct de Binance, tandis que les autres demandent les nouvelles barres toutes les 3 s sur les plateformes crypto et toutes les 10 s ailleurs. Un chargement en échec est retenté après 15 s, puis 30 s, et ainsi de suite jusqu'à une fois toutes les 5 minutes.

Tant que l'onglet est masqué, les mises à jour s'arrêtent. À votre retour, une barre clôturée entre-temps provoque un rechargement.

## Types et style de graphique

### Types de graphique

**Type de graphique** propose 16 façons de tracer les barres.

| Type | Ce qu'il trace |
|---|---|
| **Bougies**, **Bougies creuses**, **Barres** | Barres OHLC |
| **Heikin Ashi** | Bougies moyennées |
| **Ligne**, **Ligne avec marqueurs**, **Ligne en escalier**, **Zone**, **Ligne de base**, **Colonnes** | Clôtures (ligne de base : au-dessus / en dessous de la clôture d'il y a 150 barres) |
| **Renko**, **Line break**, **Barres de range**, **Kagi**, **Point & figure** | Graphiques de prix seuls : brique, range et retournement tirés de l'ATR 14 ; Line break sur 3 lignes ; P&F se retourne après 3 cases |
| **Footprint** | Flux d'ordres dans chaque bougie (voir Flux d'ordres) |

Le volume s'affiche en bas du panneau des prix, sauf en Renko, Line break, Kagi et Point & figure. Les indicateurs qui colorent les barres, comme le `barcolor` de Pine ou Couleurs de bougies RSI, ne recolorent que les bougies, les bougies creuses et les barres.

### Réglages

L'engrenage à droite de la barre du haut ouvre **Réglages**, en deux parties : les réglages généraux et le style du graphique. Les réglages généraux valent pour tous les graphiques.

| Champ | Choix |
|---|---|
| **Barres à charger par graphique** | De 200 à 20,000 (3,000 par défaut) |
| **Axe du temps** | **Mon heure locale** ou **UTC** (unités intraday) |
| **Volume sur le graphique des prix** | **Afficher** / **Masquer** |
| **Après avoir placé un dessin** | **Retour au curseur** / **Garder l'outil** |
| **Réticule entre graphiques** | **Indépendant** / **Synchronisé** (même heure sur tous les graphiques) |
| **Compte à rebours de la bougie** | **Afficher** / **Masquer** |

### Style du graphique

Le style du graphique change l'apparence des bougies et du fond sur lequel elles se détachent. Vous pouvez régler la couleur du corps des bougies haussières et baissières, activer ou non les bordures et les mèches et leur donner leurs propres couleurs, et choisir le fond, les lignes de grille verticales et horizontales (chacune activée ou non, avec sa couleur), le réticule, ainsi que la couleur et la taille du texte des échelles, de 9 à 16 (11 par défaut).

Les couleurs acceptent la transparence, et bordures et mèches suivent la couleur des corps tant que vous ne les réglez pas. Les barres de volume suivent les bougies recolorées, à 45 % d'opacité. Seul ce qui diffère du thème est enregistré : le passage clair / sombre et les thèmes de Deepwater recolorent donc toujours le reste. **Enregistrer** applique vos changements, et **Réinitialiser le style** revient aux couleurs et à la taille du thème.

## Indicateurs

### Ajouter et régler un indicateur

**Indicateurs** ouvre le sélecteur, où les indicateurs sont classés par famille et se cherchent par leur nom, en français ou en anglais. Chaque entrée précise si elle se trace **sur le prix** ou dans un **panneau séparé**, et le nombre par graphique n'est pas limité. **Créer un indicateur** ouvre l'**Éditeur d'indicateurs** pour écrire le vôtre. Les indicateurs ancrés, comme le VWAP ancré, vous demandent après l'ajout de cliquer la barre d'ancrage.

L'engrenage d'une ligne de légende ouvre les réglages de l'indicateur : ses paramètres, son **Unité de temps** (**Comme le graphique**, 5m, 15m, 1h, 4h, 1D, 1W, 1M) et les **Couleurs** de chaque sortie. **Par défaut** remet paramètres et couleurs à leur valeur d'origine, **Appliquer** enregistre. Une unité de temps supérieure n'affiche sa valeur qu'une fois chacune de ses barres clôturée, si bien qu'elle ne lit jamais l'avenir ; une unité inférieure à celle du graphique est refusée.

Un paramètre **source** peut être un prix (close, open, high, low, hl2, hlc3, ohlc4, hlcc4) ou n'importe quelle sortie d'un autre indicateur du graphique : vous obtenez alors un indicateur sur indicateur, tracé dans le panneau de ce dernier. Les paramètres de date ont **Choisir sur le graphique**, pour désigner la barre d'un clic. Les niveaux fixes, comme les 30 et 70 du RSI, sont tracés en pointillés. Pour redimensionner les panneaux, glissez le séparateur qui les sépare ; leurs hauteurs sont conservées.

Les tableaux qui suivent donnent les 92 indicateurs intégrés par famille, dans les groupes du sélecteur.

### Moyennes mobiles

| Indicateur | Signification |
|---|---|
| Moyenne mobile (SMA) | Moyenne simple des N derniers prix |
| Moyenne mobile exponentielle (EMA) | Moyenne qui pèse davantage les prix récents |
| Moyenne mobile pondérée (WMA) | Moyenne à pondération linéaire |
| Moyenne mobile pondérée par le volume (VWMA) | Moyenne pondérée par le volume |
| Moyenne mobile de Hull (HMA) | Moyenne rapide et lisse, peu de retard |
| Double EMA (DEMA) | EMA à retard réduit |
| Triple EMA (TEMA) | EMA à retard encore plus réduit |
| Moyenne mobile lissée (SMMA / RMA) | Lissage de Wilder |
| Moyenne mobile Arnaud Legoux (ALMA) | Moyenne à pondération gaussienne (décalage, sigma) |
| Moyenne mobile des moindres carrés (LSMA) | Point final d'une régression linéaire |
| McGinley Dynamic | Moyenne qui adapte sa vitesse au marché |
| Croisement de MM | Moyennes rapide et lente avec marques de croisement |
| Ruban de moyennes mobiles | MM 20, 50, 100 et 200 |
| Moyennes mobiles multiples de Guppy | Deux groupes d'EMA courtes et longues |

### Volume

| Indicateur | Signification |
|---|---|
| Volume | Histogramme du volume avec sa moyenne |
| Prix moyen pondéré par le volume (VWAP) | Prix moyen pondéré par le volume, remis à zéro chaque jour, semaine, mois ou année |
| VWAP ancré | VWAP depuis une barre choisie, avec deux bandes d'écart-type |
| Prix moyen pondéré par le temps (TWAP) | Moyenne simple des prix de la période |
| Money Flow Index (MFI) | RSI pondéré par le volume |
| On Balance Volume (OBV) | Cumul du volume des barres haussières moins celui des baissières |
| Chaikin Money Flow | Pression acheteuse contre vendeuse sur N barres |
| Accumulation/distribution | Cumul du volume pondéré par la position de la clôture |
| Oscillateur de Chaikin | Momentum de l'accumulation/distribution |
| Price Volume Trend | Volume pondéré par la variation du prix en %, cumulé |
| Ease of Movement | Mouvement du prix par unité de volume |
| Elder Force Index | Variation du prix × volume |
| Oscillateur de Klinger | Oscillateur de tendance fondé sur le volume, avec signal |
| Oscillateur de volume | Écart entre une moyenne rapide et une lente du volume |
| Volume net | Volume signé par le sens de la barre |
| Profil de volume, zone visible (VPVR) | Volume par prix sur les barres à l'écran, avec POC et zone de valeur |
| Profil de volume de session (SVP) | Volume par prix pour chaque jour, semaine ou mois |

### Volatilité

| Indicateur | Signification |
|---|---|
| Bandes de Bollinger | Moyenne ± N écarts-types |
| Bandes de Bollinger %B | Position du prix dans les bandes |
| Largeur des bandes de Bollinger | Écart entre les bandes |
| BBTrend | Bandes de Bollinger courtes contre longues : sens et force de la tendance |
| Canaux de Keltner | Moyenne ± N ATR |
| Canaux de Donchian | Plus haut et plus bas sur N barres |
| Enveloppe | Moyenne ± un pourcentage fixe |
| Average True Range (ATR) | Amplitude moyenne des barres, gaps compris |
| Écart-type | Dispersion des prix |
| Volatilité historique | Écart-type annualisé des rendements |
| Indice de volatilité relative | RSI calculé sur l'écart-type |

### Momentum

| Indicateur | Signification |
|---|---|
| Indice de force relative (RSI) | Gains contre pertes, de 0 à 100 |
| Couleurs de bougies RSI | Colore les bougies selon la zone de RSI |
| Connors RSI | RSI, RSI des séries et rang de la variation, combinés |
| Stochastique | Clôture dans l'amplitude récente (%K, %D) |
| RSI stochastique | Stochastique appliqué au RSI |
| MACD | Écart entre deux EMA, signal et histogramme |
| Commodity Channel Index (CCI) | Écart à la moyenne en écarts moyens |
| Williams %R | Clôture dans l'amplitude récente, de 0 à −100 |
| Transformée de Fisher | Prix transformé en oscillateur quasi normal |
| Taux de variation (ROC) | Variation en % sur N barres |
| Momentum | Variation du prix sur N barres |
| Oscillateur de momentum de Chande | Hausses moins baisses rapportées à leur somme |
| Indice de corrélation de rang | Corrélation de rang du prix avec le temps |
| TRIX | Taux de variation d'une triple EMA |
| Awesome Oscillator | Moyenne 5 contre 34 du milieu des barres |
| Ultimate Oscillator | Pression acheteuse sur trois fenêtres |
| True Strength Index | Momentum doublement lissé |
| Indicateur/oscillateur SMI Ergodic | TSI avec signal et oscillateur |
| Oscillateur de prix (PPO) | MACD en pourcentage |
| Courbe de Coppock | Somme lissée de deux ROC longs |
| Know Sure Thing | Somme pondérée de quatre ROC lissés |
| Oscillateur de prix sans tendance | Prix moins une moyenne décalée : les cycles |
| Elder Ray (Bull/Bear Power) | Plus haut et plus bas face à une EMA |
| Balance of Power | Clôture moins ouverture rapportée à l'amplitude |
| Indice de vigueur relative | Clôture-ouverture contre plus haut-plus bas, lissé |

### Tendance

| Indicateur | Signification |
|---|---|
| Mouvement directionnel (DMI / ADX) | +DI, −DI et force de la tendance |
| Indice directionnel moyen (ADX) | Force de la tendance seule |
| Aroon | Barres depuis le plus haut et le plus bas |
| Indicateur Vortex | Mouvement haussier contre baissier |
| Choppiness Index | En tendance (bas) ou hachée (haut) |
| Chop Zone | Couleur selon l'angle de l'EMA 34 |
| Indice de force de tendance | Corrélation du prix avec le temps, de −1 à +1 |
| Mass Index | Élargissement des amplitudes qui précède les retournements |
| SAR parabolique | Points de stop suiveur qui basculent au retournement |
| Supertrend | Bande d'ATR qui suit la tendance |
| Stop de volatilité | Stop suiveur en ATR |
| Chande Kroll Stop | Stops long et short en ATR |
| Nuage d'Ichimoku | Lignes de conversion et de base, spans avancés (nuage), span retardé |
| Alligator de Williams | Mâchoire, dents et lèvres, trois moyennes décalées |
| Points pivots standard | P, R1–R3, S1–S3 du jour, de la semaine, du mois ou de l'année précédents |
| Plus haut/bas de la veille | Plus haut et plus bas du jour, de la semaine ou du mois précédents |
| Points pivots hauts/bas | Sommets et creux marqués sur le graphique |
| Zig Zag | Mouvements supérieurs à un pourcentage (graphique seul : il utilise les barres suivantes) |

### Figures, quant et flux d'ordres

| Indicateur | Signification |
|---|---|
| Figures de chandeliers | Marque les figures de bougies classiques |
| Fractales de Williams | Marque les sommets et creux fractals |
| Régime de Markov (haussier, baissier, latéral) | Régime selon le rendement glissant ; probabilités du régime suivant d'après les transitions passées |
| Heatmap de liquidité | Profondeur du carnet Binance dans le temps (voir Flux d'ordres) |
| Heatmap des liquidations | Niveaux de liquidation estimés (voir Flux d'ordres) |
| Niveaux de liquidité (SMC) | Liquidité buy-side et sell-side, balayages (voir Flux d'ordres) |
| Big Trades | Gros ordres agressifs en bulles (voir Flux d'ordres) |

## Dessins

### Outils de dessin

Chaque outil se place en un à trois clics sur le panneau des prix.

| Outil | Clics | Remarques |
|---|---|---|
| **Ligne de tendance**, **Rayon**, **Ligne étendue** | 2 | Segment, prolongé à droite, prolongé des deux côtés |
| **Ligne horizontale**, **Rayon horizontal**, **Ligne verticale** | 1 | |
| **Canal parallèle** | 3 | |
| **Rectangle** | 2 | |
| **Retracement de Fibonacci** | 2 | Niveaux 0, 0.236, 0.382, 0.5, 0.618, 0.786, 1, 1.618 |
| **Mesure de prix** | 2 | Variation, pourcentage et nombre de barres |
| **Position longue**, **Position courte** | 2 | Premier clic = entrée, second = stop et largeur ; objectif à 2 R ; affiche entrée, stop, objectif et R:R |
| **Note** | 1 | Demande le texte |

### Placer et gérer les dessins

Choisissez un outil dans la barre de dessin, puis cliquez sur le panneau des prix, le seul endroit où vivent les dessins. Entre deux clics, la forme suit la souris, et défilement et zoom sont coupés tant qu'un outil est armé. Une fois le dessin posé, le curseur revient, sauf si vous avez choisi **Garder l'outil** dans les réglages.

Avec le curseur, un clic sur un dessin le sélectionne. Suppr ou Retour arrière, ou encore la corbeille, l'efface, et Échap le désélectionne. **Alerte quand le prix croise la ligne sélectionnée** ouvre la fenêtre d'alerte avec cette ligne déjà choisie ; cela fonctionne avec les lignes de tendance, les rayons, les lignes étendues, les lignes et rayons horizontaux, et la première ligne d'un canal. **Supprimer tous les dessins de ce symbole** demande d'abord confirmation.

Les dessins appartiennent au symbole et non au graphique : tous les graphiques de ce symbole les affichent, et ils reviennent chaque fois que vous le rouvrez. Sachez qu'un dessin placé ne peut plus être ni déplacé ni recoloré.

## Flux d'ordres

### Quels marchés

La plupart des outils de flux d'ordres lisent les trades et le carnet d'ordres d'une plateforme, et pour l'instant cela veut dire Binance. Footprint, Big Trades et heatmap de liquidité exigent un graphique Binance, spot ou perpétuel USD-M (`.P`), chargé depuis Binance même et non depuis une source de repli, et sans symbole comparé. Partout ailleurs, la légende explique pourquoi rien n'est tracé.

La heatmap des liquidations exige un graphique crypto Binance, puisqu'elle lit les statistiques USD-M de Binance. Les niveaux de liquidité (SMC), en revanche, fonctionnent sur tous les graphiques, car ils n'utilisent que les barres.

Quand l'onglet est masqué depuis 30 s, le footprint, Big Trades et les heatmaps s'arrêtent et ferment leurs connexions, sauf un Big Trades avec alerte et une heatmap qui enregistre. Ils repartent à votre retour.

### Footprint

Le type de graphique **Footprint** ouvre chaque bougie pour montrer le volume échangé à chaque prix, d'après les trades Binance. Au chargement, il récupère jusqu'à 300,000 trades récents, soit environ 1 à 4 heures de BTCUSDT, puis suit les trades en direct. Les bougies plus anciennes restent classiques, sauf si l'historique de footprint du marché est stocké dans [Data › Technical data](help:data), en niveaux de 1 point de base. La légende indique depuis quand le footprint est complet, et une bougie au footprint complet tire son ouverture, son plus haut, son plus bas, sa clôture et son volume de ses propres trades.

Ce que vous voyez dépend du zoom : des chiffres quand les lignes sont assez hautes, des couleurs seules quand elles se resserrent, et des bougies classiques quand c'est trop étroit. L'engrenage à côté de **Type de graphique** ouvre **Réglages du footprint**.

| Réglage du footprint | Choix |
|---|---|
| **Affichage** | **Bid x Ask** (ventes au bid, achats à l'ask), **Delta** (ask − bid), **Profil de volume** |
| **Lignes de prix** | **Auto (suit le zoom)** ou de 1 à 10,000 ticks par ligne |
| **Ratio de déséquilibre** | 3 par défaut : acheteurs à un prix au moins ce nombre de fois les vendeurs de la ligne du dessous (et l'inverse) |
| **Volume minimum pour un déséquilibre** | Les lignes en dessous ne comptent jamais |
| **Déséquilibres empilés : lignes consécutives** | De 2 à 8 (3 par défaut), tracés en bande à droite des cases |
| **Point of control (POC)** | Contour de la ligne au plus fort volume |
| **Delta et volume sous la bougie** | Afficher / masquer |
| **Couleurs : acheteurs, vendeurs, POC** | Couleurs du thème tant qu'elles ne sont pas changées |

### Big Trades

Big Trades signale les gros ordres agressifs par des bulles, placées au prix moyen et à l'instant de chaque ordre ; la couleur donne le sens et la taille le volume. Survoler une bulle affiche son sens, son heure, son volume, son prix moyen, sa fourchette de prix et ses exécutions, et la légende affiche le seuil en cours.

Pour retrouver ces ordres, Deepwater commence par regrouper les trades, selon **Regroupement, ms** et **Prix d'un même ordre** (même prix seulement, ou balayage sur plusieurs niveaux). Le **Seuil** décide ensuite de ce qui compte comme gros. En **Auto**, c'est un centile des ordres récents sur une fenêtre en minutes, ajustable à la volatilité, qui demande d'abord une phase de démarrage ; en **Manuel**, vous fixez un volume minimum. Dans les deux cas, vous pouvez ajouter un maximum.

Côté aspect, vous choisissez le **Marqueur** (cercle, carré, losange ou texte), la plus petite et la plus grande taille, la plus faible opacité, et l'affichage du volume dans les grands marqueurs. L'**Alerte** peut être **Visuelle** (message et notification système), **Sonore**, ou **Sonore et visuelle**, pour les ordres d'au moins N fois le seuil d'affichage (3 par défaut). Les alertes arrivent au plus une par seconde, les autres étant comptées dans la suivante.

### Heatmap de liquidité

La heatmap de liquidité trace le carnet d'ordres sous les bougies : plus un prix est clair, plus la taille qui y attend est grande. Les meilleurs bid et ask courent en deux lignes, et des hachures marquent les moments où le carnet n'était pas synchronisé. **Carnet d'ordres** choisit **Celui du graphique**, ou le carnet spot ou perpétuel USD-M de la même paire.

À l'écran, vous avez des colonnes de 100 ms sur les 10 dernières minutes et de 1 s sur les 6 dernières heures, plus ce que les sessions précédentes ont enregistré. **Enregistrer sur disque (1 s)** est activé par défaut, et **Jours gardés sur disque** vaut 7.

Pour affiner l'aspect, vous pouvez régler le pas de prix d'une case, la façon dont se combinent les prix qui partagent une case (la plus grosse quantité ou leur somme), la palette (Profonde, Thermique, Mono), une échelle des couleurs log ou linéaire, les centiles du contraste auto, les bornes de quantité, la courbe du contraste et l'opacité.

### Heatmap des liquidations

La heatmap des liquidations estime l'endroit où les positions à levier ouvertes sur ces bougies seraient liquidées ; plus c'est clair, plus il y en a. Elle repose sur l'open interest Binance, en points de 5 minutes, et sur le ratio long/short des comptes. Binance garde 30 jours d'open interest et Deepwater conserve ce qu'il collecte tant qu'il tourne : la légende indique donc sur combien de bougies l'estimation repose.

**Positions** montre les deux côtés, les longs ou les shorts. **Répartition des leviers** peut être standard (5–100×), élevée (25–125×), faible (2–20×), ou **Calibrée sur les liquidations réelles**, ce qui demande 30 liquidations réelles dans les bougies. Les autres réglages portent sur la position type, les longs contre les shorts, le biais de la bougie, la demi-vie d'effacement, la part retirée quand le prix atteint un niveau, les lignes de prix, la palette, le contraste et l'opacité. **Liquidations réelles (points)** ajoute les liquidations publiées par Binance, rouges pour les longs, vertes pour les shorts, de taille selon la quantité.

### Niveaux de liquidité (SMC)

Cet indicateur repère la liquidité buy-side au-dessus des sommets et la liquidité sell-side sous les creux, plus forte quand sommets ou creux sont égaux. Un niveau est balayé quand le prix le traverse, puis il reste grisé pendant 20 bougies. Les niveaux sont tracés en zones ou en lignes, avec étiquettes ; leur opacité suit un score fait des membres, des tests, de l'âge et de l'unité de temps, et **Score minimal affiché** masque les plus faibles.

Les sommets et les creux viennent des **Pivots** : une fenêtre de bougies de chaque côté, un retournement en pourcentage ou en ATR. Vous réglez aussi la tolérance des equal highs / lows, l'épaisseur des zones (en ATR, en pourcentage ou en ticks), et si un niveau est **Balayé par** une mèche ou une clôture. L'**Unité de temps de détection** est celle du graphique ou n'importe laquelle de 15m à 1W, sur bougies clôturées seulement. En plus des sommets et des creux, l'indicateur peut tracer les niveaux du jour et de la semaine précédents, le jour commençant à 00:00 UTC ou à 17:00 heure de New York, et ceux des sessions Asie, Londres et New York, chacun avec une variante ICT.

## Alertes

### Créer une alerte

Une alerte se crée depuis **Alerte** dans la barre du haut, depuis le bouton d'alerte de la barre de dessin quand une ligne est sélectionnée, ou depuis **Nouvelle alerte** dans le panneau latéral. La fenêtre commence par le **Symbole**, la portée sous **Vérifier sur** (**Ce symbole seulement** ou **Chaque symbole de** une liste) et l'**Unité de temps**.

Viennent ensuite jusqu'à cinq **Conditions**, chacune faite d'un côté gauche, d'un opérateur et d'un côté droit. Un côté peut être une valeur, un champ de prix (ouverture, plus haut, plus bas, clôture, volume), n'importe quelle sortie d'indicateur avec ses propres réglages, ou une ligne tracée. Les opérateurs sont : est au-dessus de, est sous, est ≥ à, est ≤ à, passe au-dessus de, passe sous, croise. **Déclencher si** choisit entre **toutes sont vraies** et **l'une est vraie**, et **Déclenchement** entre **Une seule fois**, **Une fois par barre** et **Une fois par clôture de barre**.

Pour finir, vous pouvez ajouter un **Message (facultatif)** et, sous **Envoyer aussi à (facultatif)**, une URL de webhook Discord, Slack ou Telegram ; **Tester** envoie un message d'essai.

### Au déclenchement

Les alertes sont vérifiées toutes les 15 secondes tant que l'application tourne, même onglet fermé. Une alerte déclenchée affiche un message pendant 12 s, émet un bip et envoie une notification système.

L'onglet **Alertes** du panneau latéral liste chaque alerte avec son état (**Active** ou **Arrêtée**), son déclenchement, son webhook et le nombre de fois où elle s'est déclenchée, ainsi que **Arrêter** / **Relancer** et **Supprimer**. Sous la liste, **Récentes** montre les 30 derniers déclenchements.

## Panneau latéral

### Liste

La liste de suivi présente des listes de symboles avec leur prix et leur variation du jour, rafraîchis toutes les 5 s tant que l'onglet est affiché. La liste par défaut, **Main**, contient BTC, ETH, SOL, AAPL, NVDA, SPY, QQQ et EUR/USD. **+ Liste** crée une nouvelle liste et **Supprimer** retire la liste courante, sachant qu'il doit toujours en rester une.

Pour ajouter un symbole à la liste courante, tapez-le dans **Ajouter un symbole (Entrée)** ; × le retire. Un clic sur un symbole l'ouvre sur le graphique actif.

### Calendrier et infos

Le calendrier a trois vues. **Économique** montre les publications de la semaine avec prévision et précédent, filtrées sur **Tous**, **Moyen + fort** ou **Impact fort**. **Résultats** et **Dividendes** couvrent les cinq prochaines séances, et **Mes listes** ne garde que vos symboles.

**Infos** donne les titres liés au graphique actif : cryptos chez Cointelegraph et Decrypt, actions US chez Nasdaq. Un clic sur un titre ouvre l'article.

## Testeur de stratégie

### Construire une stratégie

Le testeur fait tourner une stratégie à base de règles sur les barres du graphique actif. Le plus simple pour commencer est **Charger une stratégie…**, qui propose des **Exemples** (Croisement EMA (9/21), Retour à la moyenne RSI (30/55), Suivi Supertrend, MACD + filtre de tendance EMA 200, Breakout Bollinger), vos stratégies **Enregistrées** et les **Stratégies Pine**.

Une stratégie repose sur quatre listes de conditions : **Acheter (entrée long) quand**, **Vendre (sortie long) quand**, **Entrer short quand** et **Sortir du short quand**. Les conditions d'entrée doivent toutes être vraies, et une liste laissée vide est ignorée. Les signaux utilisent la clôture de la barre et s'exécutent à l'ouverture de la suivante. Le reste du formulaire règle les sorties, le risque et les coûts :

- **Sorties** : **Sortir quand** toutes ou une seule des conditions de sortie sont vraies, et **Sortir après N barres**.
- **Risque** : **Stop loss %**, **Take profit %** et **Stop suiveur %**.
- **Capital** : **Capital initial**, puis la taille de position, les frais et le slippage par côté et le spread du bid à l'ask, chacun en pourcentage ; **Spread en direct** lit le bid et l'ask du moment (crypto).
- **Exécutions dans une barre**, pour les stops et les objectifs : stop d'abord si une barre touche les deux, chemin TradingView (ouverture, extrême proche, extrême lointain, clôture), ou **Loupe de barres**, qui utilise les vraies barres d'unité inférieure contenues dans chaque barre.

**Enregistrer la stratégie** lui donne un nom et la garde, et **Modifier en JSON** la montre dans le format qu'écrivent les agents IA.

Vous pouvez aussi apporter vos propres données. **Importer des barres (CSV)** attend time, open, high, low, close et volume ; les barres s'affichent ensuite en `CSV:NOM`, sur leur unité de temps et au-delà. **Importer un signal (CSV)** attend time et position (1 long, 0 neutre, −1 short, à partir de la clôture de la barre) et le charge sous forme de règles.

### Lancer un test

**Lancer sur le graphique** teste la stratégie sur le symbole et l'unité de temps du graphique actif. Les barres sont celles du graphique, sauf si [Data › Technical data](help:data) stocke ce marché et cette unité (BTCUSDT sur Binance, de 1m à 1W) : le test prend alors ses 100,000 barres les plus récentes, ou 20,000 pour une stratégie Pine. En replay, il ne voit que les barres du graphique jusqu'à la barre rejouée.

Les résultats commencent par les chiffres ci-dessous, suivis d'une courbe de capital face au buy & hold et de la liste des trades (sens, entrée, sortie, prix, P&L %, barres, motif de sortie).

| Résultat | Ce qu'il indique |
|---|---|
| **Profit net** | Ce que la stratégie a gagné ou perdu sur le test, en % du capital de départ |
| **Buy & hold** | Ce qu'aurait rapporté le simple fait de garder le marché de la première à la dernière barre, en % |
| **Drawdown max** | La plus forte baisse du capital depuis un sommet, en % |
| **Trades** | Nombre de trades |
| **Taux de réussite** | Part des trades gagnants |
| **Profit factor** | Gains bruts divisés par les pertes brutes |
| **Sharpe** | Rendement moyen par barre divisé par sa volatilité, annualisé |
| **Sortino** | Idem, en ne comptant que la volatilité à la baisse |
| **Trade moyen** | Résultat moyen d'un trade, en % |
| **Temps en position** | Part des barres passées en position, en % |

Entrées et sorties sont marquées sur le graphique ; **Effacer les marques** les retire et **Exporter les trades (CSV)** enregistre la liste.

Chaque backtest, les vôtres comme ceux des agents IA, est inscrit au **Registre d'essais**. Sa ligne donne le nombre de stratégies essayées sur ce marché et cette unité, et le Sharpe dégonflé de la meilleure : plus on essaie de versions, plus la meilleure risque de devoir sa place au hasard, et un Sharpe dégonflé au-dessus de 0.95 indique un avantage probablement réel.

### Contrôles quant

Un beau backtest peut encore tenir du hasard ou du surajustement. **Lancer les contrôles quant** relance la stratégie environ 25 fois avec des réglages voisins, sur votre ordinateur, et la juge sur cinq contrôles.

| Contrôle | Réussi quand |
|---|---|
| Walk-forward | Sharpe supérieur à 0 sur des données jamais utilisées pour choisir les réglages |
| Surajustement (PBO) | Moins de 50 % de risque que les meilleurs réglages apparents soient sous la moyenne sur des données inédites |
| Sharpe déflaté | Probabilité supérieure à 0.95 que l'avantage soit réel après les versions testées |
| Stress des coûts | Rendement supérieur à 0 avec frais doublés, slippage accru et spread doublé |
| Réglages voisins | Au moins 60 % des réglages voisins gardent la moitié du Sharpe |

Sous ces verdicts s'affichent aussi le tableau du walk-forward, une grille des rendements nets par paire de réglages (les vôtres encadrés), un Monte Carlo des trades et le Sharpe probabiliste. Walk-forward, surajustement et réglages voisins demandent un réglage entier à faire varier, comme une longueur d'indicateur : ils ne tournent donc ni sur des règles fixes ni sur un signal importé.

## Outils du panneau du bas

### Screener

Le screener passe en revue tout un marché d'un coup : **Crypto (paires USDT)**, **Actions US** ou **ETF US**. Vous pouvez le restreindre avec **Recherche**, **Prix min** / **Prix max**, **Var. % min** / **Var. % max**, **Volume min**, et pour les actions **Capitalisation min** et **Secteur**. Le tableau montre symbole, nom, prix, variation % et volume, plus capitalisation et secteur pour les actions, avec 500 lignes au plus ; un clic sur un en-tête trie, un clic sur une ligne ouvre son graphique.

**Filtre technique** ajoute une condition, vérifiée **Sur** 1h, 4h, 1D ou 1W. **Scanner le top 100** l'applique aux 100 lignes les plus échangées, et **Effacer le scan** la retire.

### Heatmap, Saisonnalité et Données financières

**Heatmap** dessine des tuiles pour les **Actions US (200 plus grandes)** ou les **Crypto (100 plus échangées)**, de taille selon la capitalisation ou le volume et colorées par la variation du jour ; un clic ouvre le graphique. **Saisonnalité** montre les variations mensuelles en % du symbole actif pour chaque année civile, d'après les barres journalières, sur 3, 6, 10 ou 20 ans, avec la moyenne et la part d'années en hausse.

**Données financières** couvre les actions cotées aux États-Unis : compte de résultat, bilan, flux de trésorerie, ratios et chiffres clés, en annuel ou en trimestriel, avec un graphique chiffre d'affaires / résultat net. Les chiffres viennent de Nasdaq, en milliers de dollars US.

## Paper trading

### Passer un ordre

Le paper trading permet de s'exercer sur les prix en direct, sans argent réel ni connexion à une plateforme ; pour de vrais ordres, voir [Compte](help:account). Le ticket porte sur le symbole du graphique actif. Choisissez le **Type** (**Au marché**, **Limite** ou **Stop**), le **Prix** et le **Montant ($)**, remplissez si vous le souhaitez **Stop loss % (facultatif)** et **Take profit % (facultatif)**, puis appuyez sur **Achat** ou **Vente / short**.

Un ordre au marché s'exécute au prix en direct. Limite et stop s'exécutent quand le prix en direct les atteint, et une limite ne s'exécute jamais à un prix moins bon que le sien. Un stop loss ou un take profit devient un ordre de sortie une fois l'entrée exécutée, et si vous fixez les deux, l'un annule l'autre, ce qu'indique **(bracket)**. Les ordres restent surveillés quand d'autres pages sont affichées, et une exécution envoie alors une notification système.

### Le compte

Le compte affiche **Capital**, **Liquidités** et **P&L total**, puis **Positions** avec **Clôturer**, **Ordres en attente** avec **Annuler**, et **Historique** avec les 100 dernières exécutions. **Solde initial** vaut 100,000 par défaut et s'applique par **Réinitialiser le compte** ; **Frais % par trade** vaut 0.05 par défaut.

## Replay de barres

Le replay de barres ramène un graphique dans le passé, pour vous exercer ou tester comme si le marché se déroulait de nouveau. Appuyez sur **Replay** dans la barre du haut, puis cliquez la barre de départ : les barres suivantes disparaissent.

La barre de replay propose **Reculer d'une barre**, **Lecture / pause** et **Avancer d'une barre**, à une **Vitesse** de 0.5x à 10x (2x par défaut). **Aller à…** choisit une nouvelle barre de départ, et **Quitter** repasse en direct, en rechargeant si une barre a clôturé entre-temps. Avec plusieurs graphiques, **Tous les graphiques** les fait tous rejouer jusqu'au même instant, et les autres affichent **Replay synchronisé**.

Pendant le replay, indicateurs, valeurs d'unité supérieure, couches de flux d'ordres, testeur et ticket paper ne voient que les barres jusqu'à la barre rejouée. Le ticket bascule aussi sur un compte d'entraînement séparé : les ordres s'exécutent aux prix des barres rejouées, limites et stops à mesure que le replay avance. Quitter le replay ramène au compte paper en direct.

## Pine Script et indicateurs personnalisés

### Éditeur d'indicateurs

L'**Éditeur d'indicateurs**, dans le panneau du bas, est l'endroit où vous écrivez vos propres indicateurs. Partez de **Nouvel indicateur**, qui ouvre un modèle JavaScript, ou de **Nouveau script Pine** ; donnez-lui un nom et du code, puis choisissez **Enregistrer** ou **Enregistrer et ajouter au graphique**. Chaque fichier est vérifié avant d'être enregistré, et les erreurs s'affichent sous le code.

**Vos indicateurs** liste ce que vous avez écrit, avec **Modifier**, **Ajouter au graphique** et **Supprimer**. Les indicateurs personnalisés figurent aussi dans le sélecteur sous **Personnalisés**, et un exemple, EMA ribbon squeeze, est installé la première fois que vous utilisez l'éditeur. Les agents IA peuvent également écrire des indicateurs pour vous (voir [IA](help:ai)).

### Prise en charge de Pine Script

Vous pouvez coller un script TradingView, version 5 ou 6 : il tourne en arrière-plan à l'intérieur de Deepwater, grâce à PineTS. Ses input deviennent les réglages de l'indicateur. Le premier input de source devient **source**, tandis que les input de couleur, d'unité de temps, de symbole, de session et de texte gardent leur valeur par défaut.

Les appels `plot`, `plotshape`, `plotchar`, `plotarrow` et `barcolor` du script sont tracés, de même que les niveaux et les couleurs par barre ; `bgcolor`, `fill`, les labels, lignes, boîtes, tableaux et polylignes ne le sont pas encore. Un script trop long ou trop gourmand en mémoire est arrêté, avec un message dans la légende, et une bibliothèque Pine ne trace rien.

Un script `strategy()` apparaît sous **Stratégies Pine** dans le testeur. Ses propres entrées, sorties, taille et coûts s'y appliquent, exécutés comme TradingView exécute les ordres, et ses réglages se changent dans l'éditeur. Les contrôles quant font varier ses deux premiers input entiers.

## Raccourcis et souris

### Clavier

Ces touches fonctionnent sur la page Graphiques ; les raccourcis communs à toute l'application sont dans [Deepwater](help:start).

| Touche | Action |
|---|---|
| Une lettre, un chiffre ou ^ | Recherche de symbole commençant par ce caractère |
| ↑ / ↓, Entrée | Parcourir les résultats, ouvrir |
| Entrée (champ d'unité de temps) | Appliquer l'unité tapée |
| Échap | Fermer la fenêtre, sinon revenir au curseur, sinon désélectionner le dessin |
| Suppr / Retour arrière | Supprimer le dessin sélectionné |

### Souris

| Geste | Action |
|---|---|
| Molette (verticale) | Zoom autour du pointeur |
| Molette horizontale, Maj + molette | Défilement dans le temps |
| Glisser sur le graphique | Défilement en temps et en prix |
| Glisser l'échelle de prix | Étirer les prix |
| Glisser l'échelle de temps | Étirer le temps |
| Double-clic sur l'échelle de prix | Réajuster automatiquement les prix |
| Double-clic sur l'échelle de temps | Réinitialiser l'échelle de temps |
| Glisser entre panneaux | Redimensionner les panneaux d'indicateurs |

## Enregistrement

Tout ce que contient cette page est enregistré sur votre ordinateur, dans le dossier charts du dossier de données de Deepwater, un instant après chaque changement et quand vous quittez l'onglet. Cela comprend la disposition, le graphique actif, les réglages de chaque graphique, les dessins de chaque symbole, les listes, les réglages et le style du graphique, vos stratégies enregistrées et les comptes paper. Alertes, indicateurs personnalisés, fichiers CSV importés et enregistrements de heatmap sont gardés dans le même dossier, sous forme de fichiers.
