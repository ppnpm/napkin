# Item cards — an adversarial spec

Independent review. I did not write `SPEC.md` or `docs/UX_TWO_PANE.md` and I treat
both as claims. Everything below was measured: contrast from composited sRGB
luminance against the actual Breeze Light/Dark palette values, geometry from
pixel scans of `UI.png`, behaviour from renders of the real `MainWindow` built in
my own tree (`/tmp/rev`), never the project's `build/`.

## Renders

| File | What it is |
|---|---|
| `/tmp/shots/window_light.png`, `/tmp/shots/window_dark.png` | The **real build** (libs as of 07:04), 1280×860, buffer of 9 mixed items |
| `/tmp/shots/window_light_sel.png`, `/tmp/shots/window_dark_sel.png` | Same, with one card selected — the rail |
| `/tmp/shots/cards_light.png`, `/tmp/shots/cards_dark.png` | **Side by side**: A current build · B mockup taken literally · C proposal |
| `/tmp/shots/ablate_light.png`, `/tmp/shots/ablate_dark.png` | Six rows, **one variable per row**: canvas surface × border alpha × footer treatment |
| `/tmp/shots/board_light.png`, `/tmp/shots/board_dark.png` | The **whole proposed window** at 1280×860, left pane included |

Sources: `/tmp/rev_shot.cpp`, `/tmp/cards_mock.cpp`, `/tmp/ablate.cpp`, `/tmp/board.cpp`,
`/tmp/repro.cpp`. Rebuild with `-I/home/aragorn/Projects/code/napkin/src` and
`/tmp/rev/libnapkin_{ui,media,core}.a`.

---

## 0. Two bugs that matter more than the border

Before any of the styling: **the current build silently discards most of the text
in a text card.** This is almost certainly what the user is reacting to, and the
in-flight rewrite still carries it.

`TextItemCard::contentHeightForWidth` does:

```cpp
doc->setTextWidth(innerWidth);
const int lines = std::max(1, int(doc->size().height()));
return lines * edit_->fontMetrics().lineSpacing() + 2;
```

`QPlainTextDocumentLayout` **ignores `setTextWidth`** and reports its height in
*laid-out lines*, and a document that has never been shown has laid out nothing,
so every block counts as 1. Measured (`/tmp/repro.cpp`):

```
QPlainTextDocumentLayout: doc->size() = (1161.0, 1.0)   <- width unwrapped, height = 1 "line"
  -> ItemCard computes height = 22 px
QTextDocument (rich layout):  size() = (248.0, 105.0)   <- pixels, correct
```

**22px allocated for 105px of content.** In `/tmp/shots/window_light.png` three
separate paragraph cards each show exactly one line and the rest is gone. There is
no fade, because `clipped_` is false — the app does not know it truncated anything.

Fix: measure with a scratch `QTextDocument` (default rich layout), not with the
live `QPlainTextEdit`'s document. `setDefaultFont`, `setDocumentMargin(0)`,
`setPlainText`, `setTextWidth(inner)`, then `size().height()` is pixels and right.
This is what all four of my mock programs do.

Second: `TextItemCard`'s constructor calls `edit_->moveCursor(QTextCursor::End)`,
which scrolls the viewport to the bottom. A clipped card therefore shows its
**tail** with the head unreachable — the 40-line paste in the render starts at
"line 13" and lines 1–12 cannot be got at by any gesture. Drop the `moveCursor`
from construction; keep it in `focusText`/`beginEditing` where it belongs.

Third (pre-rewrite, may already be gone): `ImageItemCard::heightForColumn` capped
the image at `kCardMaxHeight - chrome` while `rescale()` capped it at
`kCardMaxHeight`. They disagree by exactly `chrome`, so the caption is painted on
top of the image — visible on the 400×700 card in `window_light.png`.

---

## 1. Card anatomy

### 1.1 The surface question comes first

The mockup's card fill and its canvas fill are **the same colour**. I scanned it:

| Sampled in `UI.png` | Value | Contrast vs white |
|---|---|---|
| Canvas region background | `#ffffff` | — |
| Card fill | `#ffffff` | 1.00:1 |
| Card border | `#eeeeee` (darkest edge sample `#e1e1e1`) | **1.16:1** (1.31:1) |
| Footer label / icon / timestamp | `#b3b3b3` | **2.10:1** ≈ `Text` α88 |
| Left-pane resting card border | `#f6f6f6` | 1.04:1 |

So in the mockup the card is carried by a single 1.16:1 hairline. That works in a
Figma export at 937px with perfect antialiasing. It will not work on a Breeze
desktop with subpixel rendering, and **it fails Napkin's own written floors** —
`Tokens.h` says 126 (3:1) is the "floor for a meaningful affordance" and 161
(4.51:1) the floor for text. The mockup's border is at ~α40 and its footer text at
~α88.

The in-flight `kCardBorder = 62` is the same mistake in project units: composited
over `Base` in Breeze Light it is **1.63:1**, against a rule the same file states
four lines above it. Meanwhile `BufferCardDelegate` draws the left-pane cards at
α128 — so the two halves of one window would be four stops apart.

Row 1 of `/tmp/shots/ablate_dark.png` shows what α62 costs in dark: the card edges
are essentially gone.

**The fix is not a darker border, it is a surface step.** Give the canvas
`QPalette::Window` and the cards `QPalette::Base`:

| Theme | Window | Base | Step |
|---|---|---|---|
| Breeze Light | `#eff0f1` | `#ffffff` | 1.14:1 |
| Breeze Dark | `#2a2e32` | `#1b1e20` | 1.27:1 |

1.14:1 is not enough on its own either — but surface step *plus* a compliant border
is, and neither has to shout. Rows 3 and 4 of the ablation isolate exactly this.
(The author has already made this change in flight; it is the single most
important one and it is right.)

### 1.2 Geometry — text card

All numbers at 1× with the app base font (10pt ≈ 13px) and canvas body at base+2pt
(≈16px, `lineSpacing` 20–21px).

| Property | Value | Note |
|---|---|---|
| Outer radius | **10** | list cards move 6 → 8; two radii in one window at 6 vs 10 is a mismatch |
| Border width, resting | **1px**, drawn at a 0.5px inset so it is not half-clipped | |
| Border colour, resting | `Text` **α128** light / **α108** dark | 3.42:1 vs the card's own fill, **3.00:1** vs the canvas behind it (light); 4.00:1 / 3.15:1 (dark) |
| Border colour, hover | `Text` α160 light / α140 dark | |
| Fill, resting | `QPalette::Base`, opaque | |
| Padding | **16** left / right / top | |
| Content width | `cardW − 32` | |
| Gap, content → footer | **10** | |
| Footer row height | **28** | |
| Padding, footer → card bottom | **10** | asymmetric on purpose: the footer is optically lighter than body text, so an equal 16 reads bottom-heavy |
| Divider above footer | **none** | see §2.3 |
| Gap between cards | **20** | up from the current 16 |
| Canvas padding | 32 x, 28 top | unchanged |

Total chrome, text card: `16 + 10 + 28 + 10 = 64px`.

### 1.3 Geometry — image card

Same shell. Content box is `cardW − 32` wide, image **inset by the padding** (the
user is right; the current build bleeds it to within 12px of the right edge and
38px of the left, which is neither inset nor full-bleed).

| Property | Value |
|---|---|
| Image box | `cardW − 32` wide, max **333** tall (= 420 − 64 chrome − 17 caption − 6 gap) |
| Scaling | `KeepAspectRatio`, `SmoothTransformation`, **never upscaled** |
| Horizontal alignment | **centred** in the content box |
| Image radius | 6, clipped |
| Image edge | 1px `Text` **α36** inside the clip, so a white screenshot has an edge |
| Gap, image → caption | 6 |
| Caption | base − 1.5pt, `Text` **α161**, one line, left-aligned to the content box |

**I disagree with `UX_TWO_PANE.md` §2.4 on alignment.** That document argues narrow
images should left-align to "produce one strong left edge for everything textual."
That argument was written for a single 780px column where the image shared the
text's left edge. Inside a card the *card's* edge is the strong edge, and a 190px
image hugging the left of a 428px card with 206px of dead fill to its right reads
as a layout bug. Centre it. `/tmp/shots/board_light.png` has a 120×90 in a 280px
card; centred it reads as deliberate.

### 1.4 Reusing `Tokens.h`

Keep: `kTextPrimary 255`, `kTextSecondary 170`, `kTextTertiary 161`, `kHairline 36`,
`kBorderEditing 200`, `kGapTight 8`, `kPadX 32`, `kPadTop 28`.

Change:

| Token | Now | Proposed | Why |
|---|---|---|---|
| `kCardBorder` | 62 | **128** (light) / **108** (dark) | 1.63:1 → 3.00:1 at the binding edge; matches the left pane's existing 128 |
| `kCardBorderHover` | 104 | **160** / **140** | hover must be visibly more than resting, and 104 is below the resting value I'm proposing |
| `kCardRadius` | 10 | 10 | keep |
| `kRadiusCard` (list) | 6 | **8** | one radius family |
| `kCardPad` | 16 | 16 | keep |
| `kCardFooterH` | 30 | **28** | 7×4; 28 fits a 13px glyph + 11px label with 7px of air |
| `kCardMinHeight` | 92 | **88** | see §3 |
| canvas item gap | 16 (`kGapTight*2`) | **20** | with borders, 16 makes adjacent edges read as a 2px rule |
| `kRailOffset`, `kRailWidth`, `kSelectionBleed` | 26/3/12 | **delete** | see §4 |

Delete rather than reuse: `kCardResting`/`kCardActive` are `BufferCardDelegate`'s
private duplicates of the same idea under different names. One name per concept.

---

## 2. The footer

### 2.1 Commitment: yes, always visible, with the label

I went in expecting to argue the label away as repeated clutter and to keep only a
hover-revealed icon. **The ablation talked me out of it.** Rows 4, 5 and 6 of
`/tmp/shots/ablate_light.png` are identical except for the footer:

- Row 6 (no footer): the one-word card is an 88px box containing "ok" and 60px of
  nothing. The minimum height looks arbitrary because nothing occupies it.
- Row 5 (icon only): the icon sits orphaned at the bottom-left with an unexplained
  13px mark and a timestamp 250px away. It reads as a rendering artefact.
- Row 4 (icon + label): the row has two anchored ends and a reason to exist. The
  repetition of "Copy text" across three cards is not loud at α161 — it is a
  baseline, and the eye stops reading a repeated string after the second card.

So: **icon + label, always visible, never hover-only.** Hover-only fails touch,
fails keyboard, fails screenshots, and fails the person who does not yet know the
control is there — which in a copy-paste utility is the person you cannot afford
to fail.

The footer also earns the minimum height (§3) and is the only place per-type
affordances can live without becoming hover overlays (§2.4).

### 2.2 Resolving Copy-button vs Ctrl+C

These are not two mechanisms for one job. They are one mechanism at two scopes,
and the way to keep that true is an invariant:

> **After any copy, the clipboard's contents correspond to what is visibly selected.**

Therefore: **clicking a card's Copy sets the selection to that card, then copies.**
One click, one visible result, one clipboard.

The in-flight implementation in `ItemCanvas::addCard` does the opposite:

```cpp
const auto keep = selected_;
selected_ = {id};
copySelection();
selected_ = keep;          // <- selection restored, clipboard is something else
```

Click Copy on card X while Y and Z are selected and you get: Y and Z still drawn
as selected, clipboard holding X. The next Ctrl+V pastes something the screen is
not showing you. That is a trap, not a convenience. Drop the save/restore.

Rest of the model stays as built and as documented: single click selects, double
click edits text / opens an image, `Ctrl`/`Shift` extend, `Ctrl+C` copies the
selection (which may be many cards; the button never can). `Esc` ladder unchanged.

### 2.3 No divider

A 1px `Text` α36 rule above the footer would add a second horizontal line 28px from
the card's own bottom border. Two parallel hairlines that close together read as a
double rule, and the 10px gap plus the alpha drop from 255 to 161 already separates
content from chrome. The mockup has no divider either; on this it is right.

### 2.4 Per type

| Item type | Left slot | Right slot |
|---|---|---|
| Text | `[copy] Copy text` | relative timestamp |
| Image | `[copy] Copy image` | relative timestamp |
| Link (text whose trimmed content is exactly one URL) | `[open] Open` · `[copy] Copy link` | relative timestamp |
| Code snippet (future) | `[copy] Copy code` | `bash` · timestamp |

The link row is the one case that justifies two actions: the reason to keep a URL
is to open it later, and "select the card, press Enter, hope" is not an affordance.
Cap the row at two actions; a third means the type needs a context menu, not a
wider footer.

**The footer is the extension point, and that is an argument for having one.** A
card with no footer has nowhere to put a per-type affordance, so every future one
becomes a hover overlay floating on the content — which is the pattern this design
is trying to get away from.

### 2.5 The timestamp is not decoration

It is the only thing that makes the board's order recoverable. See §7.1.

**I disagree with the mockup's timestamp colour.** `#b3b3b3` is 2.10:1. Napkin's
whole organizing principle is recency, and the mockup renders the recency signal as
the least legible text on screen. `Text` α161 (4.51:1). Same for the label and the
icon — the icon is the control's identifier, so 3:1 is its floor and α161 clears it
with room.

---

## 3. Sizing

| | Text card | Image card |
|---|---|---|
| Min width | **280** (column min) | 280 |
| Max width | **460** (was 400) | 460 |
| Min height | **88** | 88 |
| Max height | **420** | 420 |
| Max content height | 420 − 64 = **356** | 356 − 17 caption − 6 = **333** |

### 3.1 Why 88 and not 160

`16 + 21 (one body line) + 10 + 28 + 10 = 85`, rounded to 88 on the 4px grid.

A card holding "ok" should be **exactly one line tall plus its chrome**. Padding it
to 160 to look "balanced" makes the board lie about how much is in it, and on a
surface whose value is seeing many things at once, every padded pixel costs you an
item. The min height is a floor for degenerate cases — empty text, a zero-height
content widget — not a design target.

The user asked for a minimum height, and 88 *is* the answer they want, even though
the number is small: in the current build "ok" renders as ~40px of bare text with
no border and no footer, which is why it looks like stray text rather than a card
(`/tmp/shots/window_light.png`, middle column). Border + fill + footer at 88px is a
card. Compare row 6 vs row 4 of the ablation.

### 3.2 The 40-line paste

At a 280px column, `cmake --build build -j8 && ctest --output-on-failure` wraps to
two visual lines. 40 logical lines become ~80 visual lines ≈ 1680px, clipped to 356
of content ≈ 8 logical lines, with a 28px fade to `Base` at the bottom.

**This is where I most strongly disagree with the author's own spec.**
`UX_TWO_PANE.md` §2.2 sets the text column at 780px and argues it explicitly:

> "chosen because Napkin holds pasted logs and shell commands as often as prose, and
> wrapping a command line is worse than a slightly long measure."

The build then shipped 280–400px — 32 to 48 characters — which wraps every command
line in the corpus the argument was about. One of those two documents is wrong and
they are by the same author. The masonry is the right layout (a three-word note
does not deserve a 780px row) but the measure that came with it silently reverses a
stated decision.

Partial fix, committed: **max width 460** (from 400). At 1600px the canvas gives 4
columns × 284; at 1440, 3 × 332; at 1280, 2 × 428 or 3 × 280 depending on the left
pane. 460 lets a two-column layout on a normal window reach ~55 characters instead
of ~48.

Not fixed, flagged: **there is no card width at which a 40-line log is readable and
a three-word note is not absurd.** Options, none free:

1. Accept it. A clipped log card is a *recognition* target, not a reading target —
   you open it to read. Cheapest, and what I would ship first.
2. Let a card span 2 columns when its longest line exceeds the measure. Still a
   pure function of the item's own content, so it does not violate "a block's size
   must not depend on other items" — but it complicates `MasonryLayout` a lot.
3. Render content whose longest line exceeds the measure in the monospace UI font
   with `NoWrap` and a right-edge fade, preserving the *shape* of the log. Cheap,
   and honest about the card being a preview, but it hides content horizontally as
   well as vertically.

I recommend 1 now, 3 later. I do not recommend 2.

### 3.3 400×700 portrait next to 900×520 landscape

At a 428px column (content box 396):

- **900×520** → 396×229. Card height `16 + 229 + 6 + 17 + 64 = 332`.
- **400×700** → 693 at full width, over the 333 cap, so **190×333** centred in a
  396px box. Card height `16 + 333 + 6 + 17 + 64 = 436` → clamped to **420**.

They sit side by side at 332 and 420. That is exactly what masonry is for, and
neither is cropped or stretched. The portrait loses 206px of horizontal box to
centred whitespace; that is the correct trade against upscaling or letterboxing.

### 3.4 "A block's size must not depend on other items" — one real violation

`heightForColumn(width)` is a pure function of the item and the column width, and
the column width is a pure function of the viewport width. Good.

But `ItemCanvas` sets `setHorizontalScrollBarPolicy(AlwaysOff)` and leaves the
vertical bar on `AsNeeded`. Adding one item can push the board past the viewport
height → the vertical scrollbar appears → the viewport loses ~14px → the column
width changes → **every card in the buffer resizes.** That is literally a block's
size depending on other items, and it is almost certainly what the user noticed.

Fix: reserve the scrollbar width unconditionally — `ScrollBarAlwaysOn`, or subtract
`style()->pixelMetric(PM_ScrollBarExtent)` from the width handed to the layout
whether or not the bar is up. My `/tmp/board.cpp` reserves 14px; `window_light.png`
does not, which is why its 3 columns are 292px and mine are 280px.

Related fragility, same cause: `MasonryLayout::columnCount` at 1280px with a 320px
list pane gives `(881 + 20) / (280 + 20) = 3` columns of exactly 280 — the minimum.
Widen the list pane to 340 and it drops to 2 columns of 420. **The column count at
the most common window size is decided by ±20px of unrelated chrome.** Not a bug to
fix so much as a number to choose on purpose and then test at 1280, 1366, 1440,
1600 and 1920.

Also: `MasonryLayout`'s own defaults (`minColumn_ 280`, `maxColumn_ 420`, `gap_ 16`)
disagree with `Tokens.h` (280 / 400 / 24). Two sources of truth for three numbers.

---

## 4. Selected / hover / editing — kill the rail

**Commit: remove the gutter rail entirely.** `kRailOffset`, `kRailWidth`,
`kSelectionBleed`.

1. Its stated justification is gone. `ItemCard.cpp`'s own comment: *"What makes a
   borderless block read as an object is the gutter rail."* The premise was
   borderlessness.
2. It costs 26px of every card. At a 280px column that is 9% of the measure spent
   on permanently empty space, and it is why the current build's text starts 38px
   from the card's left edge while nothing aligns to anything.
3. Two nested boundaries. `/tmp/shots/window_light_sel.png`: a detached 3px blue bar
   floats 26px to the left of a blue-bordered card. It reads as a stray line.
4. Border + fill is already two signals, satisfying the project's own "every state
   changes exactly two things" rule without a third element.

SPEC §14 says focus is never signalled by colour alone. A hue change at constant
width would breach that, so the **width** carries the non-colour signal:

| State | Border | Fill | Other |
|---|---|---|---|
| Rest | `Text` α128 / α108, **1px** | `Base` | — |
| Hover | `Text` α160 / α140, **1px** | `Base` | cursor |
| Selected | `Highlight` α200, **2px**, drawn at a 1px inset | `Base` + `Highlight` α20 light / α34 dark | — |
| Editing | `Highlight` α200, **2px** inset | `Base`, **no tint** | caret |
| Selected, window inactive | `Text` α178, 2px inset | `Base` + `Text` α14 | — |

Notes:

- Draw the 2px selected border **inset by 1px** (`rect().adjusted(1,1,-1,-1)`), not
  outset. The card's outer geometry never changes, so nothing reflows and the card
  does not appear to grow when you click it. The in-flight `1.6px` is worse than
  either 1 or 2: on a 1× display it renders as a smeared 2px.
- Editing keeps no tint. The author's reasoning here is right and I am not
  relitigating it: a wash behind text you are typing degrades it, and the 2px
  border plus a live caret are two signals already.
- Multi-selection uses the same treatment on every member. There is no "anchor"
  styling; the anchor is a keyboard concept, not a visual one.

`/tmp/shots/board_light.png` — the 900×520 card is selected. No rail, and it is
unambiguous at a glance in both themes.

---

## 5. The dashed "Item Preview" container — no

**It does not earn its place.** The author was right to leave it out.

1. The caption reads *"Item Preview"*. That is a Figma frame name, not a word any
   user needs. Nobody has ever needed to be told that the rectangle containing the
   items is the items rectangle. Its appearance in both mockups is the same frame
   in the same file, not two independent design decisions.
2. It is the same visual weight as the card hairlines it contains, so it reads as a
   tenth card wrapping the nine. Once the cards have real borders, this gets worse,
   not better.
3. It costs double padding — 28px of canvas padding, then the dash, then the cards'
   own inset — for zero information.
4. A dashed rectangle has a meaning in every other UI on the desktop: *drop target*
   or *empty placeholder*. Drawing one permanently around a full board spends that
   meaning on nothing, and Napkin actually has a drop target (paste / drag-in) that
   will want it later.

**What communicates "this is the canvas region" instead**, all of it either free or
already built:

- The surface step from §1.1 — `Window` behind, `Base` cards. This is the whole job.
  Compare `/tmp/shots/board_light.png` against `/tmp/shots/window_light.png`: the
  canvas region is obvious in the former with no boundary drawn at all.
- The existing splitter, promoted to a visible 1px `Text` α36 rule on the canvas's
  leading edge.
- The empty-buffer placeholder, which already exists and already says the right
  thing.

If the user insists on a boundary after seeing the surface step: a 1px **solid**
α36 rule on the canvas's left edge only. Never dashed, never a caption.

---

## 6. The left pane

The build's row is noisier than the mockup's; the mockup's is too plain. Both can
be seen in `/tmp/shots/window_light.png` (build) and `/tmp/shots/board_light.png`
(proposal).

**Cut:**

- **Three thumbnails → one, 40×40.** In the render, three 38px thumbs plus gaps
  consume 130px of a 260px content width, squeezing the primary line into a sliver
  that elides to `"This is some of the mult…"`. The *only* string that identifies a
  buffer is destroyed so three colour swatches can be shown. One thumbnail says
  "this one has pictures", which is all the strip was ever saying.
- **The overflow `+N` chip.** Gone with the strip.
- **The secondary line.** In the render it is empty on 3 of 4 rows and duplicates
  the primary on the 4th. 21px per row for nothing.
- **`kMaxCardWidth = 760` and its centring branch** in `BufferCardDelegate::cardRect`.
  The list pane is ~320px wide now; 760 is dead code from the single-pane design and
  the centring branch can never fire.

**Keep:**

- **The relative timestamp.** The mockup drops it; the mockup is wrong. It is the
  list's sort key made visible, in an app organized around recency. α161.
- **Pin and Keep glyphs.** State you cannot infer from anything else, 13px, drawn
  only when set, distinct shapes (SPEC §14). Not noise.
- **The section header** ("RECENT").

**Merge:** item count and timestamp onto one tertiary line — `9 items · just now` —
instead of two rows. Three text rows become two.

Resulting row: `[40px thumb?]  primary (Medium, elided)  /  "N items · just now" (α161)`,
height `16 + 20 + 2 + 18 + 16 = 72` against the build's ~96. **Twenty-five percent
more buffers visible per screen**, and the primary line stops being eaten.

---

## 7. What else is wrong when you look at it

### 7.1 The board's reading order is unrecoverable — hard problem

`MasonryLayout` places each card into the currently shortest column. With 3 columns
that means item 1 → col 1, item 2 → col 2, item 3 → col 3, item 4 → wherever. So
"newest first" puts the newest at top-left and the second-newest at **top-middle**,
and after four items nobody can reconstruct the order. In `window_light.png` the
oldest item ("ok") sits mid-board while newer ones are below it.

Column-balanced masonry and a strict recency order are genuinely in tension and I
do not have a free answer:

- Row-major fill preserves order but produces ragged column bottoms and wastes the
  space masonry exists to reclaim.
- Sectioning by time (Today / Yesterday / Older) preserves *coarse* order and reads
  well, but adds chrome the design has been avoiding.
- Doing nothing is survivable **only if every card carries its own timestamp** — the
  order stops being positional and becomes readable per card.

That is the strongest single argument for §2's footer timestamp, and it arrived
from the layout rather than from the mockup.

### 7.2 Link items are styled in the list and not on the canvas

`SPEC.md` §3 and `UX_TWO_PANE.md` §2.3 both promise a link chip. The left pane
renders a URL in `Highlight`; the canvas renders the same item as ordinary body
text and wraps it mid-path. One item, two treatments, in one window. Either build
the chip (`Highlight` text, α140 underline, `Open` + `Copy link` in the footer, elide
rather than wrap) or delete the promise from both documents.

### 7.3 Editing a clipped card is broken — hard problem

Double-clicking a 40-line card starts inline editing inside a 420px viewport with
`ScrollBarAlwaysOff`. The caret moves to text you cannot see and cannot scroll to.

Growing the card while you type is worse: the masonry reflows on every keystroke,
which *is* "a block's size depends on other items" from the user's point of view.

My call: **a clipped card double-clicks into a full-height overlay**, the same
mechanism `Lightbox` already provides for images. Big content opens big, for both
item types, which is at least consistent. It costs a new widget and I am flagging
it as unresolved rather than claiming the cheap version works.

### 7.4 Smaller

- `MasonryLayout` defaults (280/420/16) contradict `Tokens.h` (280/400/24).
- The canvas never shows a scrollbar, so nothing indicates the board continues
  below the fold. Reserving the bar (§3.4) fixes both problems at once.
- The `…` menu button in the toolbar is an unlabelled glyph with a second unlabelled
  arrow beside it; at 1280 there is room for a word.
- `clipped_` is `mutable` and written from a `const` method during layout, and
  `ImageItemCard::rescale()` reads `width()`, which the layout sets. Card height
  therefore depends on paint order. It happens to work; it is not a property the
  code guarantees.
- `docs/UX_TWO_PANE.md` §3.2 says *"Single click on text enters text mode, not
  object selection"* and calls it "the central call". `ItemCard.h` says the opposite
  and the build implements the opposite. The build is right — uniform selection is
  what makes "click it, then delete it" work on text as it does on an image — but
  the document still argues for the design that was abandoned. It should be updated
  or marked superseded rather than left as a spec someone might implement.

---

## 8. Summary of what to change

1. Measure text with a scratch `QTextDocument`, not `QPlainTextDocumentLayout`. **§0**
2. Remove `moveCursor(End)` from `TextItemCard`'s constructor. **§0**
3. Canvas on `Window`, cards on `Base`. **§1.1** *(already in flight — right call)*
4. `kCardBorder` 62 → 128 light / 108 dark; hover 104 → 160 / 140. **§1.4**
5. Delete the rail: `kRailOffset`, `kRailWidth`, `kSelectionBleed`. **§4**
6. Selected = `Highlight` α200 at **2px inset**, plus α20/α34 tint. **§4**
7. Footer: icon + label + timestamp, 28px, always visible, no divider. **§2**
8. Copy button **sets** the selection before copying; delete the save/restore. **§2.2**
9. Image inset by the padding, centred, capped at 333. **§1.3**
10. `kCardMinHeight` 88, `kCardMaxWidth` 460, item gap 20, list radius 8. **§1.4, §3**
11. Reserve the scrollbar width unconditionally. **§3.4**
12. Left pane: one thumbnail, drop the secondary line, merge count and time. **§6**
13. No dashed container, no "Item Preview" caption. **§5**

Open, not papered over: reading order in a balanced masonry (§7.1), editing content
taller than the cap (§7.3), and a column measure that serves both prose and pasted
logs (§3.2).
