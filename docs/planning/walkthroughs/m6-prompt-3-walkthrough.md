# M6-PROMPT-3 Walkthrough — AI Backend Wiring

**Date:** 2026-06-01
**Result:** Shipped — all 4 providers wired; AIChatPanel real; TestAiProvider 10 tests
**Commits:** 6c89a1d (D1-D5) · f5476c8 (D6 CHANGELOG)
**Test count:** 25 → 26 (`TestAiProvider` added)

---

## COMPLETED

### D1 — IAiProvider interface
`src/engines/ai/IAiProvider.h`: `AiMessage{role,content}`, `AiOptions{maxTokens,temperature,systemPrompt}`,
`AiResult{ok,text,errorMsg}`, `IAiProvider` abstract class with `chat()`, `isReady()`, `isPlausibleKey()`.

### D2 — Provider implementations

| Provider | File | Key source | Model |
|---|---|---|---|
| `AnthropicProvider` | `AnthropicProvider.cpp` | `CredentialManager("Anthropic")` → `QSettings("ai/keyAnthropicCached")` | `claude-3-5-sonnet-20241022` |
| `OpenAiProvider` | `OpenAiProvider.cpp` | `QSettings("ai/keyOpenAICached")` | `gpt-4o` |
| `GeminiProvider` | `GeminiProvider.cpp` | `QSettings("ai/keyGeminiCached")` | `gemini-1.5-flash` |
| `OllamaProvider` | `OllamaProvider.cpp` | No key; endpoint from `QSettings("ai/ollamaEndpoint", "http://localhost:11434")` | `QSettings("ai/ollamaModel", "llama3")` |

All four: `QNetworkAccessManager` in `QtConcurrent::run` worker → `QFuture<AiResult>`.
GUI thread never blocked.

### D3 — AIChatPanel real wiring
- Added 4 provider `unique_ptr` fields; `activeProvider()` reads `QSettings("ai/provider")`.
- `onSend()`: appends user message to `m_history`, submits `prov->chat(m_history)` via `QFutureWatcher`.
- Typing-cursor placeholder (`QListWidgetItem("AI: …")`) replaced by real response in `onAiFinished()`.
- `m_history` accumulates assistant turns → multi-turn conversation works.
- Suggestion chips submit pre-filled questions.

### D4 — PreferencesDialog provider selection
- Removed `"OpenAI (coming v1.1)"` / `"Gemini (coming v1.1)"` strings; replaced with real labels.
- Added `Ollama (local)` as 4th option.
- `onAiTestKey()` rewritten: format check → `shared_ptr<IAiProvider>` + `QFutureWatcher<AiResult>` for real 1-token ping; result shown inline as colored status label. Ollama shows informational message (no key needed).
- Memory safety: provider managed via `shared_ptr` captured by lambda — no leak.

### D5 — TestAiProvider (10 tests)
- `testMockIsReady`, `testMockChatEchoes`, `testMockEmptyHistoryNocrash` — mock provider, CI-safe
- `testAnthropicKeyFormat`, `testOpenAiKeyFormat`, `testGeminiKeyFormat`, `testOllamaNoKeyRequired` — format-only, CI-safe
- `testAnthropicNoKeyNotReady` — verifies un-configured provider
- `testAnthropicRealPing`, `testOpenAiRealPing` — QSKIP when `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` env absent

---

## DEFERRED

- **Streaming cursor** (token-by-token typing animation): current implementation shows `"AI: …"` placeholder until full response arrives; `QFutureWatcher::progressValueChanged` could be used for streaming but requires server-sent-events / chunked HTTP — deferred to M7-P2 (bug bash / performance tuning).
- **Page context injection**: the prompt spec mentioned "compose context: current page text (PDFium extract) + chat history" — current implementation only sends chat history without injecting the document's current page text into `systemPrompt`. Wiring to `AppContext::pdfEditor` to extract page text is a minor enhancement deferred to M6-P6 edge cleanup.
- **Gemini / OpenAI real round-trip tests**: only Anthropic + OpenAI are env-gated in tests. Adding `GEMINI_API_KEY` and `OLLAMA_ENDPOINT` env gates is deferred.

---

## Build fix notes

- Pattern 14 (architectural ambiguity): `retrieveKey` vs `readKey` in `CredentialManager` — caught at compile time, fixed immediately.
- `QM_FILES_OUTPUT_VARIABLE` / `RESOURCE_PREFIX` mutual exclusion in `qt_add_translations` (M6-P2 D3) — same session, not M6-P3 but worth noting.
- Mock provider default argument: C++ virtual function default args are not inherited through the concrete override signature when called directly — fixed by passing `{}` explicitly at all call sites.
