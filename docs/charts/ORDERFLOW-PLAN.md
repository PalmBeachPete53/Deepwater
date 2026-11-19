# Onglet Graphiques — 5 ajouts : plan

## État
- 2026-11-18 : plan validé (« feu vert », questions sans réponse → recommandations par défaut : lecteur JSON maison,
  footprint en type de graphique avec la règle des bords de novembre et 300 000 trades, tout symbole Binance).
- **Étape A faite** (socle trades) : `src/charts/core/{Ticks.h, BinanceMsg, Trades}`, `src/charts/data/{ExchangeFeed.h,
  BinanceFeed.cpp, TradeStream}`, tests `tests/charts/core/flow_trades_tests.cpp` (+ compteur d'allocations
  `AllocCount.h` / `core/alloc_count.cpp`) et `tests/charts/data/tradestream_tests.cpp`, contrôle en direct
  `tests/charts/tools/trades_live.cpp` (`charts_trades_live`). Correction au passage : `LiveFeed` futures sur
  `/market/ws/` (l'ancien `/ws/` ne recevait plus rien depuis le 2026-04-23).
- Contrôle en direct de l'étape A (2026-11-18, 400 s, budget 300 000) : spot BTCUSDT 699/699 bougies 1 min identiques
  aux klines Binance (volume, volume acheteur, plus haut/bas) ; perpétuel : bande identique à l'API aggTrades, mais les
  klines futures rangent quelques trades dans la minute voisine (écarts qui se compensent d'une minute à l'autre).
  Conséquence pour B1 : en futures, un niveau du footprint peut sortir du plus haut/bas de la bougie (1 minute sur 322).
- 2026-11-18 : choix de l'utilisateur pour le footprint : OHLC des bougies calculés à partir des trades.
- **Étape B1 faite** : `src/charts/core/Footprint.{h,cpp}` (FootprintBook : cellules triées par tick par bougie, ordre
  d'arrivée indifférent, OHLC par id ; `analyze()` : lignes denses par regroupement, delta, POC égalité → plus bas,
  déséquilibres diagonaux avec la règle des bords, minVolume, stacked). Tests `tests/charts/core/footprint_tests.cpp`
  (14, mutations 12/12). Mesuré : 15 ns par trade, 0,7 µs par bougie de 123 niveaux. Données réelles BTCUSDT spot :
  136/136 bougies 1 min du footprint identiques aux klines (volume, volume acheteur, plus haut/bas).
- **Étape B2 faite** (rendu Bid x Ask) : type de graphique « Footprint » (marchés Binance spot et perpétuels),
  `src/pages/charts/flow/FootprintFeed` (flux partagé → bougies du pane, ajouts en direct, reconstruction quand
  l'historique arrive, signal ≤ 10/s) et `FootprintLayer` (fine bougie, bid | ask par niveau, POC, déséquilibres,
  stacked, delta et volume sous la bougie ; niveaux chiffres → couleurs → bougies selon la place ; lignes Auto 1-2-5 :
  ≥ 14 px pour les chiffres, ≥ 4 px pour les couleurs). Bougies du footprint = OHLC des trades. Note dans la légende
  (chargement, depuis quand, reconnexion). Étape dev `fp:bars:<n>`, `fp:log`, `fp:perf`. Mesuré : 2,7 à 5 ms par
  image pour tout le graphique, dont 0,3 à 1,2 ms pour le footprint.
- **Étape B3 faite — footprint terminé** : modes Delta (une valeur par niveau, vert/rouge, intensité ∝ |delta|) et
  Profil de volume (barre par niveau, vendeurs puis acheteurs, POC plein) ; déséquilibres en marques de bord dans ces
  deux modes ; réglages `src/charts/core/FootprintSettings` (cfg « footprint » du graphique, valeurs bornées, testés) ;
  fenêtre « Réglages du footprint » (roue dentée à côté du type de graphique, `modal:footprint`) : affichage, lignes
  (Auto ou 1…10 000 ticks avec le prix), ratio, volume minimum, N empilés, POC, totaux, couleurs (vides = thème).
  Étape dev `fp:mode:<bidask|delta|profile>`. Mesuré : 4,8 à 5,2 ms par image, dont 0,4 à 0,8 ms de footprint.
- **Étape C1 faite** (Big Trades, cœur) : `src/charts/core/BigTrades.{h,cpp}` — Aggregator (fenêtre depuis la 1re
  exécution, 50 ms, option même horodatage, même prix ou balayage avec VWAP/min/max), RollingPercentile (blocs triés,
  0,36 µs par ordre avec 90 000 valeurs), MinuteAtr (ATR 14/120 sur barres 1 min, borné 0,5–2), Detector (manuel
  min/max avec max 0 = sans limite ; auto p99 × volatilité, jugé avant d'être ajouté, repli pendant le démarrage),
  bulles (aire ∝ volume, bornes, opacité). Tests `tests/charts/core/bigtrades_tests.cpp` (17, mutations 16/16).
  Données réelles BTCUSDT spot : 300 279 trades → 251 842 ordres, 1,01 % jugés « big » au p99 ; les plus gros sur des
  prix ronds (85,7 BTC vendus à 63 000).
- **Étape C2 faite** (bulles, mode manuel) : indicateur « Big Trades » (id `bigtrades`, groupe Order flow, après les
  88 indicateurs du JS ; entrées min et max), `src/pages/charts/flow/BigTradesModel` (un modèle par indicateur, calcul
  de l'historique sur un worker à partir d'une copie de la bande, au plus toutes les 2 s, puis ajouts en direct ;
  fermeture des ordres sur l'horloge de l'exchange) et `BigTradesLayer` (bulle au prix moyen, placée dans la barre
  selon l'heure, couleur du côté, aire ∝ volume, plus grandes d'abord ; images pré-dessinées : 7,5 ms → 0,04–0,26 ms
  pour ~140 bulles). Règle ajoutée : en manuel, min 0 = rien. Marché non Binance : message dans la légende. Étape dev
  `big:min:<v>` / `big:log` (`bt:` est déjà pris par le testeur).
- **Étape C3 faite** (mode automatique) : entrées Seuil Auto/Manuel (Auto par défaut), percentile (99), fenêtre
  (30 min), volatilité Oui/Non, min (manuel ; secours au démarrage), max. Étiquette compacte « auto p99 30 min »,
  « ≥ 2 », « 2–50 » ; seuil courant dans la légende « > 0.500 (p99 0.500 × 1.00) », « démarrage 37/100 »,
  « chargement des trades… » (rafraîchi au plus 1 fois/s, sans ramener le réticule à la dernière barre). Règle
  ajoutée : en auto, « big » = strictement au-dessus du percentile (les tailles se regroupent sur des lots ronds :
  à BTCUSDT, p99 = exactement 0,5 BTC). Densité constatée : ~50 000 ordres/h sur BTCUSDT spot → p99 ≈ 500 bulles/h.
- **Étape C4 faite** (apparence + info-bulle) : entrées regroupement (ms, 0 = même horodatage), prix d'un même ordre
  (même prix / balayage), marqueur (cercle, carré, losange, texte), tailles min/max, opacité min, volume dans les grands
  marqueurs ; couleurs achats/ventes (sorties « couleur seule », `drawnOnly` : jamais proposées dans les règles,
  alertes, screener ni « indicateur sur indicateur » ; couleur du thème tant qu'on n'en choisit pas). Fenêtre de
  réglages sans « Unité de temps » pour Big Trades. Info-bulle au survol (heure exacte, côté, volume, prix moyen,
  prix min–max, exécutions, côté estimé). Textes en images pré-dessinées aussi. Étape dev `big:hover`.
- **Étape C5 faite — Big Trades terminé** (l'utilisateur a dit oui aux alertes) : entrées Alerte (Non, Visuelle,
  Sonore, Sonore et visuelle ; Non par défaut) et facteur (≥ 1, défaut 3 × le seuil d'affichage), règle pure
  `bigTradeAlert` testée ; seulement les ordres jugés en direct, de moins d'une minute (jamais le recalcul de
  l'historique) ; au plus une par seconde, les suivantes comptées dans la prochaine ; toast + notification macOS
  (visuelle), bip système (sonore). Marche tant que Deepwater tourne, onglet Graphiques affiché ou non (le flux de
  trades est dans charts/data) ; pas quand Deepwater est fermé.
- 2026-11-18 : choix de l'utilisateur pour l'enregistrement du carnet : colonnes à taille fixe, 1 s sur disque
  (~4 Mo/h par symbole), 100 ms à l'écran.
- **Étape D1 faite** (carnet + synchro) : `src/charts/core/OrderBook.{h,cpp}` — OrderBook (fenêtre dense de 131 072
  ticks par côté, niveaux lointains dans un vecteur trié, meilleur bid/ask suivi, recentrage quand le milieu sort de la
  moitié centrale, 7,4 ns par mise à jour, aucune allocation) et BookSync (procédure Binance spot et futures vérifiée
  dans la doc, carnet croisé → resynchro). Tests `tests/charts/core/orderbook_tests.cpp` (9 : référence std::map sur
  200 000 opérations, scénarios à la main, échange simulé 300 tours × 2 marchés avec snapshot à n'importe quel moment,
  toujours identique), mutations 13/13, ASan/UBSan propres.
- **Étape D2 faite** (réseau + thread du carnet) : lecteur maison `depthUpdate` / snapshot (sans allocation),
  `Spsc.h` (anneau SPSC, modèle mémoire documenté), `Heatmap.{h,cpp}` (DepthColumn 512 lignes à taille fixe,
  DepthSampler : lignes Auto ≈ 2e-5 du prix, bande fixe tant que le prix reste dans sa moitié centrale, colonnes
  Missing quand le carnet n'est pas synchronisé), `IOrderBookSink` / `IDepthSocket` (+ socket Binance : spot /ws/,
  futures /public/ws/), `DepthStream` (un thread par marché ; colonnes, statut et snapshots par anneaux SPSC, demande
  de snapshot par drapeau atomique, démarrage/arrêt par started/finished : aucun appel Qt en file entre les threads ;
  snapshots ≥ 10 s d'écart, backoff, reconnexion, renouvellement 23 h 50). Tests : 7 purs + 5 flux (faux échange :
  trou, coupure, futures → carnet final identique), TSan propre (0 avertissement), ASan. En direct (95 s) : spot et
  perpétuel synchronisés en 2 s, 0 resynchro, 0 colonne manquante, CPU < 1 %, ~10 µs par événement appliqué, meilleur
  bid/ask égal au bookTicker de Binance quand les deux sont pris au même moment.
- **Étape D3 faite** (enregistreur) : `src/charts/core/HeatmapStore.{h,cpp}` — codes 16 bits par ligne (log, 512 pas
  par doublement, 0,068 % au plus), DepthRecord 1 064 octets, ColumnRing (anneau préalloué à indices absolus),
  HeatmapWriter (fichiers horaires UTC `<data>/charts/depth/<spot|usdm>-<SYMBOLE>/<aaaa-mm-jjTHH>.dwhm`, en-tête
  versionné de 64 octets, enregistrements fixes, dernier enregistrement coupé ignoré puis tronqué à la reprise),
  HeatmapReader (QFile::map), loadRange, pruneOlderThan. DepthStream : un thread d'écriture (std::thread, file SPSC
  depuis le thread du carnet pour les colonnes sur seconde entière, flush toutes les 5 s, aucun verrou), anneaux GUI
  live (100 ms, 10 min) et history (1 s, 6 h, rechargé depuis les fichiers au démarrage). Enregistrement actif par
  défaut, 7 jours gardés (réglable en D5). Tests : 5 purs + 1 de bout en bout (relecture octet par octet, rechargement
  au redémarrage), TSan et ASan propres. En direct : 3,83 Mo/h, aucune seconde manquante dans le temps.
- **Étape D4 faite** (rendu sous les bougies) : `src/charts/core/HeatmapRender.{h,cpp}` (pur) — table de 65 536
  couleurs par code (palettes Deep / Thermal / Mono, échelle log ou linéaire, gamma), contraste = médiane et 99,5 % des
  quantités visibles, `rasterColumn` (maximum par pixel sur les enregistrements et les lignes de prix, transparent hors
  bande, hachuré si tout est Missing). `src/pages/charts/flow/HeatmapLayer.{h,cpp}` — primitive Bottom, une image à 1 px
  par point ; temps de chaque colonne de pixels tiré des barres, colonnes 100 ms (anneau live) sinon 1 s (historique +
  fichiers) ; reconstruction seulement si la vue change, sinon colonnes nouvelles seules ; enregistrements d'une même
  bande fusionnés par colonne (max exact, plus d'échantillonnage) ; best bid/ask peints dans l'image avec un liseré
  sombre ; replay : rien après la barre rejouée. Indicateur `heatmap` (drawnOnly, groupe Order flow), branché dans
  ChartPane comme Big Trades (un `DepthStream` partagé par marché). Étape dev `heat:` (add, replay:N, log, perf).
  Outil `charts_heatmap_synth` (6 h synthétiques dans DW_PROFILE, pour mesurer). Tests : 3 purs (core 223, data 114).
  Mesures (Retina 1600×1000) : vue fixe 1,6–2,5 ms/image (dont 1,5–2,4 ms d'affichage de l'image) ; défilement sur 6 h
  d'historique 5,7 ms (29,8 ms avant la fusion par bande) ; temps réel 1 s 3,3 ms. Pas de pyramide : inutile mesuré.
- **Étape D5 faite** (réglages) : l'indicateur `heatmap` a 14 réglages — carnet (celui du graphique / spot / perpétuel
  USD-M), pas de prix d'une case, prix d'une case = le plus gros ou la somme, palette (Profonde / Thermique / Mono),
  échelle log ou linéaire, contraste auto (centiles 50 / 99,5) ou par quantités, courbe (gamma), opacité, lignes bid/ask,
  enregistrement on/off et jours gardés. Rendu en deux temps (`rasterCodes` → code par pixel, `colourise` → couleur) :
  le contraste vient de ce qui est dessiné (sommes comprises). `DepthStream::record(on, jours)` change l'enregistrement en
  marche (drapeau atomique lu à chaque échantillon, écrivain démarré à la demande, élagage immédiat si moins de jours, puis
  toutes les heures). Légende : « connexion… », « synchronisation du carnet… », « enregistrement » ; libellé court
  (« Spot 5 Σ »). Étape dev `heat:set:<réglage>=<valeur>`. Corrigé : les lignes bid/ask écrivaient hors de l'image quand
  le prix sortait de l'écran (plantage). Tests : core 224 (+1), data 115 (+1) ; TSan 0, ASan propre.
- Chantier D terminé.
- **Étape E1 faite** (modèle des liquidations, pur) : `src/charts/core/Liquidation.{h,cpp}` — `LiquidationModel`
  (grille prix fixe × barres, état en double, colonnes en float contiguës, `step` par barre = `run` complet bit à bit),
  `liquidationPrice` (formule isolée de Binance avec taux et montant de maintenance du palier), `bracketFor`. Hypothèses
  écrites en tête du fichier : hausse d'OI = autant de longs que de shorts ouverts (plafonnée par le volume), partage par
  le ratio de comptes (50/50 sinon) et option biais bougie (5–95 %), prix d'entrée = prix typique, éventail de leviers
  pondéré plafonné par le palier, consommation quand le prix atteint le niveau (ligne partiellement atteinte = partie
  retirée, part réglable), pas de liquidation dans la bougie d'ouverture, baisse d'OI = réduction proportionnelle,
  effacement optionnel (demi-vie en barres), niveaux hors grille comptés. Tests : 8 (formule contre l'expression de
  Binance, scénario OI monte puis prix chute, consommation partielle longs et shorts, baisse d'OI, plafond volume, OI
  inconnu, ratio et biais, éventail et plafond de levier, effacement, hors grille, sorties de grille par le bas et le
  haut, incrémental == complet bit à bit, zéro allocation après `reserve`). Mutants : 9 essayés, tous détectés.
  Mesure : 2016 barres × 1600 lignes en 11–14 ms (build de test RelWithDebInfo, -O2 ; limite de 100 ms vérifiée
  hors sanitizers, le build ASan étant en Debug). ASan/UBSan propres. Core 232 tests.
- **Étape E2 faite** (couche données) : `src/charts/core/FuturesData.{h,cpp}` (pur : lecture des réponses OI, ratio de
  comptes, funding et des messages `forceOrder` ; fusion par temps ; fichiers à enregistrements fixes ; alignement sur les
  barres ; table de paliers BTCUSDT marquée « à vérifier », autres symboles = un palier supposé), `IExchangeFeed`
  (+ `fetchFuturesData`, `openLiquidations`/`closeLiquidations`, `ILiquidationSink`), `src/charts/data/FuturesStats`
  (un objet partagé par symbole : disque, rattrapage, sondage toutes les 5 min, liquidations réelles en direct, fichiers
  `data/charts/futures/<SYM>/`), `FuturesCollector.cpp` (BTCUSDT enregistré dès le lancement de l'app ; pas en profil de
  test sauf `DW_FUTURES_COLLECT=1`), description de la source binance-futures. Vérifié en direct le 2026-11-18 :
  OI et ratio n'acceptent que 30 jours (400 au-delà) ; **une date de début seule renvoie les 500 derniers points** →
  pagination par fenêtres début + fin ; funding sans début = 500 derniers, avec début = 1000 vers l'avant ;
  `leverageBracket` signé (401) ; liquidations seulement sur `/market/ws/` (`/ws/` et `/public/ws/` muets). Outil
  `charts_futures_live` : 30 jours en 19 s / 39 requêtes, 8496 points OI sans trou, relance depuis le disque en
  3 requêtes, liquidations BTCUSDT reçues et enregistrées. Tests : core 6, data 5 (faux Binance réglé sur ces mesures,
  avec trous) ; core 238, data 120 ; ASan propre.
- **Étape E3 faite** (rendu) : `src/charts/core/LiqRender.{h,cpp}` (pur : `prepareLiq` = bougies clôturées seulement,
  les 3000 dernières, OI et ratio publiés à la clôture de chaque bougie (plus vieux que 15 min = inconnu), grille élargie
  de 1/levier minimal + 2 % ; `liqPixelCodes`), `src/pages/charts/flow/LiqMapModel` (calcul complet du modèle sur un
  thread à chaque nouvelle bougie clôturée, nouveaux points d'OI ou réglage ; un calcul à la fois ; résultat partagé en
  lecture seule), `LiqMapLayer` (primitive Bottom, image à 1 px par point, colonne du modèle sous chaque pixel, niveaux
  actuels prolongés jusqu'au bord droit, table de couleurs de la heatmap, palette Thermique par défaut ; points des
  liquidations réelles rouges/verts selon la quantité). Indicateur `liqmap` (13 réglages : côté, répartition des leviers
  Standard / Élevé / Faible, position type, ratio des comptes ou 50/50, biais bougie, demi-vie en heures, part retirée,
  lignes de prix, palette, contraste, opacité, points réels). Légende « estimation », « open interest sur N bougies sur M »,
  « chargement de l'open interest… ». Replay : rien après la barre rejouée. Étape dev `liq:` (add, set, log, perf).
  Mesures (BTCUSDT.P, Retina 1600×1000) : modèle 6–23 ms sur le thread (3000 bougies × 800 lignes), 4,9 ms par image en
  défilement, 1,9 ms vue fixe. Tests : 2 purs (core 240, data 120), ASan propre.
- **Étape E4 faite** (calibrateur) : `src/charts/core/LiqCalibration.{h,cpp}` — score = part de la quantité liquidée
  réelle tombée dans les 5 % de lignes les plus denses (du bon côté) de la colonne d'avant ; 70 répartitions de 5/10/25/
  50/100× par quarts, choix sur les 2 premiers tiers des liquidations, note honnête sur le dernier tiers ; < 30 dans les
  bougies : rien n'est choisi. Option « Calibrée sur les liquidations réelles » (calcul sur le thread du modèle, gardé
  tant que les liquidations n'ont pas grandi de 10 %, les bougies pas bougé et moins d'une heure). Test sur un marché
  connu « tout en 25× » : 25× à 75 %, 95 % des liquidations suivantes dans les zones denses contre 16 % par défaut, 71 ms.
  Données réelles 2026-11-18 : 7 liquidations BTCUSDT en 15 min de marché calme → la légende dit qu'il en faut 30.
- **Chantier F fait** (liquidité SMC/ICT), distinct de `analytics::Liquidity` (inchangé) :
  - F1 `src/charts/core/LiquidityLevels.{h,cpp}` : `Atr` (Wilder), `SwingTracker`/`detectSwings` (fenêtre : strict à
    gauche, ≥ à droite, confirmé N bougies après ; zigzag % ou ATR), `LiquidityLevelEngine` (ids stables, actifs triés par
    prix par côté, recherche binaire, equal highs/lows dans la tolérance ATR ou %, prix = membre le plus extrême, balayage
    mèche ou clôture avec bougie/heure/profondeur, tests, score). Règle trouvée par les tests : la bougie qui forme un
    equal high en dépassant le premier de moins que la tolérance ne le balaie pas.
  - F2 `src/charts/core/Resampler.{h,cpp}` : incrémental, bougie en formation remplacée, jours/semaines/mois dans un fuseau
    avec heure de début (QTimeZone), bougies manquantes jamais inventées ; identique à `series::resample` en UTC.
  - F3 `src/charts/core/SessionLevels.{h,cpp}` : veille, semaine précédente, sessions Asie/Londres/New York dans leur
    fuseau ; testé aux changements d'heure 2026 (US 8 mars / 1er nov., Europe 29 mars / 25 oct.) ; QDateTime seulement aux
    bords des sessions (aucune allocation dans ou entre les sessions).
  - F4/F5 `src/pages/charts/flow/SmcLayer.{h,cpp}` + indicateur `smcliq` (19 réglages : pivots, tolérance, épaisseur,
    balayage, unité de détection, jour/semaine, début du jour, 3 sessions avec préréglages ICT, durée grisée, score
    minimal, zones/lignes, étiquettes, couleurs) : moteur incrémental gardé (une bougie clôturée = un pas, la bougie en
    formation ne fait que les balayages), détection sur unité supérieure par le Resampler, balayage replacé sur la
    bougie du graphique, étiquettes sans chevauchement. Mesures : nouvelle bougie 0,00 ms, calcul complet 7–17 ms (au
    chargement seulement), rendu 0,4–0,6 ms/image. Étape dev `smc:`.
  - Tests : 16 (pivots, regroupement et tolérances, mèche/clôture, tests, gap, incrémental == complet, zéro allocation,
    resampler, sessions et heures d'été) ; mutants 10 essayés, survivant restant = équivalent. Core 258, data 120, ASan propre.
- Les 5 ajouts du plan sont faits.
- **Calibrage sur l'historique (2026-11-18)**. Aucune liquidation BTCUSDT historique n'est publiée ; l'archive publique de
  Binance (data.binance.vision) a les liquidations du **COIN-M BTCUSD_PERP du 2023-06-25 au 2024-10-14** (plus complètes
  que le flux en direct : pas de limite d'une par seconde), avec ses metrics (OI 5 min, pas de ratio de comptes) et ses
  bougies 5 min. `tools/binance_archive.py` télécharge (cache, une requête à la fois) ; `charts_liq_history` calibre
  (lecture des CSV dans `LiqCalibration`, score au fil des bougies `scoreStream`, formule de liquidation des contrats
  inverses ajoutée). OKX publie des liquidations mais seulement ~24 h en arrière.
  Résultats (137 376 bougies, 53 398 liquidations, 71 répartitions en 4,8 s) :
  - Première mesure (part dans les 5 % de lignes les plus denses) **trompeuse** : la référence « lignes à côté du prix »
    atteint 96–99,7 % parce qu'une liquidation a lieu là où est le prix ; abandonnée comme critère.
  - Mesure retenue : **rang** de la ligne de la liquidation parmi les lignes que sa bougie a traversées, selon la densité
    du modèle avant la bougie (0,5 = hasard). Répartition par défaut : **0,58 en apprentissage, 0,61 en test** ; les
    meilleures répartitions font pareil (0,58 / 0,61) ; tout en 5× = 0,51. Mois par mois : 0,61–0,69 sur les 14 mois
    ayant l'OI, **0,50–0,51 en mars–mai 2024 où l'archive n'a pas l'OI** (3 jours, 0, 0) — contrôle naturel : sans
    l'information d'OI le modèle tombe au hasard.
  - Conclusion : le modèle sait **où** les liquidations tombent mieux que le hasard, la répartition par défaut est déjà
    aussi bonne que la meilleure ; le calibrateur de l'app juge désormais par le rang (le marché test « tout en 25× » →
    25× à 100 %). Limites : COIN-M et non USDT-M, pas de ratio de comptes, événements d'une même cascade corrélés.

Demande du 2026-11-18 : footprint, liquidity heatmap (carnet), liquidation heatmap (modèle), liquidité structurelle
SMC/ICT, Big Trades. Rien n'est codé. Ce document résume l'existant, les écarts avec les briefs, puis le plan par étapes.

## 1. L'existant (vérifié dans le code le 2026-11-18)

| Sujet | Ce qu'il y a |
|---|---|
| Langage, build | C++20 (`CMAKE_CXX_STANDARD 20`), CMake avec glob de `src/**`. Pas de vcpkg/Conan/FetchContent : dépendances vendues dans `third_party/` (quickjs-ng, lzma, llama.cpp, pinets) + Qt 6 (Core Gui Widgets Network WebSockets Concurrent Sql OpenGL HttpServer…). Les sources Charts compilent avec `-ffp-contract=off`. |
| Réseau | Qt Network (`net::get`, `src/net/Http.h` : rythme par hôte, cache disque, santé des sources) et Qt WebSockets (`QWebSocket`). Tout hôte doit être déclaré par un `sources::Registrar` (`src/net/Sources.cpp`). |
| JSON | Qt JSON (`QJsonDocument`) uniquement. Ni simdjson, ni RapidJSON, ni nlohmann. |
| Rendu | Moteur QPainter maison qui imite Lightweight Charts (`src/pages/charts/plot/`). Rendu raster dans une image de base (`Chart::base_`, recalculée quand `baseDirty_`), crosshair par-dessus. Extension par **primitives** (`plot/Primitive.h`) en trois couches Bottom / Normal / Top, en coordonnées de pane ; l'axe du temps est en index logique de barre. Pas de GPU dans Charts (OpenGL n'est utilisé que par les cubes 3D de l'onglet Data). |
| Tests | Harnais maison `tests/charts/Check.h` (TEST/CHECK/CHECK_NEAR), projet CMake séparé `tests/charts` (charts_core_tests, charts_data_tests). Un dossier `build-asan/` existe déjà (ASan+UBSan). Pas de build TSan. |
| Threading | Tout le réseau Charts tourne sur le thread GUI (callbacks sur le thread GUI, `LiveFeed` y ouvre son WebSocket). Les gros JSON sont parsés sur un worker (QtConcurrent). Des `QThread` et `std::atomic` existent ailleurs, mais **aucune file inter-threads ni file lock-free** dans le projet. |
| Bougies | `charts::Bar { qint64 time /*s UTC, début*/; double open, high, low, close, volume; }`, `Bars = QVector<Bar>` (tableau de structures), prix en `double`, 3000 barres par défaut. REST `data::getCandles`, temps réel `data::LiveFeed` (kline WebSocket Binance spot et futures USD-M, sinon polling). |
| Indicateurs | Registre `charts::ind` (`Indicators.h`) : `Def` avec `inputs`/`outputs`, `calc(Bars, Params) → Result`. **Recalcul complet** à chaque rendu (pas de cycle de vie incrémental). Un indicateur peut demander un overlay via un champ du `Result` (ex. `volumeProfile` → `ChartPane` crée la primitive `VolumeProfile`). Unité de temps par instance (`"tf"`) : bougies HTF chargées puis `mapToChart` (sans look-ahead). |
| Rééchantillonnage | `charts::series::resample(bars, tf)` + `bucketStart` existent (pas d'incrémental, pas de traitement des barres manquantes). |
| Fuseaux | `QTimeZone` (Qt), déjà utilisé par `src/global/Sessions`. Pas de `std::chrono::zoned_time` ni de lib date. |
| Tick size | `exchangeInfo` spot et futures est déjà téléchargé (carte des symboles, cache 6 h) mais le `tickSize` (PRICE_FILTER) n'est pas gardé. |
| Trades / carnet / OI | **Rien** : aucun flux aggTrade, depth, forceOrder, open interest, ratio long/short, funding. |
| Liquidité SMC | `analytics::Liquidity` (`src/analytics/Liquidity.h`) existe pour la page Marché › Liquidité et les outils IA : fractales 3 barres, regroupement en % greedy, états Untested/Swept/Broken, sessions. Règles différentes de ton brief (pas d'ATR, pas de score, pas d'incrémental, pas de HTF). |

Historique : footprint, heatmap et Big Trades ont déjà été construits début novembre sur l'ancienne application
(effacée lors des redémarrages du 11 et du 14). Le code n'existe plus, mais je reprends les décisions et les pièges
mesurés à l'époque (cf. §4).

## 2. Points des briefs qui ne collent pas à l'existant (à trancher)

1. **JSON rapide / zéro allocation** : Qt JSON alloue à chaque message. Ton brief préfère simdjson/RapidJSON *s'ils sont
   déjà là* : ils n'y sont pas. Proposition : **un petit lecteur maison** dédié aux deux formats fixes `depthUpdate` et
   `aggTrade` (scan des champs, nombres décimaux convertis sans `std::string`), sans nouvelle dépendance, testé contre
   Qt JSON sur des messages réels. Qt JSON reste pour le reste (snapshots REST, OI…).
2. **Threading du carnet** : il n'y a pas d'infrastructure de messagerie à réutiliser. Proposition : un `QThread` par
   connexion depth (le `QWebSocket` vit dans ce thread), file **SPSC lock-free** maison (`src/charts/core/Spsc.h`,
   slots préalloués, `std::atomic` acquire/release) vers le thread GUI et vers le thread d'écriture disque. Le snapshot
   REST passe par `net::get` (thread GUI, pour garder le rythme et la santé des sources) puis est remis au thread réseau
   par signal en file. Aucun mutex côté thread réseau.
3. **`/fapi/v1/leverageBracket` est signé (USER_DATA)** — vérifié dans la doc : il faut une clé API. Proposition :
   table des paliers de marge de maintenance intégrée (publiée par Binance, datée, pour les symboles visés), et lecture
   de l'endpoint seulement si tu fournis une clé lecture seule (champ dans Sources).
4. **`openInterestHist` ne garde que le dernier mois** (vérifié : « Only the data of the latest 1 month is
   available », limite 500 points, 1000 requêtes/5 min). Le modèle de liquidation ne peut donc pas remonter au-delà de
   30 jours, sauf si Deepwater enregistre l'OI en continu. Proposition : collecteur OI 5 min dans le stockage Data
   (comme le collecteur de bougies BTCUSDT), qui allonge l'historique avec le temps.
5. **Enregistrement heatmap à taille fixe** : une colonne de 512 niveaux en `uint16` (code log) = 1 Ko. À 100 ms →
   ~37 Mo/h ; à 1 s → ~3,7 Mo/h. L'ancien format (journal d'événements compressé) faisait ~6 Mo/h mais n'était pas à
   taille fixe. Proposition : enregistrements fixes comme demandé, **intervalle disque séparé de l'intervalle
   d'affichage, défaut 1 s**, rétention en jours configurable.
6. **Incrémental vs système d'indicateurs** : le système recalcule tout à chaque rendu. Pour la liquidité SMC et la
   liquidation heatmap, je fais un moteur incrémental `step(bar)` ; le « calcul complet » est exactement la même boucle
   depuis le début, donc l'équivalence est vérifiable bit à bit. En direct, l'état est gardé par instance (uid) et seule
   la nouvelle bougie est traitée.
7. **Footprint, heatmap, Big Trades ne sont pas des `calc(Bars)`** (ils lisent des trades ou un carnet). Ils entrent
   quand même dans le registre (menu, réglages, légende, sauvegarde dans `state.json`) ; leur `calc` renvoie seulement
   la demande d'overlay, et `ChartPane` branche la primitive sur le flux, comme pour `vpvr`.
8. **Footprint = type de graphique** (« Footprint » dans le menu des types, à côté de bougies/barres), pas un
   indicateur, puisqu'il remplace les bougies. Le reste = indicateurs superposés, empilables.
9. **Binance seulement** pour tout ce qui lit trades/carnet/OI (spot + futures USD-M). ES/NQ demanderaient un flux payant
   (Rithmic, dxFeed) : non prévu. Binance donne le côté agresseur exact (`m`) : la tick rule et la règle des quotes sont
   codées et testées pour d'autres sources futures, et le badge « classification approximative » n'apparaît que si elles
   servent.
10. **Futures : chemin WebSocket** — la doc actuelle montre `wss://fstream.binance.com/public/stream?streams=…` alors que
    `LiveFeed` utilise `/ws/`. À vérifier à l'étape réseau (et corriger `LiveFeed` si l'ancien chemin est retiré).

## 3. Architecture commune

```
src/charts/core/            (pur, QtCore seulement, testable)
  Ticks.h                   prix ↔ int64 ticks, regroupement N ticks
  Spsc.h                    file SPSC lock-free préallouée (modèle mémoire documenté)
  TradeClassify.{h,cpp}     côté agresseur : exact / quotes / tick rule
  Footprint.{h,cpp}         agrégation par bougie, delta, POC, déséquilibres, stacked
  BigTrades.{h,cpp}         agrégateur d'exécutions, percentile glissant, ATR ratio, détection, taille des bulles
  OrderBook.{h,cpp}         carnet local en ticks, synchro Binance spot/futures
  Heatmap.{h,cpp}           échantillonneur, colonnes, anneau, pyramide de zoom, palettes, LUT
  HeatmapRecord.{h,cpp}     format disque versionné, écriture/relecture
  LiquidationModel.{h,cpp}  modèle pur (spans → grille), état incrémental
  LiquidationCalibrator.{h,cpp}
  LiquidityLevels.{h,cpp}   SwingDetector, LiquidityLevelEngine, SessionLevels (QTimeZone)
src/charts/data/            (réseau, threads)
  IExchangeFeed / IOrderBookSink (interfaces), BinanceFeed (spot + futures)
  TradeStream               un flux aggTrade par symbole, partagé (compteur de références), backfill REST
  DepthStream               un QThread par symbole : WebSocket, carnet, échantillonnage, enregistreur
  FuturesStats              OI (temps réel + hist), ratios, funding, forceOrder, paliers ; cache + collecteur OI
  MarketInfo                + tickSize par symbole
src/pages/charts/flow/      (primitives et réglages)
  FootprintLayer, BigTradesLayer, HeatmapLayer (QImage scanLine + LUT), LiqMapLayer, LiquidityLayer
tests/charts/core/          footprint, bigtrades, orderbook, heatmap, record, liqmodel, liquidity, resample
tests/charts/data/          tradestream, depthstream (fausses sources, sans réseau)
```

Règles transverses :
- Rendu limité à ~20 rafraîchissements/s pour les couches temps réel (redessins regroupés) ; seules les barres et les
  niveaux visibles sont dessinés.
- Allocations : un compteur d'allocations (operator new surchargé dans les binaires de test) vérifie **zéro allocation**
  dans les boucles chaudes après préchauffage : application d'un diff, échantillonnage, mise à jour footprint/Big Trades,
  rastérisation d'une colonne.
- Sanitizers : ASan+UBSan sur les tests (le dossier existe) ; un build TSan pour les tests du carnet, de la SPSC et des
  flux avec fausses sources (Qt n'est pas instrumenté : les tests TSan évitent les objets Qt dans les échanges vérifiés).
- Chaque nouvel hôte déclaré dans `sources::Registrar` ; aucune clé ; textes traduits (fr).
- Vérification dans la vraie app à chaque étape (profil de test `DW_PROFILE`, captures, ms/frame, CPU).

## 4. Plan par chantier

### A. Socle trades (préalable à Footprint et Big Trades)
`TradeStream` : aggTrade WebSocket (spot / futures), backfill REST `aggTrades` (1000 par requête) jusqu'à un budget
(défaut 300 000 trades, décision de novembre), bande de trades triée par id, reconnexion avec backoff, coupure des 24 h,
trous marqués. `MarketInfo` garde le tick size. Tests avec une fausse source.

### B. Footprint (3 étapes, comme demandé)
1. **Agrégation + tests** : bougie = niveaux en ticks dans un tableau dense `[lowTick..highTick]` de `{bid, ask}`
   (élargi aux bords, pas de map), mise à jour O(1) par trade ; delta niveau/bougie, volume, POC, déséquilibres
   diagonaux (ratio 3, volume min), stacked (N = 3). Règle des bords reprise de ta décision du 2026-11-02 : un niveau
   hors bougie = pas de comparaison (pas de déséquilibre acheteur au plus bas, ni vendeur au plus haut) ; un niveau non
   traité à l'intérieur compte 0. Tests à la main : classification (tick rule), arrondi, delta, POC, bords, stacked.
2. **Rendu Bid x Ask** : type « Footprint », POC, déséquilibres colorés, delta/volume sous la bougie, fine bougie à côté,
   bascule automatique chiffres → couleurs → bougie classique selon la place disponible.
3. **Modes Delta et Volume profile + réglages** (regroupement 1/2/4… ticks, ratio, volume min, N, couleurs).

### C. Big Trades (4 étapes + alertes optionnelles)
1. **Agrégation + détection + tests** : regroupement côté/fenêtre (50 ms mesurée depuis la 1re exécution, option
   timestamp exact)/prix (ou balayage avec VWAP et min/max) ; seuil manuel min/max (max = 0 désactivé) ; seuil auto :
   percentile glissant 99 sur 30 min par **blocs triés** (pas de tri complet), facteur ATR court/long borné [0,5 ; 2] ;
   démarrage sur l'historique chargé puis seuil de secours. Un agrégat est jugé avec le seuil calculé sans lui (pas de
   look-ahead dans l'historique).
2. **Bulles en mode manuel** (aire ∝ volume, racine carrée, bornes) sur l'historique et en direct.
3. **Mode auto** + seuil courant affiché dans la légende.
4. **Paramètres + info-bulle** (forme, opacité, couleurs, volume dans la bulle).
5. *(optionnel)* **Alertes** : maintenant possibles hors graphique ouvert puisque le flux est dans `charts/data`. Je te
   redemanderai à ce moment-là (tu les avais retirées en novembre).

### D. Liquidity heatmap (5 étapes, ordre de ton brief)
1. **OrderBook + tests** : quantités `double` (comme tout Deepwater), fenêtre dense de ticks autour du mid (taille
   d'après prix et tick, recentrage quand le mid sort de la moitié centrale), niveaux hors fenêtre dans un vecteur trié
   (recherche binaire). Synchro vérifiée dans la doc actuelle :
   - spot : WS d'abord, tampon, noter le `U` du 1er événement ; snapshot `limit=5000` ; si `lastUpdateId < U` →
     reprendre le snapshot ; jeter `u <= lastUpdateId` ; le 1er gardé doit contenir `lastUpdateId` dans `[U;u]` ; ensuite
     continuité `U == u_précédent + 1` ;
   - futures : snapshot `limit=1000` ; jeter `u < lastUpdateId` ; 1er traité `U <= lastUpdateId <= u` ; ensuite
     `pu == u_précédent` ;
   - quantité 0 = suppression, suppression d'un niveau inconnu = normal ; rupture → resynchronisation complète et zone
     marquée « données manquantes ».
   Tests : ajout, modification, suppression, rupture, recentrage, synchro snapshot + tampon simulée ; benchmark par diff.
2. **Couche réseau** : `IExchangeFeed`/`IOrderBookSink`, `BinanceFeed`, `DepthStream` sur son thread, lecteur JSON
   maison, SPSC vers le GUI, ping/pong (Qt répond), reconnexion avec backoff, coupure des 24 h, poids REST respectés.
3. **Enregistreur** : `HeatmapRecorder` (anneau de colonnes préalloué, agrégation N ticks, intervalle 100 ms),
   fichiers horaires `data/charts/depth/<marché>/<symbole>/…` à en-tête versionné et enregistrements fixes, thread
   d'écriture dédié, relecture séquentielle ou mmap, mode replay. Test aller-retour exact.
4. **Rendu** : `HeatmapLayer` sous les bougies, `QImage` écrit par `scanLine()` + LUT de palette (linéaire/log, palettes),
   seules les nouvelles colonnes redessinées, pyramide de colonnes pour le dézoom (piège mesuré en novembre : sans elle,
   125 ms par frame au zoom ; avec, 7 ms). Objectif 60 fps avec 1000 niveaux sur plusieurs heures, mesuré.
5. **Overlay + réglages** : bulles d'exécution (réutilise Big Trades), surbrillance des gros ordres, symbole, marché,
   pas, intervalle, échelle, palette, seuil, bulles.

### E. Liquidation heatmap (4 étapes)
1. **Modèle + tests** : `LiquidationModel` pur, entrées en `std::span` sur des séries SoA (open, high, low, close,
   volume, OI, ratio), grille prix (ticks agrégés) × temps en `float` contigu. Par bougie : ΔOI⁺ borné par le volume →
   longs/shorts selon le ratio (50/50 par défaut, biais bougie optionnel) → grille de leviers pondérés (5/10/25/50/100x)
   → prix de liquidation `entrée × (1 ∓ 1/levier ± mmr)` avec le palier de marge → accumulation, propagation,
   consommation au franchissement, décroissance optionnelle, réduction proportionnelle quand l'OI baisse. Chaque
   hypothèse commentée dans le code. Tests : prix de liquidation avec paliers, scénario « OI monte puis prix chute »,
   décroissance, consommation partielle, incrémental == complet bit à bit. Mesure : 7 jours de 5 min (2016 bougies) en
   < 100 ms.
2. **Couche données** : `FuturesStats` (klines, OI temps réel et hist paginé, ratios global/top, funding, paliers,
   forceOrder), cache disque, remplissage historique puis incrémental, collecteur OI.
3. **Rendu** : réutilise `HeatmapLayer` (palette, LUT, colonnes) ; calcul sur un worker, publication par échange de
   buffer atomique ; marqueurs des liquidations réelles.
4. **Calibrateur** : part des liquidations réelles dans les zones denses, recherche par grille des poids de levier
   (optionnelle).

### F. Liquidité structurelle SMC / ICT (5 étapes)
1. **Détecteur + moteur + tests** : `SwingDetector` sur `const Bar*`/taille (pivots confirmés N barres après, provisoires
   séparés ; méthode fenêtre, % ou ATR), `LiquidityLevelEngine` (identifiants stables, niveaux triés par prix, recherche
   binaire sur `[low, high]`, regroupement equal highs/lows en tolérance ATR, score : membres, âge, unité de temps,
   tests sans cassure ; balayage mèche/clôture avec bougie et profondeur). Tests de bords, tolérances, mèche vs clôture,
   balayage partiel, incrémental == complet.
2. **Resampler** : étendre `series::resample` (déjà utilisé) en version incrémentale réutilisable, alignement des
   bornes (jour/semaine UTC ou fuseau de session), bougies manquantes. Tests.
3. **Sessions** : veille/semaine précédente, Asie/Londres/New York via `QTimeZone` (aucune arithmétique d'offset).
   Tests aux changements d'heure 2026 : US 8 mars et 1er novembre, Europe 29 mars et 25 octobre (semaines où l'écart
   Londres–New York n'est que de 4 h).
4. **Rendu** : `LiquidityLayer` (zones ou lignes jusqu'au balayage ou au bord droit, buy-side/sell-side, opacité selon
   le score, grisé N barres après balayage, étiquettes), seuls les niveaux dans la plage de prix visible.
5. **Paramètres** + détection HTF : le mécanisme `"tf"` existant charge les bougies HTF ; les niveaux sont transmis
   par **temps** (pas par index) pour se placer correctement sur l'unité affichée.

## 5. Ordre proposé

A (socle trades) → B Footprint → C Big Trades → D Heatmap → E Liquidation → F SMC.
Footprint et Big Trades partagent le flux de trades ; la liquidation réutilise le rendu de la heatmap ; SMC est
indépendant (peut passer en premier si tu veux un résultat rapide). Feu vert demandé avant chaque étape, résultat montré
à chaque fois (tests, captures, mesures).

## 6. Hypothèses à confirmer
- Paires visées : BTCUSDT (et ETHUSDT ?) spot et perpétuel USD-M.
- Le footprint et les Big Trades n'ont pas d'historique au-delà du budget de backfill ; la heatmap n'a d'historique que
  ce que Deepwater a enregistré.
- Poids REST des `aggTrades` (spot et futures) à revérifier à l'étape A : en futures, le backfill sera plus lent.
