# Governing what LLM agents can access and do through tools (finance / trading focus), state as of 2026-11-20

Scope: tool-only data access, permissions, audit logs, provenance, human approval of consequential actions, prompt injection through data. Target design: a desktop trading terminal whose agents read data only through checked, journaled tools, served over MCP to Claude Code and to local models.

Source quality: items marked "(prior knowledge, not re-fetched)" come from well-known publications I did not open again in this session. The report writer should treat them as lower confidence than fetched items.

## 1. Model Context Protocol (MCP): current spec, authorization, tool annotations, security practice, attack classes

### Takeaway
The current MCP revision is **2026-07-28**, released 28 July 2026. It has a stateless core with no `initialize` and no `Mcp-Session-Id`, Tasks moved into an extension, Roots/Sampling/Logging deprecated, hardened OAuth (issuer `iss` check, CIMD replacing DCR) and header routing. On safety it stays normative but thin. Tools are model-controlled, and there "SHOULD always be a human in the loop with the ability to deny tool invocations". Annotations are hints that clients "MUST consider untrusted unless they come from trusted servers". Servers MUST validate inputs, enforce access control, rate-limit and sanitize outputs. Clients SHOULD confirm sensitive operations, show inputs, validate results and log usage for audit. Defenses against tool poisoning, rug pulls and injection through results are left to the implementer.

### Cited Findings
**Versions and changes**
- 2026-07-28 is the latest revision, and its release moved 2025-11-25 to Final. A release candidate came out on 2026-05-21 — [MCP blog, "The 2026-07-28 Specification"](https://blog.modelcontextprotocol.io/posts/2026-07-28/); [AAIF migration post](https://aaif.io/blog/mcp-2026-07-28-whats-changing-and-how-to-migrate)
- Changes in 2026-07-28:
  - **Stateless core:** each request carries protocol version, client identity and capabilities in `_meta`. `server/discover` is optional, and `initialize`/`initialized` and `Mcp-Session-Id` are retired (SEP-2575, SEP-2567).
  - **New mechanisms:** Multi Round-Trip Requests (SEP-2322) replace server-initiated sampling, elicitation and roots requests. List results are cacheable (`ttlMs`, `cacheScope`, deterministic order; SEP-2549).
  - **Extensions and deprecations:** a formal extensions framework covers MCP Apps and Enterprise Managed Authorization. Tasks move to the `io.modelcontextprotocol/tasks` extension (SEP-2663). Roots, Sampling and Logging are deprecated with at least 12 months of support (SEP-2577). Legacy HTTP+SSE is deprecated, and removals need at least 12 months' notice.
  — [MCP blog 2026-07-28](https://blog.modelcontextprotocol.io/posts/2026-07-28/)
- Authorization hardening in 2026-07-28:
  - Authorization servers SHOULD send `iss` (RFC 9207), and clients must check it before exchanging the code, which closes a "mix-up hole" (SEP-2468).
  - Client credentials are bound to their issuer (SEP-2352).
  - `application_type` in DCR lets localhost redirects work for desktop and CLI apps (SEP-837).
  - DCR is formally deprecated in favor of Client ID Metadata Documents (CIMD) and "will be removed in a future version".
  - Streamable HTTP requests MUST carry `Mcp-Method` and `Mcp-Name` headers so gateways and WAFs can act without parsing bodies (SEP-2243).
  — [MCP blog 2026-07-28](https://blog.modelcontextprotocol.io/posts/2026-07-28/)
- 2025-11-25 (first-anniversary release) added experimental Tasks, URL-mode elicitation (SEP-1036), OpenID Connect Discovery support, default scopes, and authorization extensions for client credentials and enterprise IdP control. SDK tiering arrived in SEP-1730 — [MCP 2025-11-25 changelog](https://modelcontextprotocol.io/specification/2025-11-25/changelog); [WorkOS summary](https://workos.com/blog/mcp-2025-11-25-spec-update)

**Tools page (2026-07-28), normative text**
- "Tools in MCP are designed to be model-controlled". Also: "For trust & safety and security, there SHOULD always be a human in the loop with the ability to deny tool invocations." Applications SHOULD make clear which tools are exposed, show visual indicators when tools are invoked, and present confirmation prompts — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- "clients MUST consider tool annotations to be untrusted unless they come from trusted servers." — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools). The fetched page lists `annotations` as "Optional properties describing tool behavior". It did not show the per-hint definitions, so see the gap below on `readOnlyHint`, `destructiveHint`, `idempotentHint` and `openWorldHint`.
- Security Considerations section: "Servers MUST: Validate all tool inputs; Implement proper access controls; Rate limit tool invocations; Sanitize tool outputs." "Clients SHOULD: Prompt for user confirmation on sensitive operations; Show tool inputs to the user before calling the server, to avoid malicious or accidental data exfiltration; Validate tool results before passing to LLM; … Implement timeouts for tool calls; Log tool usage for audit purposes." — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- `tools/list` "MUST NOT vary per-connection or as a side effect of other requests". It "MAY vary by the authorization presented on the request", meaning only the tools the caller's scopes allow. Servers SHOULD return tools in deterministic order — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- `outputSchema`: if provided, servers "MUST provide structured results that conform", and clients "SHOULD validate". For a no-parameter tool the spec recommends `{"type":"object","additionalProperties":false}` — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- Errors split in two. Protocol errors are JSON-RPC errors such as an unknown tool. Tool execution errors are returned with `isError: true` and carry actionable text such as a validation failure, so the model can self-correct. Clients SHOULD pass execution errors to the model — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- Stateful tools (non-normative): a handle "is a name, not a capability". The server should check authorization against the handle on every call and use opaque, high-entropy, expiring handles — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
- Tool calls can return `InputRequiredResult` (elicitation through MRTR) to ask the user for input before completing. This in-protocol mechanism can be used for approval — [MCP spec 2026-07-28, Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)

**Security Best Practices page (draft, tracks 2026-07-28)**
- Attack classes covered, with MUST/SHOULD mitigations — [MCP Security Best Practices](https://modelcontextprotocol.io/specification/draft/basic/security_best_practices):
  - Confused deputy through OAuth proxies with static client IDs and consent cookies. Mitigation: proxies MUST do per-client consent, exact `redirect_uri` match, and single-use `state` set only after consent.
  - Token passthrough. Servers "MUST NOT accept any tokens that were not explicitly issued for the MCP server". The page notes that passthrough also breaks audit trails.
  - SSRF during OAuth discovery. Block private and link-local ranges and use egress proxies.
  - State-handle hijacking. Servers "MUST NOT treat possession of a state handle as authentication" and SHOULD bind handles to the user ID from the verified token.
  - Mix-up attacks. Covered by the `iss` check.
  - Localhost redirect impersonation.
  - Scope minimization. Progressive least-privilege scopes, step-up challenges, and logging of elevation events with correlation IDs.
- Local MCP server compromise:
  - Clients offering one-click setup MUST show the exact command untruncated and require explicit approval.
  - Clients SHOULD sandbox servers with minimal file system and network access.
  - Server developers SHOULD use stdio, or protect HTTP transport with "an authorization token" or "unix domain sockets or other IPC mechanisms with restricted access". DNS rebinding against a localhost server is a named vector.
  — [MCP Security Best Practices](https://modelcontextprotocol.io/specification/draft/basic/security_best_practices)
- The page names no "tool poisoning" or "rug pull" class. Its threat model is mostly OAuth, transport and local execution — [MCP Security Best Practices](https://modelcontextprotocol.io/specification/draft/basic/security_best_practices)

**Attack classes documented outside the spec**
- Tool poisoning and rug pull (Invariant Labs, April 2025). A malicious MCP server first shows a harmless tool description and later swaps in instructions that make the agent exfiltrate data through a separate, trusted WhatsApp MCP server. The malicious tool never has to be called, and per the write-ups the user is never asked to approve the change. Invariant reported that several major clients were susceptible and suggested scanning servers with mcp-scan — [Invariant Labs blog](https://invariantlabs.ai/blog/whatsapp-mcp-exploited); [Docker write-up](https://www.docker.com/blog/mcp-horror-stories-whatsapp-data-exfiltration-issue/); [Simon Willison, 2025-04-09](https://simonwillison.net/2025/Apr/9/mcp-prompt-injection/)
- Docker lists the enabling weaknesses as no publisher identity check and "no change detection", meaning descriptions can change after approval — [Docker write-up](https://www.docker.com/blog/mcp-horror-stories-whatsapp-data-exfiltration-issue/)
- OWASP's agentic Top 10 cites a "GitHub MCP exploit" as its example of agentic supply-chain vulnerability (ASI04) — [OWASP GenAI, 2025-12-09](https://genai.owasp.org/2025/12/09/owasp-top-10-for-agentic-applications-the-benchmark-for-agentic-security-in-the-age-of-autonomous-ai/)

### Inferences
- For a terminal that serves its own tools, the server-side MUSTs map directly onto its design: validate inputs, access control, rate limits, sanitized outputs, and audit logging. The rug-pull class mostly does not apply when the terminal is the only MCP server the agent sees. It does apply if users add third-party servers next to it in Claude Code. If so, pin the tool-list hash and alert on change.
- Annotations (read-only, destructive) can label the terminal's tools for client UIs, but approval must never depend on the client honoring them. The server itself must refuse consequential calls without an approval token.
- A localhost HTTP MCP endpoint (for example 127.0.0.1 with a bearer token) follows the spec's "SHOULD require an authorization token" advice. DNS rebinding remains a risk, so check `Host`/`Origin` or use stdio or a unix socket.
- With the 2026-07-28 stateless core, any approval or session state must live in explicit handles that are bound to the caller and expire.
- The `InputRequiredResult`/elicitation path lets the server ask for human confirmation inside the protocol. Client support varies, so an out-of-band approval in the app's own UI is the safer default.

### Gaps
- The exact 2026-07-28 definitions and defaults of `readOnlyHint`, `destructiveHint`, `idempotentHint` and `openWorldHint` were not on the fetched Tools page. My understanding, not verified this session: they are defined in the schema `ToolAnnotations`, with defaults `readOnlyHint=false`, `destructiveHint=true`, `idempotentHint=false`, `openWorldHint=true`, introduced in 2025-03-26. Check schema.ts.
- Whether the deprecation of Logging (SEP-2577) affects server-to-client log streaming that some audit setups use was not checked.
- I found no MCP-native standard for signed tool manifests or tool-definition pinning. Proposals may exist in SEPs but none was verified.

## 2. General agent-security guidance: OWASP, NIST, Anthropic/OpenAI, dual-LLM and CaMeL designs

### Takeaway
Two threads lead the field. First, threat taxonomies: OWASP's Top 10 for Agentic Applications (Dec 2025), with goal hijack through data and tool misuse at the top, and NIST work that still lacks finished agent-specific controls. Second, architectural defenses that move trust out of the model: dual-LLM, plan-then-execute, action-selector and CaMeL. Model-level defenses such as training and classifiers lower attack rates but no vendor claims to remove them. The accepted rule is that untrusted data must not be able to choose which consequential actions run.

### Cited Findings
**OWASP**
- OWASP Top 10 for Agentic Applications was published 2025-12-09 by more than 100 contributors. Entries:
  - ASI01 Agent Goal Hijack (example: EchoLeak)
  - ASI02 Tool Misuse (example: Amazon Q)
  - ASI03 Identity & Privilege Abuse
  - ASI04 Agentic Supply Chain Vulnerabilities (example: GitHub MCP exploit)
  - ASI05 Unexpected Code Execution
  - ASI06 Memory & Context Poisoning
  - ASI07 Insecure Inter-Agent Communication
  - ASI08 Cascading Failures
  - ASI09 Human-Agent Trust Exploitation
  - ASI10 Rogue Agents (example: Replit incident)

  The Agentic Threats & Mitigations taxonomy was updated to v1.1 in sync, and the Top 10 is mapped to the Top 10 for LLMs — [OWASP GenAI](https://genai.owasp.org/2025/12/09/owasp-top-10-for-agentic-applications-the-benchmark-for-agentic-security-in-the-age-of-autonomous-ai/)
- Secondary summaries describe ASI01 as redirecting the agent's goal "through content the agent reads", including retrieved documents and tool outputs. Listed mitigations: treat natural-language input as untrusted, limit tool privileges, and require human approval for goal changes or high-impact actions. ASI02 mitigations include "least agency" tool scoping and parameter validation — [Aikido summary](https://aikido.dev/blog/owasp-top-10-agentic-applications); [Cycode summary](https://cycode.com/blog/owasp-top-10-agentic-applications/). These are secondary, and the primary OWASP fetch did not use the phrase "least agency".
- OWASP Top 10 for LLM Applications 2025 includes LLM01 Prompt Injection and LLM06 Excessive Agency, the latter covering excessive functionality, permissions and autonomy (prior knowledge, not re-fetched) — [OWASP LLM Top 10](https://genai.owasp.org/llm-top-10/)

**NIST**
- NIST AI 600-1, "AI RMF: Generative AI Profile" (July 2024). It lists 12 GenAI risks, including confabulation, information security and value-chain/component integration, with suggested actions organised under the AI RMF functions Govern/Map/Measure/Manage (prior knowledge, not re-fetched) — [NIST AI 600-1](https://nvlpubs.nist.gov/nistpubs/ai/NIST.AI.600-1.pdf)
- NIST's COSAiS project (SP 800-53 control overlays for securing AI systems) includes "Using AI Agent Systems (Single Agent)" and "(Multi-Agent)" use cases. As of March 2026, CSA notes said only a predictive-AI outline was public and no timeline had been given for the agent overlays — [NIST COSAiS](https://csrc.nist.gov/projects/cosais); [CSA research note, 2026-03-30](https://labs.cloudsecurityalliance.org/research/csa-research-note-nist-ai-agent-standards-federal-framework/)
- One search snippet claimed COSAiS was renamed "COSIS" on 2026-10-20 after an executive order. I could not corroborate this, so treat it as unverified — [NIST COSIS page (snippet only)](https://csrc.nist.gov/Projects/cosis)
- NIST red-team result, as cited by CSA: agent task-hijack success rose from 11% to 81% when attacks were tailored to LLM agents rather than taken from baseline attacks — [CSA research note](https://labs.cloudsecurityalliance.org/research/csa-research-note-nist-ai-agent-red-teaming-standards-202603/). This is secondary, and the primary NIST/CAISI blog was not opened.

**Architectural designs**
- CaMeL, "Defeating Prompt Injections by Design" (Debenedetti et al., Google/DeepMind and ETH; arXiv 2503.18813, v1 March 2025, v2 June 2025):
  - A privileged LLM writes a program from the trusted user query alone.
  - A quarantined LLM parses untrusted data and has no tool access.
  - A custom Python interpreter tracks provenance (capabilities) on every value and enforces security policies before each tool call.
  - On AgentDojo it solves 77% of tasks "with provable security" against 84% for an undefended system (v1 reported 67%).
  — [arXiv 2503.18813](https://arxiv.org/abs/2503.18813); code at github.com/google-research/camel-prompt-injection
- "Design Patterns for Securing LLM Agents against Prompt Injections" (Beurer-Kellner, Debenedetti et al., arXiv 2506.08837, June 2025) proposes six patterns:
  - Action-selector: the model only maps a request to predefined tool calls. Most restrictive and strongest.
  - Plan-then-execute: the allowed tool-call plan is fixed before any untrusted data is read. Injection cannot add tools, but it can still change arguments.
  - LLM map-reduce, dual LLM, code-then-execute, and context minimization.
  — [arXiv 2506.08837](https://arxiv.org/abs/2506.08837); [Simon Willison summary, 2025-06-13](https://simonwillison.net/2025/Jun/13/prompt-injection-design-patterns/)
- A follow-up on secure plan-then-execute verifies the plan against static policies, such as allowed information flows, before execution in an isolated environment — [arXiv 2509.08646](https://arxiv.org/abs/2509.08646)
- Dual LLM pattern (Willison, April 2023). The privileged LLM handles only references to untrusted content, such as `$VAR1`, never the content itself (prior knowledge) — [simonwillison.net 2023-04-25](https://simonwillison.net/2023/Apr/25/dual-llm-pattern/)
- "Lethal trifecta" (Willison, June 2025). Private data, exposure to untrusted content and an exfiltration channel together allow data theft, so remove at least one (prior knowledge) — [simonwillison.net 2025-06-16](https://simonwillison.net/2025/Jun/16/the-lethal-trifecta/)
- AgentDojo (Debenedetti et al., NeurIPS 2024 D&B, arXiv 2406.13352) is the standard dynamic benchmark of agent tasks with injection tasks placed in tool results (prior knowledge) — [arXiv 2406.13352](https://arxiv.org/abs/2406.13352). Newer work extends it: AutoDojo, a generative benchmark (arXiv 2606.15057), and multi-agent CaMeL (arXiv 2610.05640) — [AutoDojo](https://arxiv.org/abs/2606.15057); ["Can CaMeLs Talk?"](https://arxiv.org/abs/2610.05640)

**Vendor guidance**
- Anthropic, "Mitigating the risk of prompt injections in browser use" (2025-11-24):
  - Defenses are RL training on simulated injected content plus classifiers that scan for injected instructions.
  - Opus 4.5 reached about 1% attack success against an adaptive attacker. Anthropic's caveat: "no browser agent is immune to prompt injection".
  - Product safeguards: per-site permissions, confirmation for high-risk actions, and default blocks on financial sites.
  — [Anthropic research](https://www.anthropic.com/research/prompt-injection-defenses). Partly from search summary, so verify wording.
- During the Claude for Chrome research preview, unprotected attack success was 23.6%, falling to 11.2% with safeguards — [Anthropic, Claude for Chrome](https://www.anthropic.com/news/claude-for-chrome)

### Inferences
- The terminal's rule that data reaches models only through tools, never pasted into the prompt, fits the dual-LLM and CaMeL view of tool results as untrusted. A tool layer alone does not make results trusted, though: news headlines, RSS text and Pine scripts returned by tools can still carry injected instructions.
- The strongest affordable pattern for consequential actions such as orders or writes to user data is close to action-selector or plan-then-execute. The model proposes a typed action, a deterministic checker validates it against data the model did not author, and a human approves. The model's free text never becomes an argument without that check. This fits the user's existing "models invent, check mechanically" rule.
- For lethal-trifecta analysis: a local model that holds private data (accounts, journal) and reads untrusted text (news) should have no exfiltration channel. That means no network tool, and no tool argument that becomes a URL or an outbound message.
- Model-level defenses (Anthropic's roughly 1%) are a useful layer, but local models will not have them. The architectural controls must hold without them.

### Gaps
- OpenAI's current agent-safety guidance (Agents SDK guardrails, Operator/Atlas injection notes) was not fetched this session.
- No primary NIST/CAISI document on agent hijacking was opened, so the 11% to 81% figure comes via CSA.
- The primary OWASP ASI mitigation text and the "least agency" wording were not verified.

## 3. Finance-specific expectations: SEC, FINRA, ESMA, FCA, MiFID RTS 6

### Takeaway
No regulator has issued AI-agent-specific trading rules as of late 2026. Each says existing obligations apply to AI unchanged: MiFID II and RTS 6 algo controls, supervision and record keeping. FINRA's 2026 report (Dec 2025) is the most explicit on agents. It flags autonomy without human validation, agents exceeding scope or permissions, and auditability, and it expects human-in-the-loop procedures and logging of agent actions. RTS 6 sets the hard technical baseline for any automated order path: kill switch, four pre-trade controls, real-time monitoring, post-trade controls and testing before deployment.

### Cited Findings
**FINRA (US)**
- FINRA 2026 Annual Regulatory Oversight Report (2025-12-09) has a first standalone GenAI section and first discussion of AI agents, "systems or programs that are capable of autonomously performing and completing tasks on behalf of a user". Named risks: "Autonomy, where AI agents may act without human validation or approval", "Scope and authority, as AI agents may exceed the user's intended mandate or permissions", plus auditability and handling of sensitive data. Considerations: "monitoring AI agent system access and data handling; defining human-in-the-loop oversight procedures; tracking and logging AI agent actions and decisions" — [FINRA news release](https://www.finra.org/media-center/newsreleases/2025/finra-publishes-2026-regulatory-oversight-report-empower-member-firm); [DLA Piper, 2025-12-19](https://www.dlapiper.com/insights/publications/2025/12/finra-flags-generative-ai-risks-and-governance-expectations); [Debevoise](https://www.debevoise.com/insights/publications/2025/12/finras-2026-regulatory-oversight-report-continued). Quotes come via law-firm summaries; the PDF was not opened.

**SEC (US)**
- On 2025-06-12 the SEC withdrew 14 pending proposals, including the 2023 predictive data analytics conflicts-of-interest proposal. It said it would issue a new proposal if it acts again. Existing conflict-of-interest duties still apply — [Sullivan & Cromwell via CLS Blue Sky, 2025-06-23](https://clsbluesky.law.columbia.edu/2025/06/23/sullivan-cromwell-discusses-sec-withdrawal-of-14-proposed-rules/); [Paul Hastings](https://www.paulhastings.com/insights/client-alerts/sec-withdraws-14-pending-rule-proposals)

**ESMA (EU)**
- ESMA Public Statement on AI and investment services (2024-05-30, ESMA35-335435667-5924). MiFID II obligations apply to AI use. The management body should understand and oversee AI use and keep it within strategy, risk tolerance and compliance. Firms should keep "comprehensive records" on AI use and related complaints, and use of third-party AI falls under outsourcing rules — [ESMA statement PDF](https://www.esma.europa.eu/sites/default/files/2024-05/ESMA35-335435667-5924__Public_Statement_on_AI_and_investment_services.pdf); details via [Walkers](https://walkersglobal.com/Insights/2024/06/Initial-guidance-on-the-use-of-AI-in-the-provision-of-investment-services) and [Dillon Eustace](https://www.dilloneustace.com/insights/esma-issues-guidance-on-the-use-of-ai)

**MiFID II RTS 6 (Commission Delegated Regulation (EU) 2017/589), algorithmic trading**
- Art. 12 kill functionality: "An investment firm shall be able to cancel immediately, as an emergency measure, any or all of its unexecuted orders" — [legislation.gov.uk (UK-retained text)](https://www.legislation.gov.uk/eur/2017/589/chapter/II/section/3); [EUR-Lex](https://eur-lex.europa.eu/eli/reg/2017/589/oj)
- Art. 15 pre-trade controls on order entry:
  - "price collars, which automatically block or cancel orders that do not meet set price parameters"
  - "maximum order values"
  - "maximum order volumes"
  - "maximum messages limits"
  Orders must also be blocked where the trader lacks permission for the instrument — [legislation.gov.uk](https://www.legislation.gov.uk/eur/2017/589/chapter/II/section/3); [FCA Handbook copy](https://handbook.fca.org.uk/technical-standards/provision/s119c1037sn0p1549)
- Other articles:
  - Art. 13: automated surveillance of all trading activity.
  - Art. 14: business continuity arrangements.
  - Art. 16: monitoring of all algorithmic trading activity in real time.
  - Art. 17: post-trade controls run continuously.
  - Art. 18: IT strategy and security.
  — [legislation.gov.uk](https://www.legislation.gov.uk/eur/2017/589/chapter/II/section/3)
- Recitals: algorithm testing scales with potential market impact. Pre-trade controls run before the order reaches the venue. Record-keeping periods match the general order-record obligation — [legislation.gov.uk introduction](https://www.legislation.gov.uk/eur/2017/589/introduction)
- The UK-retained Section 3 is shown as revoked by the Financial Services and Markets Act 2023 (Sch. 1 Pt. 3), so UK requirements now sit in FCA rules. The EU text remains in force — [legislation.gov.uk](https://www.legislation.gov.uk/eur/2017/589/chapter/II/section/3)

**FCA (UK)**
- FCA AI Live Testing with Advai. Feedback statement FS25/5. The second cohort of eight firms (incl. Barclays, Lloyds/Scottish Widows, UBS, Experian) covers agentic AI, small language models and neurosymbolic AI. Testing runs from April 2026 to year-end, with an evaluation report due Q1 2027 — [FCA second cohort](https://www.fca.org.uk/news/press-releases/fca-announces-second-cohort-ai-live-testing); [FS25/5](https://www.fca.org.uk/publications/feedback-statements/fs25-5-ai-live-testing)
- In an October 2026 report the FCA said it intends to become an "agentic regulator" and is not proposing separate AI-specific financial rules — [Law360](https://www.law360.com/articles/2539334). This is a paywalled secondary source.

### Inferences
- A retail desktop terminal is not itself an investment firm under RTS 6. Still, RTS 6 is the recognized checklist for any automated order path, and aligning with it is defensible best practice:
  - a global kill switch that cancels all of the agent's open orders;
  - deterministic pre-trade checks (price collar against last price, max notional, max quantity, max orders per minute, instrument allowlist) that run outside the model, before the broker API;
  - real-time monitoring of agent order flow;
  - post-trade reconciliation;
  - testing in a non-live environment before any live use.
- The FINRA themes (autonomy, scope, auditability) map onto three controls: explicit approval for each consequential action, per-agent permission scopes enforced server-side, and an append-only journal of agent actions with inputs.
- ESMA's "comprehensive records" and outsourcing points matter if a cloud model provider is used for decisions. Journaling which model and version produced each proposal supports this.

### Gaps
- The exact MiFID II record retention, in my understanding 5 years under Art. 16(6) and 2017/565 Art. 72 and up to 7 on request, was not verified this session.
- Any SEC 2026 statement or roundtable output specific to AI agents was not found in this session's searches.
- ESMA's 2025-2026 follow-up work on AI in trading was not checked, e.g. AI in algo trading as a supervisory priority or TRV articles.
- The EU AI Act angle was not researched this session. Trading tools are not, to my knowledge, an Annex III high-risk category, but Art. 12 logging duties for high-risk systems are a useful model.
- IOSCO's 2025 AI report was not fetched.

## 4. Audit and reproducibility: tool-call logging, data vintages, run IDs, replay, counting trials

### Takeaway
Baseline guidance is general: MCP says clients SHOULD log tool usage for audit, and FINRA wants agent actions and decisions logged. The most concrete designs come from 2026 academic benchmarks of LLM trading agents. They seal every gather, analyze, decide, execute and reflect cycle into a recompute-verifiable audit chain and compute scorecards from that trail. I found no published standard schema for agent-decision journals in finance.

### Cited Findings
- MCP: clients SHOULD "Log tool usage for audit purposes". The security page says token passthrough breaks accountability because the server "will be unable to identify or distinguish between MCP Clients", and recommends logging scope elevation with correlation IDs — [MCP Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools); [MCP Security Best Practices](https://modelcontextprotocol.io/specification/draft/basic/security_best_practices)
- FINRA 2026: "tracking and logging AI agent actions and decisions"; auditability is a named agent risk — [DLA Piper summary](https://www.dlapiper.com/insights/publications/2025/12/finra-flags-generative-ai-risks-and-governance-expectations)
- CLQT (Qu & Chen, arXiv 2606.29771, v1 2026-06-29, v3 2026-09-12) seals each "gather-analyze-decide-execute-reflect cycle" into a "recompute-verifiable audit chain" and computes the scorecard from the trail. It also runs a "contamination-controlled" year-long multi-model backtest plus four weeks of live paper trading on post-cutoff data — [arXiv 2606.29771](https://arxiv.org/abs/2606.29771)
- rta-bench-1, "LLM Trading Agents Earn Beta, Not Alpha" (arXiv 2611.19530), checks every run for look-ahead. It splits returns into factor exposure and alpha and finds the apparent edge is mostly beta — [arXiv 2611.19530](https://arxiv.org/html/2611.19530)
- Papers cited in this literature, not opened: "Profit Mirage", where agent profits vanish after the knowledge cutoff, and "Alpha Illusion", where reported alpha is not evidence for deployment — via search snippets of [arXiv 2611.19530](https://arxiv.org/html/2611.19530)

### Inferences
- A journal row per tool call would hold:
  - run_id, agent/slot id, model id and version, prompt hash;
  - tool name and version, validated arguments, as-of time, data vintage per series;
  - result hash plus size, result row count, latency;
  - decision, approval status and approver.
- With such rows any decision can be replayed against the same vintages. Storing result hashes rather than full payloads keeps the log small, provided the source data stay versioned. The terminal's SQLite stores and Data-tab vintages help here.
- Statistical honesty means counting every backtest or ML run an agent launches, including discarded ones. The count feeds deflated-Sharpe or multiple-testing corrections. The tool layer is the natural place to count, because each run passes through a journaled tool.

### Gaps
- I found no regulator or standards-body schema for LLM agent audit logs in trading. OpenTelemetry GenAI semantic conventions may be relevant but were not checked.
- Trial-counting tools (deflated Sharpe, PBO) were not researched in this session. They are well known from Bailey and López de Prado but are not cited here.

## 5. Enforcing "no look-ahead" at the tool layer: as-of parameters, sandbox clocks

### Takeaway
Yes, systems already do this, and it is becoming the norm in 2026 LLM-trading research. CLQT routes all data access through a hard TimeGate. The others in this section use post-cutoff test windows, point-in-time universes, headlines released at their publication minute, and contamination probes. Their shared finding: gating data is necessary but not enough, because model weights memorize the future. Evaluation must therefore stay after each model's training cutoff, or include probes.

### Cited Findings
- CLQT "enforces point-in-time data access through a hard TimeGate". Per the search summary the gate also covers memory retrieval and news passes. The benchmark adds per-model post-cutoff windows and point-in-time universe membership against survivorship bias — [arXiv 2606.29771](https://arxiv.org/abs/2606.29771). The abstract confirms TimeGate; the per-channel detail comes from a search snippet.
- rta-bench-1 shows each headline only after its publication minute and plays each period both "blind" (anonymized) and "named", which separates memorization from reasoning — [arXiv 2611.19530](https://arxiv.org/html/2611.19530)
- "The Only Edge Was Memorized in Training" (arXiv 2611.22177) uses the same look-ahead-free setup around earnings and bad news. Its title states the finding that the apparent edge comes from memorization — [arXiv 2611.22177](https://arxiv.org/html/2611.22177v1)
- Look-Ahead-Bench (Benhenda, arXiv 2601.13770) is a standardized benchmark of look-ahead bias in "point-in-time LLMs" for finance. It tests end-to-end agentic trading in simulation and measures performance decay across regimes — [arXiv 2601.13770](https://ideas.repec.org/p/arx/papers/2601.13770.html); [code](https://github.com/benstaf/lookaheadbench)
- FinCAD, "Summoning the Oracle to Slay It" (arXiv 2605.24564): the agent sees only data timestamped before the rebalance date, and trades fill at the next day's open — [arXiv 2605.24564](https://arxiv.org/html/2605.24564)
- HindsightBench (arXiv 2607.18867) is a black-box audit protocol for "parametric hindsight" in time-indexed LLM decision tasks — [arXiv 2607.18867](https://arxiv.org/pdf/2607.18867)

### Inferences
- How the terminal could enforce this:
  - Every data tool takes a mandatory `as_of`, or inherits it from a run-level simulated clock that the agent cannot change.
  - The server filters by publication or availability time, not observation date. Macro releases, revisions and vintages matter here: GDP for Q3 is not known on 30 Sept.
  - The server refuses requests past `as_of`, and the refusal is journaled.
  - Fills happen at the next bar after the decision.
  - Memory and journal reads are filtered by the same clock.
- Results must be checked against each model's training cutoff, and periods should run anonymized (tickers and dates masked) as well as named. A local model with a known cutoff makes this simpler.
- Macro "golden series" with revision history can serve true vintages. Market bars are naturally point-in-time, but news and Facts tables need publication timestamps.

### Gaps
- Full texts of CLQT and rta-bench-1 were not read. TimeGate's implementation details (per-tool enforcement, error behavior) are unverified beyond the abstract.
- I found no commercial or open-source MCP server that exposes as-of-gated financial data as a product feature. Vendors such as point-in-time databases (e.g. ALFRED for FRED vintages) exist but were not checked against MCP.
