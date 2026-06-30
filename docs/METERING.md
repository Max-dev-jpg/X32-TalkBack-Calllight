# Meter Sources — where each value comes from

This documents how the TalkBack-CallLight firmware obtains meter levels from the
X32/M32, which **tap** (pre / gate-GR / comp-GR / post) is available for which
channel type, and the hardware limitation behind post-fader metering.

The mapping lives in [`ConfigManager::meterRoute()`](../src/ConfigManager.cpp);
this file is the human-readable reference for it.

---

## TL;DR — tap availability per channel type

`meterSignalType`: **0 = Pre**, **1 = Gate-GR**, **2 = Comp-GR**, **3 = Post**.

| Channel type | Pre | Gate-GR | Comp-GR | Post |
|--------------|-----|---------|---------|------|
| **Input** (1-32) | `/meters/1[ch]` | `/meters/1[32+ch]` | `/meters/1[64+ch]` | **`/meters/6`** ¹ |
| **Bus** (1-16) | `/meters/2[i]` | — | `/meters/2[25+i]` | **`/meters/6`** ¹ |
| **Matrix** (1-6) | `/meters/2[16+i]` | — | `/meters/2[41+i]` | **`/meters/6`** ¹ |
| **Main L/R** | **`/meters/6`** ¹ | — | `/meters/2[47]` | `/meters/2[22]` |
| **Mono M/C** | `/meters/2[24]` | — | `/meters/2[48]` | **`/meters/6`** ¹ |
| **AuxIn** (1-8) | `/meters/0[32+i]` | — | — | **`/meters/6`** ¹ |
| **FxRtn** (1-8) | `/meters/0[40+i]` | — | — | **`/meters/6`** ¹ |
| **DCA** (1-8) | — | — | — | — |

`ch` = channelNumber − 1 (0-based); `i` = number − 1.

¹ **`/meters/6` single-channel limit** — see below. Everything *not* marked
`/meters/6` is **multi-channel**: any number of triggers can use it at once.

---

## The `/meters/6` single-channel limit

Post-fader levels (and Main pre-fader) exist **only** on `/meters/6`, the
"channel strip" meter — a 4-float blob `[post-gain/trim, gate-GR, comp-GR,
post-fade]` for **one** channel selected by `<channel_id>` (0…71).

The console keeps a **single, console-wide `/meters/6` selection**. Verified on
hardware: two subscriptions for different channels both return the **last
selected** channel; an index range or omitted channel id still returns 4 floats
of channel 0. There is no way to stream several channels' strips at once
(round-robin time-sharing was tried and is unusable in practice).

**Consequence:** only **one** channel may use a `/meters/6` tap at a time. The
first such trigger claims the channel (`MixerConnection::_m6ChannelId`); further
`/meters/6` triggers on a **different** channel are **blocked** (level forced to
0, shown as `/meters/6 BLOCKED (channel busy)` in the web OSC monitor). Several
triggers on the **same** channel (e.g. different taps) all work.

The **web UI prevents this proactively**: choosing a Post-Fader tap (or Main
Pre-Fader) for a second trigger on a different channel is rejected with an
explaining popup and reverted, and the trigger form refuses to save such a
combination (`enforceM6` / `validateM6BeforeSave` in `app.js`). The firmware
block above is the backstop.

---

## Confirmed meter-bank layouts

Tap nature confirmed live on the console (not just from the OSC doc). All banks
stream simultaneously at the ~50 ms refresh.

| Bank | Floats | Contents | Tap |
|------|--------|----------|-----|
| `/meters/0` | 70 | 32 input, 8 auxin, 8 fxrtn, 16 bus, 6 matrix | **pre** |
| `/meters/1` | 96 | 32 input level [0-31], 32 gate-GR [32-63], 32 comp-GR [64-95] | **pre** + GR |
| `/meters/2` | 49 | bus [0-15], mtx [16-21], **main L/R [22,23]**, mono [24], then GRs [25-48] | **pre** (bus/mtx/mono) / **post** (main only) |
| `/meters/5` | 27 | console-surface VU (16 ch + groups + main) | **pre** |
| `/meters/6` | 4 | one channel: post-gain/trim, gate-GR, comp-GR, post-fade | all 4 (1 ch) |

Notes:
- For inputs, `/meters/0`, `/meters/1`, `/meters/4`, `/meters/5` are all
  **pre-fader** — none follow the fader.
- `/meters/2` is **pre-fader** for bus/matrix/mono; **only Main L/R is
  post-fader** there (hence Main's default tap is post from a bulk bank, while
  Main's *pre* must come from `/meters/6`).
- **DCA** levels are not present in any `/meters/*` bank, so DCA cannot be used
  as a meter source (fader/mute still work).

---

## Subscription handling

All meter sources are `/batchsubscribe` requests, so one command renews them all:

- **Aliases:** `/m0,/m1,/m2` → `/batchsubscribe <alias> /meters/<0|1|2> 0 <end> 1`
  (full banks); `/m6` → `/batchsubscribe /m6 /meters/6 <channel_id> 0 1` (single
  channel strip). The console returns each blob addressed to its alias.
- **Register once, then renew:** on connect / config change the full set is sent
  (`sendMeterSubscriptions`); afterwards a single no-arg **`/renew`** extends them
  all every `METER_RENEW_INTERVAL_MS` (`renewMeterSubscriptions`). Keep that well
  below the console's 10 s timeout; lower values tolerate more lost renews.
- **Self-heal:** all subs renew together, so a lost `/renew` lapses them
  together and meter data stops. If no meter blob arrives for `METER_STALL_MS`
  while connected, the set is re-registered.
- **`/unsubscribe` (all)** is sent before re-registering a changed set and on
  reconnect, so a stale `/meters/6` channel stops immediately.
