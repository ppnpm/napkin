> **SUPERSEDED in part.** This document designed the two-pane layout, which
> shipped. Its *keyboard model* (§3.3), *focus model* (§3.5) and
> single-click-enters-text-mode semantics (§3.2) were **not** built — the
> shipped app uses click-to-select / double-click-to-edit, and the canvas
> has no keyboard selection at all, which a later audit recorded as a
> critical gap. Read SPEC.md for what exists; read this for why the layout
> is what it is.

# Napkin — Two-Pane Layout: interaction and visual specification

> Status: design spec, ready to build. Supersedes SPEC.md §7's "inline expansion"
> decision. Where it contradicts SPEC.md it says so and argues it — see §10.
>
> Every number here is a decision, not a suggestion. Where a number is a guess,
> it says so. Where something is genuinely hard, §11 names it instead of
> smoothing it over.
>
> Reference renders (throwaway, regenerable from `/tmp/napkin-mock/mock3.cpp`):
> `/tmp/napkin-mock/m900.png`, `/tmp/napkin-mock/m1280F.png`,
> `/tmp/napkin-mock/m1600_tall.png`.

---

## 0. The decision in one paragraph

The buffer list becomes a fixed left rail. The right pane — the **item canvas** —
renders the selected buffer's items at full size: text blocks you can click into
and type, images sized to the reading column instead of squeezed into a 52px
thumbnail. Items are first-class selectable objects, so Copy, Cut and Delete
operate on *items*, not only on buffers. Everything the earlier review defended —
PINNED/RECENT, relative timestamps, thumbnails, pin/keep glyphs, one row per
buffer — survives intact in the left rail. What dies is inline expansion, and
with it the frozen-sort hack, the expanded-height plumbing, and the
thumbnail-with-a-metadata-row image widget the user correctly called ugly.

---

## 1. Layout

### 1.1 Window and pane geometry

| Thing | Value |
|---|---|
| Window minimum | 560 × 420 |
| Window default (first run) | 1180 × 760 |
| Header height | 52 |
| Divider | 1 px rule; `QSplitter::handleWidth` = 9 (hit target), rule painted centred |
| Left pane stored width `S` | default **360**, persisted in `QSettings` as `ui/listPaneWidth` |
| Left pane effective width | `clamp(S, 280, max(280, W − 520))` |
| Left pane drag limits | 280 … 480 |
| Canvas minimum | 520 |
| Single-pane threshold | window width **< 800** |

One persisted number. The left pane does **not** rescale with the window: a list
rail that grows as you widen the window is a list rail spending your new pixels
on whitespace. New width goes to the canvas, which is what the content is in.

Resulting geometry at the three reference widths (`S` = 360, window height 820):

| W | left | canvas | canvas content (−64) | text column | image column |
|---|---|---|---|---|---|
| 900 | 360 | 539 | 475 | 475 | 475 |
| 1280 | 360 | 919 | 855 | 780 | 780 |
| 1600 | 360 | 1239 | 1175 | 780 | 1040 |

### 1.2 The header spans the window and is split by the same divider

The header is one 52px band with a 1px bottom rule, divided at the same x as the
panes, so the divider is a single unbroken vertical line from y=0 to the bottom
of the window.

- **Left header cell** (width = left pane): the search field, 32px tall, 16px
  side margins, radius 6. It filters the list, so it sits over the list.
- **Right header cell** (width = canvas): on the left, the selected buffer's
  meta line — `4 items · 2 images · 12 minutes ago` at 12px / alpha 170. On the
  right, `+ New`, `⋯`, `Trash`.

**The `Napkin` wordmark is removed from the header.** It occupied the most
valuable 70px in the application to tell the user the name of the window they are
looking at, which the window title bar and the task switcher already say. See §9.

### 1.3 Wireframes

Proportions below are to scale. `│` at column N is the divider.

#### 900 px — two panes, minimum comfortable

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│ ⌕ Search                        │ 4 items · 2 images · 12 min ago    + New  ⋯  Trash │ 52
├─────────────────────────────────┼───────────────────────────────────────────────────┤
│ PINNED                          │  ┃ Phase 0 finding — Wayland serves the           │
│ ┌─────────────────────────────┐ │  ┃ clipboard only to a focused client.            │
│ │[▦] ssh -J bastion deploy@ 📌🔖│ │                                                   │
│ │    command · 3 items        │ │  ┌───────────────────────────────────────────┐    │
│ │    2 days ago               │ │  │                                           │    │
│ └─────────────────────────────┘ │  │        image, fit to column (475)         │    │
│ ┌─────────────────────────────┐ │  │                                           │    │
│ │ Quarterly plan — the      📌│ │  └───────────────────────────────────────────┘    │
│ │ 6 items                     │ │  wayland-clipboard.png · 2560×1440 · 1.4 MB       │
│ │ 5 days ago                  │ │                                                   │
│ └─────────────────────────────┘ │  ┃ So a background hotkey handler cannot          │
│ RECENT                          │  ┃ silently read the clipboard.                   │
│ ▌─────────────────────────────┐ │                                                   │
│ ▌[▦][▦] Wayland clipboard on…│ │  Write something…                                 │
│ ▌       4 items · 2 images    │ │                                                   │
│ ▌       12 minutes ago        │ │                                                   │
│ └─────────────────────────────┘ │                                                   │
│  … 32 px left/right in canvas   │                                                   │
└─────────────────────────────────┴───────────────────────────────────────────────────┘
  ◄────────── 360 ──────────────►│◄──────────────────── 539 ────────────────────────►
                                 1
```

At 900, text column == image column == 475. Everything in the canvas shares one
left edge. The list primary line elides hard; that is the cost of a 900px window
and it is acceptable because the canvas shows the whole thing.

#### 1280 px — the design target

```
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│ ⌕ Search                  │ 4 items · 2 images · 12 minutes ago           + New  ⋯  Trash │ 52
├───────────────────────────┼───────────────────────────────────────────────────────────────┤
│ PINNED                    │      ┃ Phase 0 finding — Wayland serves the clipboard only    │
│ ┌───────────────────────┐ │      ┃ to a focused client. A Qt client with no activated     │
│ │[▦] ssh -J bastion   📌🔖│ │      ┃ window reads an empty format list.                     │
│ │    command · 3 items  │ │                                                                │
│ │    2 days ago         │ │      ┌────────────────────────────────────────────────┐       │
│ └───────────────────────┘ │      │                                                │       │
│ ┌───────────────────────┐ │      │          image, fit to column (780)            │       │
│ │ Quarterly plan      📌│ │      │                                                │       │
│ │ 6 items · 5 days ago  │ │      └────────────────────────────────────────────────┘       │
│ └───────────────────────┘ │      wayland-clipboard.png · 2560×1440 · 1.4 MB               │
│ RECENT                    │                                                                │
│ ▌───────────────────────┐ │      ┃ So a background hotkey handler cannot silently read    │
│ ▌[▦][▦] Wayland clip…  │ │      ┃ the clipboard.                                        │
│ ▌ 4 items · 2 images    │ │                                                                │
│ ▌ 12 minutes ago        │ │      Write something…                                          │
│ └───────────────────────┘ │                                                                │
│ ┌───────────────────────┐ │                                                                │
│ │ https://doc.qt.io/… 🔖│ │                                                                │
│ └───────────────────────┘ │                                                                │
└───────────────────────────┴───────────────────────────────────────────────────────────────┘
  ◄────── 360 ──────────►  │◄──── 70 ──►◄────── 780 text ──────►◄──── 70 ──►
```

At 1280 the canvas content (855) is less than text column + 120, so the image
column stays at 780: one left edge for text, images and captions. Nothing bleeds.

#### 1600 px — images get room

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ⌕ Search               │ 4 items · 2 images · 12 minutes ago                    + New  ⋯  Trash │ 52
├────────────────────────┼────────────────────────────────────────────────────────────────────────┤
│ PINNED                 │            ┃ Phase 0 finding — Wayland serves the clipboard only to a  │
│ ┌────────────────────┐ │            ┃ focused client. A Qt client with no activated window       │
│ │[▦] ssh -J bas…  📌🔖│ │            ┃ reads an empty format list; the same client with a shown,  │
│ │    command · 3 items│ │            ┃ activated window reads image/png fine.                    │
│ │    2 days ago      │ │                                                                        │
│ └────────────────────┘ │      ┌────────────────────────────────────────────────────────┐        │
│ ┌────────────────────┐ │      │                                                        │        │
│ │ Quarterly plan  📌 │ │      │      image bleeds to 1040, centred on the column        │        │
│ │ 6 items            │ │      │      height capped at 560                              │        │
│ └────────────────────┘ │      └────────────────────────────────────────────────────────┘        │
│ RECENT                 │      wayland-clipboard.png · 2560×1440 · 1.4 MB                        │
│ ▌────────────────────┐ │                                                                        │
│ ▌[▦][▦] Wayland cl…│ │            ┃ So a background hotkey handler cannot silently read the   │
│ ▌ 4 items · 2 images │ │            ┃ clipboard.                                               │
│ ▌ 12 minutes ago     │ │                                                                        │
│ └────────────────────┘ │            ┌────────────────┐                                          │
│                        │            │ small image,   │  ← narrower than the text column:        │
│                        │            │ 420 natural    │    left-aligned to the text edge         │
│                        │            └────────────────┘                                          │
│                        │            crop.png · 420×300 · 61 KB                                   │
│                        │                                                                        │
│                        │            Write something…                                            │
└────────────────────────┴────────────────────────────────────────────────────────────────────────┘
  ◄──── 360 ────────►   │◄── 130 ──►◄─────── 780 text ────────►◄── 130 ──►
                        │◄─ 100 ─►◄──────────── 1040 image ──────────►◄─ 100 ─►
```

#### < 800 px — single pane

Below 800 the splitter collapses and the window shows **one** pane at a time.

```
┌───────────────────────────────┐    ┌───────────────────────────────┐
│ ⌕ Search        + New ⋯ Trash │    │ ‹ Buffers   4 items · 12m ago │
├───────────────────────────────┤    ├───────────────────────────────┤
│ PINNED                        │    │  ┃ Phase 0 finding — Wayland  │
│ ┌───────────────────────────┐ │    │  ┃ serves the clipboard only  │
│ │[▦] ssh -J bastion…    📌🔖│ │ →  │                               │
│ │    command · 3 items      │ │    │  ┌─────────────────────────┐  │
│ │    2 days ago             │ │    │  │        image            │  │
│ └───────────────────────────┘ │    │  └─────────────────────────┘  │
│  …                            │    │  Write something…             │
└───────────────────────────────┘    └───────────────────────────────┘
        list view                           canvas view
```

- Opening a buffer (click / `Enter`) pushes to the canvas view.
- `‹ Buffers` or `Esc` (with no canvas selection and no text focus) pops back.
- Crossing 800px in either direction restores the two-pane layout and keeps the
  same selected buffer. No state is lost across the transition.

This is the only place Napkin has a navigation model, and it exists only in
window sizes where two panes would be worse.

---

## 2. The item canvas

### 2.1 Structure

A **single vertical flow**, one item per row, in `items.position` order. Not a
grid, not a masonry, no pairing of adjacent images.

Why not a grid: `position` is in the schema and is meaningful — a buffer is "a
screenshot *and* a command *and* a URL kept together," and that is a sequence,
not a set. A grid also makes text blocks of wildly differing heights look broken,
and forces a decision about what "a cell" means for a 3-line note next to a
2560×1440 screenshot. Vertical flow needs no such decision.

### 2.2 Canvas metrics

| Token | Value |
|---|---|
| `canvas.padX` | 32 |
| `canvas.padTop` | 28 |
| `canvas.padBottom` | 120 (dead space below the composer, clickable to focus it) |
| `canvas.itemGap` | 24 |
| `canvas.textWidth` | `min(780, canvasW − 64)` |
| `canvas.imageWidth` | `canvasContent − textWidth ≥ 120 ? min(1040, canvasContent) : textWidth` |
| `canvas.imageMaxHeight` | 560 |
| `canvas.background` | `QPalette::Base` |

The bleed is **quantised**: an image either matches the text column exactly, or it
is at least 120px wider. A 37px bleed reads as a misalignment; a 130px bleed reads
as a decision. This is why nothing bleeds at 1280 and everything does at 1600.

780px at 16px type is ~92 characters — at the wide end of comfortable, chosen
because Napkin holds pasted logs and shell commands as often as prose, and
wrapping a command line is worse than a slightly long measure.

### 2.3 Text blocks

Resting state has **no border, no fill, no background**. The block is just text on
the paper. This is the whole point: a text item must not look like a form field,
because it is not one — it is the content.

| Geometry | Value |
|---|---|
| Font | `canvas.body`: base + 2pt (≈16px), weight 400, line-height **1.5** |
| Colour | `QPalette::Text`, alpha 255 |
| Text left edge | `textL` (the column) |
| Selection/hover rect | `textL − 12, w + 24`, i.e. it bleeds 12px outward so the *text* never moves between states |
| Rect vertical padding | 10 top, 10 bottom |
| Rect radius | 5 |
| Gutter rail | 3px wide, radius 1.5, at `textL − 26`, inset 3px from the rect's top and bottom |

The **gutter rail** is the affordance that makes a borderless block read as an
object. It is empty at rest, appears on hover, and is solid when the block is
selected or being edited. Nothing else in the canvas uses that gutter, so the rail
has no competition.

State table — each state changes exactly two things, never three:

| State | Rail | Rect fill | Rect border |
|---|---|---|---|
| Rest | — | — | — |
| Hover | `Text` α90 | — | `Text` α60, 1px |
| Selected (object) | `Highlight` α255 | `Highlight` α26 light / α44 dark | — |
| Selected + keyboard focus | `Highlight` α255 | same fill | `Highlight` α160, 1px |
| Editing (caret inside) | `Highlight` α255 | **none** | `Highlight` α200, 1px |
| Multi-selected, not focused | `Highlight` α180 | same fill | — |

Editing has **no fill**, deliberately: a wash behind text you are actively reading
and typing degrades it. The border plus the rail plus the caret are three
independent signals already.

A text item whose trimmed content is exactly one URL renders as a link chip
(SPEC.md §3) — same block geometry, text in `Highlight` with a 1px underline at
α140, and Open / Copy appearing in the gutter on hover. Unchanged from spec.

### 2.4 Images

```
drawW = min(naturalW, canvas.imageWidth)
drawH = drawW * naturalH / naturalW
if (drawH > 560) { drawH = 560; drawW = round(560 * naturalW / naturalH) }
imgL  = (drawW <= textWidth) ? textL : canvasL + (canvasW - drawW) / 2
```

- **Never upscaled.** A 200×140 favicon draws at 200×140, not stretched to 780.
  Upscaling a small image to fill a column is the single fastest way to make a
  UI look cheap.
- **Narrow images left-align to the text column; wide images centre and bleed.**
  Verified visually: this produces one strong left edge for everything textual,
  with the bleeding image as the only deliberate exception. Symmetric bleed means
  no clipping at any width.
- **Height cap 560.** A 1080×2400 phone screenshot would otherwise be 1700px tall
  and own the entire canvas. Capped, it is 252×560 — still far better than the
  52px thumbnail it replaces — and the lightbox handles genuine 1:1 inspection.
- Radius 4, clipped. 1px border `Text` α36 so a white-background screenshot has an
  edge against `Base`. **No shadow, no ring, no lift.**
- Scaling uses `Qt::SmoothTransformation`; decode is size-aware
  (`QImageReader::setScaledSize`) so a 4000×3000 photo never lands in memory whole
  — the existing `Thumbnailer` policy, applied at canvas size rather than 96px.

**Caption**, always present, one line, 8px below the image, flush with the
image's left edge:

```
wayland-clipboard.png · 2560×1440 · 1.4 MB
PNG · 2560×1440 · 1.4 MB                      ← pasted, no source_name
demo.gif · GIF · 480×270 · 2.1 MB · animated  ← items.animated = 1
```

`canvas.caption`: base − 1.5pt (≈11.3px), alpha **161**. One tertiary line is not
"dense metadata" — it is the thing a person who pastes screenshots needs (which
one is this, how big, can I use it). Dimensions and byte size are already columns
in `items`; nothing is computed.

Image selection states reuse the same rail and a 2px `Highlight` α200 ring drawn
**outside** the image bounds (so it never covers pixels), plus the α26 fill behind
the caption row.

Double-click on an image opens the lightbox. Animated images play on hover, one at
a time, exactly as in the card list today.

### 2.5 The trailing composer

The last thing in every canvas, 24px below the final item:

- Minimum height 44px, grows with content, same 780px column, same typography as a
  text block.
- Placeholder `Write something…` at alpha **161** (not 130 — placeholders are text
  and must pass AA; see §8).
- No border at rest. Rail + editing border when focused, identical to any other
  text block, because it becomes one.
- Writes no row until it has non-blank content. Invariant 5, applied at item level.

---

## 3. Item selection model

This is the hard part, so it is resolved explicitly rather than by convention.

### 3.1 Two modes, never both

An item canvas is in exactly one of:

- **Object mode** — zero or more items selected, no caret live anywhere.
- **Text mode** — exactly one text item has the caret. The object selection is
  empty.

Entering text mode clears the object selection. Selecting an object drops the
caret. They are never simultaneous, so "what does Ctrl+C copy" always has one
answer.

### 3.2 Mouse

| Gesture | Result |
|---|---|
| Click on a text block's text | **Enter text mode**, caret at the click position |
| Click on a text block's gutter rail (the 26px strip left of the column) | Select it as an object |
| Click on an image | Select it as an object |
| Double-click on an image | Select + open lightbox |
| `Ctrl` + click, any item | Toggle that item in the object selection. Never enters text mode |
| `Shift` + click, any item | Extend the object selection from the anchor to this item by `position`. Never enters text mode |
| Click on canvas background | Clear selection; canvas keeps focus, object mode |
| Drag from canvas background | **Marquee**. Selects every item whose bounds intersect the rubber band. `Ctrl`-drag adds to the existing selection |
| Drag starting on an image | Marquee, anchored at the press point |
| Drag starting inside a text block | Ordinary text selection inside that block |

**Single click on text enters text mode, not object selection.** This is the
central call and it is deliberate: typing is the primary act in a scratch
surface, and the §1 philosophy is that the app must never make you perform a
setup gesture before you can put something down. Requiring click-to-select then
click-again-to-edit would tax the common case to serve the rare one. Selecting a
text block as an object is available three ways (rail, Ctrl+click, `Esc`) and none
of them costs the common case anything.

### 3.3 Keyboard

The **Esc ladder** is the mechanism that gets you out of text mode without a
mouse, and it is a single key pressed repeatedly:

```
text mode  ──Esc──▶  that block selected as an object
                     (caret gone, block highlighted)
           ──Esc──▶  selection cleared, canvas still focused
           ──Esc──▶  focus returns to the list pane
           ──Esc──▶  clears the search field, if any
```

| Key | Object mode | Text mode |
|---|---|---|
| `↑` `↓` | Move selection to prev/next item | Move caret within the block; does **not** spill to the next item |
| `Shift`+`↑`/`↓` | Extend selection | Extend text selection |
| `Tab` / `Shift+Tab` | Next / previous item | **Leave this block** → next item (text mode if text, object if image) |
| `Enter` | Text item → text mode, caret at end. Image → lightbox | Newline |
| `Ctrl+Enter` | — | Commit this block, open a fresh composer below it |
| `Ctrl+A` | Select **all items** in the buffer | Select **all text** in this block |
| `Home` / `End` | First / last item | Line start / end |
| Printable key, nothing selected | Focus the composer and insert that character | (types) |
| Printable key, items selected | **Nothing.** Typing never replaces a selection | — |

Two of these are judgement calls, stated as such:

- **`Tab` navigates instead of inserting a tab character.** A tab character can
  still be pasted, and in a plain-text scratch surface navigation is worth more
  than literal tabs. If this proves wrong, `Ctrl+Tab` navigates and `Tab` inserts;
  the rest of the model is unaffected.
- **Arrow keys do not spill out of a text block.** The alternative — `↓` on the
  last line moving to the next item — is a well-known source of "my cursor
  vanished" bugs when a block is wrapped. `Tab` and `Esc` are explicit.

- **Printable keys never replace a multi-selection.** A scratch surface whose
  premise is "we do not lose your stuff" must not have a gesture that destroys
  four items because you leaned on the keyboard.

### 3.4 Selection scope and lifetime

Selection is per-buffer and ephemeral: it is cleared when the selected buffer
changes, when the buffer reloads from disk, and on window close. It is never
persisted. The anchor for `Shift`+click is the last item touched by a plain click
or `Ctrl`+click.

### 3.5 Which pane has focus

The §7 review's real objection. It is answered visually, in three places at once:

1. **List row selection** is filled (`Highlight` α18 + a 2.5px `Highlight` left
   bar + border α140) when the list has focus, and outline-only
   (border `Highlight` α110, no fill, no bar) when it does not. This is standard
   active/inactive selection, done on purpose rather than inherited.
2. **The canvas** shows a rail or a caret somewhere whenever it has focus, and
   nothing at all when it does not.
3. The **search field** and header buttons take a 2px `Highlight` focus ring at
   2px offset, per SPEC §14.

At most one of (1) and (2) is ever lit.

---

## 4. Copy, Cut, Delete

### 4.1 What lands on the clipboard

**Single text item** — `text/plain` = `items.text`, exact, byte for byte. No
trailing newline added, no normalisation (invariant 8 applies to the clipboard
too). Nothing else.

**Single image item** — three representations, in this order:

| Format | Content |
|---|---|
| the item's own `items.mime` (e.g. `image/webp`, `image/svg+xml`) | the blob's **verbatim bytes** |
| `image/png` | transcoded fallback, so anything can take it |
| `application/x-qt-image` (`QMimeData::setImageData`) | `QImage`, free from Qt |

Verbatim bytes first is the point: it is what preserves animation on a GIF and
geometry on an SVG, and it is the same contract §4 of SPEC.md already makes on the
way *in*. No `text/plain` is offered for an image — pasting a screenshot into a
text editor and getting `screenshot.png · 2560×1440` would be Napkin inventing
content the user never wrote.

**Multi-selection** —

| Format | Content |
|---|---|
| `application/x-napkin-items` | JSON array, `position` order: `{type, text} \| {type, blob_hash, mime, source_name, width, height, animated}` |
| `text/plain` | text items only, in order, joined with `\n\n`. Images contribute **nothing** — no `[image: …]` placeholder |
| image formats | the **first** image in the selection, as above |

`text/plain` is omitted entirely when the selection contains no text items, so a
paste into a text editor fails cleanly rather than pasting an empty string.

`application/x-napkin-items` is what makes Napkin→Napkin paste reconstruct the
exact items, multiple images included, in order. It carries blob *hashes*, not
bytes (see §11.2 for the lifetime problem this creates and how it is handled).

### 4.2 Cut

`Cut` = `Copy`, then remove those items from the buffer, then renumber the
remaining `position` values. It is **undoable for 8 seconds** via the existing
toast.

Cutting the last item leaves an **empty buffer**. It is *not* auto-trashed —
§6 of SPEC.md says Napkin never auto-deletes a live buffer, and an emptied buffer
is still a buffer the user made. It shows in the list as `Empty buffer` at
tertiary contrast with its timestamp, and its canvas is just the composer. The
user trashes it with `Delete` from the list if they want it gone.

### 4.3 Delete

`Delete` (or `Backspace`) in object mode removes the selected items. Undoable for
8 seconds.

Item delete is **already** a hard delete today (`BufferService::removeItem` has no
soft path; `items` has no `deleted_at`). Invariant 2 is about buffers. So the 8s
toast is strictly an improvement on the status quo, not a weakening of an
invariant — but it needs one change to be honest:

> **Item removal must stop unlinking blobs.** Currently the blob is unlinked when
> the last referencing row goes. If the user undoes within 8s, the row comes back
> pointing at a file that no longer exists. Fix: `removeItem` deletes the row and
> stops. Orphan blobs are reclaimed by the `reconcileBlobs()` startup sweep that
> already exists for exactly this purpose. Cost: orphan bytes live until the next
> launch. Benefit: undo is trivially correct, no timer is coupled to the blob
> store, and invariant 7 (row before unlink) is preserved by construction.

The toast needs one refactor: `UndoToast::offer(QString, BufferId)` becomes
`offer(QString label, std::function<void()> undo)`. Buffer-trash undo passes a
closure; item undo passes a different one. That removes the `BufferId` coupling
and costs nothing.

Toast copy:

```
3 items removed.                              Undo
1 item cut.                                   Undo
Buffer moved to trash.                        Undo     ← unchanged
```

### 4.4 Bindings, and how they coexist with the list

| Key | List focus | Canvas, object mode | Canvas, text mode |
|---|---|---|---|
| `Ctrl+C` | Copy buffer as text (text items joined `\n\n`) | Copy selected items | Copy text selection |
| `Ctrl+X` | **Nothing** | Cut selected items | Cut text selection |
| `Delete` | Move buffer to trash *(unchanged)* | Remove selected items | Delete character |
| `Backspace` | — | Remove selected items | Delete character |
| `Ctrl+V` | Append to selected buffer *(unchanged)* | §5 | §5 |
| `Ctrl+A` | Select all rows? **No — unbound** | Select all items | Select all text |
| `p` `k` `R` | Pin / Keep / Restore *(unchanged)* | — | — |
| `Enter` | Move focus to canvas | Edit / lightbox | Newline |
| `Esc` | Clear search | §3.3 ladder | §3.3 ladder |
| `Ctrl+N` | New draft *(global)* | | |
| `Ctrl+Shift+I` | Add image to selected buffer, at the end *(global)* | | |
| `Ctrl+K` / `/` | Focus search *(global; `/` list-scope only)* | | |

Three rules govern the whole table:

1. **Bare letters only fire with list focus.** Already true
   (`BufferListView::keyPressEvent`); unchanged, and now load-bearing, because the
   canvas is full of text fields.
2. **`Ctrl+X` is deliberately unbound in the list.** `Ctrl+C` in the list is
   useful and non-destructive; a buffer-level cut would silently remove an entire
   buffer on a key that muscle memory fires constantly. The asymmetry is
   intentional: the destructive gesture must not change scope based on which pane
   happens to have focus.
3. **`Delete` keeps its existing list meaning.** It trashes the *buffer* with list
   focus and removes *items* with canvas focus. Both are undoable from the same
   toast, and both are the obvious meaning of "delete what is selected in the
   thing I am looking at."

`Ctrl+A` is left unbound in the list on purpose. "Select all 5000 buffers" has no
action behind it and no undo story, and adding one would be the first step toward
bulk organisation, which §1 exists to avoid.

---

## 5. Where typing goes

### 5.1 Adding a text block

Three ways, all of which end in the same place:

1. **Click the composer** at the end of the canvas and type.
2. **Type with the canvas focused and nothing selected** — the first printable
   character focuses the composer and is inserted there. No hunting.
3. **`Ctrl+Enter` inside any text block** — commits it and opens a fresh composer
   immediately below, so you can write two adjacent blocks without the mouse.

There is no "add block" button. A button for a thing that already happens when you
type is decorative chrome.

### 5.2 Paste, by focus

| Focus | Clipboard has text | Clipboard has an image | Clipboard has `x-napkin-items` |
|---|---|---|---|
| Inside a text block (caret) | Insert at the caret, inside the block | New image item **after** that block | Items inserted after that block; text portion at the caret |
| Canvas, object mode, nothing selected | New text item at the end | New image item at the end | Items appended in order |
| Canvas, object mode, items selected | New text item **after the last selected item** | Same, after the last selected | Same, after the last selected |
| List pane | Append to the selected buffer; canvas scrolls to it and flashes | Same | Same |
| List pane, nothing selected | Create a new buffer from the clipboard and select it | Same | Same |

Rules behind the table:

- **Paste never replaces a selection.** You asked to add, not to delete. If you
  wanted replacement you would have hit `Delete` first, which is undoable.
- **Text goes inside a text block; an image never does.** `items.type` is `text`
  or `image` and a text item holds only text — so pasting an image mid-paragraph
  splits nothing and inserts a sibling image item after the block. (Splitting the
  paragraph at the caret to interleave the image was considered and rejected: it
  silently turns one item into three, which is surprising and unundoable at the
  item level.)
- **`application/x-napkin-items` wins over everything**, so Napkin→Napkin
  round-trips are lossless.
- **New content flashes.** An item created by a paste that happened somewhere the
  user was not looking (list-focus paste, or a paste appended below the fold) gets
  a 240ms `Highlight` α40 wash that fades over 400ms, and the canvas scrolls it
  into view. Ease: `QEasingCurve::OutCubic`. This is the only animation in the
  application and it exists because content appearing off-screen with no feedback
  is the specific failure mode of a two-pane layout.

### 5.3 Autosave, unchanged

`Autosave`'s 300ms debounce / 2s hard max still governs. Force-flush points
change from "collapse, buffer switch, window blur, close" to **"buffer switch,
text-mode exit, window blur, close"**. Collapse no longer exists. A failed flush
still refuses to leave the block and says so (§19's critical fix stands).

---

## 6. Empty and edge states

**Nothing selected, buffers exist.** Canvas shows, centred vertically and on the
text column:

```
Select a buffer                          canvas.body, alpha 170
Ctrl+N starts a new one                  base size,  alpha 161
```

No illustration, no icon, no card.

**No buffers at all.** The left pane is **hidden entirely** (not an empty rail),
the divider with it, and SPEC §7's empty state occupies the whole window
unchanged. The list reappears the moment a buffer has content.

**Buffer with only images.** No special case. Flow of image blocks, composer at
the end. The header meta line reads `3 items · 3 images · 4 hours ago`.

**Buffer with 50 items.** The canvas is a `QScrollArea` of real widgets (text
editing requires it), with two guards:

- **Lazy pixmaps.** An image block knows its aspect ratio from `items.width` /
  `items.height`, so it reserves correct geometry before decoding anything — no
  layout jump on scroll. It decodes at canvas size when within **1.5 viewport
  heights** of the visible region and releases the pixmap beyond **3 viewport
  heights**. Peak decoded pixels stay bounded regardless of item count.
- **Hard cap at 200 items rendered.** Beyond that the canvas renders the first 200
  and a `Show 143 more` button. An honest ceiling beats a window that stops
  responding. (Guess, not measurement: 200 is where 200 reserved layouts plus
  ~6 live pixmaps should still be comfortable. Measure it in Phase 2 of the
  rewrite and move the number.)

Layout must be O(n) per keystroke, not O(n²): a text block reports only its own
height change and only its own geometry is re-laid-out. The current
`desiredHeight()` / `heightChanged()` chain that walked every widget to size an
inline editor is deleted along with the inline editor (§9).

**Image whose file is missing.** Never blank, never a crash (acceptance criterion
14). The block reserves the recorded aspect ratio at the recorded size and draws:

```
┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
│                                  │   1px dashed, Text α90, radius 4
│       Image file missing         │   canvas.body, alpha 170, centred
│                                  │
└ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
screenshot.png · 2560×1440 · file not found      alpha 161
```

Selectable and deletable like any other item. A `Remove` text button appears in
the gutter on hover, because this is the one item type where removal is the only
sensible action.

**Very long text block.** Text is never truncated in storage (invariant 8) and
never truncated on copy. But §19 already records that O(text length) paint was a
real 153ms-per-repaint bug, so display is bounded:

- A text item over **20 000 characters** renders collapsed: the first ~2 000
  characters, a 40px fade to `Base`, and a button
  `Show all (48 213 characters)`.
- Expanding is per-item, remembered for the session, forgotten on restart.
- Clicking into a collapsed block to edit **expands it first**, then places the
  caret. You never edit a view of your text that is not all of it.
- Copy and Cut always take the whole text, collapsed or not.

**Brand-new empty buffer (`Ctrl+N`).** The list gets its draft row exactly as
today. The canvas shows **only the composer**, focused, caret ready — no
empty-state copy, no "this buffer is empty". §12's `Ctrl+N` → caret budget is
50ms and this path touches no database and decodes no image, so it should sit far
under it.

---

## 7. Search interaction

Search filters the **list** in place, as SPEC §7 specifies — unchanged. Two
additions the two-pane layout makes free:

- The selected buffer stays selected and its canvas stays rendered even when it is
  filtered out of the list. Filtering the list must not blank the pane you are
  reading.
- Matches inside the selected buffer are highlighted **in the canvas** as well, at
  `Highlight` α60 behind the matched run. `F3` / `Shift+F3` step through canvas
  matches. This is derived display, not a new feature: the FTS already knows the
  terms.

---

## 8. Visual system

### 8.1 Type scale

Expressed as deltas from `QApplication::font()` so it tracks the desktop's font
setting. Pixel figures assume a 10pt base at 96 dpi.

| Token | Size | Weight | Tracking | Line height | Colour |
|---|---|---|---|---|---|
| `canvas.body` | base **+2pt** (≈16px) | 400 | 0 | **1.5** | `Text` α255 |
| `canvas.caption` | base −1.5pt (≈11.3px) | 400 | +0.2 | 1.4 | `Text` **α161** |
| `list.primary` | base (≈13.3px) | **500 Medium** | 0 | 1.35 | `Text` α255 |
| `list.secondary` | base −1pt (≈12px) | 400 | 0 | 1.35 | `Text` **α170** |
| `list.timestamp` | base −1.5pt (≈11.3px) | 400 | 0 | 1.2 | `Text` **α161** |
| `list.section` | base −1pt (≈12px) | **600 DemiBold** | **+1.2px abs** | — | `Text` **α161**, uppercase |
| `header.search` | base | 400 | 0 | — | `Text` α255 / placeholder **α161** |
| `header.meta` | base −1pt | 400 | 0 | — | `Text` **α170** |
| `header.action` | base | 500 Medium | 0 | — | `Text` α255 (`Trash`, `⋯`: α170) |
| `empty.title` | base +2pt | 500 Medium | 0 | — | `Text` **α170** |
| `empty.sub` | base | 400 | 0 | — | `Text` **α161** |

Three deliberate moves:

- **Canvas body is two points larger than list text.** The list is chrome; the
  canvas is content. §7 says content must visually dominate, and the single
  cheapest way to say that is size.
- **`list.primary` goes to Medium (500).** One weight step on one line is the
  largest "modern and sleek" return available for zero pixels and zero colour.
  Everything else stays at 400.
- **Nothing readable goes below α161.** Including the composer placeholder, which
  was the obvious place to cheat. Placeholder text is text; WCAG does not exempt
  it. α161 is slightly loud for a placeholder and that is the correct trade.

### 8.2 Contrast

Alphas composited over `QPalette::Base`, measured against Breeze Light (the
harsher theme), matching the thresholds already established in
`BufferCardDelegate.cpp`:

| Alpha | Ratio (light) | Use |
|---|---|---|
| 255 | ≥ 15:1 | body, primary line |
| **170** | 5.06:1 | secondary line, header meta |
| **161** | 4.51:1 | timestamps, section labels, captions, placeholders |
| 126 | 3.07:1 | *non-text only* — floor for an affordance carrying meaning |
| ≤ 90 | — | *decorative only*, never the sole indicator of any state |

Non-text tokens:

| Token | Value |
|---|---|
| `border.hairline` (divider, header rule, image edge) | `Text` α36 |
| `border.item.hover` | `Text` α60 |
| `rail.hover` | `Text` α90 |
| `rail.active` | `Highlight` α255 |
| `border.item.selected` | `Highlight` α160 (focused) |
| `border.item.editing` | `Highlight` α200 |
| `fill.item.selected` | `Highlight` α26 light / α44 dark |
| `border.card.resting` | `Text` α128 *(unchanged)* |
| `border.card.active` | `Text` α178 *(unchanged)* |
| `focus.ring` | `Highlight` α255, 2px, 2px offset |

Every state above is carried by **two** signals (rail + fill, or rail + border),
so none of them is encoded in colour alone (§14).

### 8.3 Spacing and radii

Spacing scale, 4px base: **4 · 8 · 12 · 16 · 20 · 24 · 32 · 40 · 56 · 120.**
Every number in this document is on it.

| Radius | Use |
|---|---|
| 0 | divider, header rule |
| 4 | images, thumbnails, rail (1.5) |
| 5 | item selection rect |
| 6 | list cards *(existing `kRadius`)*, search field |
| 8 | undo toast |

Nothing above 8. §7: avoid large corner radii.

### 8.4 How the panes are separated

**Both panes sit on `QPalette::Base`. The gutters sit on `QPalette::Window`.**

That is the whole idea, and it is the metaphor doing structural work: the canvas
is one large sheet of paper, the list cards are small sheets, both on the same
desk. The separation you see is the desk showing between them, plus a 1px
`Text` α36 rule down the divider for definition in dark themes at 100% scaling.

No tint difference between the panes. No gradient. No shadow anywhere in the
application. The header is `Window` with a 1px α36 bottom rule.

---

## 9. What to drop

Delete, do not carry over:

1. **Inline expansion, entirely.** `BufferListView::expandRow` / `collapse` /
   `repositionEditor` / `syncExpandedHeight`; `BufferCardDelegate::setExpandedHeight`
   / `expandedHeight` / `expandedHeight_`; `BufferListModel::IsExpandedRole`;
   `MainWindow::collapseEditor`. `BufferEditor` survives — renamed `ItemCanvas` —
   but loses its "position me over a list row" contract and its overlay
   parenting.
2. **The frozen-sort machinery.** `BufferListModel::expandedRow_` and
   `pendingReload_` existed only because the card you were typing into would jump
   to the top. In two panes the selection is a `BufferId`, not a row index, so a
   re-sort cannot lose it. The row still should not jump under the cursor
   mid-keystroke, so: **defer the re-sort until the selection changes or the
   window loses focus.** One boolean replaces the row-index coupling.
3. **`Enter` as "expand" and `Esc` as "collapse".** Enter now moves focus from the
   list to the canvas. Esc is the §3.3 ladder.
4. **Single-click-selects-only.** A click in the list now both selects the row and
   renders the buffer. Argued in §10.
5. **`ImageItemWidget`'s thumbnail-plus-metadata-row layout.** This is the thing
   the user identified as not looking good, and it is right: a 52px thumbnail beside
   a filename is a file manager row, not a picture. Replaced by §2.4.
6. **`ImageItemWidget`'s always-visible `QToolButton` remove control.** A ✕ on every
   image is exactly the "decorative chrome" §7 forbids, and it puts an
   irreversible action one stray click from the pointer. Removal is `Delete` on a
   selection, plus the context menu, plus the hover-gutter `Remove` on a *broken*
   image only.
7. **`BufferCardDelegate::kMaxCardWidth = 760` and its centring branch.** The list
   pane can never exceed 480, so the clamp is unreachable code that implies a
   layout which no longer exists. The 760px measure constraint moves to the
   canvas's 780px text column, where it now does real work.
8. **The `Napkin` wordmark in the header.** ~70px of the most valuable horizontal
   space in the application, spent restating the window title. Reclaimed for
   search.
9. **The `＋` fullwidth glyph** in the New button — a font-fallback hazard for
   a decorative flourish. Plain `+`.
10. **`desiredHeight()` / `heightChanged()` height plumbing** in `BufferEditor` and
    `TextItemWidget`. In a `QScrollArea` the layout system does this, and the
    manual chain is O(n) per keystroke.
11. **`UndoToast`'s `BufferId` payload.** Replaced by `std::function<void()>` so
    item-level and buffer-level undo share one widget.

Keep, explicitly — these were the reasons the earlier mockup was rejected and
they are all still here:

- PINNED / RECENT sections and the section label typography.
- Relative timestamps on every row, on the tick timer.
- Up to **three** thumbnails per card plus overflow count. (38px multi / 52px
  single still fit a 280px pane with room; no reason to cut to two.)
- Drawn pin and keep glyphs, and their accessible labels.
- One row per **buffer**, never per item.
- The lightbox — but see §10(e): its job changes.

---

## 10. Where this contradicts SPEC.md §7, and why

**(a) "Decision: cards expand in place… No modal, no second pane, no navigation
model to learn… exactly one screen in the entire application."** Directly
reversed.

The review that produced that decision listed four concrete costs. Three were
properties of *that particular mockup*, not of master-detail, and this spec keeps
all three: every row elided to one truncated line (kept: three lines, primary /
secondary / timestamp), timestamps absent (kept), no home for PINNED/RECENT
(kept), one row per item rather than per buffer (kept: per buffer). The fourth —
"two panes add a navigation model: which pane has focus, two scroll positions,
focus ping-pong" — is real, survives, and is paid for explicitly in §3.5 and
§11.1 rather than denied. It is a genuine cost, not a resolved one.

**(b) "Selection and opening are separate gestures."** Reversed: one click both
selects and renders.

The original objection was specific: opening *mutated the list layout*, so
pinning, keeping or deleting a card with the mouse meant expanding it first and
collapsing it after. That objection is entirely about mutation, and in two panes
opening mutates nothing — the list geometry is identical before and after. A
click still leaves `p`, `k` and `Delete` immediately available with list focus.
The cost the separation was buying no longer exists, so the separation should go.

**(c) "List position is frozen while a buffer is expanded."** Replaced by
deferring re-sort to selection-change or window-blur (§9.2). The underlying bug
(autosave bumps `modified_at`, list re-sorts, row jumps) is real and still
guarded; only the mechanism changes.

**(d) "The lightbox buys the full-size view without making every text buffer pay
for a permanent pane."** The premise — that a text buffer *pays* — is the part
that does not hold up.

§19 records that cards were stretching to full window width and were fixed by
centring a 760px column. So on a 1600px window the application was already
spending ~800px on empty gutter. A text buffer does not pay for the canvas; it
gets a 780px reading measure at 16px type instead of two elided lines at 13px in a
760px card. The pane is not a new cost — it is the existing dead space finally
carrying content.

The lightbox stays, with a changed job: it is no longer the answer to "a
2560×1440 screenshot is cramped" (the canvas is), it is the answer to "I need to
see this at 1:1". And the §19 known-limitation — no ←/→ paging across a buffer's
images — becomes trivial to fix, because the canvas already holds the ordered
image list. Fix it.

**(e) §17 "A read mode… Opening a buffer currently means editing it; there is no
way to simply look at a long one."** This delivers it as a side effect. The canvas
in object mode with nothing selected *is* read mode.

**(f) §7's keyboard table.** `Enter` changes meaning, `Esc` changes meaning, and
`Ctrl+C` / `Ctrl+X` / `Ctrl+A` / `Tab` / `Backspace` are added with pane-scoped
meanings. The scoping principle from §7 — "bare letters only fire with list focus,
so they can never conflict with typing" — is unchanged and is what makes the
additions safe.

---

## 11. Hard problems, named

### 11.1 Focus ping-pong is real and is not fully solved

Two panes genuinely do mean two focus targets and two scroll positions, and the
§7 review was right about that. The mitigations in §3.5 (active vs inactive
selection styling, the Esc ladder, `Enter` and `Tab` as explicit crossings) reduce
it; they do not eliminate it.

The specific residual risk: a user pastes with list focus, the item lands
off-screen in the canvas, and they do not notice. §5.2's scroll-and-flash exists
for exactly that case and is the thing to test with a real person first.

### 11.2 Clipboard blob-hash lifetime

`application/x-napkin-items` carries blob hashes, not bytes, because carrying six
screenshots' bytes on the clipboard is unacceptable. A hash can go stale: copy
items, delete the buffer, empty the trash, paste — dangling reference.

Mitigations, in order of strength:

1. Item removal no longer unlinks blobs at all (§4.3), so within a session the
   file is still there.
2. `emptyTrash()` and `reconcileBlobs()` consult the hash set currently on the
   clipboard and skip those blobs.
3. Paste verifies each hash resolves before inserting anything, and reports
   `2 of 3 images are no longer available` rather than writing a broken row.

(3) is mandatory; (1) and (2) make (3) almost never fire. Across application
restarts a stale clipboard is unavoidable and (3) is the honest answer.

### 11.3 N text editors in one scroll area — the single hardest implementation call

A buffer with 40 text items means 40 `QPlainTextEdit`s, each carrying a document,
a viewport, two scrollbars and an undo stack. That is heavy, and it is heavy
whether or not the user is editing.

**Recommended architecture: one live editor.** Non-focused text items render as
painted blocks (`QTextLayout` / `QStaticText` — cheap, no widget). A single shared
`QPlainTextEdit` is reparented and geometry-matched onto whichever block enters
text mode. Benefits: constant widget cost, no per-item undo-stack sprawl,
`Autosave` binds to one thing.

The hard part is the entry gesture: a click that enters text mode must place the
caret **where the user clicked**, which means mapping the click point through the
painted layout into a document position and seeding the real editor with it. Get
that wrong and every click into text jumps the caret to position 0, which is worse
than the current design.

**Fallback if the mapping proves unreliable:** N real editors, capped at the 200
items of §6, accepting the memory. Do not ship the caret bug to save the memory.

Related: whichever approach, the swapped-in editor must re-announce itself to
AT-SPI on each move, or screen-reader users get one control that keeps changing
identity underneath them. This needs a real Orca test, not an assumption.

### 11.4 Marquee versus text drag

A drag that starts in a text block's 12px bleed padding is ambiguous: text
selection or marquee? Resolution: **the bleed padding belongs to the block** (text
selection), and the marquee starts only from true background — the gutters, the
inter-item gaps, and the region below the composer. Stated so it is testable, but
it will feel wrong to somebody and is worth watching.

### 11.5 Re-sort timing

Deferring re-sort to selection-change means a long editing session leaves the list
visibly stale — the buffer you have been typing into for ten minutes still sits
where it was. That is correct (it does not move under you) and also slightly
dishonest (its timestamp says `12 minutes ago` while its position says otherwise).
No good answer here; the alternative (live re-sort) is worse. Flagging it rather
than pretending it is clean.

### 11.6 Reordering items has no gesture

Cut-and-paste is the only way to reorder items within a buffer, because there is
no drag and drop in v1 (SPEC §0). That is probably fine for a scratch surface —
but the two-pane canvas makes ordering *visible* in a way the collapsed card never
did, so the pressure to reorder will be higher than it was. If it becomes a real
complaint, `Ctrl+Shift+↑`/`↓` on an object selection is the cheapest answer and
requires only a `position` renumber.

### 11.7 Very large text documents

`QPlainTextEdit` with a 1 000 000-character wrapped document is genuinely slow to
lay out regardless of how it is embedded. The §6 collapse rule keeps *display*
bounded, but the moment the user clicks into such a block to edit it, the full
document is loaded into the editor and the stall is unavoidable. No design fixes
this; it needs measurement, and if it is bad, a read-only mode for blocks above
some size with an explicit `Edit anyway` is the escape hatch.

---

## 12. Build order

1. `ItemCanvas` widget with vertical flow, painted text blocks, image blocks, the
   composer. Read-only. Wire selection in the list to it. Delete inline expansion.
2. Text mode: the one-live-editor mechanism (§11.3), caret mapping, `Autosave`
   rebind, the Esc ladder.
3. Selection model: click, Ctrl, Shift, marquee, keyboard.
4. Copy / Cut / Delete, the `x-napkin-items` format, the `UndoToast` refactor,
   removing the blob unlink from `removeItem`.
5. Paste routing by focus, scroll-and-flash.
6. Visual pass: type scale, alphas, spacing, the splitter, the header split, the
   `<800px` single-pane mode.
7. Edge states: missing blob, long text collapse, 200-item cap, lazy pixmaps.

Steps 1–2 are where the risk is. Everything after 3 is additive.

## 13. Tests this design adds

| Area | Case |
|---|---|
| Layout | text/image column widths at 900 / 1280 / 1600; image never clips; no upscale |
| Layout | `<800` collapses to one pane and back without losing the selected buffer |
| Selection | click on text enters text mode; click on rail selects as object; Ctrl and Shift build the expected sets |
| Selection | `Esc` ladder: text → object → none → list |
| Selection | printable key with items selected mutates nothing |
| Clipboard | text item copy is byte-identical; image copy carries the verbatim mime first |
| Clipboard | mixed selection `text/plain` contains no image placeholders; images-only offers no `text/plain` |
| Clipboard | `x-napkin-items` round-trips order, types and blob hashes |
| Clipboard | paste with a stale hash inserts nothing and reports |
| Cut/Delete | undo within 8s restores items, order and blob availability |
| Cut/Delete | emptying a buffer leaves a live empty buffer, not a trashed one |
| Paste | each focus × payload cell of §5.2 |
| Edge | missing blob reserves correct geometry and draws the placeholder |
| Edge | 20 000+ char item renders collapsed; editing expands first; copy takes all of it |
| Edge | 200-item cap; pixmaps released beyond 3 viewport heights |
| A11y | every state in §2.3 carries two signals; alphas ≥161 for all text |
