# Predictive power of order flow, order book and footprint features for short-horizon returns (BTC / crypto), state of evidence as of 2026-11-20

Scope note: researched 2026-11-20 with web search and fetches. Some figures come from abstracts or search snippets because full texts were paywalled (403) or too long; these are flagged. "Peer-reviewed" means a journal version was confirmed (Mathematical Finance, Quantitative Finance, JFE, J. Financial Econometrics, Digital Finance, Frontiers AI). arXiv/SSRN-only items are working papers. Project context: a 3-month BTCUSDT test (Aug-Nov 2026) with 30 bar-level footprint/depth/OI features gave no gain for next-bar direction at 5m/15m/1h.

## 1. Order flow imbalance (OFI), multi-level OFI, trade flow imbalance, queue imbalance: horizons, contemporaneous vs predictive, decay

### Takeaway
The strong results for OFI are **contemporaneous**: OFI over an interval explains the price change over that same interval. That relationship is linear, and its slope is inversely proportional to depth. The **predictive** part is small and lives at the scale of the next tick or the next few price changes: queue imbalance predicts the next mid-price move, Kolm et al. find an effective horizon of about 2 average price changes, and cross-asset OFI "decays rapidly". At 5-minute to 1-hour bars, the literature gives no reason to expect bar-level OFI or imbalance aggregates to predict the next bar's direction.

### Cited Findings
- Cont, Kukanov & Stoikov ("The price impact of order book events", J. Financial Econometrics 2014; arXiv 1011.6402): with NYSE TAQ data for 50 US stocks, "over short time intervals, price changes are mainly driven by the order flow imbalance". The relation is linear, "with a slope inversely proportional to the market depth". This is a contemporaneous regression (price change over [t, t+Δ] on OFI over the same interval), not a forecast. — [arXiv 1011.6402](https://arxiv.org/abs/1011.6402)
- Cont, Cucuringu & Zhang ("Cross-impact of order flow imbalance in equity markets", Quantitative Finance 2023, 23(10):1373-1393): **integrated OFI**, the first principal component of OFI across the top book levels, explains impact better than best-level OFI in and out of sample. For contemporaneous impact, cross-asset terms add nothing once multi-level OFI is used. For **future** returns, cross-asset OFIs do improve forecasts (R² and economic gain), but the effect "mainly manifests at short-term horizons and decays rapidly in time". — [arXiv 2112.13213v4](https://arxiv.org/html/2112.13213v4); [RePEc QF record](https://econpapers.repec.org/RePEc:taf:quantf:v:23:y:2023:i:10:p:1373-1393)
- Kolm, Turiel & Westray ("Deep order flow imbalance: extracting alpha at multiple horizons from the limit order book", Mathematical Finance 2023, 33(4):1044-1081):
  - Data: nanosecond-timestamped order book events for 115 Nasdaq stocks, Jan 2019 to Jan 2020.
  - Models: MLP, LSTM, LSTM-MLP, stacked LSTM and CNN-LSTM.
  - Models fed **order flow** (OFI per level) significantly beat most models fed raw order book states.
  - Forecasts are a "term structure" of mid-price returns over several horizons. The **effective horizon is about two average price changes**: alpha peaks there and declines afterwards.
  - "Information-rich" stocks are more predictable.
  — [SSRN 3900141](https://papers.ssrn.com/abstract=3900141); [IDEAS record](https://ideas.repec.org/a/bla/mathfi/v33y2023i4p1044-1081.html); [Quantpedia summary](https://quantpedia.com/how-to-use-deep-order-flow-imbalance/)
- Gould & Bonart ("Queue imbalance as a one-tick-ahead price predictor in a limit order book", arXiv 1512.03492, later published in Market Microstructure and Liquidity): logistic regressions of the next mid-price move's direction on best-queue imbalance (bid−ask)/(bid+ask), for 10 liquid Nasdaq stocks. Against a null model, classification improves considerably for **large-tick** stocks and only moderately for **small-tick** stocks. A secondary source reports binary classification gains of about 50-60%, but I could not verify that against the paper. — [arXiv/ar5iv 1512.03492](https://ar5iv.arxiv.org/html/1512.03492); [IDEAS](https://ideas.repec.org/p/arx/papers/1512.03492.html)
- Silantyev ("Order flow analysis of cryptocurrency markets", Digital Finance 2019, 1(1):191-218), using BitMEX XBTUSD perpetual trades and quotes: **trade flow imbalance (TFI) explains contemporaneous price changes better than OFI** on that venue. This points to a large market impact from aggressive trades. Crypto microstructure "shares many features with conventional markets". — [Springer DOI](https://link.springer.com/doi/10.1007/s42521-019-00007-w); [IDEAS](https://ideas.repec.org/a/spr/digfin/v1y2019i1d10.1007_s42521-019-00007-w.html); [author's Medium precursor](https://medium.com/@eliquinox/order-flow-analysis-of-cryptocurrency-markets-b479a0216ad8)
- Makarov & Schoar ("Trading and arbitrage in cryptocurrency markets", JFE 2020, 135(2):293-319): the **common component of signed volume across exchanges explains about 80% of bitcoin returns**. This is contemporaneous explanatory power, much higher than in traditional markets. The idiosyncratic component explains cross-exchange arbitrage spreads. — [EconPapers JFE record](https://econpapers.repec.org/RePEc:eee:jfinec:v:135:y:2020:i:2:p:293-319); [LSE PDF](https://personal.lse.ac.uk/makarov1/index_files/CryptocurrencyMarkets.pdf)

### Inferences
- The most-cited OFI results (CKS, Makarov-Schoar's 80%, Silantyev's TFI) are about **explaining** the price change in the same window. A feature like "bar delta" or "bar OFI" will correlate strongly with the same bar's return, which is how footprint charts look informative when viewed after the fact. That says almost nothing about the *next* bar.
- The predictive horizons in peer-reviewed work run from one tick to a few price changes. On BTCUSDT perp (a very liquid, effectively large-tick book at the top), that is milliseconds to seconds, far below a 5-minute bar. The project's null result at 5m/15m/1h matches the literature.
- The Gould-Bonart and Briola findings (below) point the same way: queue and book imbalance work best for large-tick instruments, where the best queues are thick and price moves are discrete. BTCUSDT on Binance futures has a 0.1 USDT tick at a price near $100k. The touch therefore behaves like a large-tick book: thick queues and a one-tick spread most of the time. This favors tick-level queue-imbalance prediction at very short horizons, not at bar horizons. (This inference rests on the instrument's tick size, not on a crypto-specific study.)

### Gaps
- I did not retrieve the exact R² figures of Cont-Kukanov-Stoikov. The often-quoted average R² of about 65% at 10-second intervals comes from memory and was not checked this session.
- Xu, Gould & Howison's multi-level OFI paper (arXiv 1907.06230, "Multi-level order-flow imbalance in a limit order book") was not fetched. Its horizon and decay figures are not included.
- I found no peer-reviewed paper giving an OFI predictive R² by horizon specifically for Binance BTCUSDT at the 1-minute to 1-hour scale.

## 2. Deep learning on LOB (DeepLOB and follow-ups), crypto LOB studies: horizons and profitability after costs

### Takeaway
DeepLOB-style models show real but very short-horizon skill, measured in book updates (well under a second to about 10 seconds). Skill falls quickly with horizon, especially for small-tick assets, and it degrades on new data (LOBCAST). Benchmark accuracies on FI-2010 are inflated by label smoothing and dataset quirks. The 2024-2025 benchmark papers say outright that **high forecasting metrics do not translate into actionable trading signals**. Crypto LOB studies (Coinbase, Bybit and Binance BTC) report horizons of 100 ms to a few seconds, and none I found shows net-of-cost profitability for a taker strategy.

### Cited Findings
- DeepLOB (Zhang, Zohren & Roberts, IEEE Trans. Signal Processing 2019; arXiv 1808.03668):
  - Architecture: CNN plus Inception plus LSTM on raw LOB data.
  - Results: it beat earlier models on a benchmark LOB dataset, then was tested on one year of LSE quotes with "remarkably stable out-of-sample prediction accuracy".
  - Generalization: it "translates well to instruments which were not part of the training set".
  - The abstract mentions no trading simulation net of costs.
  — [arXiv 1808.03668](https://arxiv.org/abs/1808.03668)
- Prata et al. (LOBCAST, "LOB-based deep learning models for stock price trend prediction: a benchmark study", Artificial Intelligence Review 2024; arXiv 2308.01915): 15 DL models reproduced on FI-2010, then tested on LOBSTER data. "All models exhibit a significant performance drop when exposed to new data, thereby raising questions about their real-world market applicability". Attention-based models did best. — [arXiv 2308.01915](https://arxiv.org/abs/2308.01915v2); [Springer](https://link.springer.com/article/10.1007/s10462-024-10715-4)
- Briola, Bartolucci & Aste ("Deep limit order book forecasting: a microstructural guide", Quantitative Finance 2025, 25(7):1101-1131; arXiv 2403.09267):
  - Setup: 15 Nasdaq stocks (6 small-tick, 3 medium-tick, 6 large-tick), 2017-2019 LOBSTER data with 10 levels, DeepLOB as the case study. Horizons are 10, 50 and 100 LOB updates; 10 updates usually take under 1 s, while 100 updates for small-tick stocks often take 10 s or more.
  - Average MCC (Matthews correlation coefficient) by horizon:

    | Tick class | H10 | H50 | H100 |
    |---|---|---|---|
    | Small-tick | 0.11 | 0.04 | 0.01 ("random forecasts") |
    | Medium-tick | 0.13 | 0.085 | 0.036 |
    | Large-tick | 0.29 | 0.36 | 0.26 |

  - "High forecasting power does not necessarily correspond to actionable trading signals". Their transaction-level metric, the probability of a correct *complete transaction* pT, is only about 0.06-0.17.
  - "Traditional machine learning metrics fail to adequately assess the quality of forecasts".
  — [arXiv HTML v4](https://arxiv.org/html/2403.09267v4); [UCL open-access](https://discovery.ucl.ac.uk/id/eprint/10218102); [IDEAS QF record](https://ideas.repec.org/a/taf/quantf/v25y2025i7p1101-1131.html)
- Kolm et al. 2023 (above): models fed order-flow features beat models fed raw LOB states, and alpha peaks at about 2 price changes. — [SSRN 3900141](https://papers.ssrn.com/abstract=3900141)
- Crypto LOB studies:
  - Jha et al. (arXiv 2010.01241, "Deep learning for digital asset limit order books"), with temporal CNNs on Coinbase BTC order book data: about 71% walk-forward accuracy at a **2-second** horizon. The project's GitHub reports 76%. Working paper. — [arXiv 2010.01241](https://arxiv.org/pdf/2010.01241)
  - arXiv 2506.05764 ("Exploring microstructural dynamics in cryptocurrency limit order books: better inputs matter more than stacking another hidden layer"):
    - Data and models: Bybit BTC/USDT LOB snapshots at 100 ms to multi-second sampling; logistic regression, XGBoost, DeepLOB and Conv1D+LSTM, with Kalman and Savitzky-Golay filtering and binary or ternary labels.
    - Finding: "simpler models can match and even exceed the performance of more complex networks" once preprocessing and tuning are done.
    - The abstract does not say whether costs are modelled. Working paper.
    — [arXiv 2506.05764](https://arxiv.org/abs/2506.05764); [ResearchGate](https://www.researchgate.net/publication/392513905_Exploring_Microstructural_Dynamics_in_Cryptocurrency_Limit_Order_Books_Better_Inputs_Matter_More_Than_Stacking_Another_Hidden_Layer)
  - Xiao et al., "LiT: limit order book transformer" (Frontiers in AI 2025): Binance L2 data rebuilt at millisecond resolution, top 20 levels, several horizons, benchmarked against DeepLOB and TransLOB. Profitability not found in the snippet. — [Frontiers](https://www.frontiersin.org/journals/artificial-intelligence/articles/10.3389/frai.2025.1616485/full)
  - arXiv 2602.00776 ("Explainable patterns in cryptocurrency microstructure"):
    - Data: Binance Futures perps (BTC, LTC, ETC, ENJ, ROSE) at **1-second** frequency, Jan 2022 to Oct 2025.
    - Method and findings: CatBoost with a direction-aware loss. SHAP rankings and partial-effect shapes are **stable across assets**, and they tie into OFI, spread and adverse selection.
    - Tradability: taker (top-of-book) and maker backtests behave very differently, especially in a flash crash, consistent with adverse-selection theory. The abstract gives no numbers. Working paper.
    — [arXiv 2602.00776](https://arxiv.org/abs/2602.00776)
- Costs at bar horizons (not LOB, but on the same instrument): arXiv 2606.00060 ("Machine learning-based bitcoin trading under transaction costs: evidence from walk-forward forecasting"):
  - Data: Binance BTCUSDT futures, hourly bars, 2018-2025 out of sample. XGBoost, LSTM and iTransformer with 27 walk-forward folds, at a 10 bp cost per unit of turnover.
  - "Naive sign-based strategies fail once transaction costs of ten basis points are imposed". XGBoost long-only falls from an annualized return of 73.5% gross to −64.0% net.
  - A cost-aware filter cuts trades from 10,619 to 251 and restores an annualized return of 65.4% with Sharpe 1.09, but **no cost-aware strategy significantly beats buy-and-hold on Sharpe after Holm correction**.
  - The authors place the main obstacle in "the way forecasts are converted into trades". Working paper; it uses OHLCV only, with no order-book, funding or liquidation features.
  — [arXiv 2606.00060](https://arxiv.org/html/2606.00060v1)

### Inferences
- Every credible DL-on-LOB result sits at horizons of 10-100 book events, under a second to about 10 s. At those horizons the predicted move is about 1 tick, smaller than taker fees plus the spread. Turning that into money needs maker execution (queue position, adverse selection), not bar-level direction trading.
- For a 1-minute-bar dataset, raw-book DL is the wrong tool. The useful lesson from Kolm et al. and 2506.05764 is that well-engineered *flow* features plus simple models do as well as deep nets.
- The 2606.00060 result shows that even a hypothetical edge at the 1-hour horizon on BTCUSDT is consumed by about 10 bp of cost unless turnover is cut drastically. Next-bar direction at 5m/15m is a harder target still.

### Gaps
- I did not find a peer-reviewed crypto LOB paper that reports net-of-cost P&L for a taker strategy with realistic Binance fees. The taker/maker backtests in 2602.00776 exist, but their numbers were not available.
- Exact DeepLOB horizons (k = 10, 20, 50, 100 events on FI-2010) and accuracies (roughly 70-84%) are from memory, not verified this session.

## 3. Crypto-specific signals: funding, open interest, long/short ratios, liquidations, VPIN, spot vs perp price discovery

### Takeaway
Evidence that funding, OI, long/short ratios or liquidations predict short-horizon direction is thin and mostly not peer-reviewed. The predictable part is mainly the funding rate itself, early warnings before liquidation cascades (a tail or volatility event, not direction), and toxicity predicting **jumps or volatility**. For price discovery, futures and perps mostly lead spot at 1-second sampling, with episodes where spot leads. One 2026 working paper finds that order imbalance at the quarter-hour open on Binance perps predicts returns over the following **4-12 hours**, a horizon that fits the project's data better than next-bar prediction.

### Cited Findings
- Funding:
  - Inan (SSRN 5576424, "Predictability of funding rates"): on Binance and Bybit BTC perps, double-autoregressive models beat the no-change model on forecast error and directional accuracy **for the funding rate**, not for price returns. Working paper. — [SSRN 5576424](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=5576424)
  - "Fundamentals of perpetual futures" (arXiv 2212.06888): in perp-spot trading returns, price convergence dominates and funding payments play a smaller role that seems to diminish over time. — [arXiv 2212.06888](https://arxiv.org/pdf/2212.06888)
- Liquidations, OI and long/short ratios:
  - arXiv 2607.27070 ("Where does the criticality live? Early-warning signals are event-heterogeneous across seven crypto-perpetual liquidation cascades"): before cascades, the lag-1 autocorrelation of **open interest** and of the **top-trader long/short ratio** rises in many configurations, while price autocorrelation does not. The variance spike in price is coincident with the cascade, not a warning. The signals are event-heterogeneous (7 cascades), in-sample, working paper. — [arXiv 2607.27070](https://arxiv.org/pdf/2607.27070)
  - arXiv 2102.04591 ("Liquidation, leverage and optimal margin in bitcoin futures markets"): on average about 3.5% of long OI and 1.9% of short OI is liquidated per day. This is descriptive, not predictive. — [arXiv 2102.04591](https://arxiv.org/pdf/2102.04591)
- Order flow at longer horizons:
  - "Order flow and cryptocurrency returns" (Journal of Financial Markets 2026, ScienceDirect S1386418126000029): per the search snippet, order-flow-sorted **cross-sectional** long-short strategies earn about 1.8% per week, Sharpe about 1.9. The full text returned 403, so I could not verify the method, horizon (weekly, cross-section of coins) or cost treatment. — [ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1386418126000029)
  - arXiv 2607.09426 ("The quarter-hour effect: periodic algorithmic trading and return predictability in cryptocurrency futures"):
    - Data: six Binance perps.
    - Volatility and volume burst at 1-minute, 5-minute and especially quarter-hour marks. These bursts coincide with fewer round trade sizes, which the authors attribute to algorithmic trading.
    - Opening returns are predictable out of sample, and **opening order imbalance predicts returns over about 4-12 hours**. Effects at finer clock-time frequencies are much weaker.
    - Costs are not mentioned in the abstract. Working paper.
    — [arXiv 2607.09426](https://arxiv.org/abs/2607.09426)
- Toxicity (VPIN):
  - Easley, López de Prado & O'Hara (RFS 2012) introduced VPIN as a predictor of short-term toxicity-induced volatility. Andersen & Bondarenko counter that BVC-VPIN has **no incremental power for future volatility** once trading intensity and volatility are controlled for. The debate is unresolved. — [SSRN 1695596](https://papers.ssrn.com/abstract=1695596); [CREATES rp13-43 (Andersen-Bondarenko)](https://repec.econ.au.dk/repec/creates/rp/13/rp13_43.pdf)
  - Easley, O'Hara, Yang & Zhang ("Microstructure and market dynamics in crypto markets", SSRN 4814346, 2024):
    - Data: Jan 2021 to Jul 2023, including the crypto winter, with ML classifiers on standard microstructure features.
    - Findings: own-market and cross-market effects for BTC and ETH **Roll measures and VPIN**, stable through the crypto winter. Crypto dynamics are "similar to those of other investible asset classes".
    - Their labels, from the Easley et al. 2021 framework, are mostly about volatility, spreads, skewness, kurtosis and autocorrelation, not just the sign. The exact crypto tables were not retrieved.
    — [Cornell PDF](https://stoye.economics.cornell.edu/docs/Easley_ssrn-4814346.pdf)
  - A study attributed to Kitvanitphasu et al. (2025), per a search summary: VPIN-style toxicity significantly predicts future **bitcoin price jumps** and correlates with jump size. I could not confirm the journal or URL; it may be the MDPI JRFM item in the results. — [MDPI JRFM 19(1):59 (attribution unverified)](https://www.mdpi.com/1911-8074/19/1/59)
- Price discovery (who leads):
  - A 2025 Journal of Futures Markets paper with 1-second sampling finds futures generally lead spot. It argues earlier conflicting results came from differences in measures, sampling frequency and noise (search snippet).
  - arXiv 2506.08718 compares Binance spot with CME Micro BTC futures: CME typically leads, with information share about 0.54-0.56, but spot led during the 5 Aug 2024 event (0.84-0.87).
  — [arXiv 2506.08718](https://arxiv.org/abs/2506.08718v1); [Macquarie record "Price discovery in bitcoin: spot or futures? The jury is out"](https://researchers.mq.edu.au/en/publications/price-discovery-in-bitcoin-spot-or-futures-the-jury-is-out/)
  - Kaiko (industry, not peer-reviewed) uses the perp/spot volume ratio as a proxy for where price discovery happens and reports it shifting toward futures. — [Kaiko](https://research.kaiko.com/insights/the-state-of-crypto-derivatives)

### Inferences
- OI and long/short ratios look more useful as **regime or tail-risk** inputs (cascade risk, volatility) than as next-bar direction predictors. The cascade paper's signal is a rising *autocorrelation* of OI and long/short, a critical-slowing-down style feature, not their level.
- Cross-exchange lead-lag (Binance perp vs spot, vs CME, vs Coinbase) exists at seconds-level sampling, and that is where its predictive value lies. At 5m+ bars it is arbitraged away. If the project wants a lead-lag feature, it needs second-level aligned data from 2+ venues.
- The quarter-hour paper suggests that **when** the imbalance is measured (at the quarter-hour open) matters more than bar-level averages, and that the target horizon should be hours, not the next bar.

### Gaps
- I found no peer-reviewed study that tests Binance's published long/short ratios (global accounts, top traders) as return predictors out of sample.
- No reliable study found on liquidation-heatmap or "liquidation magnet" claims (price drawn to clustered liquidation levels). These appear to be practitioner claims.
- The full text of the JFM 2026 "Order flow and cryptocurrency returns" paper was unavailable, so its horizon and cost treatment are unverified.

## 4. Footprint, volume profile, imbalance and absorption: academic evidence or practitioner claims?

### Takeaway
I found **no peer-reviewed or even working-paper evidence** that footprint constructs (diagonal bid/ask imbalances, stacked imbalances, absorption, POC/VAH/VAL, delta divergence) predict returns out of sample. These are practitioner or vendor frameworks (Market Profile, Wyckoff, VSA). The parts with academic grounding are the generic ones: signed volume or trade flow imbalance (contemporaneous impact), depth-scaled impact, and the adverse selection of resting liquidity.

### Cited Findings
- Searches for academic evidence on volume profile, footprint absorption and point of control returned only vendor blogs and TradingView scripts (ATAS, footprint indicators), with no backtests or statistical tests. These sources define absorption mechanically, for example when a third of the bar's range holds a given share of volume while delta runs against the bar, and tie it to Wyckoff/VSA. — [ATAS blog](https://atas.net/blog/volume-profile-strategy-nq-futures/); [TradingView FootprintKit](https://www.tradingview.com/script/n807k0fu-Volume-Footprint-Simplifyed/); [TradingView CME order-flow AMT lens](https://www.tradingview.com/script/EYNNWCWE-CME-Institutional-Order-Flow-AMT-Lens/)
- One indicator states that TradingView "does not provide a true bid/ask tick feed, so delta here is an approximation". Many retail footprint claims therefore rest on approximated aggressor side. — [TradingView scripts on delta](https://in.tradingview.com/scripts/delta/)
- Academic analogues of footprint ingredients:
  - Trade flow imbalance explains BitMEX price changes better than OFI (contemporaneous). — [Silantyev 2019](https://link.springer.com/doi/10.1007/s42521-019-00007-w)
  - Signed volume explains about 80% of BTC return variation (contemporaneous). — [Makarov & Schoar 2020](https://econpapers.repec.org/RePEc:eee:jfinec:v:135:y:2020:i:2:p:293-319)

### Inferences
- Binance gives a true aggressor flag, so the project's footprint data (buy/sell volume at 1 bp levels) is cleaner than most retail footprints. The absence of a next-bar effect is still expected, because the effect that footprints visualize is contemporaneous impact.
- If footprint features are to be tested fairly, two things would help. First, define them as events (absorption = high aggressive volume with little price move, i.e. high depth or low Kyle lambda), not as 30 bar-level averages. Second, test them against volatility, range or reversal targets conditioned on location (prior high/low, POC), not unconditional direction. This is a hypothesis, not established evidence.

### Gaps
- No academic study found on "stacked imbalance", "absorption" or "POC as support/resistance" for any asset. Market Profile (Steidlmayer) literature is practitioner-based. I did not find formal tests and cannot claim none exist; I found none.

## 5. Targets other than direction: volatility, range, jumps

### Takeaway
Microstructure features have a better documented record for **volatility, jumps and liquidity outcomes** than for return sign: VPIN and Roll predict crypto market dynamics (Easley et al. 2024), toxicity predicts BTC jumps, and OI/long-short autocorrelation rises before cascades. The gains are model-dependent and not automatic: naively adding order book features to BTC volatility models did not help, and EWMA is a hard baseline.

### Cited Findings
- Guo, Antulov-Fantulin et al., "Bitcoin volatility forecasting with a glimpse into buy and sell orders" (arXiv 1802.04065; ICDM 2018 per the arXiv record):
  - "Simply adding features from order book does not necessarily improve the performance".
  - A temporal mixture model that gates between volatility history and order book features does improve, and the gates show order book features matter most in high-volatility regimes.
  - A simple **EWMA beats all but the mixture models in some intervals**.
  — [arXiv 1802.04065v3](https://arxiv.org/html/1802.04065v3)
- Easley-O'Hara-Yang-Zhang (2024): Roll and VPIN predict crypto market dynamics, own- and cross-market, stable across regimes. — [Cornell PDF](https://stoye.economics.cornell.edu/docs/Easley_ssrn-4814346.pdf)
- VPIN-style toxicity predicts BTC price jumps and their size (attribution unverified, see section 3). — [MDPI JRFM](https://www.mdpi.com/1911-8074/19/1/59)
- Critique: once intensity and volatility are controlled for, VPIN has no incremental volatility-forecasting power (equity futures). — [Andersen & Bondarenko, CREATES rp13-43](https://repec.econ.au.dk/repec/creates/rp/13/rp13_43.pdf)
- Early warnings before liquidation cascades come from OI and long/short autocorrelation, not from price. — [arXiv 2607.27070](https://arxiv.org/pdf/2607.27070)
- Quarter-hour bursts of volatility and volume on Binance perps are periodic, so this structure is predictable by the clock. — [arXiv 2607.09426](https://arxiv.org/abs/2607.09426)
- Related (not fetched): a 2026 Springer paper on BTC price movements with multivariate Hawkes processes and LOB data, and arXiv 2607.17555 on volatility-aware extreme-event detection in HF markets. — [Springer 10.1007/s10203-026-00570-z](https://link.springer.com/article/10.1007/s10203-026-00570-z); [arXiv 2607.17555](https://arxiv.org/pdf/2607.17555)

### Inferences
- Realistic targets for the project's data are next-bar realized volatility, range, absolute return, jump or no-jump, and the probability of a ≥X% move or liquidation cascade within N hours. These are more predictable than direction. The right baseline is HAR or EWMA on realized volatility, and the question is whether depth, OI and footprint features add *incremental* skill over it.
- Volatility is strongly persistent, so naive models score well. The test must be incremental over HAR/EWMA, or the gain will be illusory.

### Gaps
- No study found that measures the incremental value of perp depth bands (±0.2..5%) for BTC realized volatility or range forecasting beyond HAR. This is an open, testable question.

## 6. Sample sizes, periods, typical accuracy levels and their economic meaning

### Takeaway
LOB studies use days to about a year of tick data. Predictability is measured over seconds and is strongest for large-tick assets (MCC about 0.3, accuracy above 70%). Small-tick or longer horizons fall to near-random (MCC about 0.01-0.04). At bar horizons, honest out-of-sample directional accuracy for BTC is typically in the low 50s. Even an edge that is real gross is wiped out by about 10 bp costs unless turnover is drastically reduced, and Sharpe gains over buy-and-hold are then not statistically significant.

### Cited Findings
- Sample sizes and periods:

  | Study | Data | Period / size |
  |---|---|---|
  | Kolm et al. | 115 Nasdaq stocks | 13 months |
  | Briola et al. | 15 stocks | about 45 train / 5 validation / 10 test days per year, 2017-2019; 135 experiments, 959 GPU-hours |
  | Easley et al. (crypto) | BTC, ETH | 2.5 years |
  | arXiv 2602.00776 | Binance perps, 1 s | 2022 to Oct 2025 |
  | arXiv 2606.00060 | BTCUSDT hourly | 70,128 observations, 2018-2025, 27 folds |

  — [SSRN 3900141](https://papers.ssrn.com/abstract=3900141); [arXiv 2403.09267](https://arxiv.org/html/2403.09267v4); [Cornell PDF](https://stoye.economics.cornell.edu/docs/Easley_ssrn-4814346.pdf); [arXiv 2602.00776](https://arxiv.org/abs/2602.00776); [arXiv 2606.00060](https://arxiv.org/html/2606.00060v1)
- Accuracy levels:
  - Large-tick stocks with DeepLOB reach accuracy above 0.7 without probability thresholds and above 0.9 with them, but only on the subset of confident predictions, and with low realized pT of about 0.07-0.17. — [Briola et al.](https://arxiv.org/html/2403.09267v4)
  - Crypto 2-second horizon on Coinbase: about 71% walk-forward accuracy. — [Jha et al.](https://arxiv.org/pdf/2010.01241)
- Economic meaning: at 10 bp cost, naive sign-based hourly BTC strategies go from +73.5% to −64% annualized. A cost filter restores profitability, but there is no significant Sharpe gain over buy-and-hold after multiple-testing correction. — [arXiv 2606.00060](https://arxiv.org/html/2606.00060v1)
- On new data, all 15 LOBCAST models degrade significantly. — [LOBCAST](https://arxiv.org/abs/2308.01915v2)

### Inferences
- A 3-month test (Aug-Nov 2026) holds about 26k 5m bars, 8.8k 15m bars and 2.2k 1h bars. For a binary target, the standard error of accuracy is about 0.5/√n: roughly ±0.3 pp at 5m, ±0.5 pp at 15m and ±1.1 pp at 1h (one standard error; double for 95%). An edge of 51-52% would be hard to separate from noise at 1h, and overlapping or autocorrelated features shrink the effective n further. So "no gain" at 1h is weak evidence either way. At 5m, it is fairly strong evidence that the features add nothing above about 1 pp.
- Even 52-53% directional accuracy on 5m bars rarely pays, because the average absolute 5m BTC move is of the same order as round-trip taker costs (fees plus spread plus slippage). The literature's verdict is that cost-aware trade conversion matters as much as the forecast.
- Realistic targets for the project's data:
  1. Tick or second-level next mid move from queue imbalance and OFI on the live book. This is the only place direction is well documented as predictable, and it only pays with maker execution.
  2. Volatility, range, jump or cascade-risk targets at 5m-4h, with OI and depth features tested incrementally over HAR/EWMA.
  3. Event-conditioned imbalance (quarter-hour opens, after liquidation bursts) with multi-hour horizons.
  4. Cross-venue lead-lag at second resolution.

  Next-bar direction from bar-averaged footprint features is the least supported target in the literature.

### Gaps
- No standard published figure exists for "typical BTC 5m/15m/1h directional accuracy out of sample". The low-50s level is an inference from the near-random MCC at longer horizons and the cost-driven results, not a cited number.
- The precise Binance fee tier and slippage assumptions for the project are not covered here. 2606.00060's 10 bp all-in cost per unit of turnover is one published assumption.
