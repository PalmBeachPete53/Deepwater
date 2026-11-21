# Evaluation protocols and benchmarks for financial LLM agents, and look-ahead bias / temporal contamination (as of 2026-11-20)

Status legend used below: **[PR]** = peer-reviewed venue confirmed; **[WS]** = workshop; **[PP]** = preprint only (arXiv/SSRN/NBER WP), not peer-reviewed as far as found; **[PRESS]** = secondary press source (no paper).

## 1. How are financial LLM agents benchmarked (static vs point-in-time vs live/forward), and do agents beat buy-and-hold or simple baselines?

### Takeaway
Benchmarks have moved from static QA/backtest suites (FinBen, InvestorBench, 2024-25) to post-cutoff backtests (StockBench, FINSABER) and live/forward arenas (DeepFund, LiveTradeBench, Agent Market Arena, Alpha Arena). Across all the careful ones the result is the same: once look-ahead is removed and the test window is long or broad, LLM agents do not reliably beat buy-and-hold, an equal-weight portfolio or a plain trend rule. What they produce looks like beta (market exposure), not alpha. The early "LLMs beat buy-and-hold" results (e.g. FinBen) came from in-training-window backtests.

### Cited Findings

**Static / in-window benchmarks (2024-2025)**
- FinBen (Xie et al.): 42 datasets / 24 tasks in the NeurIPS version (v1 on arXiv had 35 / 23). It was the first to include a stock-trading task; GPT-4 was best at trading — [NeurIPS 2024 D&B poster](https://neurips.cc/virtual/2024/poster/97525), [arXiv 2402.12659](https://arxiv.org/abs/2402.12659v2). **[PR, NeurIPS 2024 Datasets & Benchmarks]**
- The FinBen authors' slides claim GPT-4 had a Sharpe ratio above 1 and that every LLM beat buy-and-hold; only models above 70B could produce trading actions. These are secondary figures and come from an in-training-window backtest — [NeurIPS slides](https://nips.cc/media/neurips-2024/Slides/97525.pdf). Code: github.com/The-FinAI/PIXIU.
- InvestorBench (Li et al.): three tasks (stock trading, crypto trading, ETF investing), 13 LLM backbones, an agent with brain/perception/profile/memory/action modules, data from Yahoo Finance and SEC EDGAR, standard quant metrics — [ACL 2025 long paper](https://aclanthology.org/2025.acl-long.126), [arXiv 2412.18174](https://arxiv.org/abs/2412.18174). **[PR, ACL 2025]**
- FinSearchComp (ByteDance): 635 expert-curated questions in three tiers (time-sensitive data fetching, simple historical lookup, complex historical investigation). Best model Grok 4 (web) reached about 68.9% on the Global subset vs 75.0% for human experts; models **without search score 0% on time-sensitive tasks**. Accuracy falls monotonically from T1 to T3 — [ICLR 2026 poster](https://iclr.cc/virtual/2026/poster/10011221), [arXiv 2509.13160](https://arxiv.org/abs/2509.13160v1), [project page](https://randomtutu.github.io/FinSearchComp). **[PR, ICLR 2026]** (the 68.9/75.0 figures come via an alphaXiv summary — [alphaXiv](https://www.alphaxiv.org/overview/2509.13160)). This is a search/retrieval benchmark, not a trading one.

**Long-horizon, bias-controlled backtests**
- FINSABER (Li, Kim, Cucuringu, Ma), "Can LLM-based Financial Investing Strategies Outperform the Market in Long Run?": covers 2004-2024 and 100+ symbols with unbiased symbol selection, controlling survivorship, look-ahead and data-snooping bias. Previously reported LLM advantages "deteriorate significantly" under the broader and longer test. LLM strategies are too conservative in bull markets (they underperform passive benchmarks) and too aggressive in bear markets (heavy losses). Code at github.com/waylonli/FINSABER — [arXiv 2505.07078](https://arxiv.org/html/2505.07078v2). Listed as accepted to **KDD 2026 [PR, per listing]**.
- StockBench (Chen, Yao, Liu et al., Tsinghua/BUPT): period 3 Mar to 30 Jun 2025 (82 trading days), top 20 DJIA stocks, $100k cash at start, daily buy/sell/hold from prices, fundamentals and news. **No transaction costs, slippage or liquidity modelled.** Baseline is equal-weight buy-and-hold. **3 seeds per agent, average reported** — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2), [site](https://stockbench.github.io). **[PP, v2 Mar 2026]**
  - Baseline: +0.4% return, max drawdown −15.2%, Sortino 0.0155. Best agents: Qwen3-235B-Thinking +2.5%, Qwen3-235B-Instruct +2.4%, GLM-4.5 +2.3%, Claude-4-Sonnet +2.2%, Kimi-K2 +1.9% (best Sortino 0.042). GPT-5 +0.3% (below baseline), GPT-OSS-20B −2.8% — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2).
  - The abstract says "most LLM agents fail to beat" buy-and-hold, but the v2 table has 8 of 13 above the baseline's return. Gaps are 1-2 points over 4 months with 3 seeds and no costs, so they are probably not significant. Appendix B shows return variance differs by about 100× across models (DeepSeek-V3 0.074e-4 vs GPT-OSS-120B 10.19e-4) — [arXiv 2510.02209](https://arxiv.org/html/2510.02209v1). The abstract and the table do not agree.
  - Contamination argument: the window falls after mainstream models' cutoffs. In the appendix, GPT-5 without search appeared to "predict" AAPL's 2021 trend, which is why they used a recent window — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2).
- rta-bench-1, "LLM Trading Agents Earn Beta, Not Alpha: A Look-Ahead-Free Benchmark" (J. Wang, arXiv 2611.19530, posted 14 Nov 2026, **single author, 6 days old, [PP]**) — [arXiv 2611.19530](https://arxiv.org/html/2611.19530):
  - Design: three US periods (2026 rally, 217 days; 2022 bear, 251 days; 2020 crash/rebound, 253 days). Universe of 56 large US stocks in 11 sectors plus index/sector/bond ETFs; idle cash earns T-bill. Decisions every 5th close, long-only, max 10 stocks, $100k. Headlines appear only after their publication minute; fills at the next open with costs. Each period is run both **blind (tickers, dates, prices masked) and named**.
  - Models: GPT-5.6 Luna, Gemini 3.5 Flash, Claude Haiku 4.5, Claude Sonnet 4.6, Claude Haiku 5.5, Llama 3.2 3B. 91 main runs.
  - Baselines: SPY, the equal-weight 56-stock portfolio, and five rules (trend following, RSI(2) reversion, 20-day breakout, 50-day pullback, buy on positive headlines).
  - Results: **no run had a significant four-factor alpha** (max |t| = 1.79). Mean alpha −2.3%/yr (95% CI −4.7% to +0.1%). Factor exposures explain 78% of the median run's return (median R² 0.66). 2026 agent returns ran from −0.6% to +8.1%, all **below SPY**. **Trend following made +19.0% in 2026 and +67.4% in 2020, more than any agent.** Over 9,101 trades, the 5-day excess return was −0.10%; sells were followed by +0.32% excess (t = 2.2), so the agents sell too early. The news score's rank correlation with the next 5-day excess return was 0.004.
- Profit Mirage (Li, Zeng, Xing, Xu; arXiv 2510.07920, Oct 2025, **[PP]**): FinMem, FinAgent, QuantAgent, FinCON and TradingAgents on GPT-4o (cutoff Oct 2023), backtested on Q2-Q3 2021 vs a "generalization" window Q3-Q4 2024 with similar market returns (+13.79% vs +13.35%). Past the cutoff, Sharpe decayed 51.48% (QuantAgent) to 62.23% (FinCON) and total return 50.18% (TradingAgents) to 71.85% (FinMem) — [arXiv 2510.07920](https://arxiv.org/html/2510.07920v1).

**Live / forward benchmarks and arenas**
- DeepFund, "Time Travel is Cheating: Going Live with DeepFund for Real-Time Fund Investment Benchmarking" (Li et al., HKUST-GZ): argues backtests let models use future information from pretraining, so it evaluates on live data after the cutoff. Nine flagship LLMs tested; DeepSeek-V3 and Claude-3.7-Sonnet still had net losses live — [arXiv 2505.11065](https://arxiv.org/abs/2505.11065), [NeurIPS 2025 D&B poster](https://neurips.cc/virtual/2025/poster/121642). **[PR, NeurIPS 2025 D&B]** An earlier IJCAI-workshop version gave each model $100k in five Berkshire holdings over 24 days; only Grok 3 mini Beta finished positive — [alphaXiv overview](https://www.alphaxiv.org/overview/2505.11065), [arXiv 2503.18313](https://arxiv.org/pdf/2503.18313v2). Code: github.com/HKUSTDial/DeepFund.
- LiveTradeBench (Yu, Li, You; arXiv 2511.03628, Nov 2025, **[PP]**): 50 live trading days (18 Aug to 24 Oct 2025), 21 LLMs, long-only allocation weights on a simplex. Two markets: 15 US stocks + cash, and 10 Polymarket binary markets. **No fees, spreads or slippage modelled; buy-and-hold/equal-weight baselines not reported; one run only, no significance tests** — [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1).
  - Stocks: GPT-4.1 best (+6.25%, Sharpe 2.64). Polymarket: Claude-Sonnet-3.7 +20.54%, while GPT-4.1 lost −33.69%. The cross-market Sharpe correlation is about 0. Spearman correlation with LMArena score is 0.054 on stocks and −0.38 on Polymarket. Reasoning models give no edge and are more volatile — [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1). Live leaderboard: trade-bench.live.
- Agent Market Arena (AMA), "When Agents Trade: Live Multi-Market Trading Benchmark for LLM Agents" (Qian et al., 17 authors, arXiv 2510.11695, **[PP]**): a "lifelong, real-time" benchmark on crypto and stocks, with verified data and expert-checked news, equal capital and the same execution rules for all. It compares InvestorAgent, TradeAgent, HedgeFundAgent and DeepFundAgent on GPT-4o, GPT-4.1, Claude-3.5-haiku, Claude-sonnet-4 and Gemini-2.0-flash. Main finding: **the agent framework drives behaviour more than the backbone LLM** — [arXiv 2510.11695](https://arxiv.org/abs/2510.11695).
- Alpha Arena Season 1 (nof1.ai): six models, $10k each, live crypto perpetuals, 18 Oct to 3 Nov 2025. Final: Qwen3 Max +22.31%, DeepSeek +4.89%, Claude Sonnet 4.5 −42.01%, Gemini 2.5 Pro −45.55%, Grok −57.92%, GPT-5 −58.74% — [Forklog](https://forklog.com/en/four-out-of-six-ai-models-suffer-losses-in-trading-tournament/). **[PRESS]** Venue reported as Hyperliquid — [Decrypt](https://decrypt.co/345006/ai-crypto-trading-showdown-deepseek-grok-winning-gemini-implodes), [GNcrypto](https://www.gncrypto.news/news/qwen-wins-alpha-arena-season-1-with-22-percent-returns/). The founder said the models had only numerical time series as context, which "LLMs do not handle well" — [Forklog](https://forklog.com/en/four-out-of-six-ai-models-suffer-losses-in-trading-tournament/). Overtrading was blamed for the losses — [iWeaver blog](https://iweaver.ai/blog/alpha-arena-ai-trading-season-1-results) (low-quality source).
- Alpha Arena Season 1.5: from 20 Nov 2025, US stock tokens on trade.xyz. 8 models × 4 prompt regimes (New Baseline, Monk Mode, Situational Awareness, Max Leverage) = 32 accounts of $10k. Winner "Mystery Model", later identified as Grok 4.20, about +12% on average (figures conflict: +12.11%, +22.38%, +34.59% in different sources). One analysis says only 6 of 32 accounts finished positive, the worst at −96.15% — [GNcrypto](https://www.gncrypto.news/news/mystery-model-alpha-arena-season-1-5-winner/), [Forklog](https://forklog.com/en/ai-model-grok-4-2-triumphs-in-trading-tournament/), [OneDayAdvisor](https://www.onedayadvisor.com/2025/12/nof1ai-alpha-arena-review-season-15.html). **[PRESS]** One account per model and regime over a few weeks.

### Inferences
- There are three types of benchmark: (1) **static/in-window** (FinBen, InvestorBench): cheap and reproducible, but contaminated for any period before the model's cutoff; (2) **post-cutoff or point-in-time backtests** (StockBench, FINSABER, rta-bench): reproducible, but they expire as new models are trained on the test window; (3) **live/forward** (DeepFund, LiveTradeBench, AMA, Alpha Arena): contamination-free by construction, but short (24-50 days), one run, often without costs or baselines, so statistically weak and not reproducible.
- The more careful the protocol (costs, next-open fills, publication-time news, long windows, factor regressions), the smaller the agent edge. Where the edge is measured properly (rta-bench, FINSABER) it is not distinguishable from zero, or it is negative vs trend following.
- The arenas mostly measure risk appetite and leverage, not forecasting skill. Alpha Arena's dispersion (−96% to +35% across prompt regimes of the same model) shows that **prompt and risk regime dominate the model choice**, which matches AMA's "framework > backbone" finding.

### Gaps
- AMA's dates, assets and numerical returns vs buy-and-hold were not on the abstract page; the full PDF was not read.
- The InvestorBench result tables (returns vs buy-and-hold per backbone) were not extracted.
- Alpha Arena has no paper or methodology document. The figures come from press, and the official nof1.ai leaderboard was not fetched; whether later seasons (2026) exist was not checked.
- rta-bench-1 is a 6-day-old single-author preprint. Its figures are unverified by others, and the appendix (last 37k characters) was not read.

## 2. Look-ahead bias / temporal contamination: what do LLMs "know" about the future, how big is it, and what mitigations work?

### Takeaway
Frontier LLMs memorise pre-cutoff financial data at near-perfect precision: S&P 500 levels, macro prints, headline dates, and the company and quarter behind an anonymised earnings call. Prompting ("answer as of date X") and simple masking do **not** stop recall. In-window backtests of agents lose roughly 50-70% of their Sharpe or return past the cutoff. Even a model's stated cutoff cannot be trusted: measured cutoffs differ from vendor dates by months. Fixes that work by construction are post-cutoff testing (with a measured, not stated, cutoff) and point-in-time models (ChronoGPT, DatedGPT, Kelly et al.), but the point-in-time models are small (≤4B). The good news from point-in-time models is that, for embedding-based return signals, look-ahead inflation appears modest.

### Cited Findings

**Evidence of memorisation / look-ahead**
- Sarkar & Vafa (Harvard), "Lookahead Bias in Pretrained Language Models": direct statistical tests that assume some events are unpredictable given a set information set, so predicting them well implies leakage. They find look-ahead bias when predicting risk factors from earnings calls and election winners from candidate biographies. The tests are one-sided (they can miss leakage). They recommend pretraining data free of survivorship bias and limited to text produced before the analysis period — [ICML 2025 DataWorld workshop](https://icml.cc/virtual/2025/48685), [NBER conference PDF](https://conference.nber.org/conf_papers/f205530/f205530.pdf). **[WS, first posted Mar 2024]**
- Lopez-Lira, Tang, Zhu, "The Memorization Problem: Can We Trust LLMs' Economic Forecasts?" (GPT-4o; arXiv v2 15 Dec 2025, SSRN 5217505, **[PP]**) — [arXiv 2504.14765v2](https://arxiv.org/html/2504.14765v2), [SSRN](https://papers.ssrn.com/abstract=5217505):
  - Macro (pre-cutoff 1990-09/2023 vs post 10/2023-02/2025): threshold accuracy pre/post is GDP 96.3%/40.0%, inflation 98.0%/47.1%, unemployment 99.3%/52.9%, 10-yr yield 98.5%/29.4%.
  - S&P 500 daily levels: MAPE 0.61% pre vs 16.87% post; directional accuracy 80.6% pre vs 45.7% post.
  - Headline dating: year correct 98.5% pre vs 28.8% post; exact date 47.0% vs 7.9%.
  - **Instructions don't work**: with a fake cutoff of Dec 2010 set only in the system prompt, post-"cutoff" GDP accuracy was 98.0% (MAPE 0.05%) vs about 40% true post-cutoff accuracy.
  - **Masking doesn't work**: GPT-4o identified the company in 100% of anonymised Apple/Meta/Microsoft earnings calls and >85% for every Magnificent-7 firm, and got the quarter and year for Apple 93.2% of the time.
  - Recommendation: **don't treat small post-cutoff samples as robustness checks — they lack power to separate skill from memorisation.** Their proposed mitigation is to convert identifiable text into "anonymized economic logic".
- Glasserman & Lin (Columbia), "Assessing Look-Ahead Bias in Stock Return Predictions Generated By GPT Sentiment Analysis" (Sep 2023, GPT-3.5, **[PP]**): they compare headlines with company names vs fictional placeholders. **In-sample, the anonymised headlines did better**: a "distraction effect" (general company knowledge) outweighed look-ahead, most strongly for large caps. Out-of-sample, look-ahead is gone, but anonymisation still helped for large firms (borderline significant) — [arXiv 2309.17322](https://arxiv.org/abs/2309.17322).
- Gao, Jiang, Yan, "A Test of Lookahead Bias in LLM Forecasts" (arXiv 2512.23847, Dec 2025, **[PP]**): a formal test, likely run with Llama-3.3-70B. It reports memorisation "substantially amplifies the apparent predictive power" of LLM forecasts — [arXiv 2512.23847](https://arxiv.org/abs/2512.23847). (The body was not readable; no numbers extracted.)
- Profit Mirage / FinLake-Bench: 2,000 QA pairs (Jan 2022-Jun 2023). GPT-4o/Claude-3.7/Grok-3 accuracy: price questions 85.4%, trend 90.2%, event impact 92.9%, market performance 89.8%. Counterfactual tests: FinMem's decisions barely change when inputs are perturbed (prediction consistency 0.82, input dependency 0.28). LoRA fine-tuning on FNSPID raises in-distribution accuracy (Qwen 51.6→72.2%) but cuts unseen-data accuracy by 18-22% — [arXiv 2510.07920](https://arxiv.org/html/2510.07920v1). **[PP]**
- "LLMs Don't Know What They Know: Benchmarking Temporal Self-Knowledge for Financial Backtesting" (Qian, …, Lopez-Lira, Tang, …, arXiv 2611.06808, 4 Nov 2026, **[PP]**) — [arXiv 2611.06808](https://arxiv.org/html/2611.06808):
  - Data: 2,226 dated US facts (12 FRED series, month-end prices of 50 large firms, Fed decisions) from 1999 to 2026.
  - GPT-5 answers 76% correctly but claims to know only 38%, and gets right 61% of the facts it says it doesn't know. In a simulated "as-of" backtest, 99% of its in-window answers were labelled "predictions", 82% of them correct.
  - A historical-date instruction cuts GPT-5's knowledge claims from 0.39 to 0.01 while accuracy stays at 0.76→0.75. **The instruction hides the leak; it does not remove it.**
  - Llama-3.3-70B's real knowledge stops at 2021Q2 but its claims continue to 2023Q1 (vendor cutoff Dec 2023).
  - Point-in-time models (TiMaGPT, ChronoGPT-Instruct, DatedGPT) are near chance on both sides of their cutoffs, so "they cannot serve as controls yet".
  - Conclusion: in-window backtests "cannot be certified leak-free by self-report, instruction or stated cutoff".
- HindsightBench (H. Jia, UCD; arXiv 2607.18867, Jul 2026, **[PP]**): a black-box audit on a 258-month, vintage-correct macro panel with four arms (revealed, date-only, masked, wrong-date transplant shifted 66 months), across 15 models — [arXiv 2607.18867](https://arxiv.org/html/2607.18867v1):
  - **Date tokens alone trigger hindsight.** Significant effects in 6/6 models of the 2026 generation, 4/5 of 2025, 0/4 of the 2024 open-weight Llamas. The trigger follows training generation, not model size.
  - Claude Haiku 4.5 transplant effect +49.5pp despite only 20% date recovery. Crisis dates (2008-09, 2020-03, 2022) cause bearish calls even on mismatched data. 10-year Treasury trigger up to +63.2pp.
  - **Measured cutoffs span 2023-12 to 2025-10 and can precede vendor dates by up to 8 months.**
  - The serving configuration matters (BF16 vs AWQ-INT4 changed the estimates).
  - Cost: about $19-115 per model.
- Temporal Leakage in LLM Backtesting (Zhang & Stadie, arXiv 2608.02985, v2 10 Nov 2026; listed as **published in TMLR 11/2026 [PR]**) — [arXiv abs](https://arxiv.org/abs/2608.02985v2):
  - **The naive check "score before vs after cutoff" is uninformative.** Four flagship models "fail" it on questions that all resolved after their cutoffs, because models know more about periods near their cutoff, so recency mimics leakage.
  - They prove **no passive backtest can separate leakage from skill without outside information**. Two tools: a known cutoff (flags leakage at the boundary), and a **matched clean control** that gives a leakage-adjusted score.
  - Leakage concentrates on outcomes that surprised the crowd and were well covered in training.
  - On frontier models: one cutoff-localised leakage signature, and no significant leakage in five models with positive naive gaps.
  - Code: github.com/ZeyuZhang1901/Temporal-Leakage-Backtesting.
- rta-bench-1 memory probe: GPT-5.6 Luna's rank correlation with 2022 stock moves was 0.69 (inside its training window) vs 0.25 after its cutoff. Showing real names did not improve its forecasts — [arXiv 2611.19530](https://arxiv.org/html/2611.19530). **[PP]**
- Look-Ahead-Bench (M. Benhenda, arXiv 2601.13770, Jan 2026, **[PP]**): measures look-ahead as **alpha decay between temporally distinct regimes** inside an "AI Hedge Fund" multi-agent workflow. Llama 3.1 8B/70B and DeepSeek 3.2 show significant decay; the vendor's point-in-time "Pitinf" models do not. Larger standard models generalise worse, which the author attributes to stronger memorised priors — [arXiv 2601.13770](https://arxiv.org/abs/2601.13770v1), [GitHub](https://github.com/benstaf/lookaheadbench). Caveat: the author is tied to PiT-Inference, a commercial vendor, and there is no independent replication.

**Point-in-time ("chronologically consistent") models**
- ChronoBERT / ChronoGPT (He, Lv, Manela, Wu; WashU): one model per year from 1999 to 2024, trained only on text available at the time. All ChronoGPT vintages beat a same-size GPT on HellaSwag. In an asset-pricing / next-day news return test, their real-time outputs reach Sharpe ratios **comparable to a much larger Llama**, so look-ahead inflation is "modest" — [arXiv 2502.21206v3](https://arxiv.org/html/2502.21206v3) (v3 Jul 2025; AEA 2026 program — [AEA](https://www.aeaweb.org/conference/2026/program/paper/BrRDGQHe)). **[PP / conference presentation]**
- ChronoGPT-Instruct ("Chronologically Consistent Generative AI", Oct 2025): instruction-following vintages that consistently fail to predict future presidents or major events, i.e. no leakage — [arXiv 2510.11677](https://arxiv.org/abs/2510.11677v1). **[PP]**
- DatedGPT (Yan, Tang, Gao, Jiang, Lu; Mar 2026): twelve 1.3B models trained from scratch on about 100B tokens each, annual cutoffs 2013-2024, instruction-tuned on date-filtered data. Weights promised on acceptance — [arXiv 2603.11838](https://arxiv.org/abs/2603.11838). **[PP]**
- Kelly, Malamud, Schwab, Xu (AQR-affiliated), "Scaling Point-in-Time Language Models" (NBER WP 35247, May 2026): models up to 4B parameters, 1T chronologically filtered FineWeb tokens, **monthly checkpoints 2013-2024**. They approach Gemma-3-4B / LLaMA-7B on common-sense benchmarks, with gaps on some tasks. Strict out-of-sample portfolios built from their embeddings earn robust positive Sharpe ratios, close to full-sample (leaky) versions — [NBER w35247](https://www.nber.org/papers/w35247). **[PP, NBER WP]**
- StoriesLM (Sarkar, 2024) and TimeMachineGPT (Drinkall et al., 2024) are earlier time-locked models, used as baselines (ChronoBERT beats StoriesLM on GLUE) — [arXiv 2502.21206v3](https://arxiv.org/html/2502.21206v3), [arXiv 2601.13770](https://arxiv.org/pdf/2601.13770).

### Inferences
- Measured size of the effect:
  - Recall of pre-cutoff facts is near-perfect for frontier models: macro values 96-99%, S&P direction 81%, company identity 85-100%.
  - Agent-level backtests lose about 50-70% of their Sharpe or return past the cutoff (Profit Mirage).
  - For the narrower use of LLMs as **text embedders feeding a factor model**, point-in-time comparisons (ChronoGPT, Kelly et al.) suggest the inflation is modest.
  - So the risk is largest exactly in the setting that matters here: a **generative agent reasoning about named assets and dates**, not frozen embeddings.
- Mitigations ranked by strength:
  1. Forward/live testing (leak-free by construction).
  2. A backtest window strictly after the model's **measured** knowledge boundary (not the vendor cutoff), with a margin of months (HindsightBench: up to 8-month discrepancies).
  3. A point-in-time model as matched clean control (Zhang & Stadie). Currently weak: point-in-time models are near chance on factual probes (arXiv 2611.06808).
  4. Blind runs (mask ticker, date, absolute price level; rescale prices) **compared against named runs**. Masking alone is insufficient (Lopez-Lira: entity reconstruction), but blind-vs-named divergence is a useful leakage detector.
  5. Prompt instructions ("you are on date X"): ineffective. They suppress the model's admission of knowledge, not the knowledge itself.
- Date strings are themselves a leakage channel (HindsightBench), so masking must cover dates and regime cues (e.g. "COVID", "SVB"), not just tickers.

### Gaps
- No numbers extracted from Gao-Jiang-Yan (PDF unreadable) or from the adjusted scores of Zhang & Stadie (abstract gives no magnitudes).
- Look-Ahead-Bench's exact alpha-decay figures were not verified (the ">-15pp" figure came only from a search snippet).
- No source found on how fast agents with **web search / tools** leak future information (e.g. a search engine returning post-date pages). FinSearchComp shows search is essential for fresh data but doesn't address as-of filtering.
- Whether vendor "knowledge cutoff" figures for 2026 models (Claude 5.x, GPT-5.x) match their measured boundaries, beyond the HindsightBench and 2611.06808 samples.

## 3. Point-in-time data discipline for agents: as-of access, revision vintages, survivorship

### Takeaway
Leakage comes not only from model weights but from the data pipeline: calendar-date joins of news, revised macro data, today's index membership and same-bar fills all inflate results. The documented fixes are: news replayed by publication timestamp, fills at the next open, vintage (as-first-published) macro data, survivorship-free universes, and mechanical tests that the agent's inputs before time t do not change when the future is randomised.

### Cited Findings
- **News timing**: in rta-bench, a headline rule earned **+32.0 bp per trade when news was joined by calendar date vs −6.7 bp when replayed by publication time**. The whole apparent edge was a timing leak — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- **Mechanical look-ahead check**: replay each run's recorded agent outputs on markets with a random future after each cut day, and fail the run if any observation before the cut changes. Fixed cut days (30/55/80% of the period) **missed all one-day leaks**. Cuts on the agent's own decision days caught all 48 planted leaks in 12 runs and passed all 24 as-shipped replays — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- Formalisation: "Look-Ahead-Freedom as Temporal Non-Interference" treats leak-freeness as a verifiable correctness property of backtesting and agentic pipelines — [arXiv 2607.04958](https://arxiv.org/html/2607.04958v1). **[PP; only title/abstract seen]**
- **Revision vintages**: ALFRED (St. Louis Fed) archives every past vintage of FRED series, so a series can be viewed as it stood on a chosen date. Monthly employment revisions and quarterly GDP / annual CPI revisions are routine. ALFRED covers only FRED series, since 2006 — [St. Louis Fed: Data revisions with FRED](https://www.stlouisfed.org/publications/page-one-economics/2022/08/01/data-revisions-with-fred), [FRED blog: ALFRED at 15](https://fredblog.stlouisfed.org/2021/04/alfred-at-15-archiving-fred-data-since-2006/), [ALFRED API docs](https://fred.stlouisfed.org/docs/api/fred/alfred.html).
- HindsightBench builds its audit on a "vintage-correct" panel (the information set at each date contains only data published by then) and lists this as step 1 of its protocol — [arXiv 2607.18867](https://arxiv.org/html/2607.18867v1).
- **Survivorship**: FINSABER widens symbol coverage via "unbiased selection" to reduce survivorship and data-snooping bias, and shows prior LLM advantages shrink when the universe is broadened — [arXiv 2505.07078](https://arxiv.org/html/2505.07078v2). Sarkar & Vafa extend survivorship concerns to the **pretraining corpus itself** (text about firms that survived is overrepresented) — [ICML 2025 workshop](https://icml.cc/virtual/2025/48685).
- Fixed-universe benchmarks embed survivorship: StockBench uses today's top-20 DJIA by weight — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2); LiveTradeBench uses 15 mega-caps — [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1).
- **Execution realism**: StockBench and LiveTradeBench model no costs or slippage and say so, noting returns may be overstated — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2), [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1). rta-bench fills at the next open with costs — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- Even after statistical corrections, backtest engines can give materially different Sharpe ratios for the same strategy ("implementation risk") — [arXiv 2603.20319](https://arxiv.org/pdf/2603.20319). **[PP; abstract only]**

### Inferences
- For an agent with tools, as-of discipline must be enforced **inside every tool** (price, news, macro, fundamentals, search), not by prompt. Each tool call takes an `as_of` timestamp from the simulation clock, never wall-clock. Macro series come from first-release vintages; fundamentals by filing/acceptance date, not period end; universe membership as of the date.
- A cheap, strong unit test is rta-bench's "random future after the decision day" replay: run the same episode on two markets that share history up to t and differ after. Every tool response at or before t must be byte-identical.

### Gaps
- No source found quantifying how much macro revisions (first release vs current vintage) inflate LLM-agent backtests specifically.
- No primary source checked on point-in-time fundamentals (filing-date vs period-end alignment) for LLM agents. This is standard quant practice, but not sourced here.

## 4. Statistical evaluation of agents: runs/seeds, variance, sample size for significance, baselines, counting trials/prompt variants

### Takeaway
LLM-agent results vary a lot between seeds: on the order of the gaps between models. Most benchmarks run 1-3 seeds over 24-82 days, far too little for significance. Defensible practice: multiple seeds, rank only on gaps larger than seed spread, factor-regression alpha with HAC standard errors, per-decision tests against random same-day choices, strong simple baselines (SPY, equal-weight, trend rule), and corrections for the number of prompt/config variants tried (deflated Sharpe).

### Cited Findings
- **Seed variance**: in rta-bench, two seeds of the same model differed by **5.2 points** of return on average, vs 8.7 points between the best and worst models in 2026. Seed noise is about 60% of the between-model spread — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- **Power**: one run of about one year of daily returns has an **alpha standard error of about 7.2%/yr**, so only alphas of roughly 14%/yr or more would be significant from a single run (implied by the rta-bench numbers) — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- **rta-bench statistical toolkit**:
  - Newey-West (4 lags) factor regressions.
  - Decision-level t-stats clustered by day, with overlapping windows weighted.
  - Bootstrap intervals that **resample model × period combinations, because seeds and reruns are not independent**.
  - Null distributions from 1,000 same-sector stock swaps per decision and 200 holding swaps per run. Mean decision percentile was 54 vs 50 for no skill.
  - Exact sign tests for prompt-variant pairs. A 30% cash rule cut stock exposure 30.3 pts and max drawdown 9.4 pts in all six pairs, but **no prompt rule improved stock selection**.
  - Source: [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- Run counts in benchmarks:
  - StockBench: 3 seeds, averaged, 82 days — [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2).
  - LiveTradeBench: one 50-day live run, no significance tests — [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1).
  - DeepFund workshop version: 24 days — [alphaXiv](https://www.alphaxiv.org/overview/2505.11065).
  - Alpha Arena S1: one account per model, about 16 days — [Forklog](https://forklog.com/en/four-out-of-six-ai-models-suffer-losses-in-trading-tournament/).
- Lopez-Lira et al.: small post-cutoff samples lack the power to separate skill from memorisation — [arXiv 2504.14765v2](https://arxiv.org/html/2504.14765v2).
- LiveTradeBench: the cross-market Sharpe correlation is about 0 and the LMArena correlation is about 0 or negative, so a ranking from one market or one period does not carry over — [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1).
- **Multiple testing**: the Deflated Sharpe Ratio (Bailey & López de Prado, JPM 2014) corrects a reported Sharpe for selection bias across many trials and for non-normal returns. The expected maximum Sharpe over N trials exceeds a random trial's expected Sharpe. The same group proposed the probability of backtest overfitting and the minimum backtest length — [SSRN 2460551](https://papers.ssrn.com/abstract=2460551). **[PR, Journal of Portfolio Management]**
- HindsightBench protocol details: pin the serving configuration (quantization, thinking mode, tools off, sampling, parser version); report metrics that fail identifiability gates as undefined; keep superseded rows instead of overwriting; disclose costs — [arXiv 2607.18867](https://arxiv.org/html/2607.18867v1).
- Model updates: re-measure the knowledge boundary whenever a model is updated — [arXiv 2611.06808](https://arxiv.org/html/2611.06808).

### Inferences
- Rough sample-size arithmetic (my own calculation, using the standard approximation SE(annual Sharpe) ≈ √(1/years) for small Sharpe ratios; no source fetched):
  - Telling a 0.5 Sharpe-difference from zero at 2σ needs about 16 years of independent daily data. A 1.0 difference needs about 4 years.
  - So 50-82-day live or post-cutoff windows can only detect extreme effects. Significance has to come from **many decisions across assets** (cross-sectional, decision-level tests against random same-day alternatives) rather than from portfolio Sharpe over time.
  - This matches rta-bench's choice to use 9,101 decisions.
- Every prompt variant, model, tool set and hyper-parameter tried is a "trial" for the deflated Sharpe. Log them all from day one.

### Gaps
- No primary source found giving a formal minimum number of runs or seeds for LLM trading agents; rta-bench gives only the seed spread.
- No source found on temperature-0 determinism across providers for trading agents (API non-determinism even at T=0 is widely reported, but not sourced here).

## 5. Concrete, defensible protocol for testing a trading agent on past data without look-ahead

### Takeaway
No single trick makes a backtest of a pretrained LLM agent leak-free. The defensible approach combines (a) a test window after the model's **measured** knowledge boundary, (b) mechanically enforced as-of data, (c) leakage diagnostics (blind vs named, wrong-date transplant, planted leaks, memory probes), (d) strong baselines and factor-adjusted alpha, (e) multiple seeds with trial counting, and (f) a forward (paper) test as the final arbiter. Each step below is sourced in sections 1-4.

### Cited Findings (basis for each step)
- Measure the knowledge boundary behaviourally; don't trust the vendor cutoff, self-report or "as-of" instructions — [arXiv 2611.06808](https://arxiv.org/html/2611.06808), [arXiv 2607.18867](https://arxiv.org/html/2607.18867v1).
- The before/after-cutoff comparison is uninformative on its own; use a known cutoff plus a matched clean control — [arXiv 2608.02985](https://arxiv.org/abs/2608.02985v2).
- Replay news by publication time, fill at the next open, cut look-ahead checks on decision days, plant leaks to test the checker, run blind and named, compare with SPY / equal-weight / trend, use multiple seeds — [arXiv 2611.19530](https://arxiv.org/html/2611.19530).
- Use a long, broad, survivorship-controlled universe and cover bull and bear regimes — [arXiv 2505.07078](https://arxiv.org/html/2505.07078v2).
- Use live/forward evaluation after the cutoff — [arXiv 2505.11065](https://arxiv.org/abs/2505.11065), [arXiv 2511.03628](https://arxiv.org/html/2511.03628v1).
- Correct for the number of trials (deflated Sharpe) — [SSRN 2460551](https://papers.ssrn.com/abstract=2460551).
- Use vintage macro data (ALFRED) — [St. Louis Fed](https://www.stlouisfed.org/publications/page-one-economics/2022/08/01/data-revisions-with-fred).

### Inferences (the protocol — my synthesis)

**Step 0 — Pre-registration (before any run).** Write down and freeze:
- universe rule
- test windows
- baselines
- metrics
- number of seeds
- the model list with exact version strings and serving configuration (temperature, reasoning mode, quantization, tools)

Create a trial log: every prompt, tool or config variant ever run counts toward N for the deflated Sharpe.

**Step 1 — Measure each model's knowledge boundary.** Probe dated, domain-relevant facts by month and find where accuracy falls to chance: month-end prices of the universe, macro prints, Fed decisions, headline dating. This is the "effective cutoff" from arXiv 2611.06808 / HindsightBench. Set test-window start = max(vendor cutoff, measured boundary) + a margin of at least 3-6 months (HindsightBench saw up to 8 months of discrepancy). Re-measure whenever the model is updated, even under the same name.

**Step 2 — Split the evaluation into three tiers and report them separately.**
- Tier A, *post-boundary backtest*: the main result. Only data after the measured boundary.
- Tier B, *in-window diagnostic backtest*: earlier periods (e.g. 2020 crash, 2022 bear), used **only** to measure leakage, never to claim skill. Compare to Tier A; a large drop (cf. Profit Mirage's 50-70%) is a leakage signature.
- Tier C, *forward paper test*: same frozen agent, live paper trading, timestamps logged before the outcome is known. This is the only tier fully immune to both weight leakage and pipeline leakage.

**Step 3 — Enforce as-of data mechanically.**
- Every tool takes the simulation clock, never wall-clock.
- Prices: bars closed at or before t only. Orders fill at the next bar's open (or a VWAP proxy) with fees and spread.
- News: by publication timestamp, never calendar-date joins (cf. the +32 bp → −6.7 bp headline result).
- Macro: first-release vintages (ALFRED or equivalent).
- Fundamentals: by filing/acceptance time.
- Universe: membership as of t, delisted names included.
- Web search tools: disabled, or restricted to a dated snapshot.

**Step 4 — Leakage unit tests (CI-style, must pass before results count).**
- (a) *Random-future replay*: for each decision day t, build a synthetic market identical up to t with a random future after it. Every tool response and every agent input at or before t must be identical (rta-bench). Cut on actual decision days, not fixed fractions.
- (b) *Planted leaks*: inject a known one-day leak and check that test (a) catches it.
- (c) *Blind vs named runs*: mask tickers (sector-level aliases), shift or remove dates, rescale prices to an index of 100. A named run materially beating the blind run in Tier B but not Tier A suggests memorisation. (Glasserman-Lin caveat: names can also *hurt* via distraction.)
- (d) *Wrong-date transplant*: real data with a date shifted by years. A changed decision means date tokens are driving behaviour (HindsightBench).
- (e) *Direct memory probe*: ask for the next-month return / price of universe names at in-window dates and report its rank correlation with realised moves (cf. 0.69 in-window vs 0.25 post-cutoff).

**Step 5 — Baselines (all with the same costs and fills).**
- Cash/T-bill
- SPY or the benchmark index
- Equal-weight buy-and-hold of the universe
- One trend rule and one mean-reversion rule
- "Random agent": random same-sector picks on the same days with the same turnover
- If an ML model exists in the app: the same features without the LLM

Report the agent **minus** each baseline, not raw returns.

**Step 6 — Statistics.**
- Seeds: at least 5 per configuration. Rank two agents only if their gap exceeds the seed spread.
- Portfolio level: factor regression (market, or a four-factor model for equities) with Newey-West SEs. Report alpha, beta, R², average stock exposure, max drawdown, turnover, and costs paid.
- Decision level: percentile of each chosen asset's forward return among random same-day alternatives, with standard errors clustered by day.
- Multiple testing: deflated Sharpe using the logged N; bootstrap that resamples model × period blocks, not individual seeds.
- Sample size: aim for thousands of decisions rather than more calendar days; treat any window under about one year as anecdotal at portfolio level.

**Step 7 — Freeze, then forward-test.** The agent version that passes Steps 1-6 is frozen (prompt hash, model version, tool versions) and paper-traded forward for a pre-registered duration. Results are appended, never overwritten (HindsightBench practice). The final claim of skill rests on Tier A plus Tier C. Tier B serves only as the leakage measurement.

**Expected outcome given the literature:** on liquid large caps, expect agents to match their beta and lose to a trend rule after costs. A positive result that survives Steps 4-6 would be unusual and worth scrutiny before trust.

### Gaps
- The protocol is my synthesis. No single source prescribes all seven steps, though rta-bench §VIII comes closest. Thresholds (≥5 seeds, a 3-6-month margin, ≥1 year) are judgement calls, not sourced constants.
- No validated off-the-shelf point-in-time instruction model strong enough to serve as a clean control was found (arXiv 2611.06808 says current ones are near chance). Kelly et al.'s monthly checkpoints (≤4B) are the closest candidate, for embeddings only.
