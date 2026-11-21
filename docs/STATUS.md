# Deepwater — status (2026-11-14)

All 18 sub-tabs are built from the reference videos (`Reference/`, specs in `docs/reference/`) and run on real
data. Checked page by page at 1600x1042 (page area = reference 1600x966) against the reference frames.

## Pages, data and gaps

| Page | Data | Gaps / notes |
|---|---|---|
| Global › Overview | Yahoo tape, wire (Squawk), RiskIndex, FX strength from 28 pairs, central banks, registry, NASA globe | — |
| Global › Session Recap | Yahoo 15-min bars (index futures), wire archive, calendar releases | Headlines only for sessions while the app ran (archive grows, 70 days) |
| Global › Market Hours | Computed (tz database, DST) | Open states designed (never shown in the video) |
| News › Calendar | ForexFactory week; actuals from Myfxbook RSS, wire data headlines, BLS/OECD official series; history OECD/BLS/BoC | Actuals only recorded from now on (3 of 63 this week); no free economists' MIN/MAX (shown "—") |
| News › News Feed | investingLive, Yahoo, CNBC, MarketWatch, Fed/ECB/BoE RSS; on-device EN→FR (Apple Translation) | Investing.com, FinancialJuice, Nasdaq off by default (switchable); many feeds have no images |
| News › Bank Research | ING THINK notes/PDFs, SEC EDGAR assets of 21 systemic banks | Only ING allows reuse: 19 banks link-only, 9 excluded (reasons on the page) |
| News › Squawk | FinancialJuice, investingLive, central-bank RSS; movers from live quotes; calendar; YouTube live TV (WKWebView) | TV needs a live stream (Bloomberg off on weekends) |
| Economy › Macro | BIS, OECD, Eurostat, World Bank, IMF, ECB, BoE, BoC, SNB, e-Stat; yield curves US/EA/JP/CA/UK | No publication dates (OECD/BIS give periods only); PMI → OECD business confidence |
| Economy › Rates | Odds from futures/curves: Fed (CME ZQ), ECB (€STR fut.), BoE (OIS curve), BoJ (JPX TONA), SNB (Eurex SARON), BoC (MX CORRA) | RBA/RBNZ wait for a FinancialJuice probability picture (OCR, validated); Polymarket unreachable here (TLS host mismatch, not bypassed) |
| Positioning › Retail Sentiment | Myfxbook community outlook (user's login, password never stored) | Needs the user to connect; 24 h changes after a day of recordings |
| Positioning › COT | CFTC Socrata (Legacy, TFF, Disaggregated) | — |
| Positioning › Bank Positioning | ING forecasts (FX, commodities, US yields), bias computed by Deepwater | 1 bank (see Bank Research) |
| Positioning › Risk Sentiment | 7 Yahoo components, percentile vs 1 year, plain average | — |
| Market › Markets Watch | 127 markets (Yahoo), instrument sheet with every card connected | Retail card needs Myfxbook |
| Market › Seasonality | 20 years of Yahoo daily closes | — |
| Market › Fundamental Heatmap | MacroStore + CentralBanks, one consistent score | No free PMI (business confidence instead) |
| Market › Technical Heatmap | Daily candles, ETF volume proxies, rules on the page | USD/CNH lacks history |
| Market › Liquidity | Hourly candles, pools/sweeps/sessions computed | Thresholds are Deepwater's own (disclosed) |
| Account › Track Record | Local journal; MT4/MT5 HTML, cTrader/generic CSV import; live bridge EA/cBot | Bridge EA/cBot sources never compiled here |
| Account › Risk Calculator | Live conversion rates (QuoteHub) | Contract sizes are common CFD sizes (printed on the page) |

"Alfred" AI panels of the reference are rule-based AUTOMATED READ texts (EN/FR rules), no language model.

## Audit against the reference videos (2026-11-14)
Every frame of the 20 videos (0.5 s steps) was compared with the app in the same state; one file per page in
`docs/audit/` (finding, frame, reference, app, fix, status). 362 findings, about 240 fixed; the rest already matched,
were live-data differences, or are documented deliberate deviations (own charts instead of TradingView, AUTOMATED
READ instead of "Alfred", English UI, sources that don't exist). The ring around the cursor on clicks is the screen
recorder's click indicator (same ring in several videos) and is not reproduced.

## Responsive (2026-11-14)
Every page works from the 1024x600 minimum window to 3440x1360 ultrawide and still matches the reference at
1600x1042 (`docs/RESPONSIVE_BRIEF.md`, `tools/sizes.sh <page> <dir>` renders the size matrix). The header drops UTC,
tightens tabs, then the date and the wordmark as the window narrows. Small windows reflow (fewer columns, side panels
stacked, low-priority table columns moved to tooltips) and scroll vertically; large windows grow charts and grids, cap
prose at ~900 px and cap a few dense pages (Rates, Fundamental, Risk Calculator) at 2200–2560 px, centred. Globes cap
their render size so CPU stays flat on big monitors.

## Idle CPU (one page visible, 1600x1042, 2x screen, `ps` time over 20 s after loading)
Bank Research 22 % (globe), Overview 20 %, Macro 19 % (globe), Market Hours 10 %, Squawk 7 %, News Feed 6 %,
Risk Sentiment 5 %, every other page ≤ 4 %. Technical Heatmap uses ~30 % for its first minute while it loads and
scores 127 series, then 3 %. Hidden pages stop their timers.

## Sources tab (2026-11-15)
Header tab Sources › Data Sources (src/pages/sources): every source declared with sources::Registrar, its status
(ACTIVE / FAILING / DISABLED / NOT CONTACTED) from Deepwater's own requests (net::Http records each answer per host;
kept in <data>/sources/health.json), data, pages, terms, pace; a switch to turn a source off (settings
sources.disabled; Http then refuses its hosts, sources sharing a host are stopped too and say so); API key field for
keyed sources (keys.<name>, never shown back) and the Myfxbook account (password sent once, never stored).

Sources › Data (2026-11-15): painted diagram (src/pages/sources/DataMapPage.cpp), sources on the left grouped by their
home tab (first page they declare), main tabs and sub-tabs on the right, one curve per (source, sub-tab); hover a source,
sub-tab or tab to light its links; tooltip: publisher, data, access, terms, status (per feed for merged nodes). Built
ONLY from the registry's `pages` lists: a source feeding a new page must list it (pages absent from the navigation are
shown under the diagram). Several feeds of one institution are one node (nodeOf: Fed, ECB, BoE, BoC, Yahoo). Local
inputs drawn dashed: broker exports, downloaded models, the clock. Registry fixes made for it: duplicate ids now merge
hosts and pages (OECD, FinancialJuice, investingLive were losing their second declaration); pages completed from the
service → page include map (Yahoo 15 pages, calendar → squawk/recap/overview, macro and central banks → fundamental
heatmap, markets watch, overview). Search field (header): every word must match a source's name, publisher, id or
data, case and accents ignored, French words also by their English form (forms(): "banque" → "bank"); matches lit,
view scrolled to the first, Enter → next. Hooks DW_DATAMAP_HOVER, DW_DATAMAP_DUMP, DW_DATAMAP_SEARCH.

## AI tab (2026-11-15)
Header tab AI › LLM › Local models (src/ai/ModelHub, src/pages/ai/AiModelsPage): search the Hugging Face Hub (GGUF by default; sort by
downloads, likes, updated), model licence / gated state / files with quantisation and size, downloads into
<data>/models/<owner>/<repo>/ (one at a time, pause / resume with HTTP Range, SHA-256 checked against the Hub's LFS
hash), installed models with show / delete, free disk space. Optional token keys.huggingface (gated models), sent to
huggingface.co only, never to the CDN. Running a model is not part of the app yet. Hooks DW_AI_SEARCH, DW_AI_OPEN,
DW_AI_DOWNLOAD.
AI › Agents (src/ai/Slots, src/pages/ai/AgentsPage + VoxelCube): 4 isolated environments drawn as voxel cubes after
the reference picture. Drag an installed model onto a cube to load it (cube lights up, then breathes); drag a lit cube onto
another to move it; EJECT empties it. Saved in ai.slots; each environment has its own folder <data>/ai/env-N/ with
environment.json. The grid picks 2x2 or 4 in a row, whichever gives the bigger cube; checked from 1024x600 to 2560x1360.
Hooks DW_AI_SLOTS, DW_AI_HOVER, DW_AI_LIGHT, DW_AI_CHAT, DW_AI_ASK.
ml_compare (2026-11-19, src/ai/MlCompare.cpp + src/ml/Compare, tests/ml): the agents compare trainings with figures the app
computes. SAME MARKET (one question on one market and test period): ranked by edge over no skill, p vs no skill and best vs
second by a paired moving-block bootstrap on the same test rows (blocks ≥ the horizon), adj. p = p × distinct setups tried
on that market and question (re-runs count once, shown "×N same setup"), verdict follows the edge's sign. SEVERAL MARKETS
(one setup on several markets): each against its own no skill / buy & hold, how many it beats. Reinforcement: excess
over buy & hold, random agents beaten, their 5–95 % band as noise; clustering: silhouette / Davies-Bouldin / BIC, no test.
References (2026-11-19, user chose all four): predictions.csv carries no_skill and persistence columns (probe_predictions.csv
for self-supervised probes); result.json "persistence" scores (the target's last seen value: last change over the same
horizon, last direction, today's level) and "data" (SHA-256, bytes, rows, first/last date of the data set read); run.json
keeps data_path. ml_compare groups SAME MARKET runs by data version too, tests against both references, verdict says which
it beats. Older supervised/semi runs: Runs::completeReferences (20 s after launch, one at a time, sandboxed train.py --refs)
rebuilds their test rows and adds the columns only if dates, actual values and the no-skill score match the run exactly
(else refs.json says why; originals kept as *.before-refs.*). ML page shows a PERSISTENCE column.
COMPARE in the app (2026-11-19): the comparison moved to src/ml/Comparison (gather on the window's thread, build on a
worker: a structured Report; toText = the agents' ml_compare answer, unchanged). AI › ML › Runs: a tick box on each row
(or ⌘-click), COMPARE (n) compares the ticked runs; with none ticked, the selected run and the runs like it
(ml::comparison::similarTo: same question on the same market, same setup on other markets). src/pages/ai/MlCompareView
draws the Report as painted tables (headers wrap; the setup column shows only the parameters that differ), verdicts and
refs.json reasons translated; a row opens its run with ‹ BACK TO THE COMPARISON (no recompute). Hooks: DW_ML_COMPARE=
similar|<rows>, DW_ML_COMPARE_SCROLL=<px>, DW_ML_COMPARE_OPEN=1 (with DW_ML_RUN, without DW_ML_LOAD).
Constellation (2026-11-19, user's hand drawing photo_2026-11-19_20-31-57.jpg, not in git): AI › Agents chat header, box
WEB (TOILE in French, renamed from CONSTELLATION by the user) left of the state (ai.constellation, key kept). src/pages/ai/Constellation: the conversation card (transcript + input,
moved out of ChatView's splitter) in the middle, linked to COLLECTE (left: data tool calls get_*/read_*/ml_*dataset*,
what each asked for, the running one with its clock), ENTRAÎNEMENT (upper right: an AgentActivity in trainings-only mode)
and COMPARAISON (lower right: the agent's last ml_compare redone by ml::comparison::choose + build from its arguments,
each group's question, best and verdict). A link lights and carries moving points while its work runs; only the link
rectangles repaint (33 ms). Hooks: DW_AI_CONSTELLATION=0|1 (not saved), DW_AI_CONSTELLATION_FLIP=<ms>.
Activity panel (2026-11-19, src/pages/ai/AgentActivity): right of an environment's conversation, what its agent does in
small: the tool call running now and for how long (ai::Engine::beginTool / activeTool, set by the local tool loop and by
McpServer), the last ML training it started (ml::Run.by) with progress, stage, the trainer's live curve (train.py's
"point" events: score of each CV fold, loss of each epoch, return of each episode; kept in ml::Run.points, read back from
log.txt for older runs), its result or error and STOP, then its last tool calls. Its width drags (QSplitter, 240 px
minimum, the conversation keeps 360), saved in ai.activityWidth. A finished run shows the ML page's own result chart, small
(aipage::mlMiniChart in MlPage.cpp, the page's widgets with `compact`): map of the rows by group + groups along time
(clustering such as the Gaussian mixture, anomalies), map of rows / embeddings (reduction, self-supervised), actual vs
predicted or hit rate (supervised), simulated equity vs buy & hold (reinforcement). The trainings are a stack of tiles (one
per run of that agent): a new one slides in on top (240 ms), ← → (panel focused by a click) or ‹ › beside "n / N" go
back and forth; edges of the tiles underneath show below. Sliding tiles are pictures (paintTile Picture mode, the chart
rendered with QWidget::render); the tile on top is live (its chart widget and STOP). PUT ASIDE 2026-11-19 (user: "on reviendra dessus plus
tard"): kOffer3D = false in AgentActivity.cpp hides the box and keeps the flat stack; the code stays. Navigation speed (2026-11-19): tile pictures cached per run state
(facePicture) and drawn ahead for 4 tiles each way while idle (prefetch); presses faster than a slide jump without one;
the small charts draw once into a picture (`compact` cache in PredChart/ScatterMap/EquityChart/RegimeStrip), dense lines
one stroke per pixel column (denseLine), map dots stamped from one atlas (drawPixmapFragments). Measured: a press used
to block 9–57 ms before the slide; now 0 ms, held key included. Fast browsing (presses < 250 ms apart): only the place moves, tiles
come from their pictures or drawn without their chart, the full tile ~0.2 s after the last press. A chart's files (real
runs: up to 80 000 rows) are read and thinned (≤ 8000 map rows, ≤ 4000 line points) on a worker thread (mlMiniRows), the
widgets built from that; the tile shows an empty frame until then. "3D" box
(ai.activity3d): the same stack as standing
cards seen almost from the front (paint3D: slight turn and perspective so the charts stay readable, each tile on its
card's face through QTransform::quadToQuad, thickness on the top and far edges): the selected card in front, up to 4 older
ones behind, fanned up and to the right; a new one comes in from the front, ← sends the front one towards the viewer;
a click on a card behind brings it forward. Face pictures cached per run state (facePicture). The conversation itself (ChatView) only
re-inserts the HTML parts that changed and slides to the end (it used to setHtml the whole transcript 4×/s).
Running the models (2026-11-15): llama.cpp b12409 vendored in third_party/llama.cpp (library only, MIT, Metal, GGML_NATIVE
off), helper helpers/llm/main.cpp → Deepwater.app/Contents/MacOS/deepwater-llm (JSON lines, KV cache reused across turns,
oldest turns dropped when the context is full). ai::Engine runs one helper per lit cube under `sandbox-exec -p`: reads
only the bundle and its model file, reads/writes only its env folder and Metal's shader cache under the app id
(without it every start recompiles ggml's shaders: 6.5 s → 0.24 s), GPU user clients; no network, no other file, no
other process. Checked: prefs plist, home files, home writes, network (TCP), sh, another env's folder all refused.
Drop on a cube = model loads; restored cubes stay STANDBY until used. CHAT (or double-click) opens the conversation
(<env>/conversation.json); reasoning blocks (<think>) dimmed and not sent back. The model is told it is offline with no
data; repeat penalty off by default (it made Qwen3 drop the figure it had just reasoned about). Measured on M3 Pro:
Qwen2.5 0.5B Q4_K_M 185 tok/s, Qwen3 0.6B Q8_0 147 tok/s. Unrunnable files (image models such as the user's
Qwen-Image) fail with a clear message. Not done: sandbox on non-Apple systems.
Skills (2026-11-15): tick boxes per environment (cube card and chat header), ai.skills; first skill "read_data" =
18 tools in src/ai/DataTools.cpp over the services (no page needs to be open): get_quotes, get_price_history,
get_economic_calendar, get_news, get_squawk, get_macro_data, get_central_banks, get_cot, get_retail_sentiment,
get_risk_sentiment, get_bank_forecasts, get_bank_research, get_market_hours, get_technical_read, get_liquidity_pools,
get_seasonality, get_fundamental_scores, get_track_record (the user's journal, without broker or login). Never the
Sources/AI pages, keys or settings. Hermes/Qwen <tool_call> format in the system prompt (only granted skills' tools);
the skill is re-checked at each call (verified: unticked mid-answer → refused), ≤ 5 calls per round, ≤ 6 rounds,
30 s per call, results ≤ 6000 chars; journal <env>/tool-journal.jsonl; tool lines in the chat. Figures check under
each answer: every figure (decimals or ≥ 100, dates/years aside) must appear in the tool results, else listed in
orange (verified with planted figures). Measured: Qwen3 0.6B and 4B call the right tools and quote figures exactly;
4B still misplaces calendar rows between days (calendar now grouped by day) — the figures check cannot catch that.
Hooks DW_AI_SKILLS, DW_AI_TOOLTEST=<file> (runs every tool), DW_AI_UNTICK_ON_CALL.
Skill "ml" (2026-11-16, user's request): AI › ML for the cubes' models and Claude Code (src/ai/MlTools.cpp, 9 tools):
ml_status, ml_methods, ml_datasets, ml_load_dataset (prices / macro / COT), ml_read_dataset, ml_train, ml_runs,
ml_run (waits ≤ 25 s), ml_run_rows. No delete / stop / install, no file outside <data>/ml/datasets (ids checked by
pattern). ml_train checks every argument against ml/Catalog (moved out of MlPage, shared) and the data set: family,
algorithm of that family, needed library, target and feature columns, parameter names; numbers clamped to the page's
ranges (reported); one training at a time; runs named "Env N · …" and run.json "by" = environment and model (ai::
currentCaller, set by Engine and McpServer before each tool). Tools wait for the Python check (a real model's first
ml_train was refused as "not installed" before that). Data sets keep <id>.meta.json (title, source). Verified: hook
DW_AI_MLTEST=<file> (valid + invented arguments, DW_AI_MLTEST_SKIP=11 starts at ml_train) and Claude Code in a cube
(loaded gold, trained, quoted the tool's figures and noticed always-up beat it).
ml_run now reports everything the page shows (user's request): confusion matrix, semi-supervised self-labelled rows
and blank labels, k scan, BIC/AIC, noise share, values clipped at ±5σ, loss per epoch and result per episode (thinned to
~20 points), flat benchmark and time in the market. Checked on one run of each family (DW_AI_MLTEST_CALLS=<json>).
Kept for the user only (confirmed): installing Python/PyTorch, importing a CSV, stopping or deleting a run, the folder.
AI › LLM (2026-11-16, user's request): the sub-tab formerly "Local Models" is "LLM" (page id ai/models kept) with a bar
LOCAL MODELS | INTEGRATION (pages/ai/LlmPage, ai.llm.part, hook DW_AI_LLM_PART=0|1).
Integration = Claude Code (src/ai/ClaudeCode, pages/ai/IntegrationPage): finds the `claude` program, version and sign-in
(`claude auth status`, e-mail never shown), model (ai.claude.model: default/opus/sonnet/fable/haiku) and effort
(ai.claude.effort); LOAD / EJECT per environment, or drag "Claude Code" from AI › Agents' list onto a cube
(slot repo "integration", file "claude-code"; DW_AI_SLOTS="0:integration/claude-code").
In a cube, ai::Engine runs one `claude -p` per question in the env folder: stream-json, --tools "" (no shell, files,
web, sub-agents), --setting-sources "" and --strict-mcp-config (none of the user's settings, hooks or MCP servers),
--permission-mode dontAsk, own system prompt (snapshot off), --session-id / --resume (<env>/claude-session.json; CLEAR
starts a new session). Data only through ai::McpServer (src/ai/McpServer): Streamable HTTP, JSON replies, 127.0.0.1,
random port, /mcp/<n> with a per-env token renewed each launch (<env>/mcp.json, 0600); tools/list = tools of the cube's
skills; tools/call re-checks the skill, refuses outside an answer, ≤ 20 calls per question, recorded in the chat and
journal. Verified 2026-11-16 with the user's account: get_quotes called, figures exact (figures check ✓), session
context held no CLAUDE.md, no other MCP server. Not tested: refusal paths of the MCP server (Origin/Host/token,
skill unticked mid-answer) against a live run.
AI › ML (2026-11-16, user's request; plan in 4 steps with a go-ahead each: 1 environment + supervised [DONE],
2 unsupervised [DONE], 3 semi- and self-supervised [DONE], 4 reinforcement on simulated trading). User's choices: private Python,
Deepwater data + CSV import, RL = simulated trading.
Environment (src/ml/Environment): <data>/ml/venv from Python ≥ 3.11 found on disk (python.org, Homebrew), INSTALL on the
page (confirm dialog) → venv + pip --only-binary=:all: pinned numpy 2.5.5, pandas 3.1.1, scipy 1.18.1, scikit-learn
1.9.1, joblib 1.6.0, lightgbm 4.7.0, xgboost 3.4.1 (~300 MB). LightGBM/XGBoost link Homebrew's libomp (rpath
/opt/homebrew/opt/libomp only): listed unavailable with the reason when it is missing; scikit-learn ships its own.
Data sets (src/ml/Datasets → <data>/ml/datasets/*.csv): Prices (Yahoo by dates — range=max returns MONTHLY bars — 1d/1wk
since 1990, 1h 729 days; features from past bars only: returns, vol_20, rsi_14, MA distances, range, body), Macro
(monthly table per economy, quarterly carried 3 months, dated by reference period not release), COT (deep history,
nets per category + continuous future's close at the report day), CSV (copied in; numeric columns detected).
Training (src/ml/Runs, resources/ml/train.py → <data>/ml/train.py): one run at a time, <data>/ml/runs/<id>/ (job, data
copy, result.json, predictions.csv, model.joblib, log, stderr); sandbox-exec: deny default, read Python + venv + libomp +
script, read/write the run folder, file-read-metadata (realpath needs it). Verified: prefs plist, home read/write,
network, /bin/sh refused. Supervised: 8 algorithms (linear/logistic, random forest, hist gradient boosting, LightGBM,
XGBoost, k-NN, SVM, MLP), target = value / change / % change / direction h rows ahead, lags, chronological split,
time-series CV, metrics vs no-skill baseline, importances, test chart, confusion, latest-row prediction.
Verified in the app: EUR/USD 1d direction (47 % vs 50 % no skill: honest), XAUUSD, COT EUR, macro US, CSV with a
planted signal (73 %). Hooks DW_ML_INSTALL, DW_ML_FAMILY, DW_ML_LOAD="prices:EURUSD:1d|macro:us|cot:EUR|csv:<path>",
DW_ML_TRAIN="<algo>:<target>:<transform>:<horizon>", DW_ML_RUN=<row>. Strings in translations/ai_fr.ts (lupdate src/ai
src/pages/ai src/ml).
Unsupervised (step 2, 2026-11-16): same page, family UNSUPERVISED; the target becomes an optional OUTCOME (never used to
form groups: shown per group as "what followed"). Methods: k-means (+ k scan with silhouette), Gaussian mixture (BIC/AIC),
hierarchical (latest 8,000 rows), DBSCAN (noise = -1), Isolation Forest, LOF, PCA (explained variance, loadings),
t-SNE (latest 5,000 rows). Features standardised, z clipped at ±5σ by default (a bad Yahoo bar at +60σ made a one-row
group); anomaly explanations use unclipped z. Groups renumbered by size; per group: rows, average stretch (regime
persistence), defining features in σ, outcome mean / up share; latest row's group. Charts: groups along time, score
line, k scan, map on the first two PCA axes (0.5–99.5 % bounds). assignments.csv per run. Verified in the app: EUR/USD
k-means / GMM / Isolation Forest (finds autumn 2008), US macro PCA (PC1 rates, PC2 growth), COT EUR k-means (4 regimes
of ~20 weeks). Hooks: DW_ML_FAMILY=1 with DW_ML_TRAIN="<method>:<outcome or ->:<transform>:<horizon>".
Semi- and self-supervised (step 3, 2026-11-16; user agreed to PyTorch). PyTorch 2.15.0 is a separate install
(INSTALL PYTORCH, shown on the SELF-SUPERVISED family; ~640 MB in site-packages), CPU only (the sandbox blocks Metal;
small GRU encoders train in seconds). Semi-supervised (classes only: direction or a class column): self-training (base
gradient boosting / random forest / logistic), label spreading, label propagation; the training part keeps
labelled_share of its labels (+ rows blank in the file); compared on the same test rows with the same model on the kept
labels only and with every label (upper bound). Checked on a two-moons CSV with 10 labels: propagation 100 % vs 50 %
labels-only. Self-supervised: masked autoencoder, contrastive (SimCLR), next step, on windows of W rows (GRU encoder →
embedding); pre-trained on the training part only, scaling from it too; loss curve (train / later windows), unusual
windows by rebuild / prediction error (EUR/USD: 2008-09), embedding map, optional linear probe vs raw last row / raw
window / no skill; encoder.pt + embeddings in assignments.csv. Torch writes torchinductor_* into the run folder (TMPDIR).
Hooks: DW_ML_INSTALL=torch, DW_ML_FAMILY=2|3.
Reinforcement (step 4, 2026-11-16): own environment in train.py (no gymnasium): at row t the agent picks flat / long /
short (or long-only), held to t+1, reward = position × change of the chosen price column − cost (bp) × |position change|;
observes the scaled features + its position. Agents: tabular Q-learning (first 4 features in bins, no PyTorch), double
DQN (replay, target net), A2C (n-step 32, entropy). Learns on the training part (episodes = passes), judged on the later
test part: return, per year, Sharpe, max drawdown, trades, time in market vs buy & hold, flat, and 50 random agents that
change position as often (share beaten). Verdict green only when it beats ≥ 95 % of random agents AND buy & hold (A2C on
gold beat 96 % of random agents but not buy & hold: trend riding). Returns ≥ +1,000 % shown as ×N (DQN on SPX ×98.4M on
the training rows, −0.5 % on test: learned by heart). equity.csv, model.joblib, latest row's action. No order anywhere.
Hooks: DW_ML_FAMILY=4, DW_ML_TRAIN="<agent>:<price column>".
Fix 2026-11-16 (user's report: picked AUD/CHF, trained on EUR/USD): the DATA panel had no link between the list and the
data set; it kept the last one LOADed and TRAIN used it. Now the data set follows the selection (market, interval,
source load at once; the old one is dropped first), TRAIN waits for it and refuses data that is not the selection's
(selectionKey). Default target: direction. Hook DW_ML_PICK="<symbol>" or "<prices|macro|cot>:<key>[:<interval>]" replays
the switch (ml-pick lines on stderr); DW_ML_UNTICK=<column> unticks a feature first.
Checked the same day in all five families and the three Deepwater sources (market, interval and source switches): early
TRAIN refused, each run's data.csv identical to the selected data set. Also fixed then: ticked features are kept across
loads (per column name; filling the list no longer records itself as the user's choice), the reinforcement note follows
the test share, and train() applies the button's own checks.
Variables checked too (user's request, same day): after every switch (prices / macro / COT / CSV, five families) the
FEATURES list and the target list are exactly the new data set's numeric columns, and the job holds only its columns
(script compares against the CSV header). Changed then: a CSV's default target is its last column (was the first);
when the target is predicted at the same row (value, 0 rows ahead: supervised, semi-supervised) its own column is shown
unticked and greyed in FEATURES ("the target itself") and never sent; a table without dates shows no "→" period.

## Facts tab (2026-11-15)
Header tab Facts › Chains. A fact = one value of one indicator for one period, real or forecast; a chain = the facts of
one indicator, one kind (actual / forecast / consensus / market-implied / price), one origin, each linked to the one
before (prev, in period order). SQLite <data>/facts/facts.sqlite (QtSql; writer thread, read-only GUI connection, WAL):
tables chains, facts, chain_stats (per-chain totals kept by the writer: never aggregate millions of rows on the GUI
thread), meta. User's choices (2026-11-15): a revision is a NEW fact in the same chain with `revises` (nothing
overwritten, `recorded` keeps what was known when); history as far back as the sources go; plain links, no hashes;
all four families in v1. Chains in "changes" mode (policy rates) keep a fact only when the value changes.
src/facts/Importer: rates (BIS WS_CBPOL whole history per BIS area + the 8 banks' official rate, decisions as text
facts, market-implied path as forecast chain → rate chain), macro (macro::MacroStore::deep(): same code as the pages'
store, start dates removed; fixed English keys/names per Ind, translated on display), calendar (recorded weeks:
actual, ForexFactory consensus, "previous" of the next release = revision of the one before; each actualArrived at
once), prices (Yahoo daily OHLC by date range from 1928 — range=max returns monthly bars; trading day = UTC date, +1
day for bars opening after 20:00 UTC; never today's bar), forecasts (ING by quarter → px:<spot>:close), positioning
(Cot::deepHistory since 1986: OI + net by category; retail recordings; risk composite). First pass when the tab is
first opened, then every 6 h and at every launch (20 s after start) once deep.rates exists. Test profile result:
2,084 chains, 4.45 M facts (prices 3.95 M), 385 MB, a few minutes; a second pass adds nothing (dedup verified).
Display: chains stacked on one time axis (wheel zoom at cursor, drag, ranges 1M…ALL), block fill by value, revisions
as orange layers + every value of the period in the inspector, forecasts dashed at their target with a dotted line
to the actual block, prev/next buttons. TICK ALL / UNTICK ALL act on the chains listed (family + search); lanes load
when they come into view and are dropped when far (≤ 150 in memory): all 2,073 chains ticked cost +34 MB; footprint
152 MB after an import pass (peak 462 MB while parsing the big downloads). The date row stays on top while scrolling.
Hooks DW_FACTS_SHOW, DW_FACTS_RANGE, DW_FACTS_SELECT, DW_FACTS_TICKALL, DW_FACTS_SCROLL.
3D views (2026-11-15, user: optional, the 2D stays the default): switch 2D / LANDSCAPE 3D / MACRO CUBE 3D above the
chains. Both read facts_monthly (one row per chain and month: last value of the month and its RANK 0..1 among the
chain's months, kept by the writer; backfilled once for older databases) on a worker thread. OpenGL 3.3 core
(QOpenGLWidget, Metal underneath), one instanced draw (pages/facts/Gl3d: BoxRenderer, OrbitCamera), labels on a
transparent child widget (Overlay: QPainter inside paintGL showed nothing over the core-profile frame). main.cpp sets a
3.3 core default format + AA_ShareOpenGLContexts before QApplication (else "Could not create NSOpenGLContext with
shared context"). Landscape (pages/facts/Landscape): X time, Z the ticked chains by family plateau, forecasts right
behind their target, height = rank, today plane, slice plane (slider), double-click → the chain in 2D at that month.
Macro cube (pages/facts/MacroCube): countries × indicators × months, colour = rank (blue low, grey, orange high),
slice by date / country / indicator; cells between the slice and the camera are cut, the rest is a 3 % ghost.
All 2,073 chains: 490 k boxes, fluid. Hooks DW_FACTS_VIEW=landscape|cube, DW_FACTS_CUBE=date:YYYY-MM|country:<name>|indicator:<i>. Not done: AI / ML access
(user: later), implied-path history before today (only live observations), calendar history before the install.

Fix 2026-11-16 (user: "à chaque relance, 4 sur 6, prix de marché"): every launch replayed the six families and prices
re-fetched 100 days of every instrument with a request whose start changed every second (HTTP cache never hit). Now: a
family runs only when due (first import, or last pass ≥ 6 h ago: meta last.<family>; databases without it use deep.*),
the timer fires when the next family is due, the status says "Up to date · next pass in …". Prices: fetched from a week
before each instrument's last stored day, at midnight UTC (same request all day). A flat zero-volume bar (Yahoo turns
futures' past days into settlement-only bars) no longer revises a stored day: 8 of the 34 "revisions" of 2026-11-16
(palladium, platinum 13 Nov) were that; the 26 others (cattle, cocoa, coffee, copper, gasoline, soybean, silver) are
Yahoo's own changes to real bars and stay revisions by design. Tested on a .backup copy: 1st launch one pass (prices
66 s), 2nd launch nothing fetched.
False revisions (user's request, 2026-11-16). Principle set by the user: NOTHING real is ever refused (a period going
2.0 → 2.1 → 2.0 is a real revision); a chain holds ONE source, so a revision only ever means "this source changed its
figure". Causes found in the real DB were all mixed identities: calendar 120 (Myfxbook MoM and YoY items both given to the
m/m event, swapping at every poll; the calendar page showed −1.5 for EUR m/m), decisions 16 (statement vs inference,
text in the interface language), implied 16 (intraday market moves as revisions of the day), prices 8 (Yahoo's flat
settlement bars). What is in place:
- one chain per source: calendar actuals "cal:<series>:actual:<source>" (Myfxbook, <x> headline, <x> official series,
  ForexFactory for its "previous"; consensus targets the ForexFactory chain); decisions "decision:<bank>" (statements)
  and "decision:<bank>:inferred"; a bank's feed "rate:<iso>:<slug of its English name>" apart from the BIS chain
  "rate:<iso>"; macro and implied keep their key and start "<key>:<slug>" only if their source ever changes.
- sources stored in English and without dates (Decision::textEn shown translated via macro::decisionTextShown,
  Bank::rateSourceEn, Bank::pricingSourceEn); the chain list and lanes show "indicator · source".
- Calendar::applyActual matches an item only to an event of the same period; readWeek drops actuals taken from another
  period's item. Implied observations dated to the hour. A flat zero-volume bar gives only its close (the settlement).
- no refusal rule in Store::write (an earlier guard against A→B→A and a "once" mode were removed at the user's request).
One-time repair at open (meta repair.sources=1), converting rather than deleting: settlement closes kept, their
open/high/low copies removed; implied "revisions" become hourly observations; feed values in BIS chains taken out and
re-recorded by the next pass; calendar-actual and decision chains rebuilt by source from the recorded weeks and
statements (last.calendar / last.rates cleared); implied sources set to their English names. Ticked keys that were
split are replaced by their new chains (FactsPage); the macro cube reads the indicator as key part 3.
Tested on .backup copies: 6 copies removed, calendar and rates 0 revisions, 28 price revisions (26 real Yahoo changes +
2 settlement closes) and 1 risk-index revision kept; a forced second pass in the other language recorded 0 facts.

## Pine Script (2026-11-18)

User's plan: Pine is the definition of every indicator, the C++ engine (charts::ind) executes; user .pine scripts run
in PineTS for what Charts lacks; Deepwater goes AGPL-3.0 (LICENSE). Then Facts › Marchés (market.sqlite, BTCUSDT).
- Step 1 (spike): Charts = PineTS on 13 key indicators, 1D/1h/5m/1m BTCUSDT; only gap: Charts' stdev
  (E[x²]−E[x]², ~1e-11 relative on the bands) — fix in step 4. TA-Lib differs only in seeding.
- Step 2 (done): src/charts/core/Pine (run / runAsync, fresh QuickJS runtime per run, no fetch/files, timeout and
  memory limits, Intl.DateTimeFormat over QTimeZone, setTimeout, console), PineTS embedded at :/pinets/pinets.js,
  src/charts/core/JsStack shared with CustomJs. Tests: tests/charts/core/pine_tests.cpp (18) against Node's PineTS
  (fixture from tests/charts/tools/dump-pine.mjs): bit-for-bit equal, time zones over 2026's summer-time changes
  included. Windows: same code paths as CustomJs (QuickJS builds with MSVC), not built here (no CI in this tree).
- Step 3 (done): .pine files in charts/custom are indicators "pine:<name>" (src/charts/core/PineIndicators):
  inputs from PineTS's static scan (keys made identifiers, first source input = "source", 2nd+ = select; colour,
  timeframe, symbol, session, text inputs keep their default), outputs/levels/markers/bar colours from a run on 400
  sample bars, legend name "title (shorttitle)". Computed in the background (pine::Cache: pending → ready() → the
  chart redraws; last result kept padded while a newer one runs; 2 runs at once; off the GUI thread calc waits).
  Per bar colours for lines and histograms (ind::Result::seriesColors; the segment arriving at bar i takes bar i's
  colour, as TradingView) unless the user picks a colour. Editor: "New Pine Script" + template; POST /api/custom
  saves Pine as .pine. Not drawn yet: bgcolor, fill, label/line/box/table/polyline. Tests: pine_indicators_tests
  (6), server_tests updated. Checked in the app (scratch profile): EMA 21 and RSI 14 from Pine equal Charts'.
  Hook: DW_CHARTS_STEPS step "ind:<id>" adds an indicator. Strategy tester / alerts / screener on a Pine indicator
  get "pending" on the GUI thread → step 5.
- Step 4 (done): docs/charts/pine/<id>.pine define sma ema bb bbpb bbw rsi macd atr dmi stoch obv vwap;
  tests/charts/core/pine_reference_tests.cpp compares Charts with them on Binance BTCUSDT 1D (since 2017) / 1h / 5m
  / 1m (fixtures binance-btcusdt-*.csv), 27 settings, 108 runs in parallel (~4 s), within 1e-9 (worst 1.3e-10).
  Engine change: stdev two-pass (Pine's formula); indicators_dump_tests skips the 6 length-1 stdev cases where the
  JS one-pass gave noise. A mutation back to the old stdev makes the reference test fail (checked).
- Step 5 (done): Pine indicators answer at once outside chart drawing (Cache: async only inside a ChartScope;
  tester, alerts, screener, workers run the script and wait; GUI-thread results kept for the chart). Pine
  strategies: pine::Result::strategy (PineTS broker emulator: trades, totals, config), src/charts/core/PineStrategy
  (bt::Trade per trade, open trades "Open" at the last close, equity rebuilt bar by bar, bt::stats, costs text);
  Tester: "Pine strategies" group in the strategy list (custom .pine with strategy()), rule editor hidden, run in
  the background, quant checks not offered for Pine (they re-run rule variants). strategy() added as indicator takes
  its config's title/overlay. Tests pine_strategy_tests (6): EMA cross in Pine vs Charts rules = 66 trades on the
  same bars and prices over BTCUSDT 1D since 2017 (net profit 4415.79 % vs 4415.11 %: Pine sizes the order at the
  signal's close, Charts at the fill); a Pine indicator inside a rule strategy = the built-in's trades. Checked in
  the app. UI step "bt:pick:<value>".
- Step 6a (done): src/facts/market — MarketStore (<data>/facts/market.sqlite: markets, bars WITHOUT ROWID with
  Binance's v/qv/n/taker-buy volumes, sync, gaps) and Collector (Binance klines, 7 TFs 1m…1W, whole history for
  every TF incl. 1m — user's rule — largest TF first, closed bars only, pages of 1000 forward, mirror fallback, no
  HTTP cache, gap scan + one re-ask, then every minute). Starts 20 s after launch (CollectorStartup.cpp, app only);
  test profiles only with DW_MARKET_COLLECT=1. Tests: tests/facts (facts_market_tests, fake Binance, 5). Real run
  (scratch profile): ~3 pages/s → full BTCUSDT ~35 min once; ~94 bytes a bar → ~600 MB for all TFs; the 3379
  daily bars equal the test fixture; 1h has 28 holes / 128 hours that Binance itself lacks (e.g. 2018-02-08).
- Step 6b (done): src/facts/market/MarketIndicators — 23 columns (sma/ema 20/50/200, bb upper/basis/lower, %B,
  width, rsi14, macd/signal/hist, atr14, di±, adx14, stoch k/d, obv, vwap intraday only) computed by charts::ind on
  each series' whole history, in pieces of 200 000 bars with 5000 bars of overlap (OBV carried over), stored in
  market.sqlite's ind table (+ ind_sync with a version hash of the definitions: other definitions → recomputed);
  warm = first 600 bars. Follows the collector (2 s after the first change; holes filled in the past → recomputed
  from there). Worker thread per job; one read connection per thread (thread ids are reused). Tests: 9 in
  facts_market_tests. Real data: 15m's 323 900 stored rows vs one whole computation: EMA/RSI/MACD/ADX/VWAP identical,
  bands 8e-10 (the JS reference's old stdev), OBV 3e-9. Size: ~350 bytes per bar with its indicators → ~2.2 GB for
  BTCUSDT's 6.3 M bars of all timeframes.
- Step 6c (done): Facts › Markets sub-tab (src/pages/facts/MarketsPage.cpp, page id facts/markets): Collection
  panel (per TF: bars, period, indicators, holes from market.sqlite, state, history bar; live download and
  indicator status) and Last bars (300 newest of a TF, OHLCV, trades, taker buy %, the 23 indicators; warm-up
  greyed). Refresh at most every 1.5 s while visible. Hook DW_MARKETS_TF=<0..6>. FR complete (facts, app).
- Step 6d (done): AI › ML source MARKETS (ml::SourceKind::Market, src/ml/Datasets.cpp marketTable, written on a
  worker thread): one stored market and timeframe, newest N bars (50k / 200k default / 500k / 1M), OHLC, volumes,
  trades, taker_buy_pct, ret_1, gap_before, the 23 indicators, *_dist / *_pct versions relative to the close;
  warm-up rows left out (keep_warm for the tool). ml::levelColumns() shared by the page and MlTools (levels unticked
  by default). Cubes: ml_load_dataset(source "markets", key, interval 1m…1W, rows, keep_warm). Checked in the app
  (scratch profile, env installed with DW_ML_INSTALL=1): BTCUSDT 1h, 80 390 rows → hist_gb direction 1 row ahead:
  test balanced accuracy 52.4 % (2025-01-17 → 2026-11-17) vs 50 % no-skill, CV 54.0 %; stoch_k carries most.
  Hook DW_ML_LOAD=markets:BINANCE:BTCUSDT:<tf>[:<rows>]. The tool path was not called by a model.
- Plan done (steps 1-6). Later ideas: strategy inputs in the tester, bgcolor / fill / drawings, more markets in
  Facts › Markets, a trials counter (deflated scores) for the agents' ML runs.
- 2026-11-18: agents read Facts › Markets — read_data tools get_market_coverage and get_market_bars (src/ai/DataTools.cpp),
  checked with DW_AI_TOOLTEST and with a real Claude Code question in a test cube (figures equal the database).
- 2026-11-18: quant checks for Pine strategies in the tester's gold box: Quant.cpp split into run building and
  quant::checksFrom(RunSet); src/charts/core/PineQuant (first two input.int ×0.6…×1.4, 25 PineTS runs in parallel,
  cost stress via strategy() props: commission ×2, slippage ×3 and ≥ 0.05 % of the median close in PineTS ticks of 0.01,
  Monte Carlo scaled by the measured position share). pine::Options::props. Tests pine_quant_tests (2): EMA cross in Pine
  vs rules → same 25 settings, trades, WF picks and verdicts (26 runs in 1.3 s). usesCustom() is about threads: only
  custom: (JS) forces the GUI thread. Later: strategy inputs in the tester, bgcolor/fill/drawings.
- 2026-11-18: skill "backtest" for the cubes (src/ai/BacktestTools.cpp, 4 tools): bt_indicators (Charts' ids, inputs,
  outputs), bt_run (rule JSON, a Pine strategy() source or a tester example; market default BTCUSDT, timeframe default 1D,
  bars default 3000), bt_quant_checks (the gold box), bt_trials. Bars: Facts › Markets when it stores the market and TF,
  else charts::data::getCandles (≤ 20 000). Caps for the 30 s MCP timeout: rules 200k bars (quant 100k), Pine 50k (quant
  20k). One backtest at a time, worker thread with a 16 MB stack. Nothing written on the user's side; the only file is the
  environment's ledger <env>/backtest-trials.jsonl (market|TF, strategy hash, per-bar Sharpe, n, skew, kurtosis) →
  deflated Sharpe of the best by the number of distinct strategies tried (quant::expectedMaxSharpe + PSR). The strategy
  label shows stop / target / sizing / costs given, so the model sees they were applied. Checked with DW_AI_MLTEST_CALLS
  (bad operand, indicator script, two strategies at once refused with the format) and a real Claude Code question in a
  test cube (EMA 20/50 + EMA 200 filter on 4h: 3 tries, 3/5 gates, deflated Sharpe 0.607; replayed calls give the same
  figures). Pine costs and fills texts come translated in the app's language (from PineStrategy / PineQuant).

## Open points for the user
- Connect Myfxbook in Positioning › Retail Sentiment (Retail page and Markets Watch's retail card).
- FRED and EIA keys exist only in the old app's preferences (`com.deepwater.Deepwater`); copying them would add
  release dates and more US series.
- Bank research: only a licence or written consent from other banks would fill the cards (DNB looked closest).
- Investing.com feed: robots.txt answers 403 to the app, so it is off by default.
- Hand-entered dates to re-check yearly: RBA/RBNZ meetings (their sites refuse programs), FSB G-SIB list (November).
- Calendar actual matching from wire headlines has unit tests but no real match seen yet: check on a weekday.
- 2026-11-18 (user's request): header tab Facts renamed "Data", sub-tab Chains renamed "Macro data" (page heading
  "MACRO DATA"); same names in French. Page ids unchanged (facts/chains, facts/markets) so prefs and hooks keep working.
  Texts the agents read say "Data › Markets". Sources › Data renamed "Sources graph" (heading "SOURCES GRAPH", same in
  French; page id sources/data kept), so "Data" in the navigation is only the main tab (French "Data").
- 2026-11-18 (user's request): Data › Markets renamed "Charts Data" (heading "CHARTS DATA", same in French; page id
  facts/markets kept). Agent-visible texts say "Data › Charts Data".
- 2026-11-18 (user's request: "tout renommer", databases too): Facts → Data everywhere.
  Code: src/facts → src/macrodata (MacroDataStore, Importer; namespace macrodata), src/facts/market → src/chartsdata
  (ChartsDataStore, ChartsDataIndicators, Collector; namespace chartsdata), src/pages/facts → src/pages/data
  (MacroDataPage, ChartsDataPage, Gl3d, Landscape, MacroCube), tests/facts → tests/chartsdata (chartsdata_tests, 9 pass).
  Page ids: data/macro-data, data/charts-data, sources/sources-graph (a saved ui.page with an old id opens the new one).
  Databases: <data>/macro-data/macro-data.sqlite and <data>/charts-data/charts-data.sqlite; paths::moveDatabase() moves
  the old <data>/facts/{facts,market}.sqlite with their -wal/-shm on first open (journal first, rolled back on a failed
  rename) and removes the empty facts/ folder; checked on a test profile (same 4 270 183 facts, same bars). SQL table
  names inside the databases (facts, chains, bars, ind) unchanged. Translation context/file: "data", translations/data_fr.ts.
  Settings: macrodata.shown / macrodata.years (read facts.* once as fallback). Hooks: DW_MACRODATA_*, DW_CHARTSDATA_TF,
  DW_CHARTSDATA_COLLECT; ML source "charts-data" (tool and DW_ML_LOAD/PICK; "markets" still accepted), dataset ids
  charts-data-…, SourceKind::ChartsData. Older entries above keep the old names.
- 2026-11-18: a Charts Data series wholly inside the 600-bar warm-up (BTCUSDT 1W: 483 bars) no longer gives an empty
  table: ml::marketTable refuses before writing ("pick a shorter timeframe", FR done), ml_load_dataset says so with
  keep_warm as the way out. Checked: tool 1W refused / 1W keep_warm 482 rows / 1D fine; page shows the message in red.
- 2026-11-18 (user's request, "1 et 2"): backtests on Data › Charts Data. src/chartsdata/ChartsDataBars (storedMarket,
  storedTimeframe, newestBars, magnifierBars: finest stored lower timeframe that covers the test in ≤ 1 M bars, a covering
  one preferred over a finer one still collecting; the exchange's first day opens at 04:00 inside its 1D bar).
  (1) Agents: bt_run / bt_quant_checks on stored data use the magnifier by default when a stop, target or trailing stop
  is set (the model may still ask "worst" or "ohlc"); output says which bars. (2) Charts tester: when the chart's market
  and timeframe are stored with more bars than the chart, the test runs on the newest 100 000 (rules) / 20 000 (Pine)
  stored bars on a worker (custom JS indicators: backtest on the GUI thread), magnifier on stored bars when chosen; a
  hint line under the result header says so; not in bar replay. Checked: BTCUSDT 1D tester = 3380 bars 2017→2026, same
  figures as the tool (worst: −66.22 %, 85 trades). Bollinger breakout (trailing 10 %) 1D: worst −66 %, magnifier +92 %,
  OHLC path +176 %. The tester's magnifier choice itself was not clicked in the app (same function as the tool).
- 2026-11-18: tester "Bars" choice (under Run): "Data › Charts Data: N bars since <date>" (or "the newest 100 000 of N")
  | "The chart's bars: N since <date>"; disabled with only the chart's bars when the market / timeframe is not stored,
  the chart holds as much, or in replay. Saved in Charts settings testerStoredBars (default stored); follows the active
  chart via BottomPanel::onActiveChange / onReplayChange. UI step bt:bars:stored|chart. Checked both ways on BTCUSDT 1D
  (stored: 3380 bars from 2017 with the source line; chart: 3000 bars from 2018-09-02, no source line). FR done.
- 2026-11-18 (user: "Charts Data" was confusing next to the chart's own bars): Charts Data → **Technical data** everywhere
  (bars + their indicators). src/technicaldata (TechnicalDataStore, TechnicalDataIndicators, TechnicalDataBars,
  Collector), src/pages/data/TechnicalDataPage, tests/technicaldata (build-tests-technicaldata/technicaldata_tests, 9 pass),
  page id data/technical-data (data/charts-data and facts/markets map to it), DB <data>/technical-data/technical-data.sqlite
  (moved from charts-data/charts-data.sqlite or facts/market.sqlite; both paths checked), hooks DW_TECHNICALDATA_*, ML source
  "technical-data" (charts-data, markets still accepted), SourceKind::TechnicalData. Entries above use the older names.
- 2026-11-18 (user: no ambiguity left for the agents): read_data tools get_market_coverage / get_market_bars renamed
  get_technical_coverage / get_technical_data; get_macro_data's description says it is the Economy tab's series, not Data ›
  Macro data. New get_macro_chains (search the 1965 chains by words / family / kind) and get_macro_facts (one chain's facts,
  published / recorded / origin, revisions; as_of = what was known then: publication time, else live record time, else an
  estimate for imported history — period end + 0 d daily, + month end for month-start chains such as BIS policy rates,
  +15 d monthly, +45 d quarterly, +120 d annual — said in the answer, with "values are the source's latest"). Checked with
  DW_AI_MLTEST_CALLS and a real Claude Code question (CPI 8.58 % May 2022 and Fed 1.75 % known on 2022-07-10; BTC 1D close
  2022-07-11 19963.61).
- 2026-11-18 (user: record pairs from the Technical data tab, with options for timeframes and indicators; choices: any
  Charts indicator with its settings, a first day to choose, Binance spot): **pairs**. src/technicaldata/TechnicalDataSpec
  (PairSpec: timeframes, start, indicators {id, params, outputs, running, tfs}; columnsOf / versionOf / warmupOf;
  checkIndicator; scaleOf). technical-data.sqlite: table pairs(market, spec JSON, added), sync.start_req; BTCUSDT seeded
  once with defaultSpec() = its 7 timeframes and 23 columns, same names and same version (nothing computed again); the
  ind table gains a column per new indicator output (ALTER, never rebuilt; other pairs' columns NULL). Saving deletes a
  timeframe removed and the bars before a later first day (asked first); an earlier first day fetches only the bars before
  the first stored one; removing a pair deletes all it holds; pages arriving afterwards are dropped by the writer.
  checkIndicator, by computing on generated bars: refuses what looks ahead (output by output: zigzag; Ichimoku's lagging
  span only), what needs more than the 5000 bars recomputed (EMA 3000, Markov except on 1m), accepts running sums with a
  carry (OBV and 2 others); 80 of Charts' 89 accepted. Column names: id + first number + non-default settings + output
  (rsi7, rsi7_ma, ema100_hl2, bb20_mult2p5_upper); warm-up 3 × the longest whole-number setting, at least 600. ML: per-pair
  columns, new price-level columns get _dist / _pct and are left out of default features by scaleOf (prices ×10, volumes
  ×10). Agents: get_technical_coverage lists each pair's timeframes, first day and columns; get_technical_data refuses a
  timeframe the pair does not record. UI: pair pills + SETTINGS / + PAIR in Collection; PairDialog (Binance exchangeInfo +
  first daily bar, first day, timeframes, indicator list with add form from Charts' inputs, output boxes, size estimate:
  BTCUSDT 6.3 M bars ≈ 2.2 GB as the real file). Hooks DW_TECHNICALDATA_PAIR / _DIALOG / _PAIR_SYMBOL / _PAIR_PICK /
  _PAIR_TFS / _PAIR_START / _PAIR_ADD / _PAIR_SAVE / _PAIR_SCROLL, DW_TECHNICALDATA_ONLY (collector limited to these pairs).
  Checked: 12 technicaldata tests (second pair from its own day, earlier / later day, new column, removal); live e2e in a
  scratch profile: ETHUSDT 4h/1D/1W from 2024-01-01 + EMA 9 added through the window, collected from Binance (6314 / 1052 /
  150 bars, indicators complete), agent tools and ML data set (ema9_dist) and bt_run on it. FR done.
- 2026-11-18 (user: add the forex majors; choices: Dukascopy, bid, day cut at 17:00 New York, default 5m→1W from 2003):
  src/technicaldata/Dukascopy (7 majors, .bi5 parse via third_party/lzma = LZMA SDK 24.09 LzmaDec, public domain; New York
  sessions: 1D 17:00→17:00, 4h from 17:00, 1W Sunday 17:00→Friday 17:00, summer time followed; closed(), nextExpected()
  so weekends are no holes) and ForexCollector (its own queue next to Binance's: monthly hour files → 1h and derived
  4h/1D/1W; recent days' minute files → hours; minute history in order for 1m/5m/15m; cursors meta fx.hours / fx.recent /
  fx.minutes:<market>; Saturdays skipped; 404 = empty when old, unpublished when recent; up to 120 files a turn, pairs
  in turn; refused files retried 5 times over ~9 min, then the pair again in 10 min). Store: markets DUKASCOPY:<PAIR>, the
  7 majors seeded once (meta fx_seeded) with defaultForexSpec(); forex specs always keep 1h with 4h/1D/1W; an earlier
  first day re-reads the minute files (minute series deleted, cursors reset); forex gaps = holidays only, marked checked.
  net::Http: Source.parallel (requests on the wire per host, default 1; Dukascopy 2, pace 1 s). Binance collector takes
  BINANCE: pairs only. UI: crypto / forex choice in the pair window (majors list, 1h kept with 4h/1D/1W, size × 5/7),
  page rows from ForexCollector, forex prices 5 decimals, trades and taker buy "—", forex daily dates with their 21:00 /
  22:00 UTC time, refusals in orange "asked again in 10 min". Agents / ML / Charts tester: storedMarket resolves EURUSD,
  EUR/USD, FX:EURUSD, OANDA:EURUSD → DUKASCOPY:EURUSD; ML forex data sets without quote volume / trades / taker columns,
  gap_before with the weekend rule. Tests: 16 technicaldata pass (synthetic .bi5 fixtures from
  tests/technicaldata/fixtures/make_dukascopy.py, no Dukascopy data in the repo). Live: Dukascopy's datafeed answered
  ~15 s a file and refused most requests with 503 "No server is available" (19 of 22 in the first run); EURUSD
  2025-09 → 2025-11 collected (1h 1559, 4h 389, 1D 64 at 21:00/22:00 UTC, 1W 12) with indicators.
- 2026-11-18 (user: pause and cancel a collection; check closing / reopening during a collection; choices: cancel = stop
  and delete what it downloaded, the pair stays paused and empty; pause per pair + everything, kept across restarts).
  Store: isPaused / pairPaused / allPaused / setPaused / setAllPaused (in memory at once, then the spec's "paused" /
  meta collection_paused; a settings save keeps the pause), cancelPair (data + forex cursors deleted, pair paused);
  writer drops bars of a paused pair still on their way and indicator rows of a series deleted meanwhile. Collectors:
  paused pairs get no work, the job running stops after its page (Binance) or at once (forex: m_abort ends the files on
  the wire, nothing written, no retry); progress rows keep paused pairs (Progress.paused). Closing (CollectorStartup,
  aboutToQuit): collectors stopped (an answer still coming writes nothing), Indicators::shutdown (the piece running
  ends, then nothing new; waits ≤ 20 s), then the store's writes. UI: ⏸ PAUSE / ▶ RESUME and CANCEL on the pair, ⏸ PAUSE
  ALL at the top, "paused" rows, ⏸ on paused pills, notes. Tests: 20 technicaldata pass (pause mid-history crypto and
  forex then resume = one run; pause kept on reopening; cancel; closed mid-history then reopened = one run, no bar twice).
  Closing checked for real (scratch profile, BTCUSDT + GBPUSD collecting): a normal quit mid-collection first crashed
  (SIGABRT, bad_function_call: ending a forex turn cleared m_abort while it ran) → fixed (abortTurn() calls a copy) + test
  with a slow fake server (21 tests pass). Then: quit at 60 s exit 0, kill -9 at 50 s, relaunch: integrity_check ok,
  every sync.count = its bars, ind_sync.count = its rows, the collection goes on (BTC 15m 62 000 → 165 000); a forex
  month written but not yet in its cursor is simply read again. Not clicked in the app: the pause / cancel buttons
  themselves (their Store calls are tested).
- 2026-11-18 (user: "annulée" shown as "en pause" after a cancel; then: run a battery of tests on this part). Cancelled
  state (spec "cancelled", Store::cancelled, Progress.cancelled): rows "annulée", ▶ RELANCER, ANNULER off, ✕ on the pill.
  UI driver DW_TECHNICALDATA_STEPS (pick / pause / resume / cancel / pauseall / resumeall / tf / wait / waitfor / dump /
  shot / quit) writing pills, buttons, rows and notes at each step; scenario over three launches (pause and cancel kept
  across restarts). Independent review found, all fixed with a test each: (1) START AGAIN during a cancel's delete
  restarted from the old last bar → Store::busy (cancel / remove / settings save queued: the pair counts as paused, its
  buttons off) ; (2) forex earlier first day mid-turn could restore cleared cursors → same; (3) an unreadable forex file
  at the end looped forever → the turn ends as failed (also 403 …); (4) a Binance hole marked checked after a failed or
  dropped page, and long holes asked only one page → asked to the end, checked only when asked through; (5) indicator
  rows of bars trimmed or cancelled meanwhile → Store::generation, dropped by the writer, replace deletes rows before the
  first bar; (6) the window blocked on Store::wait() → whenWritten / scanGaps in queue order / a per-turn hour tail;
  (7) forex rows "running" only for the timeframes fetched, Binance errors forgotten on a change, removal message.
  Also found by the new tests: forex settings change mid-turn looked idle between the abort and the new turn (queued
  first now); the new hour tail had to be re-read after a jump. technicaldata_tests: 32, five runs in a row all pass.
- 2026-11-20 (user: years of 1-minute BTCUSDT footprint for ML and the agents; archives, not REST; levels as fractions
  of the price; both spot and USD-M; "ces données de footprint sont des données techniques": in technical-data.sqlite,
  part of the collection, shown in a table of their own). **Footprint history.** src/technicaldata/FootprintArchive (pure:
  zip inflated as it is read with third_party/miniz 3.0.2 tinfl, MIT; both CSV formats, spot time in µs since 2025;
  1 bp geometric grid g = 1.0001, level k = floor(ln p / ln g); Minute = OHLC, bought / sold (double), trades, first /
  last id, dense float levels; merge() level by level; encodeDay columnar + qCompress; statsOf: POC, 70 % value area,
  diagonal imbalances and stacks via charts::flow::analyze on the levels). ArchiveCollector: pairs whose spec has
  "footprint": {"venues", "start"}; newest day first, both markets of a day together; .CHECKSUM then zip, sha256 checked,
  built on a worker (≈ 0.5 s a day); 404 older than 3 days → fp_missing, recent → asked again in an hour; network
  failure stops the turn (timer 15 min). Store: fp_days(market, venue, day, minutes, trades, levels, bytes, zip_bytes,
  built, data) + fp_missing; deleted with the pair, a venue removed, a later first day, a cancel; written only while
  wanted and not paused. Source "binance-archive" (data.binance.vision, pace 250 ms). UI: PairDialog Footprint section
  (Spot / Perpetual USD-M, first archive or a day, 3 months back by default, size, deletions asked); Collection gains a
  footprint row per market (days, period, missing, size, state); FootprintPanel under Last bars: a stored day's minutes
  (volume, bought, sold, delta, trades, levels, POC, VA, imbalances, stacks) and the levels of the minute picked.
  Charts: FootprintFeed falls back to the stored days for bars the live trades don't cover (fixed-length timeframes;
  minutes merged, each level at the tick nearest its middle), rows never finer than a level; legend "live from …, stored
  history from …". Measured on 2026-11-18: spot 0.9 M trades → 142 KB, perp 1.7 M → 125 KB a day, ≈ 9 levels a minute.
  Checked: 13 footprint tests (fixtures = 1 trade in 20 of the real 2026-11-18 files, expected values from
  fixtures/make_footprint.py in Python; fake archive for the collector); tools/check_footprint_history.py compares random
  stored minutes with Binance REST aggTrades by id (USD-M serves only the last 2 days): all match, float error ≤ 6e-8.
- 2026-11-20 (user: which of Charts' three liquidity views gives an informational edge, then collect it like the
  footprint; choices: 1 book depth archive, 2 open interest, 3 the live book "seulement en mémoire cache", for live
  trading). Answer given: only the order book is not a function of the bars (SMC levels come from OHLC; the liquidation
  map is a model whose real input is the open interest); Binance publishes no full L2 history, but USD-M bookDepth
  (±0.2/1/2/3/4/5 %, ~30 s, from 2023-01-01; ±0.2 % only later) and metrics (OI + long/short ratios, 5 min, from
  2020-09-01; the first months list rows twice). Built: src/technicaldata/PerpArchive (parse, depth day blob "PDD1",
  imbalance); FootprintCollector renamed **ArchiveCollector** with kinds spot / perp / depth / oi, up to 4 days in flight
  (one footprint day), source binance-archive parallel 4 → ≈ 2 day-files a second (full depth + OI ≈ 30 min). Store:
  depth_days, oi (+ oi_days), arch_missing; PairSpec "perp": {"depth", "oi"}, "liveBook". **Live book**:
  charts/core/BookBands.h (the bookDepth bands from the local OrderBook + how far the REST snapshot reached), computed in
  DepthStream's depth thread each whole second, GUI keeps 24 h (bands()); technicaldata/LiveBook holds the spot and perp
  DepthStreams of liveBook pairs, record(false) as its wish, nothing written (checked: charts/depth empty). Measured: the
  perp snapshot (1000 levels) reaches only ≈ −0.3 / +0.1 %, so live perp bands past ±0.2 % undercount (≈ 30 % at ±1 %,
  half at ±5 % vs the archive); spot (5000 levels) reaches ≈ ±2.2 %. UI: PairDialog "Order book and positions" (3
  toggles, size, deletions asked); Collection rows per archive kind; FootprintPanel → "Order flow" tabs Footprint / Book
  depth / Open interest / Live book (1 s refresh, bands past the reach greyed). Depth ≈ 120 KB a day, OI ≈ 17 KB.
  Checked: 3 parser tests (Python expected values, fixtures/make_perp.py), collector test with the fake archive, 2
  BookBands tests; tools/check_perp_history.py: 5 depth days + 5 OI days re-downloaded, all match; charts data 154, core
  286, technicaldata 48 pass. Not done: agents / ML reading these (step 3), the live book for an agent tool.
  User's rule (same day): Charts' heatmap recordings (charts/depth/*.dwhm, its own on/off and days kept) may go on but
  are not part of technical data: nothing reads them into technical-data.sqlite, the Data page or ML; the live book's
  bands come only from the live stream.
- 2026-11-20 (user: "la suite" = step 3, does the order flow help a model?). src/technicaldata/OrderFlowFeatures: a bar's
  scale-free order flow values known at its close (footprint per venue: delta_pct, poc_dist, va_width, va_pos,
  imbalances, stacks, low/high quarter deltas, perp volume ratio; depth: bids' share ±0.2/1/5 %, ±1 % depth changes,
  last snapshot strictly before the close; OI: change over the bar, top positions / accounts / takers ratios).
  ml::DatasetSpec::orderFlow ("-of" data set): those columns, and only the bars every recorded family covers, so runs
  with and without them share their rows; ml_load_dataset order_flow, ML page checkbox "order flow". Test: 1 (values,
  coverage, no look-ahead). **Result** (scratch profile, BTCUSDT, 2026-08-20 → 11-18, test 11-01 → 11-18, direction of
  the close, A = 23 scale-free bar columns, B = A + 30 order flow columns, same rows): 15m h1 linear A 52.6 / B 50.6,
  hist_gb A 51.6 / B 52.0; 15m h4 linear A 52.0 / B 50.2, hist_gb A 49.7 / B 51.2 (A+footprint 51.5, A+depth/OI 51.0):
  none distinguishable from no skill after tries; 5m h1 hist_gb A 52.5 % (adj. p < 0.001) / B 51.4, linear A 51.6 / B
  51.1. No gain from these order flow features on 3 months; the only significant edge is bars-only at 5m. Next options
  given to the user: more footprint history, other targets (1m, size of the move / volatility), better features.
  User's decision: keep the data as is (3 months + the collection), no longer history, no step 4.
- 2026-11-20 (user: "il le faut", an agent reading the order flow as a trader reads a chart). src/ai/OrderFlowTools.cpp,
  skill read_data: get_footprint (bars 1m … 1D of spot or perp: volume, buy %, delta, CVD, POC, value area, imbalances,
  stacks; ladder = one bar level by level with POC / VA / imbalances / stacks / open / close marks, levels grouped to
  ≤ rows), get_order_book (live, both markets: best bid / ask, spread, bands ±0.2 … ±5 % with the snapshot-reach caveat,
  changes over 1 min … 1 h, 5 largest resting orders each side within ±0.5 % and how long they stood; end = the
  perpetual's stored bookDepth at a past time), get_open_interest (metrics rows at a step). get_technical_coverage
  lists each pair's order flow. Today's bars: src/technicaldata/LiveFootprint (pure) folds the live TradeStream tape into
  1-minute footprints on the archive's 1 bp grid (DayBuilder, same minutes), whole minutes only (first / open / holed
  minutes left out), 48 h kept; LiveBook now keeps the TradeStreams too (memory only) and folds every 30 s. Limit: the
  live footprint starts with the trades the tape loads (TradeStream budget 300 000 ≈ 30-40 min on the perpetual, ~3 h
  on spot), so today's bars before Deepwater started are not covered until tomorrow's archive. Test: 1 (same minutes as
  the archive builder, partial / holed minutes, 48 h). Verified with the fake claude on the scratch profile.
- 2026-11-20 (user: "Commençons par l'étape 1" of reports/Architecture agents ML évaluation.md). Trial registry,
  src/trials: <data>/trials/trials.sqlite, append-only by construction (SQLite triggers abort UPDATE / DELETE). Every
  backtest of Charts › Tester (user), bt_run / bt_quant_checks (agents) and every finished AI › ML training (user or
  ml_train, one row per run id) is a try: family (bt|market|tf seconds, ml|market␟question), setup fingerprint, who,
  period, per-bar Sharpe / skew / kurtosis, returns compounded per UTC day (for later PBO across tries), ML score vs no
  skill. Readers: the agents' deflated Sharpe (bt_run, bt_quant_checks, bt_trials) now counts everyone's tries on that
  data, not one environment's; ml_compare's Bonferroni tries come from the registry (deleted runs still count); the
  Tester shows "Registre d'essais : N stratégies… (x par vous, y par les agents)" and the best one's deflated Sharpe.
  The environments' old ledgers (<env>/backtest-trials.jsonl) and the existing ML runs are imported once. Tool tests
  (env < 0) are not tries. Charts test hook: DW_CHARTS_STEPS step click:<button text>. Tests: tests/trials (8).
  Verified on the scratch profile (agent tools, Tester screenshot, ml_compare). Not done yet (step 2): the quant
  checks' own deflated Sharpe still counts only their ~25 nearby settings.
- 2026-11-20 (user: "Feu vert étape 2"). (1) Quant checks on the trial registry (trials::withRegistry, Charts › Tester
  box and bt_quant_checks): the deflated Sharpe counts the strategy's nearby settings AND the other strategies tried on
  that data ("after 28 tries on this data: 24 nearby settings and 4 other strategies"); new gate "Overfitting across
  tries (PBO)": CSCV on the returns per UTC day of every strategy tried on this data (those run on ≥ 75 % of this run's
  days, other days flat; needs 3 strategies, 40 days). Port change: Checks.trialSharpes (not in toJson). (2) AI › ML
  walk-forward (supervised): job validation = walk_forward, wf_folds 2..20 (page: "Partie test prédite par", 5 or 10
  retrainings; ml_train: validation / wf_folds): the test part in blocks, each predicted by a model retrained on every
  row before it minus the h rows whose target reaches into the block (purge), no-skill per block written per row; same
  test rows as one cut, so ml_compare compares both (setup gets "|wf<k>", algorithm shown "· walk-forward k"). (3)
  ml_compare: PBO across the setups of a SAME MARKET prediction group (ml::compare::pbo on per-row scores whose mean is
  the balanced accuracy, or −|error|), in the agents' text and the COMPARE view. Tests: trials 10, ml 10, charts quant
  17. Verified on the scratch profile: WF run 52.5 % vs one cut 52.6 % on the same 1688 rows; PBO 81 % / 76 % across
  the BTCUSDT 15m setups. Not done: CPCV (the existing purged TimeSeriesSplit CV and walk-forward cover choosing and
  deployment); walk-forward for the semi-/self-supervised and reinforcement families.
- 2026-11-20 (user: "On continue" = step 3). Evaluation clock for the agents: ai::Slots::asOf (settings ai.asof), set per
  environment from the cube's EVALUATION button (dialog: on/off, UTC date; badge "ÉVALUATION · date" on the cube) or
  DW_AI_ASOF="0:2026-11-10 00:00". ToolDef::clock declared in ONE place (Tools::Tools): Live by default = REFUSED in
  evaluation mode (closed by default; refusal journaled), AsOf (get_technical_coverage, get_technical_data, get_footprint,
  get_order_book, get_open_interest, get_macro_facts, bt_run, bt_quant_checks, bt_trials), Timeless (bt_indicators,
  ml_status, ml_methods). Checked in both dispatchers (McpServer, Engine); ToolCaller.asOf; tools read it via
  ai::asOfFor(name). AsOf rules: a bar counts once closed (t + tf <= asOf); stored footprint days begun before asOf, no
  live memory; book = stored perp snapshot at asOf; OI rows strictly before asOf; macro facts as_of forced (chain totals
  and later "recorded" stamps hidden); backtests on stored bars only; registry tries visible only when their period had
  ended (trials::visibleAt); an explicit later time is refused. Prompts say "EVALUATION MODE: the date is …". Leak test:
  tools/check_asof_leaks.py (+ tools/asof_leaks/fake_claude.py): same calls on a profile copy and on one whose future
  is random (bars, indicators, sync, gaps, fp/depth/oi days, OI rows, future registry tries); answers must match; then
  DW_PLANT_LEAK=get_technical_data (scratch profiles only) must be caught; refusals checked. PASS at 2026-11-10 00:00
  and 2026-11-14 13:20; it caught 2 real leaks on the way (OI header's stored day, OI row at exactly D). Not covered:
  ML tools (Live: refused in evaluation mode), the macro DB (empty on the scratch profile: not leak-tested), a day
  archive begun before a mid-day D (minutes after D in it not randomised).
- 2026-11-20 (user: "il faut d'abord tester l'horloge"). Tested with the REAL Claude Code on a scratch profile copy at
  2026-11-14 13:20: present-tense question → get_quotes / get_price_history / get_news refused, answer built from the
  bars, OI and book as of that moment and dated so; explicit 18-20 Nov asks → refused (journaled), said so, nothing
  invented, backtest cut at 14/11 12:00; mode off → live again. Found a real leak: the conversation crossed clock changes
  (a live answer stayed in Claude's session after switching to evaluation). Fixed by construction: Engine::syncClock()
  at each question (and on Slots::asOfChanged) compares the clock the conversation was held under (last "clock"
  message) with the environment's; when they differ: a "clock" message (shown in the chat as a line) and a new Claude
  Code session; a local model is sent only the messages after the last "clock". Re-tested: the model no longer sees
  the earlier answer. tools/check_asof_leaks.py still PASS.
- 2026-11-20 (user: "ok et la suite ?" = step 4). AI › ML supervised runs are judged in money after costs: train.py
  trading_eval() trades the test predictions (position decided at each test row's close, held to the next row; long
  when the model says up, flat otherwise, or long_short) when the target is a price (all values > 0) and the classes are
  up / down (or a change / level regression). Cost a side: job cost_bp, defaulted in ml::Runs::start by market (Binance
  15 bp = 0.10 % fee + 0.05 % slippage as the Strategy tester; forex 2; others 5); costs ×0/×1/×2/×3. References on the
  same rows: buy & hold, trend (above its 20-row average), constant exposure = the model's average position held all
  along (timing apart from exposure) with a paired moving-block bootstrap p (and p_worse). Shown in ml_run (TRADED
  block), AI › ML (table TRADÉ APRÈS FRAIS), ml_compare / COMPARE (net after costs per run, buy & hold), and the trial
  registry's ML rows (sharpe, net). ml_train: trade_side, cost_bp. Verified: BTCUSDT 15m direction, linear 52.6 %
  balanced accuracy → −45.9 % after costs (362 trades), before costs −6.8 %, buy & hold −19.4 %; hist_gb −63.5 %;
  both lose against their own constant exposure, not by chance. Not done: semi-/self-supervised trading, a cost field
  on the page (default by market only).
- 2026-11-20 (user: "oui on y va" = step 5). AI › ML targets of how much a price moves (train.py frame, VOL_TARGETS):
  realized_vol, range, abs_return, big_move (beyond move_pct, default the training rows' 80th percentile, windows ending
  before the test) over the next h rows; persistence = the same measure over the last h rows. Every such run is judged
  against HAR (regression on the realized vol of the last h, 5h, 22h rows) and EWMA (λ 0.94), fitted on the same
  training rows (walk-forward: per block), balanced classes for big_move; paired moving-block bootstrap (p, p_worse);
  predictions.csv gets har / ewma columns; result "references". Shown in ml_run (AGAINST HAR / EWMA), AI › ML (CONTRE
  LES RÉFÉRENCES DE VOLATILITÉ), ml_compare / COMPARE. Order flow: new column fp_<venue>_open_delta_pct (the bar's
  opening minute delta; arXiv 2607.09426). Checked: targets look ahead exactly h rows, references unchanged by a changed
  future. Experiment (scratch profile, BTCUSDT 15m, 8537 rows, test 1 704 rows 2026-11-01 → 11-18, h = 16 = 4 h):
  realized vol: no model beats HAR / EWMA (hist_gb bars MAE 0.348, with order flow 0.367, EWMA 0.343, HAR 0.353);
  big move (≥ 1.19 %): hist_gb 56.5 % (bars) / 55.7 % (order flow, AUC 66.1 vs 62.6) vs EWMA 57.6 %, HAR 53.5 %, all
  not distinguishable; persistence 59.6 % best; direction 4 h / 12 h with order flow: 44-48 % balanced accuracy, lose
  after costs and against their own exposure. Caveat: 18 test days (≈ 27 independent 12 h windows).
- 2026-11-20 (user: "on continue" = step 6). Agents' evaluation protocol: src/ai/Evaluation (runner) + src/trials/
  AgentEval (pure figures, tests/trials 13). KNOWLEDGE PROBE: fresh context, clock set to 2015 (tools useless), the
  model gives Bitcoin's monthly closes from memory; against Binance's 1M candles → last month known (within 10 %, and 2
  of the 3 months ending there) = measured frontier, <env>/knowledge-probe.json. PROTOCOL: N moments over a period (at
  least one horizon apart), R repeats; at each, Slots::setAsOf (fresh context via Engine::syncClock), the question
  Evaluation::prompt, the last "DECISION: LONG|SHORT|FLAT" line read (none = flat, "unparsed"); scored on stored 1h bars
  after 15 bp a side against buy & hold, flat, trend (above 20-hour average), and the newest ML run answering the same
  question (direction, H rows, the market's 1h data set) at the same moments; tier A = after frontier + 1 month, tier
  B = before; repeat spread; paired bootstrap. Report as a "report" message in the conversation (never sent to the
  model), <env>/evaluations/<stamp>.json, a try (kind agent) in the registry. UI: the cube's ÉVALUATION dialog gets a
  Protocole section (period, horizon, decisions, repeats, Sonder sa mémoire / Lancer); the cube's badge shows progress.
  Test hooks: DW_AI_EVAL=probe | run:from,to,h,n,r, DW_AI_EVAL_QUIT; tools/asof_leaks/fake_claude.py FAKE_CLAUDE_EVAL.
  Real Claude Code probe: every monthly close Jan 2024 → Nov 2025 within 0.1 % (although told the date is 2015), unknown
  from Dec 2025: frontier 2025-11, decisions from 2026-01 are tier A. Not built: blind trials (masked ticker and dates:
  tool texts would need rewriting, prices give BTC away), forward paper test (tier C).
  Real run (Claude Code, 6 decisions 2026-11-05 → 11-17, 4 h, tier A): 2 SHORT + 4 FLAT, +0.75 % total vs buy & hold
  −2.68 %, trend −0.97 %, ML without LLM (walk-forward linear, 1h, direction 4 rows) −1.71 %; none distinguishable
  (p 0.07-0.24). Fixes found by the runs: probe answers "~42 580" (tilde, narrow spaces) now read; inside a QObject,
  tr() is QObject::tr (context "ai::Evaluation"): the helper is T(); duplicate ai_fr.ts entries removed.
- 2026-11-20 (user: "on continue" = step 7). Replayable journal: <env>/tool-journal.jsonl now has kind question
  (question_id, text, question_sha, system_prompt_sha, skills, as_of, app_version) / tool (arguments, result_sha =
  SHA-256 of the exact text, 16 hex, clock class, external, ms) / answer (answer_sha, model, device = version, reason,
  tool_calls); evaluation decisions carry their question_id. tools/replay_journal.py replays the asof calls at the same
  clock on a profile copy through the MCP server and compares fingerprints (5/5 identical incl. bt_run; a tampered
  fingerprint reported). MCP hardening: token compared in constant time; ToolDef::external (get_news, get_squawk,
  get_bank_research, declared in Tools::Tools) → result wrapped in <external-text> (inner marks defused) + kExternalRule
  in both system prompts. Host / Origin checks were already there. tools/check_asof_leaks.py still PASS. Not done:
  MCP outputSchema / structured content (the tools answer text), a wrong-token request not tested by hand.
- 2026-11-20 (user: "partons là-dessus" = the four follow-ups). (1) Trading after costs for semi-supervised runs (result
  "trading") and self-supervised probes (probe "trading", each test window decides at its last row); cost by market for
  those families too (Runs::start); shown in ml_run, AI › ML (addTrading), ml_compare, registry. (2) ML tools under the
  clock: ml_load_dataset cuts technical-data sets at the moment (DatasetSpec::asOf, id "-asof-yyyyMMddHHmm", title
  "· as of …"; other sources refused), ml_datasets / ml_read_dataset / ml_train / ml_runs / ml_run / ml_run_rows /
  ml_compare see only those sets and the runs on them; declared AsOf; leak test extended (17 calls, PASS). (3) Forward
  test, tier C (Evaluation::startForward / forwardTick, <env>/forward-test.json): asked in the present now and every N h,
  missed times recorded (never asked late), scored when the horizon has passed, report + registry; agent changes flagged
  (compared to the first answered decision); scheduler started in main.cpp (resumeForward). Fixed on the way: a lambda
  with Qt::UniqueConnection asserted at launch (would have crashed every start). (4) Blind trials (src/ai/Blind, tests
  3): dates shifted 9-13 years ahead, BTCUSDT / Bitcoin / Binance masked (MARKET:ASSET1), price-level numbers × a secret
  factor; arguments mapped back; applied in both dispatchers when Slots::blindSeed and asOf; prompts say BLIND; a
  change of blind starts a fresh context; protocol modes named / blind / both with agreement rate; replay skips blind
  rows. Statistic fix: no p under 6 decisions (kMinTested). Tests: trials 16, ml 10, technicaldata 50, leak test PASS.
- 2026-11-20 (user: "Tu pourrais lancer l'évaluation ?"). First real run of the protocol with Claude Code (opus-5-5), on a
  copy of the test profile, BTCUSDT, 30 moments 2026-10-12 → 2026-11-19, 4 h, mode both, 15 bp a side, ML baseline =
  linear walk-forward direction 4 h (balanced accuracy 0.514). Fixed on the way: (a) the probe ran at the 2015 clock in
  evaluation mode, so Claude obeyed "do not use what you remember" and refused; a refusal was counted as "knows no
  month" = everything tier A. Now: present clock, no skills (put back after), fresh context (Engine::freshContext),
  and no price at all = "inconclusive" (frontier 0, every decision tier B). (b) A question cut by the timeout left the
  Claude process ending; the next ask failed and the evaluation stopped ("did not take the question"). askNext now
  waits up to 60 s for the environment to be free. Result: probe frontier 2025-10 (closes within 0.01 %), all 60
  decisions tier A, none unparsed. Agent named −7.26 %, blind −8.54 %, buy & hold −9.89 %, trend rule −4.02 %, ML
  −8.59 %; no difference significant (p ≥ 0.36); same decision named/blind at 24 of 30 moments. Tool calls: 4h bars
  refused (not recorded in that profile), calendar / news refused as live: correct.
- 2026-11-21 (user: "Fais un build de deepwater pour IOS"). iOS device build (arm64) in build-ios: `~/Qt/6.12.0/ios/bin/qt-cmake
  -S . -B build-ios -G Ninja -DQT_HOST_PATH=$HOME/Qt/6.12.0/macos -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64`,
  then `cmake --build build-ios` → build-ios/Deepwater.app (64 MB, unsigned). Qt WebSockets + HttpServer for iOS added with
  aqtinstall. No simulator build: the aqt iOS kit's simulator slice is x86_64 only (needs Rosetta, absent). Changes:
  core/Process.h (dw::Process = QProcess, or on iOS a stand-in whose start() fails with FailedToStart: Claude Code, local
  models, ML trainer, translation helper unavailable there), CMake iOS branches (UIKit, no AppKit/WebKit, no
  deepwater-llm / dw-translate, NativeTv.mm out → NativeTvStub), Pdf.mm print off on iOS. The Mac build is unchanged.
  Signed + installed on the user's iPhone 15 Plus the same day (user: "oui"): Xcode project build-ios-xcode (qt-cmake -G
  Xcode, DEVELOPMENT_TEAM from the Apple Development certificate, automatic signing, xcodebuild -allowProvisioningUpdates),
  `xcrun devicectl device install app`; profile expires 2026-11-28 (free account: 7 days). Crash at launch fixed:
  Q_COREAPP_STARTUP_FUNCTION bodies starting a QTimer run before iOS's run loop exists (CFRunLoopAddTimer on null) →
  core/Startup.h DW_STARTUP_FUNCTION (iOS: kept and run by main() right after QApplication; elsewhere unchanged), used by
  the 6 startup functions; main.cpp: no GL 3.3 core format on iOS. Runs with live data (Global › Overview seen). Debug
  trick: devicectl launch --start-stopped --json-output, then `lldb --batch -o "device select <udid>" -o "device process
  attach --pid N" -o "process continue" -k "bt"` (-k: commands run on crash). Not done: touch / phone layout (the
  window wants 960 x 540: texts overlap on the phone), GL pages not checked.
- 2026-11-21 (user: "il faudrait que ça soit responsive"; choices: hamburger menu, iPhone portrait + landscape). Step 1
  (bases) done: ui/Touch.h (ui::compact(width) < 760 = phone layout, each widget decides from its own width;
  ui::touch::install: a QScroller on every scroll area's viewport, finger on iOS, left mouse with DW_TOUCH_SIM=1 on a
  Mac); Header compact (one 48 px row: ☰, TAB › SUB, time, gear) when narrow or the window < 500 high (phone turned);
  app/NavDrawer (menu from the left, every tab and sub-tab, tap = open); window minimum lifted on iOS, and on a Mac by
  `--size 430x932` and by `--layout-audit --audit-sizes 430x932,932x430`. Pilot page Global › Overview: one column
  (OverviewPage::layoutPanels), tape without session clocks, risk gauge above its history, strength controls on a
  second row, banks table without the year. Checked on Mac captures (430x932, 932x430) and on the iPhone.
  Next (feu vert per group): Global + News, Economy + Positioning, Market + Account, Data + AI + Sources, Charts.
- 2026-11-21 phone layout, group 1 (Global + News) done by three agents, checked on captures and on the iPhone:
  Session recap (one column; also a landscape layout; fixed a crash at 430 wide: an hour-label bound check in
  CentreView), Market hours (clock, timeline with names above the bars, cards), Calendar (2-line rows, pinned headers,
  SURPRISE/MIN/MAX dropped on phones, values open the range panel), Squawk (wire, TV, calendar in one column), News feed
  (two-row header, lead story, headlines, cards; reader full width), Bank research (notes, globe, registry in a scroll
  area). Rule used: page `ui::compact(W)` (Calendar/Squawk also `H < 420`), widgets `width() < 520`; desktop
  unchanged (compared with captures). Known leftovers: lists that scroll inside a scrolling page (a drag on the list
  scrolls the list), Bank research's last note under its sources footer (as on desktop), Feed reader's side panel cut.
- 2026-11-21 phone layout, group 2 (Economy + Positioning) done by three agents, checked on captures: Macro (globe on
  top, registry with finger scrolling, Asset File and detail view stacked), Rates (2x2 cards, two-band bank rows,
  charts and committees in one column), Retail sentiment (cards 2x3, table reduced to symbol / bar / long % / status,
  modal one column), COT (header buttons wrap, every panel full width, report tables condensed), Bank positioning
  (three-row header, two-line calls, overlay one column), Risk sentiment (one column). Desktop unchanged (pixel diffs).
  Installed on the iPhone; the launch failed because the phone was locked. Possible shared tweak not made:
  pk::TitleBar dropping its badge itself when it would reach the title.
- 2026-11-21 phone layout, group 3 (Market + Account) done by three agents, checked on captures (desktop pixel diffs at
  1600x1042 and 1024x600): Markets watch (wrapping pills, 2x2 cards, stacked panels; instrument sheet two-row toolbar),
  Seasonality (toolbar wraps and scrolls with the cards), Fundamental heatmap (two-line currency rows), Technical heatmap
  (two-line instrument rows with six tiles, pinned header, signal file as an overlay; MarketKit setTitleRoom/setWrap),
  Liquidity (stacked cards, wrapped legend, touch instrument list), Track record (three-row toolbar, one-column
  dashboard, two-line trade rows, full-width dialogs), Risk calculator (form first, stacked cards). New test hooks:
  DW_TH_FILE, DW_RC_SCROLL. Installed on the iPhone; the launch failed because the phone was locked.
- 2026-11-21 phone layout, group 4 (Data + AI + Sources) done by four agents, checked on captures (desktop pixel diffs):
  Macro data (one column, chains panel then the view; fixed a crash of the DW_MACRODATA_SELECT hook), Technical data
  (one column below 960, narrow tables, order-flow tabs scroll sideways; PairDialog phone version), LLM (Local models
  and Integration stacked; new src/pages/ai/PhoneScroll.h), Agents (cubes in one column, chat full screen with the
  activity panel below, EVALUATION dialog phone version; drag a model onto a cube by finger NOT done), ML (DATA |
  MODEL | RUNS bar showing one column at a time, compare rows on two lines), Sources (cards with 48x34 switches, 2x3
  filter grid, stacked key fields), Sources graph (two columns fitted to the width, tap lights a node's links). Also:
  the EVALUATION dialog scrolls above OK / Cancel at every size (macOS squeezed its rows at 1600x1042); with touch,
  MainWindow::open clears a text field's focus so iOS doesn't open the keyboard by itself. iOS message of ML ("install
  Python from python.org") not adapted yet. Charts (group 5) remains.
