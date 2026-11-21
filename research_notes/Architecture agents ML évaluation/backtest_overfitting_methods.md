# Methods against backtest overfitting and for out-of-sample evaluation of trading strategies / financial ML (state as of 2026-11-20)

Notation used below: "verified" means the fact was read in a fetched primary page or repo during this session (2026-11-20). "From knowledge" means standard literature facts I did not re-fetch; they are flagged and listed again under Gaps so the writer can soften them if needed.

## 1. López de Prado toolkit: purged k-fold + embargo, CPCV, PBO/CSCV, Deflated Sharpe, MinBTL, triple-barrier, meta-labeling; and the evidence comparing walk-forward vs CPCV

### Takeaway
The López de Prado / Bailey family of tools attacks two different failure modes: leakage between train and test (purging, embargo, CPCV) and selection bias from trying many configurations (PBO via CSCV, Deflated Sharpe, MinBTL). The only controlled head-to-head study found (Arian, Norouzi Mobarekeh & Seco, Knowledge-Based Systems 2024) favours CPCV over walk-forward and plain k-fold on PBO and DSR, but it is a synthetic-data study by the method's proponents' school and has not been widely replicated.

### Cited Findings

**Purged k-fold and embargo (AFML 2018, ch. 7)**
- Purging = removing from the training set every observation whose label horizon overlaps in time with a test-set label; embargo = additionally removing training observations that immediately follow a test block, to guard against serial correlation. This is how skfolio documents its implementation (`purged_size`, `embargo_size`) — [skfolio CombinatorialPurgedCV docs](https://skfolio.org/generated/skfolio.model_selection.CombinatorialPurgedCV.html)
- skfolio's `purged_size` removes observations on both sides of each test set (end of training sets before a test set and start of training sets after it); `embargo_size` removes observations only at the start of training sets that follow a test set — [skfolio docs](https://skfolio.org/generated/skfolio.model_selection.CombinatorialPurgedCV.html)
- When to use (from knowledge, AFML ch. 7): whenever labels span several bars (e.g. triple-barrier labels, forward returns over h bars) so that adjacent samples share information; plain `KFold` with shuffling leaks future information. Limit: purging needs per-sample label start/end times (t0, t1); embargo size is a heuristic (AFML suggests ~1% of samples) — not re-verified this session.
- scikit-learn's simpler substitute: `TimeSeriesSplit(gap=...)` — "Number of samples to exclude from the end of each train set before the test set", added in v0.24 together with `test_size`; `max_train_size` caps the training window (rolling instead of expanding). Docs current version 1.9.1 (2026). Example with 12 samples, `n_splits=3, test_size=2, gap=2`: fold 0 train [0..3] test [6,7] — [scikit-learn TimeSeriesSplit docs](https://scikit-learn.org/stable/modules/generated/sklearn.model_selection.TimeSeriesSplit.html). Limit: `gap` only purges before the test fold (it is walk-forward, never trains on data after the test), and it counts samples, not label end times.

**Combinatorial Purged Cross-Validation (CPCV, AFML 2018, ch. 12)**
- skfolio describes CPCV as: split into `n_folds` consecutive unshuffled folds; each split uses `n_test_folds` (p) folds as test and k − p as train (vs k − 1 for KFold); while KFold recombines a single test path, CPCV "can recombine multiple testing paths from the combinations of train/test sets". Constraints: n_folds ≥ 3, n_test_folds ≥ 2; defaults n_folds=10, n_test_folds=8; exposes `n_splits`, `n_test_paths`, `recombined_paths`, `get_path_ids`, `plot_train_test_folds`, `summary`; reference cited: López de Prado, *Advances in Financial Machine Learning* (2018) — [skfolio docs](https://skfolio.org/generated/skfolio.model_selection.CombinatorialPurgedCV.html)
- skfolio's `CombinatorialPurgedCV` returns a Population of portfolios (one per path) whereas `WalkForward` returns a single MultiPeriodPortfolio (third-party guide, consistent with the official docs) — [silviobaratto optimizer guide](https://silviobaratto.github.io/optimizer/guide/validation/)
- Combinatorics (from knowledge, AFML ch. 12): with N groups and k test groups there are C(N,k) splits and φ = (k/N)·C(N,k) backtest paths; e.g. N=6, k=2 → 15 splits, 5 paths. Each path is a full out-of-sample history, so one gets a distribution of Sharpe ratios instead of one number.
- Limits (inference + practitioner commentary): most training sets contain data from after the test period, so CPCV is valid only if the data-generating process is roughly stationary over the sample and the model does not exploit time ordering; cost grows combinatorially; paths are not independent (they reuse the same observations).

**Probability of Backtest Overfitting via CSCV (Bailey, Borwein, López de Prado & Zhu)**
- "The Probability of Backtest Overfitting", *Journal of Computational Finance*, published April 2017 (DOI 10.21314/JCF.2016.322; SSRN 2326253 first posted 2013, listed as forthcoming 2015). It proposes a framework to estimate PBO "implemented through combinatorially symmetric cross-validation (CSCV)" — [WMU ScholarWorks record](https://scholarworks.wmich.edu/math_pubs/42); [SSRN 2326253](https://papers.ssrn.com/abstract=2326253); [Risk.net / J. Computational Finance](https://www.risk.net/journal-of-computational-finance/2471206/the-probability-of-backtest-overfitting)
- The journal summary states that CSCV gives reasonable PBO estimates with relatively small errors — [Risk.net](https://www.risk.net/journal-of-computational-finance/2471206/the-probability-of-backtest-overfitting)
- Mechanics (from knowledge): take the T×N matrix of returns of all N tried configurations, split rows into S equal blocks, for every one of the C(S, S/2) halvings pick the in-sample best, record its out-of-sample rank ω; logit λ = ln(ω/(1−ω)); PBO = share of halvings where λ ≤ 0 (the IS winner is below the OOS median). Requires the full record of all trials, not just the winner. Model-free and nonparametric, but assumes the blocks are exchangeable (stationarity) and gives equal-sized IS/OOS halves, so it is weak on short histories.

**Deflated Sharpe Ratio (Bailey & López de Prado 2014)**
- "The Deflated Sharpe Ratio: Correcting for Selection Bias, Backtest Overfitting and Non-Normality", *Journal of Portfolio Management* 40(5), 94–107, 2014 (40th anniversary issue) — [SSRN 2460551](https://papers.ssrn.com/abstract=2460551); [Wikipedia summary](https://en.wikipedia.org/wiki/Deflated_Sharpe_ratio)
- DSR "corrects for two leading sources of performance inflation: selection bias under multiple testing and non-normally distributed returns"; "not controlling for the number of trials involved in a particular discovery leads to overly optimistic performance expectations" — [SSRN 2460551](https://papers.ssrn.com/abstract=2460551)
- DSR is "a PSR where the rejection threshold is adjusted to reflect the multiplicity of trials" (PSR = Probabilistic Sharpe Ratio, which uses skewness and kurtosis) — [CAIA commentary](https://caia.org/node/5735); [CXO Advisory summary](https://www.cxoadvisory.com/?p=25715)
- Inputs needed (from knowledge): observed SR, track length T, skew, kurtosis, number of independent trials N, and the variance of SRs across trials (the benchmark SR0 ≈ expected max of N draws). The weak point is N: trials are correlated, so the effective N must be estimated (see ONC below).

**Minimum Backtest Length (MinBTL; Bailey, Borwein, López de Prado & Zhu, "Pseudo-Mathematics and Financial Charlatanism", Notices of the AMS, 2014)**
- The companion paper examines the effects of backtest overfitting on out-of-sample performance and discusses a minimum backtest length as a function of the number of trials attempted — [UW student review of the paper](https://sites.math.washington.edu/~morrow/336_18/papers17/akshay.pdf); full text (CC BY-NC 4.0) at [eScholarship](https://escholarship.org/content/qt4w1110bb/qt4w1110bb.pdf)
- From knowledge, not re-verified (the AMS PDF returned 403 and the eScholarship PDF could not be text-extracted): MinBTL grows with ln(N); headline example "with 5 years of daily data, no more than ~45 independent configurations should be tried" or one should expect an in-sample SR ≈ 1 strategy with zero expected OOS SR; the paper also argues that a simple hold-out is insufficient because the researcher re-uses it.

**Implementations of these specific statistics**
- pypbo (esvhd/pypbo) implements PBO, probability of OOS below threshold, performance degradation, stochastic dominance, PSR, MinTRL, MinBTL and DSR; TODO list includes a Harvey & Liu threshold and more tests; licence AGPL-3.0; 146 stars; last push 2026-07-06; no releases — [GitHub esvhd/pypbo](https://github.com/esvhd/pypbo); licence/date via GitHub API, 2026-11-20
- Stefan Jansen's *Machine Learning for Trading* repo has a multiple-testing / DSR notebook (chapter 08 workflow) — [GitHub ml4t 08_ml4t_workflow/01_multiple_testing](https://www.github.com/stefan-jansen/machine-learning-for-trading/blob/main/08_ml4t_workflow/01_multiple_testing)

**Triple-barrier labeling and meta-labeling (AFML 2018, ch. 3)**
- From knowledge, not re-fetched: triple-barrier labels each event by which is hit first — profit-take barrier, stop-loss barrier (both scaled by rolling volatility) or a vertical time barrier; output label in {−1, 0, 1} plus the label end time t1 that purging needs. Meta-labeling trains a secondary binary classifier that, given a primary model's side (long/short), predicts whether to act and how big (probability → bet size); it improves precision/F1 and separates "side" from "size". Overfitting relevance: overlapping triple-barrier labels are exactly why purging and sample-uniqueness weights (AFML ch. 4) are needed. Implemented in mlfinlab (proprietary, see §4) and in many MIT re-implementations; I did not verify a specific maintained open-source package this session.

**Evidence: walk-forward vs purged k-fold vs CPCV**
- Arian, Norouzi Mobarekeh & Seco, "Backtest overfitting in the machine learning era: A comparison of out-of-sample testing methods in a synthetic controlled environment", *Knowledge-Based Systems*, 2024, DOI 10.1016/j.knosys.2024.112477 (SSRN preprint 4778909). The framework accounts for non-stationarity, autocorrelation and regime shifts; CPCV shows lower PBO and higher DSR statistic than traditional methods; walk-forward "exhibits notable shortcomings in false discovery prevention, characterized by increased temporal variability and weaker stationarity"; the authors introduce Bagged CPCV and Adaptive CPCV — [Scribd copy of SSRN 4778909](https://www.scribd.com/document/725401650/SSRN-id4778909); abstract details via search snippets (ScienceDirect and SSRN returned 403)
- The preprint mentions merging the Heston stochastic-volatility model into its synthetic environment; other process details (jumps, regimes) could not be confirmed — [Scribd copy](https://www.scribd.com/document/725401650/SSRN-id4778909)
- Practitioner framing: walk-forward produces only a single backtest path, which makes overfitting hard to assess; CPCV is called the "gold standard" by an MQL5 practitioner article (opinion, not evidence) — [MQL5 article 21603](https://www.mql5.com/en/articles/21603)

**Comparison table (synthesis of the sources above + AFML; the "best for" column is inference)**

| | Walk-forward (rolling/expanding; sklearn `TimeSeriesSplit(gap)`) | Purged k-fold + embargo | CPCV |
|---|---|---|---|
| Train data after test? | Never | Yes | Yes |
| Number of OOS paths | 1 | 1 (each obs tested once) | φ = (k/N)·C(N,k) |
| Leakage control | Gap only (sample count) | Purge by label t1 + embargo | Purge + embargo |
| Output | One equity curve, one SR | One SR | Distribution of SR → PBO-like stats |
| Main assumption | Past→future causality; realistic for deployment | Stationarity across sample | Stationarity; exchangeable blocks |
| Main weakness | High variance, one path is easy to overfit by repeated tries; uses little data for early folds | Uses future to predict past; one path | Costly; paths correlated; same stationarity caveat; still needs trial counting |
| Evidence | Worse at preventing false discoveries in Arian et al. 2024 | Middle | Best PBO/DSR in Arian et al. 2024 (synthetic) |
| Best for | Final deployment simulation, regime-sensitive / adaptive models, final hold-out | Hyper-parameter tuning with overlapping labels | Model selection / overfitting diagnostics when many configs are tried |

### Inferences
- A common robust protocol is: CPCV (or purged k-fold) for model and hyper-parameter selection → PBO and DSR computed over the full set of trials → a final, untouched walk-forward / forward paper-trading period that is evaluated once. Walk-forward and CPCV answer different questions (realistic deployment vs. robustness of the selection), so they are complements, not substitutes.
- None of the leakage tools corrects selection bias: a perfectly purged CV still overfits if 1,000 configurations are tried and the best is reported. DSR/PBO need the full trial log.
- For an app where agents launch ML runs, the cheapest high-value step is to log every run (config, returns series) so that PBO/DSR can be computed afterwards; without the log neither statistic can be computed.

### Gaps
- Full text of Arian et al. 2024 was not accessible (ScienceDirect/SSRN 403); exact PBO/DSR numbers, the synthetic processes beyond Heston, and the Bagged/Adaptive CPCV algorithms are unverified. I found no independent replication of its CPCV > walk-forward result.
- MinBTL formula and the "45 configurations / 5 years" example are from knowledge; the AMS PDF returned 403.
- The CPCV path formula φ = (k/N)·C(N,k), the 1% embargo heuristic and triple-barrier/meta-labeling details are from AFML (knowledge); not re-fetched.
- I did not find a 2025–2026 López de Prado paper that supersedes these methods; his later work (e.g. *Machine Learning for Asset Managers*, Cambridge 2020, and causal-factor investing papers) was not reviewed in this session.

## 2. Multiple testing: Harvey-Liu-Zhu t > 3, Harvey-Liu haircut Sharpe, White's Reality Check, Hansen's SPA, Romano-Wolf stepdown, FDR; counting trials (humans and agents)

### Takeaway
Finance has two families of corrections: (a) p-value/t-stat adjustments across a set of tested strategies (Bonferroni, Holm, BHY-FDR; the Harvey-Liu haircut turns these into a Sharpe haircut and a t > 3 hurdle), and (b) bootstrap data-snooping tests that use the joint dependence of all strategies (White's Reality Check, Hansen's SPA, Romano-Wolf StepM, Model Confidence Set) — all implemented in the `arch` Python package. Every method needs an honest trial count; for LLM/agent research a 2026 paper shows that logging every evaluation and deflating by it rejects all agent-discovered strategies tested.

### Cited Findings

**Harvey, Liu & Zhu — "...and the Cross-Section of Expected Returns"**
- *Review of Financial Studies* 29(1), 5–68, 2016; NBER WP 20592 (2014). Argues that with extensive data mining "it does not make sense to use the usual criteria for establishing significance"; a newly discovered factor needs to clear a t-statistic > 3.0 — [NBER w20592](https://nber.org/papers/w20592); [RePEc](https://ideas.repec.org/a/oup/rfinst/v29y2016i1p5-68..html)
- They catalogue 316 factor candidates from 313 papers (number as reported by a citing paper) — [search snippet citing HLZ; NBER PDF](https://www.nber.org/papers/w20592.pdf)
- From knowledge: they use Bonferroni, Holm (FWER) and BHY (Benjamini-Hochberg-Yekutieli FDR under dependence) adjustments; the t > 3 hurdle is argued to be possibly too low because unpublished trials are not counted.

**Harvey & Liu — "Backtesting" (haircut Sharpe)**
- *Journal of Portfolio Management*, Fall 2015. Convert SR to t-ratio, adjust the p-value for the number of tests, convert back; the % difference is the "haircut". Example: t = 3.0 → adjusted p ≈ 0.05 → adjusted t = 2.0. The haircut is nonlinear: "the highest Sharpe ratios are only moderately penalized while the marginal Sharpe ratios are heavily penalized", i.e. the industry rule of thumb of a flat 50% haircut is wrong — [Duke-hosted JPM PDF](https://people.duke.edu/~charvey/Research/Published_Papers/P120_Backtesting.PDF); [Man Group / Oxford-Man summary](https://www.man.com/insights/backtesting); [CME Group copy](https://www.cmegroup.com/content/dam/cmegroup/education/files/backtesting.pdf)
- The paper builds on Harvey & Liu's framework with three multiple-testing approaches (from knowledge: Bonferroni, Holm, BHY) — [Duke PDF](https://people.duke.edu/~charvey/Research/Published_Papers/P120_Backtesting.PDF)

**Bootstrap data-snooping tests (as listed and cited in the `arch` documentation)**
- White's Reality Check: White, H. (2000), "A reality check for data snooping", *Econometrica* 68(5), 1097–1126 — tests H0: no strategy beats the benchmark, accounting for the search over all strategies — [arch background & references](https://bashtage.github.io/arch/multiple-comparison/background.html)
- Hansen's SPA: Hansen, P. R. (2005), "A test for superior predictive ability", *Journal of Business & Economic Statistics* 23(4) — a studentized, less conservative Reality Check (from knowledge: it is robust to adding many poor strategies, which make RC lose power) — [arch background](https://bashtage.github.io/arch/multiple-comparison/background.html)
- Romano-Wolf StepM: Romano, J. P. & Wolf, M. (2005), "Stepwise multiple testing as formalized data snooping", *Econometrica* 73(4), 1237–1282 — identifies *which* strategies beat the benchmark while controlling FWER, stepping down — [arch background](https://bashtage.github.io/arch/multiple-comparison/background.html)
- Model Confidence Set: Hansen, Lunde & Nason (2011), *Econometrica* 79(2), 453–497 — set of models containing the best one at a given confidence — [arch background](https://bashtage.github.io/arch/multiple-comparison/background.html)
- `arch` (Kevin Sheppard) implements SPA (also labelled Reality Check / Bootstrap Data Snooper), StepM and MCS, controlling the familywise error rate, plus IID, stationary, circular-block, moving-block and local-block bootstraps — [GitHub bashtage/arch](https://github.com/bashtage/arch); [arch multiple-comparison docs](https://bashtage.github.io/arch/multiple-comparison/multiple-comparisons.html)
- Assumptions/limits (from knowledge): need the full matrix of loss/return differentials of all tested strategies vs a benchmark over the same period; rely on stationarity and a block-bootstrap block length; FWER control is conservative with thousands of correlated strategies (FDR is the usual alternative).

**False discovery rate**
- From knowledge: Benjamini-Hochberg (1995) controls the expected share of false discoveries under independence; Benjamini-Yekutieli (2001) under arbitrary dependence (the "BHY" used by Harvey-Liu-Zhu). Implemented in `statsmodels.stats.multitest.multipletests` (methods 'bonferroni', 'holm', 'fdr_bh', 'fdr_by') — not re-verified this session.

**Counting trials — effective number, humans and agents**
- López de Prado & Lewis (2019), "Detection of false investment strategies using unsupervised learning methods", *Quantitative Finance* 19(9), 1555–1565: an unsupervised algorithm (ONC clustering) "determines the number of effectively uncorrelated trials carried out in the context of a discovery", which then feeds a familywise false-positive probability; builds on the False Strategy Theorem; validated by Monte Carlo — [RePEc](https://ideas.repec.org/a/taf/quantf/v19y2019i9p1555-1565.html); [CAIA summary](https://caia.org/node/5858); [SSRN 3167017 copy](https://smallake.kr/wp-content/uploads/2020/04/SSRN-id3167017.pdf)
- Human trials: in the Quantopian cohort, the more a strategy was backtested by its author, the larger the gap between backtest and out-of-sample performance — [Quantopian archive, Wiecki et al. 2016](https://quantopian-archive.netlify.app/forum/threads/q-paper-all-that-glitters-is-not-gold-comparing-backtest-and-out-of-sample-performance-on-a-large-cohort-of-trading-algorithms)
- Agent trials: Gençay, "What survives honest evaluation? Leakage-safe, search-aware assessment of LLM-driven trading strategy discovery", arXiv 2608.27734, 27 Aug 2026. Two safeguards: agent may only use registry-validated tools whose feature space excludes look-ahead by construction, and every strategy evaluation is logged and reported performance deflated by the total trial count. Results: a deliberately leaky oracle with Sharpe 35 passed both DSR and PBO (so statistical deflation does not catch leakage); honest evaluation rejected every LLM-discovered strategy across two frontier models, search budgets up to 100 candidates and five repeated runs; passive benchmarks were certified (OOS CIs excluding zero). Data: 453-stock point-in-time US universe and 39-ETF universe with transaction, impact and borrow costs — [arXiv 2608.27734](https://arxiv.org/abs/2608.27734)
- Related agent papers: AlphaAgent (arXiv 2502.16789) limits overfitting via AST-based complexity constraints rather than trial deflation — [arXiv 2502.16789](https://arxiv.org/html/2502.16789v2); an HKUST-GZ agentic factor-investing system (arXiv 2603.14288, March 2026) applies multiple-hypothesis adjustment and OOS signal-decay checks — [arXiv 2603.14288](https://arxiv.org/html/2603.14288v1); "The Alpha Illusion" (arXiv 2605.16895) argues reported alpha of end-to-end LLM trading agents is backtest/prototype evidence, not deployment evidence — [alphaXiv 2605.16895](https://www.alphaxiv.org/abs/2605.16895.md); a 2025 benchmark notes data-snooping bias is especially severe in finance (small samples, low signal-to-noise) — [arXiv 2505.07078](https://arxiv.org/pdf/2505.07078v4)

### Inferences
- For agent-driven research, the trial count must include every config an agent evaluated (including discarded ones and re-runs with different seeds), and the human's own manual iterations; correlated trials can be collapsed to an effective N (ONC or simply clustering of return series).
- Gençay 2026 implies an ordering of guards: leakage-proof data access first (look-ahead by construction), statistics (DSR/PBO/SPA) second — statistics cannot rescue leaked features.
- Practical choice: Harvey-Liu haircut/DSR for a single reported winner; SPA/StepM (arch) when comparing a set of candidate strategies against a benchmark on a common period; BHY-FDR when screening many signals where some false positives are acceptable.

### Gaps
- Harvey-Liu-Zhu's exact adjustment methods (Bonferroni/Holm/BHY) and the 316 count were not read in the paper itself in this session.
- statsmodels FDR implementation details and Benjamini-Hochberg/Yekutieli citations are from knowledge.
- Gençay 2026 is a single-author arXiv preprint (not peer-reviewed as of 2026-11-20); its human-trader benchmark result is not in the abstract.
- I found no standard, published method for counting "trials" made implicitly by an LLM (e.g. ideas it considered without running them); this remains open.

## 3. Time-series-specific robustness: block / stationary bootstrap, Monte Carlo of trades, cost and slippage stress, regime splits

### Takeaway
Resampling methods must preserve serial dependence: block bootstraps (moving, circular, stationary) are the standard and are implemented in `arch`. Trade-order Monte Carlo, cost/slippage stress and regime-split evaluation are mostly practitioner techniques with little formal literature; the main citable evidence is that adding realistic costs materially cuts results (e.g. Qlib's own example) and that leakage and costs, not only selection, drive backtest–live gaps.

### Cited Findings
- `arch` provides IIDBootstrap, StationaryBootstrap, CircularBlockBootstrap, MovingBlockBootstrap and a local block bootstrap, usable for confidence intervals of Sharpe ratios and as the resampling engine of SPA/StepM/MCS; arch v9.0.0 released 2026-11-11 (last release supporting Python 3.10 and NumPy 1.x) — [GitHub bashtage/arch](https://github.com/bashtage/arch); release date via GitHub API, 2026-11-20
- From knowledge: the stationary bootstrap (Politis & Romano 1994, JASA 89(428)) draws blocks of geometric random length with mean block length 1/p, so the resampled series stays stationary; block length choice (e.g. Politis-White automatic selection, available in `arch` as `optimal_block_length`) drives results. Assumes weak dependence/stationarity; regime breaks are not reproduced.
- Monte Carlo of trades (reshuffling or resampling the sequence of trade P&Ls to get a distribution of max drawdown / time-under-water): from knowledge, standard in retail tools (e.g. it is a common feature of strategy-testing platforms); assumes trades are i.i.d., which ignores autocorrelation and regime clustering — no primary academic source found this session.
- Cost stress: Qlib's documented example backtest reports annualized excess return ≈ 0.152 without costs vs ≈ 0.103 with costs (open_cost 0.0005, close_cost 0.0015, min_cost 5, limit_threshold 0.095) and max drawdown −0.059 vs −0.075 — illustrating the size of the cost effect on a daily equity strategy — [Qlib docs, strategy/backtest](https://qlib.readthedocs.io/en/latest/component/strategy.html)
- Gençay 2026 evaluates with transaction, impact and borrow costs on a point-in-time universe (avoids survivorship bias) — [arXiv 2608.27734](https://arxiv.org/abs/2608.27734)
- An arXiv 2026 paper "Implementation Risk in Portfolio Backtesting: A Previously Unquantified Source of Error" (arXiv 2603.20319) addresses error from implementation choices in backtests; contents not reviewed — [arXiv 2603.20319](https://arxiv.org/pdf/2603.20319)
- Regime splits: Arian et al. 2024 build regime shifts into their synthetic test environment, and propose "Adaptive CPCV" that adjusts to market conditions — [Scribd copy of SSRN 4778909](https://www.scribd.com/document/725401650/SSRN-id4778909)
- Synthetic data as an alternative: GAN-generated paths to avoid overfitting a single history (arXiv 2209.04895) and a DRL crypto paper using PBO to reject overfitted agents (arXiv 2209.05559) — [arXiv 2209.04895](https://arxiv.org/pdf/2209.04895); [arXiv 2209.05559](https://arxiv.org/pdf/2209.05559)

### Inferences
- A defensible minimal stress pack: (1) stationary-bootstrap CI on Sharpe and max drawdown; (2) costs at 1×, 2×, 3× of the estimated level plus one-bar execution delay; (3) results split by regime (e.g. high/low volatility, bull/bear, or HMM states — this app already has an HMM/MS-GARCH engine) and a report of the worst regime; (4) parameter-perturbation surface (performance should degrade smoothly around the chosen parameters).
- Trade-reshuffle Monte Carlo is cheap and useful for drawdown distributions but should not be used to claim significance, because it destroys the dependence that produces drawdowns.

### Gaps
- No primary academic source found for trade-sequence Monte Carlo or for standard cost-stress multipliers; these are practitioner conventions.
- Politis & Romano 1994 and Politis-White block length details are from knowledge, not re-fetched.

## 4. Implementations and licences (mlfinlab, skfolio, timeseriescv, scikit-learn, arch, pypbo, vectorbt, Qlib, QuantConnect Lean, Nautilus Trader) and what could be reused

### Takeaway
For permissive reuse, the strongest building blocks in late 2026 are skfolio (BSD-3, CPCV + WalkForward, actively released), arch (NCSA-style permissive, SPA/StepM/MCS + block bootstraps, v9.0.0 Nov 2026), scikit-learn `TimeSeriesSplit(gap)` (BSD-3) and timeseriescv (MIT, but unmaintained since 2022). mlfinlab is proprietary; pypbo is AGPL-3.0; vectorbt is Apache-2.0 *with Commons Clause* (no selling); Nautilus Trader is LGPL-3.0; Lean and zipline-reloaded are Apache-2.0; Qlib is MIT. None of the backtest engines ships PBO/DSR/CPCV as first-class features — those come from the statistics libraries.

### Cited Findings (licence and activity checked via GitHub API on 2026-11-20 unless noted)

| Tool | What it gives for this problem | Licence | Activity (2026-11-20) |
|---|---|---|---|
| skfolio | `CombinatorialPurgedCV` (purge, embargo, path recombination, `optimal_folds_number`), `WalkForward`, `MultipleRandomizedCV`; sklearn-compatible portfolio optimisation | BSD-3-Clause | v1.14.2 released 2026-11-19; 2,660 stars |
| arch | SPA/Reality Check, StepM, MCS; IID/stationary/circular/moving/local block bootstraps | NCSA-style (University of Illinois/NCSA text: "Copyright (c) 2017 Kevin Sheppard"; GitHub reports NOASSERTION; downstream spec labels it NCSA) | v9.0.0 released 2026-11-11 |
| scikit-learn | `TimeSeriesSplit(n_splits, gap, test_size, max_train_size)` | BSD-3-Clause (knowledge) | docs v1.9.1 |
| timeseriescv | `PurgedWalkForwardCV`, `CombPurgedKFoldCV` (sklearn-style) | MIT | last push 2022-02-15; 290 stars (unmaintained) |
| pypbo | PBO (CSCV), PSR, DSR, MinTRL, MinBTL, stochastic dominance | AGPL-3.0 | last push 2026-07-06; 146 stars; no releases |
| mlfinlab (Hudson & Thames) | Full AFML toolkit (labelling, purged CV, CPCV, sample weights, bet sizing, backtest statistics) | Proprietary "all rights reserved"; LICENSE "last updated November 2021"; commercial use needs a paid Business/Enterprise licence | public GitHub repo last commit 2021-12-01, last push 2023-10-02 |
| vectorbt (open-source) | Vectorised backtesting, parameter sweeps (useful to *generate* the trial matrix for PBO) | Apache 2.0 with Commons Clause v1.0 (no right to "Sell") | v1.2.1 released 2026-11-11; 9,899 stars |
| Qlib (Microsoft) | Data → model → backtest pipeline; exchange cost/limit model; `risk_analysis` (mean, std, annualized return, IR, max DD, with/without cost) | MIT | v0.9.8 released 2026-10-08; 50,445 stars |
| QuantConnect Lean | Event-driven engine; walk-forward optimisation via scheduled `self.train(...)` re-fitting over trailing history | Apache-2.0 | pushed 2026-11-19; 22,628 stars |
| Nautilus Trader | High-performance event-driven backtest/live engine (Rust core, Python API) | LGPL-3.0 | v1.231.0 released 2026-08-02; pushed 2026-11-20; 31,701 stars |
| zipline-reloaded | Event-driven equity backtester (Quantopian lineage) | Apache-2.0 | pushed 2026-11-02 |

Supporting sources:
- skfolio CPCV parameters/methods — [skfolio docs](https://skfolio.org/generated/skfolio.model_selection.CombinatorialPurgedCV.html); repo [GitHub skfolio/skfolio](https://github.com/skfolio/skfolio); paper "skfolio: Portfolio Optimization in Python" — [arXiv 2507.04176](https://arxiv.org/pdf/2507.04176)
- arch contents — [GitHub bashtage/arch](https://github.com/bashtage/arch); licence text — [arch LICENSE.md](https://github.com/bashtage/arch/blob/main/LICENSE.md); NCSA label in a downstream spec — [openEuler python-arch.spec](https://eur.openeuler.openatom.cn/results/mywaaagh_admin/pypi/openeuler-22.03-x86_64/00038032-python-arch/python-arch.spec)
- timeseriescv — [GitHub sam31415/timeseriescv](https://github.com/sam31415/timeseriescv)
- pypbo — [GitHub esvhd/pypbo](https://github.com/esvhd/pypbo)
- mlfinlab licence — [mlfinlab LICENSE.txt](https://github.com/hudson-and-thames/mlfinlab/blob/master/LICENSE.txt); "NOT open-source", commercial licences — [MlFinLab documentation (docsearch mirror)](https://docsearch.algolia.com/mcp/docs/repo/hudson-and-thames/mlfinlab); moved to a closed-source/open-core model around August 2020 (blog, secondary) — [dhanushka2001 blog](https://dhanushka2001.github.io/posts/lib_became_open_source); older forks still carry BSD-3 text from the pre-2020 open version, but derived forks such as famlafl state mlfinlab-derived code remains under H&T "all rights reserved" — [PyPI famlafl](https://pypi.org/project/famlafl/); Snyk flags it as possibly discontinued (no PyPI release in 12 months) — [Snyk advisor](https://snyk.io/advisor/python/mlfinlab)
- vectorbt licence — [vectorbt LICENSE.md](https://github.com/polakowo/vectorbt/blob/master/LICENSE.md)
- Qlib backtest/risk analysis — [Qlib docs](https://qlib.readthedocs.io/en/latest/component/strategy.html); [GitHub microsoft/qlib](https://github.com/microsoft/qlib)
- Lean walk-forward: scheduled `self.train` sessions, trailing ~365-day history, grid of (min, max, step) parameters, objective e.g. cumulative return, best set applied; docs note that optimising less frequently "reduce[s] the chance of overfitting" — [QuantConnect WFO docs](https://www.quantconnect.com/docs/v2/writing-algorithms/optimization/walk-forward-optimization)
- Other small packages exist (not evaluated): `purgedcv` on PyPI/conda-forge — [PyPI purgedcv](https://pypi.org/project/purgedcv/); temporalcv "Financial Cross-Validation" — [temporalcv docs](https://temporalcv.readthedocs.io/en/latest/api/cv_financial.html); mlfinpy (an mlfinlab-inspired fork) — [GitHub baobach/mlfinpy](https://github.com/baobach/mlfinpy)

### Inferences
- Licence fit for a public repo shipping a desktop app (this project's repo is public): BSD/MIT/Apache/NCSA (skfolio, arch, sklearn, timeseriescv, Qlib, Lean) are safe to reuse or port; AGPL (pypbo) would force the combined work under AGPL if linked/distributed — re-implementing PBO/DSR from the papers (each is < 200 lines) is cleaner; mlfinlab cannot be reused; vectorbt's Commons Clause forbids selling a product deriving its value from it; Nautilus LGPL-3.0 is usable as a dynamically linked library but modifications to it must be released.
- Since the app's ML stack is C++ with a Python/PyTorch sandbox, the low-cost path is: skfolio (CPCV/WalkForward splitters) + arch (SPA/StepM/bootstraps) inside the Python sandbox, plus own ~100-line PBO-CSCV and DSR functions (or a port of pypbo's logic written from the paper, not copied, to avoid AGPL).
- Backtest engines (Lean, Nautilus, vectorbt, Qlib) solve execution realism (fills, costs, latency), not selection bias; they are worth reusing only if the app's own simulated-trading engine lacks realistic order/cost models.

### Gaps
- skfolio's exact formula for `n_test_paths` and the behaviour of `optimal_folds_number` were not described on the fetched page.
- Whether QuantConnect's walk-forward and its Optimizer are fully available in self-hosted open-source Lean (vs cloud only) was not confirmed.
- Nautilus Trader and vectorbt PRO (the commercial successor of vectorbt) features for robustness testing were not reviewed.
- Licence of mlfinpy, purgedcv and temporalcv not checked.
- Qlib's rolling/online retraining (walk-forward-like) modules were not covered by the fetched page.

## 5. Paper-to-live decay: how much backtested performance shrinks

### Takeaway
Two large-sample studies anchor expectations: for published equity anomalies, returns fall ~26% out-of-sample (pre-publication) and ~58% after publication (McLean & Pontiff 2016); for 888 retail-quant Quantopian algorithms, backtest Sharpe explains almost nothing of out-of-sample Sharpe (R² < 0.025) and more backtesting widens the gap (Wiecki et al. 2016). Planning should assume a large haircut (often half or more) on any backtested Sharpe, more for marginal strategies (Harvey & Liu).

### Cited Findings
- McLean & Pontiff, "Does Academic Research Destroy Stock Return Predictability?", *Journal of Finance*, February 2016, 97 predictors: average long-short return ~26% lower out-of-sample (upper bound on statistical bias/data mining) and ~58% lower post-publication, implying a ~32% publication (arbitrage) effect — [paper PDF (gwern mirror)](https://Www.Gwern.net/doc/economics/2016-mclean.pdf); [CFA Digest summary](https://rpc.cfainstitute.org/research/cfa-digest/2016/06/does-academic-research-destroy-stock-return-predictability-digest-summary)
- Wiecki, Campbell, Lent & Stauth, "All that Glitters Is Not Gold: Comparing Backtest and Out-of-Sample Performance on a Large Cohort of Trading Algorithms", 9 March 2016 (SSRN 2745220), 888 algorithms each with ≥ 6 months OOS: backtest Sharpe is a weak predictor (R² < 0.025); higher-order risk measures (volatility, max drawdown) and portfolio-construction features (e.g. hedging) are more predictive; more backtesting → larger backtest/OOS discrepancy (evidence of overfitting); non-linear classifiers on backtest features reach R² = 0.17 on hold-out, and a portfolio built from ML predictions did significantly better OOS than one built from highest-backtest-Sharpe algorithms — [Quantopian forum archive](https://quantopian-archive.netlify.app/forum/threads/q-paper-all-that-glitters-is-not-gold-comparing-backtest-and-out-of-sample-performance-on-a-large-cohort-of-trading-algorithms); [Quantpedia summary](https://quantpedia.com/quantopians-academic-paper-about-in-vs-out-of-sample-performance-of-trading-alg/); [CXO Advisory summary](https://www.cxoadvisory.com/big-ideas/in-sample-vs-out-of-sample-performance-of-888-trading-strategies)
- Secondary summaries give R² of 0.01–0.02 for in-sample Sharpe; one says the in-sample period was 2010–2015 and OOS June 2015–Feb 2016, and that the 888 were filtered from 7,152 algorithms; one reports a top-10 ML portfolio net Sharpe 1.8 vs 0.7 for top-10 by backtest Sharpe — these numbers are secondary and not confirmed against the paper — [Quantpedia](https://quantpedia.com/quantopians-academic-paper-about-in-vs-out-of-sample-performance-of-trading-alg/); [CXO Advisory](https://www.cxoadvisory.com/big-ideas/in-sample-vs-out-of-sample-performance-of-888-trading-strategies)
- Conflicting secondary readings: one source says the most predictive measure was the Sharpe over the last in-sample year; another says non-return risk metrics are more predictable than return ratios — [CXO Advisory](https://www.cxoadvisory.com/big-ideas/in-sample-vs-out-of-sample-performance-of-888-trading-strategies); [Quantpedia](https://quantpedia.com/quantopians-academic-paper-about-in-vs-out-of-sample-performance-of-trading-alg/)
- Harvey & Liu: the multiple-testing haircut is nonlinear — high Sharpe ratios are moderately penalised, marginal ones heavily; a flat 50% haircut is inappropriate — [Duke PDF](https://people.duke.edu/~charvey/Research/Published_Papers/P120_Backtesting.PDF)
- LLM-agent strategies: none survived honest, trial-deflated evaluation in Gençay (2026) — [arXiv 2608.27734](https://arxiv.org/abs/2608.27734)

### Inferences
- Rule of thumb for the app's agents: report DSR/haircut Sharpe next to raw Sharpe, show the trial count, and expect live Sharpe roughly half (or less) of a backtest Sharpe that was selected among many trials; track realised vs. backtested Sharpe in paper trading to measure the app's own decay rate.
- Wiecki et al. suggest ranking candidate strategies on risk-based and construction features (volatility, drawdown, hedging) rather than on backtest Sharpe alone.

### Gaps
- Exact Wiecki et al. numbers (top-10 OOS Sharpe values, dates, 7,152 initial count) only from secondary summaries; the PDF copies were not parsed.
- Other decay studies I know of but did not verify this session: Suhonen, Lennkh & Perez (2017, JPM, alternative risk premia backtest vs live haircut), Falck, Rej & Thesmar (2021–2022, "Why and how systematic strategies decay"), Hou, Xue & Zhang (2020, "Replicating anomalies"), Chen & Zimmermann (open-source asset pricing replication). Treat as leads, not findings.
- No large-sample study of crypto strategy backtest-to-live decay was found.
