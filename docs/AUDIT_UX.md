# Napkin — independent UX audit (third pass)

Adversarial review of the **cards build** (libs as of 07:47). I did not write
`SPEC.md`, `docs/UX_TWO_PANE.md` or `docs/UX_CARDS.md` and I treat all three as
claims. Every number below was measured, not estimated.

**Method.** Built in `/tmp/uxaudit` (never the project's `build/`). Drove the real
`MainWindow` over an in-memory `Database` with `QTest::mouseClick` /
`keyClick` / `mouseDClick` / `QWheelEvent` under `QT_QPA_PLATFORM=offscreen`, and
read the resulting PNGs. Contrast is composited-sRGB luminance against the actual
Breeze Light (`Window #eff0f1`, `Base #fcfcfc`, `Text #232629`, `Highlight #3daee9`)
and Breeze Dark (`#2a2e32` / `#1b1e20` / `#fcfcfc` / `#3daee9`) values, cross-checked
against glyph-core pixels sampled out of the rendered framebuffer.

Probe source: `/tmp/uxa/probe.cpp` · contrast model: `/tmp/uxa/contrast.py` ·
renders: `/tmp/uxa/shots/`. Nothing under `src/` or `tests/` was modified.

---

## Verdict up front

Everything the two prior reviews fixed still holds — **in the left pane**. The
board of cards was built beside it without inheriting any of it. Accessible
names, focus visibility, keyboard reachability and the contrast derivation were
all re-solved for `BufferCardDelegate` and then not carried across to `ItemCard`,
`CardFooter` and `ItemCanvas`. The result is a window whose two halves are at
different standards.

Two things are outright broken as shipped: **a keyboard-only user cannot select,
copy, edit or delete a single item on the board** (§1.1), and **the `⋯` menu that
§20 records as the fix for undiscoverable shortcuts is empty** (§1.6).

The deepest problem is not a defect. It is that three independent decisions —
newest-first, balanced-column masonry, and no search — compose into a surface
with **no stable handle on anything**. Each is defensible alone. Together they
mean every insert permutes the board, every resize re-wraps it, and the list row
that is your only index relabels itself behind your back (§4).

---

# A. CONFIRMED — measured, reproducible

## 1. Severity: CRITICAL

### 1.1 Keyboard-only users cannot operate the board at all

Driven directly (`/tmp/uxa/probe.cpp`, mode `kbd`). Starting from the list, with
a buffer of 9 items open:

| Gesture | Result |
|---|---|
| `Enter` on a list row | canvas takes focus, **selection size 0** |
| `↓` on the board | selection 0 — no handler exists |
| `→` on the board | selection 0 — no handler exists |
| `Tab` | Qt focus moves to a card; **selection still 0** |
| `Space` on a focused card | selection 0 |
| `Enter` on a focused card | not editing; selection 0 |
| `Ctrl+C` on a focused card | clipboard unchanged (sentinel survived) |
| `Delete` on a focused card | 9 cards before, 9 cards after |

`ItemCanvas::keyPressEvent` handles `Ctrl+C`, `Ctrl+X`, `Ctrl+A`, `Delete`,
`Esc` and `Enter` — but every one of them operates on `selected_`, and **there is
no keyboard gesture anywhere that puts anything into `selected_`.** `setItems()`
only seeds a selection when called with `selectIndex >= 0`, which happens on one
path: after a delete.

Net: a keyboard user can select *all* items (`Ctrl+A`) or *none*. Copying one
card, editing one card, deleting one card are mouse-only operations.

- SPEC §14 — *"Keyboard navigation for every action"* — is false.
- `UX_TWO_PANE.md` §3.3 specifies the entire model (`↑↓` moves selection, `Tab`
  navigates items, `Enter` → text mode, `Home`/`End` → first/last item, a
  four-rung `Esc` ladder). **None of it is implemented.** The build has two of
  four `Esc` rungs; `Esc` never returns focus to the list.

**Fix, smallest version:** `setItems()` selects index 0 when the canvas is
entered from the keyboard; `↑↓←→` move the selection through `cards_` in board
order; `Space` selects the focused card. That is ~40 lines and it converts the
board from unusable to usable without keyboard.

### 1.2 The Copy button on every card is unreachable by keyboard

`CardFooter` is a bare `QWidget` that never calls `setFocusPolicy`. Measured for
all 9 footers: `focusPolicy = 0` (`Qt::NoFocus`).

```
footer accName="Copy image" focusPolicy=0
footer accName="Copy text"  focusPolicy=0        (×9)
```

The footer is the design's headline affordance — `UX_CARDS.md` §2.1 argues at
length for it being *"always visible, never hover-only"* because *"hover-only
fails touch, fails keyboard…"*. It then shipped as a hand-painted rect with
`mousePressEvent` and no focus policy, no `QAccessible::Button` role, no
`Space`/`Enter` activation and no key binding. It fails keyboard exactly as
completely as a hover overlay would have.

### 1.3 Text cards have no accessible name

Measured across the 9-card board: **6 of 9 cards expose `accessibleName = ""`
and `accessibleDescription = ""`.**

```
card 0 ImageItemCard accName="favicon.png"  accDesc="favicon.png, 120 × 90, 736 B"
card 1 TextItemCard  accName=""             accDesc=""
card 3 TextItemCard  accName=""             accDesc=""      <- the 40-line log
card 4 TextItemCard  accName=""             accDesc=""
...
```

`ImageItemCard`'s constructor calls `setAccessibleName`/`setAccessibleDescription`.
`TextItemCard`'s does not, and neither does the `ItemCard` base. A screen reader
walking the board announces three images and six unnamed containers.

`BufferListModel` gets this right for the list (`Qt::AccessibleTextRole` at
line 125 assembles primary + secondary + Pinned/Kept/In trash + relative time).
That work was simply never done for the board. §20's *"No accessible names …
fixed"* holds for the left pane only.

### 1.4 Keyboard focus is invisible on every card

`ItemCard::paintEvent` reads `selected_`, `hovered_` and `hasEditFocus()`. It
never reads `hasFocus()`. Rendered with card 3 holding real Qt focus
(`hasFocus() == true`, `isSelected() == false`): `/tmp/uxa/shots/crop_focus.png`
— the focused card is pixel-identical to its neighbours.

SPEC §14 — *"Visible focus states"* — false for the board.
`UX_TWO_PANE.md` §8.2 specifies `focus.ring` = `Highlight` α255, 2px, 2px offset.
Not implemented anywhere in `src/`.

### 1.5 Nothing indicates which pane the keyboard is in

Rendered the same window twice, once with `view_` focused and once with
`canvas_` focused, then diffed the 340×814 list-pane region:

```
list-pane pixels differing between list-focused and canvas-focused: 0
```

`/tmp/uxa/shots/focus_list_light.png` vs `/tmp/uxa/shots/focus_canvas_light.png`.

This matters more here than in most apps because **`Delete` means two different
things in the two panes**: in the list it trashes an entire buffer; on the board
it deletes items. `BufferCardDelegate::paint` branches on `State_Selected` and
`State_MouseOver` only — never on `State_HasFocus` or `State_Active`.

`UX_TWO_PANE.md` §3.5 names this *"The §7 review's real objection"* and says it
is *"answered visually, in three places at once"* — filled-vs-outline row
selection, and two more. **Zero of the three exist in the build.** The document
asserts a solution that was never written.

### 1.6 The `⋯` overflow menu is empty — §20's discoverability fix regressed

```
overflow menu actions: 2
   action: "---"                    (separator)
   action: "Keyboard shortcuts…"
window actions: 4
   New buffer      Ctrl+N
   Paste           Ctrl+V
   New text block  Ctrl+T
   Add image…      Ctrl+Shift+I
```

`MainWindow::buildOverflowMenu()` does `for (QAction* action : actions())` — but
`buildUi()` calls `rootLayout->addWidget(buildHeaderWidget())` at line ~105 and
the four `addAction()` calls happen at lines ~140–175. The menu is built against
an empty action list. It ships containing a leading separator and one item.

Consequence: **`Ctrl+T` is the only way to create a text card** (there is no
composer any more) and it appears in exactly two places — a string inside the
empty-buffer placeholder, and behind `⋯ → Keyboard shortcuts…`, which is itself
a modal `QMessageBox`. `Ctrl+Shift+I` appears only in the modal. This is the
precise failure §20 records as *"Three `QAction`s attached to no menu — bindings
undiscoverable | high | **fixed**"*. It is not fixed; it regressed with the
header rewrite, and no test covers menu contents.

One-line fix: build the menu lazily, or move `buildHeaderWidget()` after the
action block.

---

## 2. Severity: HIGH

### 2.1 A clipped card gives no indication whatever that it is clipped

`/tmp/uxa/shots/crop_fade.png`. The 40-line log renders lines `[001]`–`[021]`,
line 21 drawn at **full strength**, then white space, then the footer. Lines
22–40 are gone with no fade, no ellipsis, no scrollbar, no count.

The fade *is* drawn. It is drawn in `ItemCard::paintEvent`, on the parent
widget. `QPlainTextEdit` is a **child widget** and paints after its parent, with
`background: transparent` — so the gradient washes the card fill and the glyphs
are then painted over it at full opacity. The fade can never affect the only
thing that needed fading.

This breaks the central promise of the sizing model. `Tokens.h` line 47:
> *"Past this a card would own the board, so it clips with a fade and opens on
> double-click."*

Neither half is true: there is no fade (this section), and double-click does not
open it (§2.3). A 1 MB paste and a 21-line note are visually indistinguishable
(`/tmp/uxa/shots/onemb_light.png` — the 1 MB card is a plain 420px card).

### 2.2 The wheel is trapped by clipped cards

Measured (`probe.cpp`, mode `wheel`), one notch of `-120` delivered over a
read-only clipped card's viewport:

```
READ-ONLY card wheel: board scroll 0 -> 0 | card text scroll 0 -> 3
EDITING   card wheel: board scroll 0 -> 0 | card text scroll 19 -> 20 (max 20)
```

The board does not move. The card's hidden text scrolls instead — silently, with
`ScrollBarAlwaysOff`, so nothing on screen changes position except the text
itself. Two consequences:

1. You cannot scroll past a tall card by pointing at it. On a 200-item board
   every tall card is a dead zone for the wheel.
2. A card can end up permanently showing its middle, with no affordance to get
   back to its top, and nothing telling you it moved.

`ItemCanvas` sets `setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded)` on itself
but `TextItemCard` sets `ScrollBarAlwaysOff` on the editor while leaving the
viewport scrollable. A scroll region that accepts wheel input and shows no
indicator is a straightforward usability defect regardless of the rest.

### 2.3 Editing a clipped card: caret in a 420px box, and the card does not grow

Measured on the 40-line card (`probe.cpp`, mode `edit`):

```
tall card height 420  clipped true
editing? true  vbar policy ScrollBarAlwaysOff  vbar max 20  viewport h 358
after Ctrl+End:  vbar value 19  card h 420
after typing:    card h 420     vbar value 20
```

`/tmp/uxa/shots/edit_start_light.png` → `/tmp/uxa/shots/edit_typed_light.png`.
Double-clicking near the top starts editing and the card *keeps showing lines
1–21*; press `Ctrl+End` and it jumps to lines 21–40 with no scrollbar ever
appearing and no way to tell the view moved. Typed text lands at the bottom of a
viewport the user cannot scroll with any visible control.

`UX_CARDS.md` §7.3 named this exactly and recommended *"a clipped card
double-clicks into a full-height overlay"*, flagging it as unresolved. It is
still unresolved and it is now worse than that section describes, because §2.1
means the user has no warning the card was clipped in the first place.

`UX_TWO_PANE.md` §11.7 is right that a 1 M-char document is slow to lay out — but
that is not what bites here. What bites is a 2 KB log.

### 2.4 In Breeze Light, selecting a card makes its border *less* visible

Composited over `Base #fcfcfc`:

| State | Colour | Ratio (light) | Ratio (dark) | Floor |
|---|---|---|---|---|
| Resting border | `Text` α128 / α108 | **3.10:1** | 4.00:1 | 3.0 |
| Hover border | `Text` α174 / α154 | 5.30:1 | 6.71:1 | 3.0 |
| **Selected border** | `Highlight` α160 | **1.74:1** | 3.45:1 | 3.0 |
| **Editing border** | `Highlight` α200 | **2.00:1** | 4.64:1 | 3.0 |
| Selected fill tint | `Highlight` α20 / α34 | **1.07:1** | 1.25:1 | — |

So in the harsher of the two themes — the one `Tokens.h` says every alpha was
derived against — **the selected state is 1.8× lower contrast than the resting
state it replaces.** The only remaining non-hue signal is 1px → 2px, and a 2px
line at 1.74:1 reads fainter than a 1px line at 3.10:1.

The code comment asserts compliance:
> *"Selection changes the border's WIDTH as well as its colour, so the state is
> never carried by colour alone (SPEC.md §14)."*

That satisfies §14's letter while inverting its intent. `UX_CARDS.md` §4 proposed
`Highlight` α200 and applied the 3:1 floor to every *other* border in the same
table without ever measuring this one. `UX_TWO_PANE.md` §8.2 lists the same
values and claims *"Every state above is carried by two signals (rail + fill, or
rail + border)"* — the rail was deleted in the cards rewrite, so one of the two
signals named in that sentence no longer exists.

In practice the selection *is* visible to normal colour vision
(`/tmp/uxa/shots/crop_sel_l.png`) because the blue hue reads against grey. It is
carried by hue, at 1.07:1 luminance. That is the thing §14 exists to prevent.

The list pane has the same issue at lower stakes: its selected accent rail is
solid `Highlight` on `Base` = **2.43:1** in light (6.73:1 in dark).

### 2.5 Two text styles fail WCAG AA, and α161 has no margin

The token block says every alpha was *"composited over `QPalette::Base`"*. The
canvas was then moved to `QPalette::Window` (`#eff0f1`, darker) and the alphas
were not re-derived.

| Style | Where | Alpha | Ratio (light) | Verdict |
|---|---|---|---|---|
| Canvas placeholder (*"Nothing here yet…"*) | over `Window` | 161 | **4.33:1** | FAIL (needs α165) |
| Empty state *"Ctrl+N to begin"* | over `Window` | 130 (hardcoded) | **3.08:1** | FAIL (needs α165) |
| Footer label + timestamp | over `Base` | 161 | 4.52:1 arithmetic | see below |

`/tmp/uxa/shots/crop_empty.png` — *"Ctrl+N to begin"* is measurably the faintest
text in the application and it is the **only instruction on the first-run
screen**. Glyph-core pixel sampled from the render: `#888a8e` on `#eff0f1` =
**3.03:1**. It is not even a token; it is `c.setAlpha(130)` inline in
`MainWindow::buildUi()`. §20 records *"four styles failed WCAG AA | fixed"* —
this one was missed and is worse than several that were fixed.

**And α161 itself is too tight.** 4.52:1 against a 4.50:1 floor is a 0.4% margin.
Sampling the glyph core of *"Copy text"* out of the actual framebuffer gives
`#787778` on `#fcfcfc` = **4.35:1 — below AA**. Antialiasing and gamma eat the
margin. `kTextTertiary` is the most-repeated text in the new UI (two strings on
every card, plus every list timestamp and section label). Raise it to ~170, which
costs nothing and buys 5.04:1.

Dark passes everywhere (7.21:1 at α161; 4.67:1 for the empty-state hint).

### 2.6 Card width is non-monotonic in window width

Measured by resizing the real window:

| Window | Canvas | Card width | Columns |
|---|---|---|---|
| 1024 | 680 | 291 | 2 |
| **1280** | 936 | **419** | **2** |
| **1366** | 1022 | **301** | **3** |
| 1440 | 1096 | 326 | 3 |
| 1600 | 1256 | 379 | 3 |
| **1920** | 1576 | **283** | **5** |

Dragging the window 86px wider (1280 → 1366) makes **every card 28% narrower**
and re-wraps every line of text on the board. Going to 1920 makes cards *narrower
than at 1024*. `kCardMaxWidth = 460` is unreachable at every tested size: the
column count is chosen as *the most columns that fit at the 280px minimum*, so the
width always lands in `[280, 430]`.

Related: `MasonryLayout::columnCount` contains two dead constructs — a
`while (columns > 1 && …) break;` whose body is an unconditional `break`, and an
`if (widest > maxColumn_)` branch whose right-hand side can never exceed
`columns`. The function reduces to `max(1, (usable + gap) / (min + gap))`.

### 2.7 Every insert permutes the whole board

Compare `/tmp/uxa/shots/main_light.png` with `/tmp/uxa/shots/ctrlt_light.png`
(one `Ctrl+T`, nothing else changed):

| | before | after |
|---|---|---|
| top-left | favicon card | new composer |
| top-right | *"remember to email sam…"* | favicon card |
| second-left | 40-line log | *"remember to email sam…"* |

Because a new card is inserted at index 0 and `layoutInto` re-runs
shortest-column placement over the whole list, **one paste can move every card on
the board.** The same happens on every window resize (§2.6). A board is supposed
to buy you spatial memory; this one has none to buy — see §4.

### 2.8 The default window opens too narrow for its own board

```
DEFAULT window size: QSize(608, 760)   splitter sizes (260, 344)
list min 260   canvas min 344
```

`MainWindow::buildUi()` calls `resize(560, 760)` and asks the splitter for
`{340, 660}`. Both are overridden: 560 is below the sum of the two minimum
widths, so the window opens at 608 with the list at its 260px floor and the
canvas at its 344px floor. `344 − 2×32 padding = 280` = exactly one column.

A product whose stated value is *seeing many things at once* opens showing one
column of cards, on every first run, until the user resizes it. Window geometry
is not persisted.

---

## 3. Severity: MEDIUM

### 3.1 Undo is an 8-second, mouse-shaped, content-obscuring toast

`/tmp/uxa/shots/crop_toast.png`. Measured geometry after deleting one item in a
1280×860 window: `QRect(544, 795, 192×45)`.

- **It lands on top of a card.** `reposition()` centres on `parentWidget()`,
  which is the whole central widget spanning both panes — so at 1280 it sits at
  x=544, inside the board's left column, over the screenshot card. It also
  swallows clicks in that rect for the full 8 seconds.
- **There is no `Ctrl+Z`.** Grepped: no undo shortcut, no undo action, no menu
  entry. The only undo in the product is this button.
- **"Undo" does not look like a control.** Flat `QPushButton`, default text
  colour, no border, no underline, same weight as *"Item deleted"* beside it.
- **The toast is nearly invisible on a light board.** `Base` fill against a
  `Window` canvas is a 1.11:1 surface step, with a `Text` α80 = **1.92:1** border
  and no shadow. It reads here only because it happened to land on a dark image.
- Reaching the button by keyboard inside 8s means tabbing past ~15 card stops.

§20 already records *"the undo toast still replaces rather than stacks, and does
not name the buffer"*. The overlap, the missing `Ctrl+Z` and the non-button
styling are additional.

### 3.2 The list row's identifying label is derived from the newest item

`/tmp/uxa/shots/crop_list.png`. The 9-item buffer holds a screenshot, a 40-line
build log, a URL, a paragraph and a shopping note. Its row reads:

```
[green 52px thumb]  favicon.png
                    9 items
                    just now
```

`derivePreview` takes `head.front()`, and `listForBuffer` is
`ORDER BY modified_at DESC` — so **the primary line is whatever you pasted most
recently.** Paste a screenshot into a buffer and its label becomes that image's
filename; paste an unnamed one and the label becomes *"Screenshot"*. Two buffers
whose last paste was a screenshot are literally indistinguishable:
`Screenshot / N items / just now`.

With no search (§4), this row is the *only* index into the product, and it
relabels itself every time you use the buffer.

### 3.3 "Keep" currently protects against nothing

Grepped `src/`: there is no sweep. `kept` is written, read, guarded by a SQL
trigger, drawn as a bookmark glyph, exposed to accessibility — and consumed by
nothing. The shortcut sheet tells the user *"K — Keep — never removed by a
sweep"*, describing protection against an event that cannot occur.

This is honestly scheduled (Phase 6), but it is presented in the UI as a live
guarantee. Two of the five verbs in §21's governing test — *find it* and *keep
it* — are not in the build.

### 3.4 A missing image still offers "Copy image"

`/tmp/uxa/shots/crop_missing.png`. The card correctly says *"This image is no
longer on disk."* and then shows a footer reading **`[copy] Copy image`**.
Clicking it: `copySelection()` finds `QImage(path)` null, falls through to the
text branch, and puts the string `gone.png` on the clipboard — silently. The user
asked for an image and got a filename, with no error.

### 3.5 Copy gives no feedback at all

Verified the footer button works (`probe.cpp`, mode `copybtn`): it selects the
card and copies all 1959 characters of the log, not the visible 21 lines. Good
behaviour. But there is **no confirmation of any kind** — no flash, no toast, no
label change. In a copy-paste utility this is the single most frequent action,
and the only cue is the card acquiring a 1.74:1 selection border (§2.4).

### 3.6 The list row wastes 25% of its height on an empty slot

Measured row pitch: **96px**. Text-only rows draw a primary line at the top, a
timestamp at the bottom, and ~40px of nothing between them, because the secondary
line slot is reserved whether or not there is a secondary line — which for
single-item text buffers there never is. `/tmp/uxa/shots/crop_list.png`.

`UX_CARDS.md` §6 costed this exactly (*"height 72 against the build's ~96 …
twenty-five percent more buffers visible per screen"*) and it was not done. At
5000 buffers that is 120,000 wasted scroll pixels.

Also still present and still dead: `BufferCardDelegate::kMaxCardWidth = 760` and
its centring branch, in a pane clamped to `setMaximumWidth(520)`.

### 3.7 Header controls do not read as controls

`/tmp/uxa/shots/crop_header.png`. `＋ New`, `⋯` and `Trash` are all flat, all
right-aligned, all in a 1280px header whose left 1050px is empty. Findings:

- Nothing has a border or a fill, so all three read as static labels. `Trash` is
  a *checkable* button whose unchecked state is indistinguishable from a caption.
- `＋ New` uses U+FF0B FULLWIDTH PLUS, which falls back to a different font and
  renders visually detached from the word beside it.
- The `⋯` `QToolButton` renders as three low dots plus a menu arrow, baseline-
  misaligned against the text buttons and far lighter than either. It is also the
  emptiest control in the app (§1.6).
- The `＋ New` button creates a row in the **left pane**; it sits ~1100px away in
  the top-right corner.

### 3.8 The first-run screen does not mention pasting

`/tmp/uxa/shots/crop_empty.png` says *"Napkin / Put something here. / Ctrl+N to
begin"*. In a product whose §1 premise is *"you paste things in"*, the empty state
names a keyboard shortcut for creating an empty container and never mentions
`Ctrl+V`, which works from cold and does the right thing. It also does not point
at the `＋ New` button that is visible on the same screen.

---

## 4. The compound problem: nothing in this UI is stable enough to be found again

This is the answer to "does the product work end to end", and it is not a bug
list. Search is not implemented (Phase 5) — fine on its own. What is not fine is
that **the rest of the design has been optimised in ways that make its absence
worse rather than neutral.**

Measured, at 5000 buffers (seeded and driven; perf itself is excellent — see
§6.3):

```
list content height: 480,072 px    viewport 858 px    => 559 screens
row pitch 96 px                    ~8.5 rows visible
sections available: PINNED, RECENT   (that is all)
```

- **50 buffers** — 5,800px, ~7 screens. Survivable. You still scan every row
  because nothing is grouped by anything but pinned/not-pinned.
- **500 buffers** — 48,000px, ~56 screens. Already past what scanning solves.
  There is no date section, no filter, no sort control, no jump.
- **5000 buffers** — 559 screens, and §18 acceptance criterion 16 claims *"5000
  buffers → scroll and search stay inside §12 targets"*. Scroll does. Search does
  not exist.

The design choices that turn "no search yet" into "no way to find anything":

1. **No stable label.** §3.2 — the list row's primary line is whatever you
   pasted last. Names are forbidden by the premise (correctly), so this derived
   line is the whole identity of a buffer, and it mutates.
2. **No stable position.** The list re-sorts on `modified_at`, so *using* a
   buffer moves it to row 1 and pushes everything down by one.
3. **No stable board layout.** §2.7 and §2.6 — every insert permutes the
   masonry, every resize changes the column count and re-wraps the text.
4. **No coarse time structure.** Only `PINNED` / `RECENT`. `relativeTime()`
   collapses everything older than a week to a date string, but nothing groups
   by it. A "Today / Yesterday / This week / Older" section key is free — the
   model already computes `SectionNameRole` — and it is the single cheapest
   partial answer to findability before FTS lands.

**The rationale I think is wrong.** SPEC §1 *"Mess is a feature"* and §7's
no-titles stance are right, but they are only affordable if *something else*
carries identity. In the built product nothing does. "Mess is a feature" is a
claim about the user's effort at capture time; it is not a licence for the
application's own labels to be non-deterministic.

**Acceptance criteria that cannot pass in this build:** 3 (link chip), 6
(search), 7 (sweep), 8 (undo the sweep), 16 (search at 5000). §18 presents all 17
as *"the happy path … still correct"* with no note that five are Phase 5–6. That
should be marked in the document, not discovered by a reader.

---

## 5. Answers to the specific questions

### 5.1 Masonry reading order — can a user reconstruct recency?

**No, and the timestamp does not rescue it.**

`/tmp/uxa/shots/order_light.png` annotates each card with its board index. At
1280 with two columns:

```
col 1:  0        col 2:  1
        3                2
        5                4
                         6
```

Scanning normally (left→right, top→bottom) you read 0, 1, 3, 2, 5, 4, 6. Item 2
sits *below* item 1 in the same column, physically beside item 3. After four
items the positional order is gone — as `UX_CARDS.md` §7.1 predicted.

`UX_CARDS.md` §2.5 defends the footer timestamp as *"the only thing that makes
the board's order recoverable"*. Measured, it does not, for one reason the
document does not consider: **`relativeTime()` has one-minute granularity at the
top end.** Everything under 60s is `"just now"`; everything under an hour is
`"N minutes ago"`. Napkin's stated use is a work session — you paste six things
in two minutes. In that regime the footer reads `just now` on every card (visible
in every render here) and carries zero ordering information. Past an hour it
switches to a clock time, which orders but does not tell you *which came first*
without arithmetic.

**Is newest-first even right for a board?** No — and the reason is §2.7, not
taste. Newest-first only pays if position is stable enough that "up and left is
newer" becomes a habit. Here the insert re-balances the columns, so the habit
never forms: you learn "newest is top-left", then paste once and the thing that
was top-left is now top-right. A board earns its keep through spatial memory; a
board that repacks on every write has the cost of a board and the recall of a
list.

The honest options, none free (I agree with §7.1's framing and disagree with its
conclusion that "doing nothing is survivable"):

1. **Row-major fill.** Order is recoverable by construction; you lose the ragged-
   bottom packing masonry exists for. Given that most Napkin items are short text
   (88px min), the packing win is small in practice — look at
   `/tmp/uxa/shots/many_light.png`, where 200 uniform cards make the masonry a
   plain 2-column grid anyway.
2. **Date sections** (Today / Yesterday / Older) with masonry inside each.
   Preserves coarse order, adds chrome, and is the same section machinery the
   list already has.
3. **Append-at-end instead of insert-at-top**, so existing cards never move and
   the board grows downward. Costs "newest first"; buys a stable board. For a
   surface you return to, I think this trade is better than it looks.

I would ship 1. It is the option that makes the position mean something, and the
board is the only place position could have meant anything.

### 5.2 The buffer concept — is the two-level model pulling its weight?

**Partly. It is justified by the data model and not by the current UI.**

The argument *for* buffers is real and SPEC §7 states it well: *"a buffer is a
screenshot and a command and a URL kept together"*. Grouping is genuinely what
you want when you paste a stack trace, a screenshot of it, and the command that
produced it. A flat item board would lose that, and re-deriving it by
timestamp-clustering would be exactly the "automatic intelligence" §1 forbids.

But the **UI** currently gives you a two-level model with one level's worth of
information, and charges you a navigation model for it:

- The list row shows a thumbnail, a derived primary line, a count and a time —
  and the primary line is a copy of what the top-right card on the board already
  says (§3.2). It is a *worse* copy: elided to ~30 characters, and re-derived
  from a different item each time you paste.
- The board shows all of the same content, properly.
- So the list is not a summary. It is a lossy, mutating re-render of the
  board's first card, plus a count.

**What the list is actually for, once the board exists**: it is a *switcher*, not
a summary. Its job is "which pile am I in" and "get me back to the pile I was in
twenty minutes ago". That is a job the current row does badly, because the one
field that could do it (the primary line) is derived from the wrong item and the
one field that does it well (the timestamp) is rendered smallest.

**Would one flat board with the list as a filter be better?** No — but not for
the reason the spec gives. The reason is deletion and lifecycle: Pin, Keep,
Trash, sweep and the 8s undo all operate on buffers, and collapsing to items
would mean redesigning all five. The two-level model is load-bearing in the data
layer and in the lifecycle; it is just underserved in the presentation.

**What I would change instead of collapsing it:**

1. Pin the primary line to the **oldest** item in the buffer, or to the first
   *text* item, and never let it change. A buffer's identity should be what
   started it, not what last landed in it. This single change gives the product
   the stable handle it currently lacks (§4).
2. Drop the empty secondary slot (§3.6) and merge count + time onto one line, as
   `UX_CARDS.md` §6 already costed.
3. Add date sections to the list. Free, and the only findability the product can
   have before FTS.

### 5.3 The left pane vs the board — redundancy or hierarchy?

Useful hierarchy, implemented as redundancy. See 5.2. The concrete redundancy:
for a single-item text buffer the list row and the board card display *the same
string* in two panes, one of them elided, 400px apart. For a multi-item buffer the
list row displays the newest card's label and a count, which is the least
informative summary available — *"9 items"* tells you nothing about which nine.

### 5.4 Editing a card taller than 420px

Broken, measured in §2.3. Made worse by §2.1 (no fade warning) and §2.2 (wheel
trap). I agree with `UX_CARDS.md` §7.3: this needs a full-height overlay, and the
`Lightbox` precedent makes it consistent rather than novel. Shipping a caret in a
clipped viewport with scrollbars disabled is the worst of the available options —
it *looks* like it works.

### 5.5 Accessibility summary

| Requirement (SPEC §14) | Left pane | Board |
|---|---|---|
| Keyboard navigation for every action | yes (`P`/`K`/`Delete`/`R`/`Enter`/arrows) | **no** (§1.1) |
| Visible focus states | selection rail only; no focus/active distinction (§1.5) | **none** (§1.4) |
| Screen-reader labels | full, incl. Pinned/Kept/time | **6 of 9 cards unnamed** (§1.3) |
| Predictable tab order | n/a (one widget) | **no** — see below |
| Sufficient contrast, both themes | passes except the rail (2.43:1) | 2 text styles fail; selection 1.74:1 |
| Never meaning by colour alone | pin/keep are distinct shapes ✓ | selection is hue + 1px (§2.4) |

**Tab stops to reach the 10th card.** Measured chain for a 9-card mixed buffer:
`ItemCanvas` + **15 focusable stops** for 9 cards, because every `TextItemCard`
contributes two (the card, then its `QPlainTextEdit`, which is focusable even in
`NoTextInteraction` mode and has no accessible name and no caret). For an
all-text buffer that is 2 stops per card, so **the 10th card is the 20th `Tab`
press** — and none of those presses selects anything (§1.1).

The order is also not the board's order. Measured x-positions along the chain, at
two columns: `32, 471, 471, 32, 471, 32, 471, 471, 471`. Tab follows `cards_`
(model order), the eye follows the masonry (packed order), and the two diverge
after the third card.

**Pin / keep glyphs — prior fix HOLDS.** `/tmp/uxa/shots/glyphs.png` at 8×: the
pin (flat head, crossbar, tapered point) and the keep bookmark are unambiguously
different silhouettes at 13px, neither reads as a magnifying glass, and both are
backed by accessible text in `BufferListModel`. No regression.

**Full contrast table** (both themes, computed; `/tmp/uxa/contrast.py`):

| Style | α | Light | Dark | Floor | |
|---|---|---|---|---|---|
| Card body text | 255 | 14.82 | 16.33 | 4.5 | pass |
| Card footer label + timestamp | 161 | 4.52 *(4.35 sampled)* | 7.21 | 4.5 | **marginal / fails as rendered** |
| Image caption | 161 | 4.52 | 7.21 | 4.5 | marginal |
| List primary | 255 | 14.82 | 16.33 | 4.5 | pass |
| List secondary | 170 | 5.04 | 7.89 | 4.5 | pass |
| List timestamp / section label | 161 | 4.52 | 7.21 | 4.5 | marginal |
| Canvas placeholder (on `Window`) | 161 | **4.33** | 6.30 | 4.5 | **FAIL (light)** |
| Empty state *"Ctrl+N to begin"* (on `Window`) | 130 | **3.08** | 4.67 | 4.5 | **FAIL (light)** |
| Card border, resting, over `Base` | 128 / 108 | 3.10 | 4.00 | 3.0 | pass |
| Card border, resting, over canvas `Window` | 128 / 108 | 3.01 | 3.70 | 3.0 | pass (barely) |
| Card border, hover | 174 / 154 | 5.30 | 6.71 | 3.0 | pass |
| **Card border, selected** | `Hi` 160 | **1.74** | 3.45 | 3.0 | **FAIL (light)** |
| **Card border, editing** | `Hi` 200 | **2.00** | 4.64 | 3.0 | **FAIL (light)** |
| Selected fill tint | `Hi` 20 / 34 | 1.07 | 1.25 | — | hue only |
| List card border, resting | 128 | 3.10 | 5.06 | 3.0 | pass |
| List card border, selected | 178 | 5.57 | 8.52 | 3.0 | pass |
| **List selection rail** | `Hi` 255 | **2.43** | 6.73 | 3.0 | **FAIL (light)** |
| Pin / keep glyph | 215 | 9.02 | 11.88 | 3.0 | pass |
| Copy icon | 161 | 4.52 | 7.21 | 3.0 | pass |
| **Toast border** | 80 | **1.92** | 2.80 | 3.0 | **FAIL (both)** |
| Hairline (decorative) | 36 | 1.31 | 1.54 | — | exempt |
| **Surface step, card `Base` vs canvas `Window`** | — | **1.11** | **1.23** | — | see note |

*Surface step*: `UX_CARDS.md` §1.1 quotes 1.14:1 / 1.27:1, computed against
`#ffffff`. Breeze Light's `Base` is `#fcfcfc`, so the real step is **1.11:1**. The
section's own conclusion — *"1.14:1 is not enough on its own either"* — is
correct and slightly understated; the card is carried almost entirely by its
3.10:1 border, which is why §2.4's selected state going to 1.74:1 is a real
regression and not a quibble.

### 5.6 Empty, edge and error states

| State | Render | Verdict |
|---|---|---|
| No buffers | `empty_light.png` | works; instruction fails AA (§2.5); never mentions `Ctrl+V` (§3.8) |
| Buffer with one item | `single_light.png` | one 88px card in a 1280px pane, 92% empty. Honest, but the board reads as broken rather than sparse |
| 200 items | `many_light.png` | ~16 cards visible per screen; board is 10,800px ≈ 13 screens; masonry degenerates to a plain 2-column grid because uniform-height cards have nothing to pack |
| Missing image | `missing_light.png` | message is correct and clear; footer still offers "Copy image" and silently copies a filename (§3.4) |
| 1 MB single-line paste | `onemb_light.png` | renders fast (no stall observed); clipped to 420px with **no indication at all** that 999,000 characters are missing (§2.1) |
| Undo toast | `toast_light.png` | lands on top of a card and blocks clicks on it for 8s (§3.1) |

Chrome-to-content ratio at the minimum: `kCardMinHeight = 88` with
`chromeHeight() = 62` leaves **26px for content — 30% of the card**. On a
200-item board that is 12,400px of footers to show 5,200px of text. The `88` is
defended in `UX_CARDS.md` §3.1 as "one line plus chrome" and the arithmetic is
right; what the section does not weigh is that the footer it is sizing around is
identical on every card (*"Copy text"* / *"just now"*, ×200) and therefore carries
no per-card information at all.

---

# B. SUSPECTED — believed true, not fully isolated

1. **`ImageItemCard::rescale()` reads `width()`, which the layout sets, while
   `heightForColumn()` is `const` and writes `clipped_` through `mutable`.** Card
   geometry therefore depends on paint/layout ordering. `UX_CARDS.md` §7.4 raised
   this; it is unchanged. I did not produce a render where it misbehaves, so it is
   suspected, not confirmed.
2. **`view_->setEnabled(false)` on the missing-image label** should draw it in the
   Disabled colour group (≈2.7:1 on Breeze Light). My offscreen palette has no
   Disabled group, so it sampled at full `Text`. On a real Breeze desktop I expect
   the only content in that card to fail AA.
3. **`setUniformItemSizes(false)` with 5000 rows.** Paints in 2–5ms here, but
   `QListView` computes per-row layout without uniform sizes; a real compositor
   with variable row heights may behave worse than offscreen. SPEC §7 claims
   *"Virtualized from day one"*; `BufferListModel::reload()` loads all rows into
   memory, which is not the same thing.
4. **Animated-GIF hover playback in the list** (`QMovie`, `CacheNone`, restarted on
   every row change) — not exercised; a fast pointer sweep down a list of GIFs
   creates and destroys a `QMovie` per row.

---

# C. Document defects

### C.1 `SPEC.md` contains 290 duplicated lines specifying a design that was abandoned

```
## 5. Data model → ### Search      (line 298)   and again (line 587)
## 6. Lifecycle                    (line 334)   and again (line 623)
## 7. UI                           (line 382)   and again (line 671)
```

The first `## 7. UI` says **"Editing: a second pane"** and argues the two-pane
reversal. The second says **"Editing: inline expansion"** with
`double-click → expands into an editable region, list position frozen` — the v1
design the build explicitly abandoned. There is no marker between them. 22% of a
1315-line specification is a stale duplicate that contradicts the live text, and a
reader following the document in order arrives at the wrong UI second.

### C.2 `docs/UX_TWO_PANE.md` is not marked superseded and specifies a UI that does not exist

1038 lines. Its §3.2 (*"Click on a text block's text → enter text mode"*), its
§3.3 keyboard model (§1.1), its §3.5 focus model (§1.5) and its §8.2 `focus.ring`
are all contradicted by the build or absent from it. `UX_CARDS.md` §7.4 already
flagged §3.2 and asked for the document to be updated or marked superseded; it
was not. The repository currently contains two normative interaction specs, and
the longer and more detailed one describes a product that was never built.

### C.3 `docs/UX_CARDS.md` §7.2 states something false about the build

> *"The left pane renders a URL in `Highlight`; the canvas renders the same item
> as ordinary body text."*

Grepped: there is **no URL detection anywhere in `src/`**. `BufferCardDelegate`
paints the primary line in `QPalette::Text` unconditionally. Both panes render a
URL as plain text; the asymmetry the section describes does not exist. The
underlying point — that the link chip is promised in `SPEC.md` §3,
`src/domain/Item.h` line 8, `UX_TWO_PANE.md` §2.3 and acceptance criterion 3, and
delivered nowhere — stands, and is a real finding. The evidence given for it is
wrong.

### C.4 Dead and contradictory tokens in `Tokens.h`

| Token | State |
|---|---|
| `kRailOffset`, `kRailWidth`, `kSelectionBleed` | `UX_CARDS.md` §4 committed to deleting these; still present, now unreferenced by the card path |
| `kPadBottom = 120` | comment reads *"clickable dead space below the composer"*; there is no composer |
| `kRadiusCard = 6` vs `kCardRadius = 10` | two radii in one window; §1.4 asked for one family |
| `kCardMaxWidth = 460` | unreachable at every window width tested (§2.6) |
| `BufferCardDelegate::kMaxCardWidth = 760` | dead; the pane is capped at 520 |
| `kItemHover`, `kRailHover`, `kBorderSelected`, `kCardResting`, `kCardActive` | duplicated or unused after the rewrite |

---

# D. Recommended order

**Before anything else — they are cheap and they are correctness, not taste:**

1. Move `buildHeaderWidget()` after the action block, or build the menu lazily.
   One line; restores the entire shortcut-discovery story. §1.6
2. `CardFooter::setFocusPolicy(Qt::StrongFocus)` + `Space`/`Enter` activation +
   `QAccessible::Button` role. §1.2
3. `TextItemCard`: `setAccessibleName(first line)` /
   `setAccessibleDescription(…)`. §1.3
4. `kTextTertiary` 161 → 170; empty-state hint 130 → 170. §2.5
5. Selected border → `Highlight` blended toward `Text`, or `Text` α200 + a
   2px `Highlight` inner edge; whatever it is, measure it at ≥3:1 in Breeze
   Light. §2.4

**Then, in order of how much they change the product:**

6. Board keyboard model: arrow navigation over `cards_`, initial selection on
   entry, `Space` to select, focus ring on the focused card. §1.1, §1.4
7. Clipped cards: real fade (paint it in the *child's* `paintEvent`, or use a
   `QGraphicsOpacityEffect`, or draw the text yourself) **and** a full-height
   overlay on double-click. Stop swallowing the wheel. §2.1–2.3
8. List primary line ← the buffer's *first* item, frozen. Date sections. §3.2, §4
9. Row-major masonry fill, or date sections on the board. §5.1
10. Persist window geometry; open at ≥1000px. §2.8
11. `Ctrl+Z`; toast positioned over the board's bottom edge, not its centre;
    `Undo` styled as a button. §3.1
12. Reconcile the three documents: delete `SPEC.md`'s duplicate block, mark
    `UX_TWO_PANE.md` superseded, correct `UX_CARDS.md` §7.2, and annotate §18's
    acceptance criteria with the phase each one depends on. §C

---

## What the author's documents get right, and I am not relitigating

- Removing the gutter rail. Two nested boundaries were worse than one.
- No dashed "Item Preview" container. The reasoning in `UX_CARDS.md` §5 is
  correct and the four arguments are all good ones.
- Cards on `Base`, canvas on `Window`. It is only 1.11:1, but it is the right
  *kind* of signal and it costs nothing.
- Editing carries no fill tint. Correct: a wash behind text you are typing is a
  legibility cost for a signal the border already gives.
- Images never upscaled, centred, capped. Measured correct in every render.
- Copy sets the selection before copying. `UX_CARDS.md` §2.2's invariant is right
  and the build honours it (verified: clipboard held all 1959 chars, selection = 1).
- Pin and Keep as distinct drawn shapes with accessible labels. Verified at 8×.
- Performance at 5000 buffers: seed 93ms, window construct 4ms, first paint 2ms,
  scroll-to-row-4999 + paint 4ms, reload 5ms. Comfortably inside §12.
- `guarded()` around every DB-writing slot, and `currentBufferIsLive()`. The §19
  lessons hold.
