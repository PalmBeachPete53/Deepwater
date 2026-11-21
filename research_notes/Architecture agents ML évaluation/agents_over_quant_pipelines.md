# LLM agents over quant / ML trading pipelines, and LLM trading-agent frameworks (state at 2026-11-20)

Scope note: research done 2026-11-20, about 25 tool calls. Numbers are the authors' own claims unless marked "independent". Dates are arXiv first-submission dates unless stated. Older items (2023-2024) are marked as such.

## 1. Microsoft RD-Agent and R&D-Agent(Q) with Qlib

### Takeaway
R&D-Agent(Q) (arXiv 2505.15155, May 2025, NeurIPS 2025 Datasets & Benchmarks track) is the clearest case of an "agent proposes, quant pipeline evaluates" design. LLM agents write factor and model *code*. Qlib backtests that code on daily A-share data. A bandit picks whether the next loop works on factors or on models. The authors report clear gains over Alpha158/Alpha360 and deep time-series baselines on CSI 300, but there is no multiple-testing correction and no independent replication. The code is open source (MIT) and Linux/Docker only.

### Cited Findings
- **Venue and status:** Li et al. (Microsoft Research Asia + CMU). Submitted May 2025, revised Sept 2025, accepted at NeurIPS 2025 (Datasets and Benchmarks track) — [arXiv 2505.15155](https://www.arxiv.org/abs/2505.15155); [NeurIPS proceedings](https://papers.nips.cc/paper_files/paper/2025/hash/ac5c2b6e423883cbcacbcccf88491b78-Abstract-Datasets_and_Benchmarks_Track.html)
- **Research stage:**
  - A *Specification Unit* sets goal-aligned context and constraints.
  - A *Synthesis Unit* keeps a "knowledge forest" of past experiments and generates factor or model hypotheses.
  - Source: [arXiv 2505.15155v2 HTML](https://arxiv.org/html/2505.15155v2)
- **Development stage:**
  - *Co-STEER* is a code-generation agent with a DAG task scheduler and a growing knowledge base. It is capped at 10 inner debug iterations per task.
  - A *Validation Unit* runs Qlib backtests. New factors with IC ≥ 0.99 against the current SOTA library are dropped as redundant.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Feedback and scheduling:** an *Analysis Unit* reviews each round and feeds back into Synthesis. A two-arm contextual Thompson-sampling bandit (factor vs model, 8-dimensional state) picks the next direction — [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Data:** daily data only, with one-day-ahead return labels.
  - Main study: CSI 300, train 2008–2014, validation 2015–2016, test Jan 2017 to Aug 2020.
  - Extension: CSI 500 and NASDAQ 100, train 2008–2021, validation 2022–2023, test 2024 to June 2025.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Backtest:** daily long-short strategy ranked on predicted returns, with holding-retention rules and transaction costs. The top-k and cost values sit in Appendix C.4, which I did not read — [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Baselines:**
  - Factor libraries: Alpha101, Alpha158, Alpha360, AutoAlpha.
  - Models: Linear, MLP, LightGBM, XGBoost, CatBoost, DoubleEnsemble, GRU, LSTM, ALSTM, Transformer, PatchTST, iTransformer, Mamba, TRA, MASTER, GATs.
  - Baselines were tuned and run with 5 seeds. Median ARR is reported, with no error bars.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Search budget:**
  - 6 h per module and 12 h for the joint run.
  - In the o3-mini ablation: 44 loops, of which 24 were valid and 8 were selected as SOTA. Random scheduling gave 33 loops.
  - LLM backbones tested: GPT-4o, o3-mini, o3, GPT-4.1, o1, GPT-4o-mini, o4-mini.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Results on the CSI 300 test period (authors' claims):**

  | System | IC | ARR | IR | MDD |
  |---|---|---|---|---|
  | R&D-Agent(Q), o3-mini | 0.0532 | 14.21% | 1.74 | −7.42% |
  | R&D-Agent(Q), GPT-4o | 0.0497 | 11.44% | 1.32 | — |
  | Alpha158 | 0.0341 | 5.70% | 0.85 | — |
  | Alpha360 | 0.0420 | 4.38% | 0.67 | — |
  | TRA | — | 6.49% | 1.01 | — |
  | MASTER | — | 8.96% | 1.34 | — |

  Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Results on the newer period, 2024 to June 2025 (o4-mini):**
  - CSI 500: IC 0.0288, IR 2.17, MDD −6.56%.
  - NASDAQ 100: IC 0.0162, IR 1.77, MDD −6.34%.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Headline claim:** about 2× higher ARR than factor libraries, with over 70% fewer factors, for under $10 per run — [microsoft/RD-Agent README](https://github.com/microsoft/RD-Agent)
- **Leakage controls:**
  - The LLM sees only schema-level information, never raw market data or the temporal splits.
  - The newer test period is set to fall at or after the LLMs' training cutoffs.
  - The paper does not discuss multiple-testing correction.
  - Acknowledged limitation: the system relies only on the LLM's internal financial knowledge.
  - Source: [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **Repository state (fetched 2026-11-20):**
  - MIT license, about 15.4k stars, Linux only, Docker required.
  - Scenarios: fin_factor, fin_model, fin_quant (joint factor + model), fin_factor_report (factors read from financial reports), general_model (model from a paper), data_science/Kaggle, llm_finetune (FT-Agent).
  - LLM access through LiteLLM (OpenAI, Azure, DeepSeek, and others). A Web UI is available via `rdagent server_ui`.
  - Recent news items: FT-Dojo (ICML 2026), "Reasoning as Gradient" (ACL 2026 Findings).
  - No release tags were visible.
  - Source: [github.com/microsoft/RD-Agent](https://github.com/microsoft/RD-Agent)
- **Survey description:** a Sept 2026 survey describes RD-Agent(Q) as alternating research and development, jointly optimising factors and models with backtest feedback and a bandit scheduler — [Hua et al., arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)

### Inferences
- Even the newer period (2024–2025) evaluates a search procedure that was designed and tuned by people who had already seen earlier periods.
- Selecting "SOTA" among roughly 44 loops by validation results is a multiple-testing setting. The reported IR is the best of many candidates and is not deflated.
- The design that matters for an "agent above the ML pipeline" product:
  - The LLM never touches prices.
  - It only writes code.
  - A deterministic backtester is the judge.
  - Redundancy is filtered (IC ≥ 0.99 against existing factors).
  - The search budget is explicit (hours and loop counts).

### Gaps
- I did not read the exact transaction-cost and top-k settings (Appendix C.4) or the cost breakdown (Appendix D.5).
- I found no independent replication of RD-Agent(Q) results.
- I found no release version or date for the repo.

## 2. Inventory of LLM trading-agent frameworks (TradingAgents, FinMem, FinAgent, FinCon, FinRobot, QuantAgent, Alpha-GPT, AlphaAgent, StockAgent, and 2025–2026 systems)

### Takeaway
There are two families:
- **(a) LLM as trader / analyst team.** TradingAgents, FinMem, FinAgent, FinCon and QuantAgent. These read daily prices, news, filings and sometimes charts, and output buy/hold/sell decisions. Their evaluations are typically 3–8 months on 3–8 famous tickers.
- **(b) LLM as alpha miner or researcher over a backtester.** Alpha-GPT, AlphaAgent and RD-Agent(Q). These generate formulaic factors or code and are scored on IC/IR in Qlib-style backtests over multiple years.

From 2025 to 2026 the field moved toward three things:
- live or contamination-free benchmarks (StockBench, LiveTradeBench, Agent Market Arena, Alpha Arena, DeepFund);
- hybrid designs where the LLM produces scores and a deterministic engine sizes positions;
- large production records, which show no directional edge.

### Cited Findings

**Family (a): LLM as trader / analyst team**

- **TradingAgents** (Xiao, Sun, Luo, Wang; Tauric Research; v1 28 Dec 2024, v7 3 Jun 2025).
  - Sources: [arXiv 2412.20138](https://arxiv.org/abs/2412.20138); [code](https://github.com/TauricResearch/TradingAgents); details from [v7 HTML](https://arxiv.org/html/2412.20138v7).
  - Roles:
    - fundamentals analyst (statements, earnings, insider transactions);
    - sentiment analyst (Reddit/X, sentiment models);
    - news analyst (Bloomberg, Yahoo, EODHD, FinnHub, macro);
    - technical analyst (OHLCV and 60 indicators);
    - Bull vs Bear researchers who debate;
    - a trader;
    - a risk-management team with several risk profiles;
    - a fund manager who approves.
  - LLMs: gpt-4o-mini/gpt-4o for "quick thinking" and o1-preview for "deep thinking". About 11 LLM calls and 20+ tool calls per decision.
  - Test: 1 Jan to 29 Mar 2024 (about 3 months). Table 1 covers AAPL, GOOGL, AMZN, while the text names AAPL, NVDA, MSFT, META, GOOGL (inconsistent).
  - Baselines: Buy & Hold, MACD, KDJ+RSI, ZMR, SMA.
  - Not reported: transaction costs, slippage, number of runs/seeds, significance tests.
  - Results:

    | Ticker | Cumulative return | Sharpe | Max drawdown |
    |---|---|---|---|
    | AAPL | 26.62% (Buy & Hold −5.23%) | 8.21 | 0.91% |
    | GOOGL | 24.36% | 6.39 | — |
    | AMZN | 23.21% (Buy & Hold 17.10%) | 5.60 | — |

  - The authors themselves say the Sharpe "exceeds expected empirical range".
- **FinMem** (Yu et al., 2023; older).
  - Single agent with layered memory (working, short, mid and long term) and a configurable risk "character".
  - Original evaluation per FINSABER: 6 months, 5 symbols, code released.
  - Sources: [FINSABER, arXiv 2505.07078v5](https://arxiv.org/html/2505.07078v5); [Agentic Trading survey, arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **FinAgent** (Zhang et al., KDD 2024; older).
  - Multimodal: news, prices and K-line chart images fused by cross-attention, with dual-level reflection memory.
  - Original evaluation: 6 months, 6 symbols, code released.
  - The 2026 survey notes it relies on prompt-level rather than database-level temporal constraints.
  - Sources: [FINSABER](https://arxiv.org/html/2505.07078v5); [arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **FinCon** (Yu et al., NeurIPS 2024; older).
  - Manager–analyst hierarchy with "conceptual verbal reinforcement" (beliefs updated from P&L) and dual-level risk control.
  - Evaluation: single stocks on 8 assets, plus two 3-stock portfolios ([TSLA, MSFT, PFE] and [AMZN, GM, LLY]).
  - Original evaluation per FINSABER: 8 months, 8 symbols, code not released. Inference cost is not reported.
  - Sources: [NeurIPS 2024](https://proceedings.neurips.cc/paper/2024/hash/f7ae4fe91d96f50abc2211f09b6a7e49-Abstract-Conference.html); [FINSABER](https://arxiv.org/html/2505.07078v5); [arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **FinRobot** (AI4Finance; arXiv 2405.14767, May 2024; older).
  - Open-source agent platform, aimed mainly at financial analysis and research reports rather than backtested trading.
  - FINSABER lists its evaluation period and symbols as "not stated".
  - Sources: [FinRobot paper](https://arxiv.org/pdf/2405.14767); [GitHub](https://github.com/AI4Finance-Foundation/FinRobot); [FINSABER](https://arxiv.org/html/2505.07078v5)
- **StockAgent** (arXiv 2407.18957, July 2024; older).
  - Event-driven *simulated* market with LLM investor agents (gpt-3.5-turbo-0125, Gemini-pro-1.0).
  - Aim: study how external factors affect trading behaviour while avoiding test-set leakage. It is a behavioural simulation, not an alpha claim.
  - Sources: [arXiv 2407.18957](https://arxiv.org/html/2407.18957v4); [code](https://github.com/MingyuJ666/Stockagent)
- **QuantAgent** (Xiong, Zhang, Feng, Sun, You; v1 12 Sep 2025, v4 27 Jul 2026).
  - Four agents: Indicator, Pattern, Trend, Risk. Works on price data only (technical indicators, chart patterns, trends) over short windows.
  - 10 instruments, including BTC and Nasdaq futures. Zero-shot, with 4-hour trading intervals.
  - Compared only against random baselines. Called "high-frequency" but uses 4-hour bars, not order books.
  - Sources: [arXiv 2509.09995](https://arxiv.org/abs/2509.09995v2); [Hua et al. survey](https://arxiv.org/html/2608.31041v1) ("price-driven multi-agent system for HFT with risk-bounded orders")

**Family (b): LLM as alpha miner over a backtester**

- **Alpha-GPT** (2023; older): turns a researcher's natural-language ideas into executable alpha formulas, with a human in the loop — [Hua et al. survey](https://arxiv.org/html/2608.31041v1)
- **AlphaAgent** (Tang et al., arXiv 2502.16789, KDD 2025).
  - Multi-agent factor mining with regularised exploration:
    - AST-based originality check against existing alphas;
    - LLM-judged consistency between hypothesis and factor;
    - complexity control.
  - Evaluation: CSI 500 and S&P 500, Jan 2021 to Dec 2024.
  - Results after costs: CSI 500 annual excess return 11.0% (IR 1.5); S&P 500 8.74% (IR 1.05). Also an 81% improvement in hit ratio and about 30% fewer tokens.
  - Code: [RndmVariableQ/AlphaAgent](https://github.com/RndmVariableQ/AlphaAgent). No independent replication found.
  - Source: [arXiv 2502.16789](https://arxiv.org/html/2502.16789v2)

**Other 2025–2026 systems** (each with a one-line description in [Hua et al., "Agentic Quantitative Trading: A Survey of Workflows, Systems, and Evaluation", arXiv 2608.31041, Sept 2026](https://arxiv.org/html/2608.31041v1)):
- ContestTrade: internal contests rank agents by market feedback.
- QuantEvolve: quality-diversity evolutionary archive of strategies.
- QuantAgents: simulated trading meetings with dual rewards.
- FLAG-Trader: LLM policy tuned with PPO.
- HedgeAgents: hedging experts coordinated through conferences.
- AgenticAITA: typed JSON contracts and deterministic hard gates before execution.
- AlphaCrafter: full stack from factors to execution, with regime-aware factor ensembles.
- Invest-Teams: fine-grained task decomposition.
- FinArena: human–agent collaboration.
- MountainLion: multimodal, GraphRAG.
- PACE: parent-order execution.
- Hubble: constrained factor tools.
- Also listed: LLM-MCTS, AlphaPROBE, FactorMAD, AlphaLogics, RAPTOR, TrustTrade, ATLAS, WebCryptoAgent.
- An "Orchestration" system (Li et al. 2025b) links alpha, portfolio, risk and execution agents over MCP and A2A protocols.

**Survey statistics** ([Hua et al., arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)), over the 20 systems in its Table 1:
- All 20 do signal discovery. Factor mining is core in 4, portfolio construction in 8, order execution in 3, risk management in 9.
- Only 2 treat all five pipeline stages as core.
- Of the 17 multi-agent systems, 15 aggregate outputs, 5 use selection, 3 use debate, and 2 use gating.

**Hybrid design with deterministic sizing** ("Inside the Trading Loop", Jia, Mu, Yu, Liu; arXiv 2611.11657, 8 Nov 2026):
- Two Claude Haiku 4.5 analysts (company sentiment, macro) produce scores. A rule-based engine does the sizing, and the LLM cannot place orders.
- Paper account on Alpaca, 11 Mar to 28 Aug 2026: +34.5% vs SPY +14.4%, but 26.8% max drawdown vs 6.3%.
- A constant 5% cash control nearly matches the LLM's stance-based sizing. Enabling sentiment alone changes no selected stock.
- Source: [arXiv 2611.11657](https://arxiv.org/html/2611.11657)

**Live and contamination-free benchmarks (2025–2026):**
- **StockBench** (Tsinghua; arXiv 2510.02209, v2 Mar 2026).
  - Window: 3 Mar to 30 Jun 2025, 82 trading days. Universe: 20 DJIA stocks. Inputs: opening prices, fundamentals, up to 5 news items from the prior 48 h.
  - 13 models, 3 seeds each. Equal-weight buy & hold returned 0.4% (max drawdown −15.2%). Best composite was Kimi-K2 at 1.9% (−11.8%).
  - The abstract says most models fail to beat buy & hold. By the fetched table, about 8 of 13 beat the 0.4% return, so the paper is internally inconsistent.
  - Another inconsistency: a split-period analysis mentions Jan–Apr and May–Aug, which falls outside the stated window.
  - Source: [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2)
- **LiveTradeBench** (arXiv 2511.03628, Nov 2025).
  - 50-day live run of 21 LLMs. Inputs: prices and news. Output: portfolio allocations.
  - LMArena rankings correlated weakly or negatively with live trading results.
  - Sources: [alphaXiv 2511.03628](https://www.alphaxiv.org/fr/abs/2511.03628); [secondary review](https://liner.com/review/livetradebench-seeking-realworld-alpha-with-large-language-models)
- **Agent Market Arena** (arXiv 2510.11695): lifelong, real-time, multi-market benchmark. Agent framework design drove behaviour more than the backbone model did — [arXiv 2510.11695](https://arxiv.org/pdf/2510.11695)
- **DeepFund**: most LLMs lost money live — [Hua et al. survey](https://arxiv.org/html/2608.31041v1)
- **KTD-FIN**: attribution analysis suggests agents' returns mainly reflect market and style exposure rather than stock selection — [Hua et al. survey](https://arxiv.org/html/2608.31041v1)
- **Alpha Arena Season 1** (nof1, 17/18 Oct to 3 Nov 2025). Press coverage, not peer reviewed:
  - Six frontier models, $10k each, crypto perpetuals on Hyperliquid, no human intervention.
  - Qwen 3 Max won (+22%) and DeepSeek v3.1 was second. GPT-5 ended around $3,733. More than half lost money.
  - Gemini and GPT-5 loss figures differ across outlets.
  - Sources: [GNcrypto](https://www.gncrypto.news/news/qwen-wins-alpha-arena-season-1-with-22-percent-returns/); [Decrypt](https://decrypt.co/345006/ai-crypto-trading-showdown-deepseek-grok-winning-gemini-implodes)
- **Production record** ("What LLM Trading Agents Actually Do in Production", Barton et al.; arXiv 2609.05663, 4 Sep 2026). Two fleets:
  - 3,505 user-funded vaults trading Base memecoins over 21 days (Feb–Mar 2026);
  - about 100 active agents on Hyperliquid perpetuals (Jun–Aug 2026).
  - Scale: about 7.5M model calls and 14,596 fills.
  - Findings:
    - no directional edge: win rate 41% vs a 50% retail benchmark;
    - leverage ignores volatility: median 5.0× in every volatility bucket;
    - one high-risk slider setting caused 62% of liquidations;
    - 49.3% of positions that reached +300 bps closed at a loss;
    - a mechanical bracket adds about +39 bps per position;
    - frontier models were statistically indistinguishable in paired replay over 416 scenarios.
  - Source: [arXiv 2609.05663](https://arxiv.org/abs/2609.05663)

### Inferences
- The family (b) systems, where the agent writes factors or code and a backtester judges, report multi-year out-of-sample periods and IC/IR against established factor libraries. The family (a) systems, where the LLM itself decides trades, mostly report months on a few tickers. The family (b) evidence is structurally stronger but still self-reported.
- Independent and live evidence (FINSABER, StockBench, DeepFund, Alpha Arena, the production fleets) consistently shows little or no edge from LLM-as-trader designs. Gains seem to come mostly from exposure, sizing and risk mechanics rather than the LLM's views.

### Gaps
- I did not retrieve FinMem's or FinAgent's exact original test dates and tickers. FINSABER's table gives only lengths and counts.
- I did not read Alpha-GPT's original paper.
- I could not confirm whether the QuantAgent repo exists.
- I did not read the full text of DeepFund, KTD-FIN or PolyBench (they are cited via the survey only).

## 3. Which systems use intraday or microstructure data (order book, order flow)? Any agent reading footprint/order-flow data?

### Takeaway
Almost none. LLM trading agents in the literature run on daily prices, news, filings, social media and sometimes chart images or 4-hour bars. Order-book work with LLMs is either a simulation (LLM agents placing limit orders in a synthetic book) or a study of whether LLMs or foundation models *understand* the book. I found no published LLM agent that reads footprint or order-flow features as input for real trading decisions. The 2026 surveys treat microstructure as an execution-layer constraint, and LLM latency (hundreds of milliseconds or more) as unfit for high-frequency decisions.

### Cited Findings
- **RD-Agent(Q):** daily data, one-day-ahead labels — [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
- **TradingAgents:** daily OHLCV, indicators, news, social media, fundamentals — [arXiv 2412.20138v7](https://arxiv.org/html/2412.20138v7)
- **QuantAgent:** described as "high-frequency" but runs at 4-hour intervals on price-derived indicators, patterns and trends, with no order book mentioned — [arXiv 2509.09995](https://arxiv.org/abs/2509.09995v2)
- **"Can Large Language Models Trade?"** (Lopez-Lira, arXiv 2504.10789, Apr 2025): open-source simulated market with a persistent order book and market/limit orders, populated by LLM trader agents with different roles. It is a simulation, not real data — [arXiv 2504.10789](https://arxiv.org/html/2504.10789v1)
- **"Do LLMs Understand Limit Order Book Dynamics?"** (Chen & Glasserman, arXiv 2608.23706, 2026): an LLM trained on synthetic LOB event sequences generates valid sequences, but its implicit model of the book state is flawed, which biases forecasts — [arXiv 2608.23706](https://arxiv.org/pdf/2608.23706)
- **"What Do Foundation Models for Market Microstructure Learn?"** (arXiv 2610.20104, Oct 2026): next-token pretraining on synthetic LOB order flow infers market regimes. Real markets are noted as more complex than the simulators — [arXiv 2610.20104](https://arxiv.org/html/2610.20104v1)
- **Agentic Trading survey** (Xia et al., Shenzhen University, arXiv 2605.19337, May 2026):
  - It treats order-book imbalance, spread and queue position as hard feasibility constraints on the action space, in the execution layer.
  - It flags LLM inference latency (hundreds of ms or more) as too slow for high-frequency decisions.
  - EarnHFT, the cited HFT system, is RL, not an LLM.
  - Source: [arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **Hua et al. survey:** execution work relies mostly on backtests, historical replay or dry runs, with little live-liquidity testing. In PolyBench (prediction markets), order-book liquidity and slippage change model rankings — [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
- **Non-LLM order-book agents** also exist: RL market makers and multi-agent RL for HFT, such as JaxMARL-HFT (arXiv 2511.02136) and "When AI Trading Agents Compete" (arXiv 2510.27334). Order-book "agents" in the literature are mostly RL, not LLM — [arXiv 2511.02136](https://arxiv.org/pdf/2511.02136); [arXiv 2510.27334](https://arxiv.org/pdf/2510.27334)
- **Production fleet on Hyperliquid perpetuals:** the excerpt does not say whether the agents saw the order book — [arXiv 2609.05663](https://arxiv.org/abs/2609.05663)

### Inferences
- An LLM agent that reads footprint or order-flow summaries (for example aggregated delta, imbalance, depth heatmap statistics) to propose hypotheses, which ML models and backtests then evaluate, seems to be an open niche. This is an inference from absence in two 2026 surveys, not a proven claim.
- Given the latency findings, the realistic pattern is that the LLM works at research time: it proposes order-flow features or rules. Learned models or rules act at bar or tick time.

### Gaps
- I did not run a targeted search on "LLM footprint chart", "LLM order flow imbalance feature" or "LLM tick data agent".
- I could not confirm the order-book inputs for QuantAgent v4 (Jul 2026) or the Hyperliquid fleets.

## 4. Common architecture patterns: tool use, memory, reflection, debate, agent vs execution separation, human in the loop

### Takeaway
The recurring patterns are:
- role-specialised multi-agent pipelines that mimic a firm (analysts, researchers, trader, risk, manager);
- layered memory with reflection (working, episodic and semantic memory, plus reflection on P&L);
- debate, or more often plain aggregation;
- for alpha mining, a closed loop of hypothesis, code, backtest and feedback with novelty and complexity regularisers and a search scheduler (bandit, MCTS, evolutionary archive).

The 2026 trend is to separate the LLM from capital. LLM outputs pass through typed contracts and deterministic gates, and rule-based engines size and execute. People mostly decide the scope and set risk knobs. Few systems keep a human approving each trade.

### Cited Findings
- **Perception, memory, reasoning, action:**
  - Taxonomy: perception (text, time series, visual), memory (working, episodic, semantic), reasoning (reactive, reflective, strategic) and action/execution, framed as an expert-system pipeline (knowledge base, inference engine, action interface, rule constraints).
  - Cited cases: FinMem's layered memory; FinAgent's dual-level reflection; FinCon's verbal reinforcement from P&L.
  - Source: [Xia et al., arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **How multi-agent outputs are combined:** of 17 multi-agent systems, 15 aggregate, 5 select, 3 debate and 2 gate. 12 of 20 systems use an iterative structure — [Hua et al., arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
- **Role pipeline with debate:** TradingAgents runs analysts, then Bull/Bear debate, then trader, then a risk team, then fund-manager approval. It splits "quick" and "deep" thinking models — [arXiv 2412.20138v7](https://arxiv.org/html/2412.20138v7)
- **Research/development split:**
  - RD-Agent(Q): hypothesis generation from a knowledge forest, a code agent with a knowledge base, Qlib validation, and a bandit choosing factor vs model. [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)
  - AlphaAgent: AST originality, alignment check and complexity penalty. [arXiv 2502.16789](https://arxiv.org/html/2502.16789v2)
  - QuantEvolve (quality-diversity archive) and ContestTrade (internal contests). [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
- **Separating the agent from execution:**
  - AgenticAITA: typed JSON contracts and deterministic hard gates before execution. [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
  - "Inside the Trading Loop": LLMs cannot place orders or set sizes; a deterministic coordinator does. [arXiv 2611.11657](https://arxiv.org/html/2611.11657)
  - The same paper finds that shorter response contracts cut parse failures, but the macro stance still flips on about half the dates. Margin-based call gating keeps about 98% of allocation changes while calling the model on about 2/3 of dates. [arXiv 2611.11657](https://arxiv.org/html/2611.11657)
- **Tool protocols:** MCP and A2A orchestration across alpha, portfolio, risk and execution agents (Li et al. 2025b) — [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
- **Human in the loop:**
  - Alpha-GPT (researcher states ideas) and FinArena (investor risk preferences). [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
  - In production fleets, humans set a "risk slider" that explains leverage (+0.425 per level), and agent fixed effects explain 60% of variance. The operating layer matters more than the strategy text. [arXiv 2609.05663](https://arxiv.org/abs/2609.05663)
- **Framework over model:** framework design drove behaviour more than the backbone LLM — [Agent Market Arena, arXiv 2510.11695](https://arxiv.org/pdf/2510.11695)

### Inferences
- Patterns that keep recurring and look robust:
  1. The LLM proposes and deterministic code disposes, with backtester or engine as the judge.
  2. Explicit memory of past experiments (what was tried, IC, failure reason) to avoid repeats.
  3. Novelty and redundancy filters (IC correlation, AST distance).
  4. A budgeted search scheduler.
  5. Typed outputs plus hard risk gates.
- Debate gets a lot of attention but is rare in practice (3 of 17 systems) and not shown to add value independently.

### Gaps
- I found no ablation-backed, independent evidence that debate or reflection improves risk-adjusted returns out of sample.

## 5. Documented evaluation weaknesses, independent replications and critiques

### Takeaway
The weaknesses are well documented by 2026:
- short windows (3–8 months);
- a handful of mega-cap tickers;
- missing transaction costs;
- no seeds or significance tests;
- survivorship bias;
- search without multiple-testing control;
- look-ahead through the LLM's pretrained knowledge, which may be the biggest problem.

Independent re-evaluations show the gains largely vanish:
- FINSABER: 20 years, hundreds of symbols, no significant alpha.
- Profit Mirage: agents fall to random after the model's knowledge cutoff.
- Live benchmarks and production fleets: no directional edge.

### Cited Findings
- **FINSABER** (Li, Kim, Cucuringu, Ma; arXiv 2505.07078, May 2025; v5 accepted at KDD 2026). Independent re-evaluation.
  - Prior evaluation windows and symbols:

    | System | Window | Symbols |
    |---|---|---|
    | TradingAgents | 3 months | 3 |
    | FinMem | 6 months | 5 |
    | FinAgent | 6 months | 6 |
    | FinCon | 8 months | 8 |
    | TradExpert | 1 year | 30 |
    | MarketSenseAI | 1.25 years | 100 |
    | MarketSenseAI 2.0 | 2 years | 100 |

  - FINSABER's own setup:
    - 2004–2024;
    - over 7,000 US equities including delisted names;
    - 15.7M news items, plus 10-K/10-Q filings;
    - historical S&P 500 constituents and rolling reselection;
    - commission $0.0049/share (minimum $0.99), long only;
    - baselines: Buy & Hold, rules, ARIMA, XGBoost, A2C/PPO/SAC/TD3.
  - Results:
    - FinMem's claimed TSLA Sharpe of 2.679 replicated at 0.927 on the *same* window with GPT-4o-mini.
    - Over 2004–2024, FinMem beat Buy & Hold only on TSLA.
    - In composite universes FinMem's Sharpe was −0.292 to 0.025, against Buy & Hold 0.315 to 0.703.
    - Paired t-tests favour Buy & Hold (p ≈ 1e-6 to 1e-2). Neither agent shows significant alpha (all p > 0.34).
    - By regime: too conservative in bull markets, larger losses in bear markets (FinMem Sharpe −0.97 in bear years).
  - Source: [arXiv 2505.07078v5](https://arxiv.org/html/2505.07078v5)
- **Profit Mirage** (Li, Zeng, Xing, Xu, Xu; arXiv 2510.07920, Oct 2025).
  - Builds FinLake-Bench (2,000 historical QA pairs such as "did the market rise on date T?").
  - Finds almost all published LLM agents fail to beat a random baseline after the model's knowledge cutoff, attributing earlier gains to memorised outcomes.
  - Proposes FactFin (counterfactual perturbations, MCTS, RAG, strategy code generation), claiming 1.4× the out-of-sample Sharpe of the best baseline (authors' claim).
  - Source: [arXiv 2510.07920](https://arxiv.org/html/2510.07920v1)
- **Look-Ahead-Bench** (Benhenda, arXiv 2601.13770, Jan 2026): standard LLMs (Llama 3.1 8B/70B, DeepSeek 3.2) show significant look-ahead bias measured by alpha decay across regimes. Point-in-time models (Pitinf) generalise better. The abstract gives no magnitudes — [arXiv 2601.13770](https://arxiv.org/abs/2601.13770)
- **Other leakage tools:** HindsightBench (arXiv 2607.18867) is a black-box audit for parametric hindsight. "Summoning the Oracle to Slay It" (arXiv 2605.24564) proposes counterfactual decoding and argues that input-side protections miss leakage stored in model weights — [arXiv 2607.18867](https://arxiv.org/pdf/2607.18867); [arXiv 2605.24564](https://arxiv.org/pdf/2605.24564)
- **Five recurring biases:** look-ahead, survivorship, narrative, objective and cost bias. Backtests built on temporally inconsistent information "reflect that inconsistency rather than alpha" — [arXiv 2602.14233](https://arxiv.org/html/2602.14233v1)
- **Agent-layer leakage:** LLM-as-judge and agent-to-agent steps can leak future information even when the quant components are properly tested. The authors say their own procedures do not eliminate look-ahead bias — [Self-Driving Portfolio, arXiv 2604.02279](https://arxiv.org/pdf/2604.02279)
- **Agentic Trading survey evidence audit** (arXiv 2605.19337, studies screened through March 2026). In the primary empirical subset (n=19):

  | Practice | Studies |
  |---|---|
  | Time-consistent split protocol | 2/19 |
  | Explicit transaction-cost model | 1/19 |
  | Universe or survivorship handling | 1/19 |
  | Coded R0 (lowest reproducibility) | 15/19 |
  | Reaching R3 reproducibility | 0/19 |

  - Named problems:
    - "Oracle Fallacy": retrieved memories expose outcomes after the fact;
    - news timestamps record publication, not availability;
    - revised filings leak future information;
    - search overfitting from MCTS and tree search (report search budgets, use walk-forward);
    - prompt versions, tool calls and memory rules rarely logged;
    - rationales not faithful to the actual decision process.
  - Note: the fetched page says 92 records and 77 included. A search snippet said 185 studies reviewed, so the counts conflict.
  - Source: [arXiv 2605.19337](https://arxiv.org/html/2605.19337v1)
- **Evaluation practice critique** (AlphaForgeBench, arXiv 2602.18481, 2026): live evaluations are tied to a single execution window and cannot be replicated. Single-run results are statistically unreliable because LLM outputs vary across runs — [arXiv 2602.18481](https://arxiv.org/pdf/2602.18481)
- **Self-reported warning signs:**
  - TradingAgents reports Sharpe 5.6–8.2 over about 3 months, with no costs and no seeds, and the authors call it beyond the expected range.
  - Its paper is inconsistent on tickers, and its stated max-drawdown claim conflicts with its own table (AMZN 2.11 vs "did not exceed 2").
  - Source: [arXiv 2412.20138v7](https://arxiv.org/html/2412.20138v7)
- **Live evidence:**
  - StockBench: gains over buy & hold are small (best 1.9% vs 0.4% over about 4 months), and all agents underperformed in the downturn sub-period. [arXiv 2510.02209v2](https://arxiv.org/html/2510.02209v2)
  - Production fleets: no edge. [arXiv 2609.05663](https://arxiv.org/abs/2609.05663)
  - KTD-FIN: returns are exposure, not selection. [arXiv 2608.31041](https://arxiv.org/html/2608.31041v1)
  - "Inside the Trading Loop": a constant-cash control nearly matches LLM sizing, and the authors say reported alpha "should not be treated as deployment evidence". [arXiv 2611.11657](https://arxiv.org/html/2611.11657)
- **Even the stronger alpha-mining papers:** RD-Agent(Q) reports median ARR without error bars and no multiple-testing correction over about 44 loops — [arXiv 2505.15155v2](https://arxiv.org/html/2505.15155v2)

### Inferences
- **Recurring evaluation weaknesses (synthesis):**
  1. Test windows of months, inside or near the LLM's pretraining period.
  2. Hand-picked famous tickers (TSLA, AAPL, AMZN, MSFT), with survivorship bias.
  3. No or toy transaction costs, slippage and latency.
  4. Single runs, with no seeds, confidence intervals or significance tests.
  5. Search or selection among many candidates with no deflated Sharpe or multiple-testing correction.
  6. Weak baselines: random, or MACD/SMA rules instead of factor models or plain buy & hold over full regimes.
  7. Leakage through model weights, news timestamps, revised filings and memory retrieval.
  8. Code or prompts not released, so studies cannot be reproduced.
  9. Returns explained by market or style exposure and sizing rather than LLM judgement.
- **Mitigations that recur in 2025–2026 work:**
  - test windows after the model's cutoff, or point-in-time models;
  - live forward benchmarks;
  - several seeds;
  - attribution against factor exposures;
  - constant-exposure controls;
  - explicit search budgets with walk-forward validation;
  - deterministic execution layers.
- For an app where agents propose and an ML pipeline evaluates, the cheapest high-value safeguards are:
  - log every trial so the full search count is known;
  - deflate the selected result for that count;
  - enforce point-in-time data in the data layer, not in the prompt;
  - always compare against buy & hold and a constant-exposure control after costs.

### Gaps
- I found no independent replication of RD-Agent(Q), AlphaAgent or TradingAgents. FINSABER replicated FinMem and FinAgent only.
- Profit Mirage's peer-review status is unknown.
- The DeepFund, KTD-FIN and Alpha Arena official reports were not read directly.
- I found no study applying deflated Sharpe or White's Reality Check to LLM agent searches specifically.
