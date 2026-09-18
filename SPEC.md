# Napkin — Revised Specification (v2)

> The single source of truth for Napkin: product philosophy and buildable
> contract in one document. Supersedes and absorbs the original
> `PROJECRT_DETAILS.txt`, which has been removed.

**A persistent scratch surface for your computer.**
Put it here. Use it. Decide later whether it matters.

---

## 0. What changed from v1, and why

| Cut | Reason |
|---|---|
| Clipboard *history / manager* | Klipper and friends already do this. Never was the product. |
| Multi-format file paste (`CF_HDROP`, `x-special/gnome-copied-files`, cut-vs-copy verbs) | Highest platform-risk surface in the whole spec, for a capability almost nobody uses. |
| Drag and drop | Second-highest platform risk. Paste covers ~90% of real capture. Re-addable later; the item model is designed so it drops in cleanly. |
| Generic file attachments (PDF, zip, arbitrary binaries) | Kills the large-file policy, MIME sniffing, "reveal in file manager", broken-link handling, and the "am I a file manager?" identity crisis — all at once. |
| macOS/Windows as v1 targets | Linux-first. Portability preserved in architecture, not paid for in v1 schedule. |

**Kept, deliberately:** paste (`Ctrl+V`) and a narrow image file picker. See §3.

**Net effect:** the two item types are `text` and `image`. That is the whole
content model. Every simplification below follows from it.

---

## 1. Product philosophy

Napkin is inspired by the physical idea of writing something down on a napkin.

```text
Have something
      |
Throw it into Napkin
      |
Use it later
      |
Decide whether it matters
      |
Keep it, or let it go
```

The user should never have to think:

```text
What should I call this?   Which category?   Which folder?
Which notebook?            Which tags?       Which project?
```

Instead: **just put it on the napkin.**

### What Napkin is not

Not Evernote, Notion, Obsidian, or OneNote. Not a task manager, document
manager, knowledge base, project manager, or file manager. Not a clipboard
history application — the OS already has one, and that was never the product.
Not "a simplified Notion" or "a prettier clipboard manager."

### Mess is a feature

The application must tolerate accumulation. A user should be able to have a
shell command, a random thought, a screenshot, a URL, a copied paragraph, and
another screenshot all sitting together, unsorted, indefinitely.

Napkin exists specifically to **postpone the decision** to organize. Forcing
organization at capture time is the failure this product is designed to avoid.
Ten buffers or five thousand, the app should not care.

### No automatic intelligence

Napkin never summarizes, categorizes, names, classifies, tags, interprets
screenshots, or analyzes content. The application is deterministic and local.

Generated previews from obvious local metadata are fine and expected — first
line of text, URL hostname, image dimensions, item count. That is derived
display, not inference. There is no AI in Napkin.

### The editor stays small

Plain text only: multiline input, selection, copy/paste, undo/redo, keyboard
navigation. Deliberately **not** in v1: Markdown preview, rich text, formatting,
tables, code blocks, syntax highlighting, WYSIWYG.

If a user pastes Markdown, Napkin stores Markdown as text. That is the whole
feature.

## 2. Invariants

These are testable MUSTs. Everything else in this document is guidance.

1. A **kept** buffer is never deleted by any automatic process.
2. Nothing is ever hard-deleted without passing through the trash first.
3. Napkin never modifies, moves, or deletes a file outside its own data directory.
4. Napkin makes no network requests. Ever. There is no code path that opens a
   *network* socket. (It does open one AF_UNIX socket, inside the 0700 data
   directory, solely so a second launch can raise the first window.)
5. A buffer row is never written until the buffer has content.
6. An image blob is written and fsynced to disk *before* the DB row referencing it commits.
7. A DB row is deleted and committed *before* its blob is unlinked.
8. Text is never silently truncated, normalized, or reformatted on save.
9. All timestamps are stored as UTC epoch milliseconds.
10. No user content appears in logs.

Invariants 6 and 7 both deliberately bias toward *orphan blobs* (harmless, GC'd)
over *dangling references* (user-visible corruption).

---

## 3. Concepts

**Buffer** — a container of items. The user-facing unit. Never named by the user.

**Item** — one piece of captured content. Type is `text` or `image`.

**Pin** — affects *placement*. Pinned buffers sit at the top.

**Keep** — affects *lifecycle*. Kept buffers are never swept.

> ### Rename: Lock → Keep
> v1 called this "Lock" and then had to spend a section explaining that lock is
> not encryption. If a name needs a disclaimer, it is the wrong name — a padlock
> icon promises security Napkin does not provide. **Keep** is honest and pairs
> cleanly: *Pin keeps it at the top, Keep keeps it forever.*

Pin and Keep remain fully orthogonal. All four combinations are valid.

### Links are not a type

A URL is stored as a `text` item. A text item whose trimmed content is exactly
one URL is *rendered* as a link chip with Open / Copy actions. URLs inside prose
are linkified at render time.

This is derived presentation, consistent with "preview is derived data." It costs
one function, survives any future change to URL-detection rules with no
migration, and gives URL search for free via the text index.

---

## 4. Input

Exactly three ways content enters Napkin:

1. **Typing** into an expanded buffer.
2. **Paste** (`Ctrl+V`) — the clipboard is inspected in this fixed order:
   ```
   1. the richest decodable image the source offers, kept BYTE FOR BYTE
        vector           image/svg+xml
        animation-capable  image/gif, image/apng, image/webp, image/avif
        static raster    image/png, image/jxl, image/heif, image/jpeg, …
   2. any other image representation  →  transcoded to PNG (lossy; last resort)
   3. text/plain                      →  text item
   4. anything else                   →  ignored, quietly
   ```
   That preference order is a contract. Test it. Two ambiguous cases are real
   and both are covered: copying an image from a browser offers a bitmap *and* a
   URL, and the image wins; a source offering a GIF *and* a PNG must resolve to
   the GIF, or the animation is silently flattened to one frame.

### Image formats

Napkin stores every format it can decode **verbatim** — nothing is re-encoded,
so animation, vector geometry and original quality all survive, and export hands
back exactly what arrived. Only bytes nothing can read are refused.

What "can decode" means is **discovered at runtime** from
`QImageReader::supportedMimeTypes()`, never hard-coded: it depends on which Qt
image plugins are installed, and a missing plugin degrades to the next-best
representation rather than failing.

| Source | Formats |
|---|---|
| Qt built-in | PNG, JPEG, GIF, BMP, ICO, PPM/PGM/PBM, XPM, WBMP |
| `qt6-imageformats` | WebP, TIFF, JPEG 2000, MNG, ICNS, TGA |
| `qt6-svg` | SVG, SVGZ |
| `kimageformats` | **AVIF, HEIC/HEIF, JPEG XL**, PSD, XCF, EXR, DDS, QOI, RAW (CR2/NEF/ARW/DNG/…), and more |

Packaging must list `qt6-svg`, `qt6-imageformats` and `kimageformats` as
recommended dependencies. Without them Napkin still runs; it simply understands
fewer formats, and says so instead of pretending.

**Animation.** A multi-frame image is detected once at import and recorded in
schema v3 (`items.animated`) — reopening the file on every repaint would mean a
file open per visible card per frame. It plays:

- in the **lightbox**, always, decoded frame by frame so a long animation never
  sits in memory whole;
- in its **card, while the pointer is over it** — one at a time. Animating every
  visible GIF at once would spend §12's idle-CPU budget on decoration. A card
  showing a still first frame carries a small `GIF` badge so it is not mistaken
  for a static image.

> **Known limitation, not a defect.** Whether a paste preserves animation is up
> to the *source*. Browsers commonly rasterise to `image/png` on "Copy Image",
> in which case the clipboard never contains the animation and Napkin cannot
> recover it. Copying the file itself, or using **Add image…**, keeps the frames.

**SVG is sanitised at import, because rendering it is *not* inherently safe.**

An earlier version of this section claimed, "measured, not assumed: Qt's SVG
renderer loads no local file references." **That claim was false, and the probe
behind it was written in a way that could not fail** — it tested only
`xlink:href='file:///…'`, the one spelling Qt already rejects. A bare filesystem
path loads fine:

```
xlink:href='file:///tmp/secret.png'   → not read
href='/tmp/secret.png'                → LOADED
xlink:href='/tmp/secret.png'          → LOADED
```

A hostile SVG could therefore render any local image the user can read into a
card — and the thumbnailer would persist a copy of it inside Napkin's own data
directory. Napkin now **refuses at import** any SVG whose `href`, `src` or
`url(...)` points anywhere but a `data:` URI or an in-document `#fragment`, and
refuses compressed SVGZ outright since it cannot be inspected without
decompressing it.

What *did* hold under test: `http://` references trigger no connection, and XXE
(`<!ENTITY SYSTEM 'file:///etc/passwd'>`) is blocked. **Invariant 4 itself was
never breached.** The lesson is about the test, not the renderer: a probe that
only exercises the case you expect to pass is not evidence.
3. **Add image…** (`Ctrl+Shift+I`) — a native file dialog whose filter is built
   from the formats this build can actually open, which copies the chosen file
   into the blob store immediately. This is the reliable path for animated GIFs.

> **On keeping the picker.** You cut *files as a content type*, not *ways to get
> an image in*. Without the picker, a user with `diagram.png` on disk has to open
> an image viewer and copy it — a dead end for no gain. The picker introduces no
> new item type, no MIME handling, no large-file policy, and no lifecycle. It is
> roughly 30 lines. Strike it if you disagree; nothing else depends on it.

There is no drag-and-drop, no generic file import, and no "attach file."

---

## 5. Data model

SQLite, WAL mode, `foreign_keys = ON`, `synchronous = NORMAL`.

```sql
CREATE TABLE buffers (
  id           INTEGER PRIMARY KEY,
  created_at   INTEGER NOT NULL,           -- UTC epoch ms
  modified_at  INTEGER NOT NULL,
  pinned       INTEGER NOT NULL DEFAULT 0,
  kept         INTEGER NOT NULL DEFAULT 0,
  deleted_at   INTEGER                     -- NULL = live; set = in trash
);

CREATE TABLE items (
  id           INTEGER PRIMARY KEY,
  buffer_id    INTEGER NOT NULL REFERENCES buffers(id) ON DELETE CASCADE,
  position     INTEGER NOT NULL,
  type         TEXT    NOT NULL CHECK (type IN ('text','image')),
  created_at   INTEGER NOT NULL,

  text         TEXT,        -- type='text'
  blob_hash    TEXT,        -- type='image', sha256 hex
  source_name  TEXT,        -- original filename if from picker; NULL if pasted
  width        INTEGER,
  height       INTEGER,
  byte_size    INTEGER
);

CREATE INDEX idx_items_buffer   ON items(buffer_id, position);
CREATE INDEX idx_buffers_recent ON buffers(deleted_at, pinned, modified_at DESC);
```

### Image storage

Content-addressed, no refcount table. Images are kept **byte for byte as they
arrived** rather than normalised to PNG: re-encoding a JPEG photo would inflate
it several times over and add generation loss for nothing. Schema v2 therefore
carries a `mime` column, and the blob keeps its own extension.

```
~/.local/share/napkin/
├── napkin.db
├── napkin.db-wal
├── blobs/
│   └── ab/abcdef0123….png      (or .jpg, .webp, .gif — verbatim)
└── thumbs/
    └── ab/abcdef0123…_96.png
```

Only formats Napkin cannot serve directly are converted, and only on the way in.
Anything above 64 MB is refused with a readable message rather than swallowed.

Pasting the same screenshot twice dedupes for free. On item delete, unlink the
blob only if `SELECT 1 FROM items WHERE blob_hash = ? LIMIT 1` returns nothing.
A query, not a counter — so no refcount drift is possible.

Directory mode `0700`, DB file `0600`. A scratch surface *will* contain tokens
and passwords in practice.

### Search

```sql
CREATE VIRTUAL TABLE items_fts USING fts5(
  text, source_name,
  content='items', content_rowid='id',
  tokenize='unicode61 remove_diacritics 2'
);
```

Kept in sync by `AFTER INSERT/UPDATE/DELETE` triggers on `items`. Results roll
**up** to buffer level and dedupe — the UI shows buffers, so a match on item 3's
`source_name` surfaces the whole buffer. Rank with `bm25()`, snippet with
`snippet()`. No query language in v1; substring-ish prefix matching only.

### Enforcing invariant 1 below the application layer

```sql
CREATE TABLE napkin_meta (key TEXT PRIMARY KEY, value TEXT);

CREATE TRIGGER guard_kept_delete BEFORE DELETE ON buffers
WHEN OLD.kept = 1
 AND COALESCE((SELECT value FROM napkin_meta WHERE key='allow_kept_delete'),'0') <> '1'
BEGIN
  SELECT RAISE(ABORT, 'refusing to delete a kept buffer');
END;
```

The confirmed-delete path sets the flag inside its transaction and clears it
after. This is what "must be enforced by the data layer, not the UI" actually
looks like — a bug anywhere in the service layer cannot destroy kept data.

Migrations from commit one, via `user_version`. Forward-only.

---

## 6. Lifecycle — the decision v1 never made

v1 was contradictory: it declared at length that kept buffers survive "automatic
cleanup," while also showing cleanup as a manual dialog and promising to tolerate
5000 buffers forever. The unanswered question — *does anything ever delete
without being asked?* — made the whole cleanup phase unbuildable.

**Decision: Napkin never auto-deletes a live buffer. There is no expiry.**

Instead, age changes *visibility*, not existence:

```
PINNED    pinned buffers, newest first
RECENT    everything modified within the last 30 days
OLDER     collapsed section, dimmed, still searchable, still there
```

Cleanup ("Sweep") is always user-initiated, surfaced by a quiet inline nudge
once the buffer count crosses a threshold:

```
┌──────────────────────────────────────────────┐
│ 83 buffers · 61 older than 30 days           │
│  4 kept — excluded                Review  ✕  │
└──────────────────────────────────────────────┘
```

Sweep moves buffers to **trash**, it does not erase them. `kept` buffers are
excluded from the sweep's default selection, permanently — that is what Keep
buys you: you decide once, and never re-decide on any future sweep.

### Trash and undo

v1 had no undo anywhere, while also declaring "never silently discard user
content." For an app whose premise is *throw things in without thinking*,
accidental deletion is the single fastest way to lose a user forever.

- Delete is a soft delete (`deleted_at`), always, for every path.
- An **Undo** toast appears for ~8 seconds after any delete or sweep.
- Trash is browsable and restorable, and can be emptied on demand — a confirmed,
  irreversible action, which then reclaims the blobs those buffers held.
- Trash purges items older than 30 days on startup. **This is the only automatic
  hard delete in Napkin, and it only ever touches things the user already deleted.**
  Emptying the trash skips any buffer still marked kept, which is the safe failure.
- Deleting a `kept` buffer requires explicit confirmation, even into trash.

---

## 7. UI

Single window. One vertical stack. Virtualized from day one — §12 promises 5000
buffers, and retrofitting virtualization into a card list is miserable.

### Editing: a second pane

**This reverses the decision the previous two versions of this section argued
for, and the reversal is the right call.**

§7 previously chose inline expansion over master-detail, and rejected a two-pane
mockup on the grounds that two panes add a navigation model. That argument was
sound when a buffer was a note. It did not survive buffers holding four images
and needing item-level operations:

- A card sized for a two-line preview cannot host a 900×520 screenshot, so the
  implementation showed a 52px centre-crop — which is how "images do not look
  very good" happened. It was a layout problem wearing a rendering problem's
  clothes.
- The independent review reached the same conclusion unprompted: *"multi-item
  buffers and inline expansion are in tension, and the spec never reconciled
  them."* It also pointed out that the built app had already paid the
  navigation-model cost — a list focused separately from an editor that stole
  focus, nested scroll regions, keys that silently changed meaning — while
  getting none of the benefit.
- Selecting an *item* in order to copy, cut or delete it needs somewhere for
  items to be objects. A card has no room to be a canvas.

```
+----------------+--------------------------------------+
| PINNED         |  [ text block                      ] |
|  [ card ]      |  [ image, at pane width, captioned ] |
| RECENT         |  [ text block                      ] |
|  [ card ]      |  [ image                           ] |
|  [ card ]      |  [ composer: type or paste…        ] |
+----------------+--------------------------------------+
   selection                  the selected buffer
```

**What the mockup got right, and what it got wrong, both stand.** The list keeps
sections, relative timestamps, thumbnails and the pin/keep indicators — the
things the earlier review correctly said a bare rail would lose — and the list
is still one row per *buffer*, not per item. Only the editing surface moved.

Selection *is* opening: there is no expand step, so a single click both selects
the row and fills the canvas. Enter or double-click puts the caret in the
canvas.

### A board of cards, not a page

Reference: `docs/UI_cards_reference.png`.

Napkin is temporary storage, not an editor. The canvas is a **masonry board**:
uniform column width, each card its own height, newest first.

| Decision | Why |
|---|---|
| **No composer.** | A trailing "write something" box assumed writing is the primary act. It is not — pasting is. `Ctrl+T` summons a card when you do want to type. |
| **`Ctrl+N` shows an empty board**, not a blank page | A new buffer is somewhere to paste into. Saying "Nothing here yet · Ctrl+V" is what it is for. |
| **Emptying a text card deletes the item** | An item holding nothing is not a thing, and a blank card is litter. If it was the last item, the buffer goes too. |
| **Newest first** (schema v4, `items.modified_at`) | On a scratch surface the thing you just put down is the thing you want. "Newest" has to mean edited as well as added, or amending an old note leaves it buried. |
| **Column-balanced flow, not a grid** | A grid forces a common height and crops the tall ones; a single column gave a three-word note a 780px row. Each card goes to whichever column is currently shortest. |
| **Cards clip at 420px with a fade** | Past that one phone screenshot owns the board. The fade says "there is more"; double-click opens it. |

Column width is 280–400px: as many columns as fit, then widened to share the
space, so a card is never cramped and never stretched merely because the window
is large.

> The card architecture is deliberately open. `ItemCard` is a base class with
> `heightForColumn()` and `asPlainText()`; text and image are two subclasses.
> Link previews, code snippets with syntax colouring, and map coordinates are
> further subclasses and nothing else has to change.

### The visual system

Tokenised in `src/ui/Tokens.h` rather than scattered as literals, so contrast is
a property of the system instead of a thing each call site gets right or wrong.
Every readable alpha is a measured threshold over `QPalette::Base` against
Breeze Light, the harsher theme: **255** body, **170** secondary (5.06:1),
**161** tertiary — timestamps, captions, section labels **and placeholders**,
which are text and get no exemption. **126** is the floor for a non-text
affordance carrying meaning; anything at or below 90 is decorative and never the
sole indicator of a state.

- **A card is a card.** Own surface (`Base`), own edge, 16px inset, 10px radius.
  The edge is `Text` **α128 light / α108 dark** (3.09:1 and 4.00:1) — the same
  3:1 floor everything else here obeys. An earlier value of α62 was described as
  "quiet", measured **1.63:1**, and contradicted the constant four lines above it
  in the same file. Selection changes the border's *width* as well as its colour,
  so no state is carried by colour alone. An earlier build drew text cards with no
  border and no fill on the reasoning that "a text item is content, not a
  widget" — which was true about the *content* and wrong about the *card*. The
  board read as loose text rather than as things you can pick up, and that is
  what the user was reacting to. A gutter rail was standing in for the edge; with
  a real edge it is redundant chrome, and it is gone.
- **The board is `Window`; the cards are `Base`.** The desk, and paper on it. A
  card needs something to sit against, so the two cannot both be `Base`. Still no
  gradient and no shadow anywhere.
- **Every card carries a footer**: a one-click *Copy text* / *Copy image* on the
  left, the item's age on the right. The button duplicates `Ctrl+C` deliberately
  — `Ctrl+C` acts on the *selection*, so it needs a selection first, while the
  footer is the one-click path for "give me that one thing". Two mechanisms for
  two intents, not one job done twice. Using it does not disturb the selection.
> **Measure against your own document, not the editor's.** `QPlainTextEdit` uses
> `QPlainTextDocumentLayout`, which ignores `setTextWidth` and wraps to the
> *viewport's current width*, then reports its height in **lines rather than
> pixels**. A card measured at construction — before its viewport has a width —
> therefore came back as one line, which is why a freshly pasted paragraph
> arrived as a single scrollable line. Height is now measured against a plain
> `QTextDocument`, which honours `setTextWidth` and answers in pixels before the
> widget has ever been shown.

- **Sizes are per-card and absolute.** Min 88px tall — one line plus chrome, and
  deliberately not padded to something rounder, because a minimum that exceeds
  what a short card needs makes the board lie about how much is in it. Max 420px
  so one screenshot cannot own the board. Columns 280–460px, floored at the
  minimum rather than squashed below it; a window too narrow for one full card
  scrolls horizontally, which is visible, instead of silently breaching the
  stated minimum.

### The board is virtualized

> **Virtualization has a cost the design has to pay back.** Heights come from
> the item data rather than from widgets — which is what makes it possible — but
> item data goes stale the moment someone types into a card. Two things keep it
> true: the live text is pushed back into the board's copy on every keystroke,
> and the measurement cache is invalidated for that item. Without the first, a
> card never grew as you pasted into it; without the second, an *existing* card
> never grew either, because the cache is keyed on the item's last **saved**
> time and that does not move while you are typing.


Card heights are computed from the **items**, not from widgets: text against one
shared `QTextDocument`, images from the dimensions already stored in the row. So
the whole board's geometry is known without constructing anything, and only the
cards inside the visible band plus an overscan actually exist.

| 1000 text items in one buffer | before | after |
|---|---|---|
| Live `QPlainTextEdit` widgets | 1000 | **12** |
| Peak RSS | 365 MB | **53 MB** |
| Per resize event | 270 ms | **31 ms** |
| Opening the buffer | 357 ms | 165 ms |

Memory is now flat in the item count — 52 MB at 50 items and 53 MB at 1000.
Opening still scales, because measuring a thousand documents is real work; that
is a one-off per buffer and a reasonable next target, not a correctness problem.

> §12 previously claimed virtualization was "Phase 2 architecture, not Phase 10
> polish", and argued that "retrofitting virtualization into a card list is
> miserable" — and then the board was built with none of it. The argument was
> right and the code ignored it. Retrofitting it was indeed miserable.

> **A card's size depends on its own content and nothing else.** That is harder
> than it sounds. The layout width is computed from the widget width **minus the
> scrollbar extent, unconditionally** — because with an as-needed scrollbar,
> adding one item makes the bar appear, shrinks the viewport by ~14px, changes
> the column width and resizes *every card in the buffer*. Both halves are
> asserted: a tall neighbour must not change a short card's height, and adding
> twelve items must not change the width of the card that was already there.
- Images draw at the column width, **never upscaled**, with one caption line:
  filename or format, dimensions, size, and *animated* when it moves.

### Items are selectable objects

The hard part is that a text block must be both a selectable object and an
editable field. Resolved by making a bare click mean the obvious thing for what
is under it:

| Gesture | Meaning |
|---|---|
| Single click | **select** the item, whatever it is |
| Double click | edit it (text), or open the lightbox (image) |
| `Enter` on a selected block | edit it |
| Ctrl / Shift + click | extend the selection |
| `Esc` while editing | stop editing, keep the block selected |
| Click empty canvas | clear the selection, focus the composer |
| `Ctrl+A` | select every item (not the unwritten composer) |

**Three states, three treatments.** At rest: hairline border. Selected: accent
border at 2px plus a faint tint. **Editing: accent border and no tint at all** —
a wash behind text you are actively reading and typing is the thing that makes
it unreadable, and the border plus the caret are two signals already. Editing
also clears the selection, so the two can never stack.

**Selecting in the canvas moves the keyboard there.** Without that, clicking a
card left focus on the buffer list, so `Delete` was delivered to the *list* —
which trashes a whole buffer. And because a delete destroys the card that had
focus, the canvas takes focus back after every one; otherwise the first `Delete`
worked and the second went nowhere.

> **Corrected.** An earlier build put the caret straight into text on a single
> click, on the reasoning that typing is the primary act. That made a text block
> the one thing in the canvas the mouse could not select or delete — you could
> click an image and press Delete, but not a paragraph. Selection is now
> uniform: a block is read-only until you ask to edit it, so "click it, then
> delete it" works on text exactly as it works on an image. Only one block edits
> at a time, so the canvas never has two carets or an ambiguous `Ctrl+C`.
>
> The trailing composer is the single exception: it is empty and has no row, so
> selecting it would mean nothing. Clicking it just starts writing.

`Ctrl+C`, `Ctrl+X` and `Delete` act on the selection when the canvas has focus,
and never while a caret is in a text block — there, they mean what they always
mean. A single selected image copies as an **image**, so it pastes into anything;
any other selection copies as text, joined in document order.

Deleting every item in a buffer trashes the buffer itself: an item-level delete
that leaves an empty husk behind is just litter. That goes through the ordinary
undo toast.

**A delete leaves the next item selected**, clamping to the new last item when
you delete off the end — so a run of deletes does not require re-aiming the
mouse between each one.

### One rule for paste

A paste goes into the buffer you are looking at, and makes a new one only when
you are looking at nothing. This holds for text and images alike.

> **Corrected.** Text used to always create a new buffer while an image appended
> to the selected one, so the same gesture did two different things depending on
> what you had copied. `Ctrl+T` adds an empty text block to the current buffer
> by the same rule, and is the keyboard route to what pasting text does.

> **Item removal does not unlink blobs, deliberately.** An earlier version
> deleted the rows and reclaimed the files in one step, so undoing inside the
> 8-second window restored a buffer whose items no longer existed — measured at
> **0 items recovered out of 4**. Removal now captures the items, restores them
> at their original positions on undo, and leaves the files alone; the startup
> sweep reclaims them once undo is no longer on offer. The toast takes a closure
> rather than a `BufferId`, so buffer-level and item-level undo share one widget.

> **The freeze applies to ORDER, never to MEMBERSHIP.** Deleting every item in a
> buffer trashes it — but with the order frozen from typing, the reload was
> deferred and the buffer stayed visible in the list while the toast beneath it
> said it had been moved to the trash. Every structural change (trash, restore,
> pin, keep, sweep, undo) releases the freeze before reloading.

**The order freeze survives the change.** Autosave still bumps `modified_at` on
every flush, so the list would still re-sort under the buffer being edited. The
canvas freezes the order on the first keystroke and releases it when the
selection moves on or the window loses focus.

### Search

```sql
CREATE VIRTUAL TABLE items_fts USING fts5(
  text, source_name,
  content='items', content_rowid='id',
  tokenize='unicode61 remove_diacritics 2'
);
```

Kept in sync by `AFTER INSERT/UPDATE/DELETE` triggers on `items`. Results roll
**up** to buffer level and dedupe — the UI shows buffers, so a match on item 3's
`source_name` surfaces the whole buffer. Rank with `bm25()`, snippet with
`snippet()`. No query language in v1; substring-ish prefix matching only.

### Enforcing invariant 1 below the application layer

```sql
CREATE TABLE napkin_meta (key TEXT PRIMARY KEY, value TEXT);

CREATE TRIGGER guard_kept_delete BEFORE DELETE ON buffers
WHEN OLD.kept = 1
 AND COALESCE((SELECT value FROM napkin_meta WHERE key='allow_kept_delete'),'0') <> '1'
BEGIN
  SELECT RAISE(ABORT, 'refusing to delete a kept buffer');
END;
```

The confirmed-delete path sets the flag inside its transaction and clears it
after. This is what "must be enforced by the data layer, not the UI" actually
looks like — a bug anywhere in the service layer cannot destroy kept data.

Migrations from commit one, via `user_version`. Forward-only.

---

## 8. Persistence and durability

v1 said "debounced persistence" and separately demanded survival of `kill -9`.
Those are in tension: the debounce window *is* the loss window. Concretely:

- **WAL + `synchronous = NORMAL`.** You then lose data only on OS or power loss,
  never on application crash or `kill -9` — which is exactly the test that matters.
- **Debounce 300 ms, hard max-delay 2 s.** Continuous typing still commits at
  least every two seconds.
- **Force flush** on collapse, buffer switch, window blur, and close.

### Image write ordering

```
temp file in blobs/  →  fsync  →  atomic rename to blobs/ab/<hash>.png  →  commit row
```

and on delete, commit the row removal *before* unlinking. A reconciliation sweep
at startup quarantines blobs with no referencing row, and flags rows whose blob
is missing (never silently — the item renders as a broken-image placeholder that
the user can remove).

---

## 9. Stack

Priorities from v1 §31 were internally conflicting — low memory and minimal
dependencies rule out Electron, while rich clipboard and native integration are
where lightweight options are weakest. **The cuts resolve that tension:** with
the file-clipboard matrix and DnD gone, the requirements are now just text
editing, image display, SQLite, theming, accessibility, and Linux-first.

**Recommendation: C++20 + Qt 6 Widgets, CMake.**

- `QClipboard::image()` / `::text()` — clipboard image paste is a two-line
  problem here, which is the entire remaining platform risk.
- Native on KDE Plasma and Wayland, which is the primary target.
- `QPlainTextEdit` gives real text editing, IME, undo/redo, selection for free —
  the thing immediate-mode GUI toolkits (egui, Slint) get wrong, and the reason
  they are ruled out given accessibility requirements.
- Genuine accessibility support (AT-SPI on Linux), which is a hard requirement.
- Widgets, not QML: this is a document-like UI, and QML buys animation
  infrastructure that a restrained app does not want.
- ~60–80 MB resident, sub-300 ms cold start.

**If you would rather write Python:** PySide6 is a legitimate v1 and the
architecture below ports cleanly. The honest cost is ~1 s startup, a 150 MB+
bundle, and PyInstaller packaging pain — a direct hit to two of your stated
priorities, but not a correctness problem.

Tauri becomes viable now that DnD is cut, but on Linux you would still be on
WebKitGTK: a different engine per platform, and the memory/performance wildcard.

Testing: Catch2 or Qt Test. CI on GitHub Actions, Linux job blocking.

---

## 10. Architecture

```
napkin/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/       Application, single-instance guard, XDG paths, settings
│   ├── domain/    Buffer, Item, BufferService, SearchService,
│   │              SweepService, ImageService      ← no QtWidgets dependency
│   ├── data/      Database, Migrations, BufferRepository, ItemRepository, Fts
│   ├── media/     BlobStore, Thumbnailer
│   └── ui/        MainWindow, BufferListView, BufferCard, InlineEditor, SearchBar
├── resources/     icons, napkin.desktop, qss themes
└── tests/
```

`domain/` may depend on QtCore but never on QtWidgets, so the entire model and
every invariant is testable headless in CI.

**Single-instance enforcement** (`QLocalServer` lock): two processes on one
SQLite file is easy to forget and painful to debug. A second launch raises the
existing window.

### Data locations

```
Linux    ~/.local/share/napkin/    ~/.config/napkin/     (XDG)
Windows  %LOCALAPPDATA%\Napkin\
macOS    ~/Library/Application Support/Napkin/
```

Never beside the executable. Never requires root.

---

## 11. Privacy

Unchanged in spirit from v1 §32/§33, plus three gaps that section missed:

- Data dir `0700`, DB `0600`.
- Thumbnails live in Napkin's own data dir, never a shared XDG cache that other
  applications and indexers read.
- **Exclude the data directory from desktop search indexing** (Baloo on KDE,
  Tracker on GNOME). Otherwise everything the user "threw away" becomes
  system-wide searchable — a real leak for an app explicitly designed to hold
  half-considered material.

And stated plainly in the README, so nobody misreads the feature name:
**Keep is retention, not encryption. There is no at-rest encryption.**

No account, no cloud, no telemetry, no analytics, no AI, no external API, no
automatic URL fetching. Invariant 4 says there is no socket-opening code path at
all — that is auditable, unlike a policy.

---

## 12. Performance targets

| Metric | Target |
|---|---|
| Cold start to interactive | < 300 ms |
| `Ctrl+N` to caret ready | < 50 ms |
| Search keystroke to results, 5000 buffers | < 100 ms |
| Idle memory, 5000 buffers | < 120 MB |
| Idle CPU | 0% |

Requires: list virtualization, windowed queries (never `SELECT *` over all
buffers), pre-generated fixed-size thumbnails, lazy full-resolution image
loading. **All of these are Phase 2 architecture, not Phase 10 polish.**

---

## 13. Export

v1 had no export and no backup story. Local-first without an exit is its own
kind of lock-in, and there is no recovery path if the DB corrupts.

- **Export buffer** → a folder: `.txt` per text item, images as files, plus
  `manifest.json` with timestamps, pin/keep flags, and ordering.
- **Export all** → the same, one directory per buffer.

Roughly a day of work; disproportionate trust returned.

---

## 14. Accessibility and error handling

### Accessibility

Keyboard navigation for every action. Visible focus states. Screen-reader labels
(AT-SPI on Linux, which Qt provides). Sensible text scaling. Predictable tab
order. Sufficient contrast in both themes.

**Never encode meaning in color alone.** Pinned and kept states carry an icon and
an accessible label, not just a tint.

### Error handling

Errors are stated in plain language, with the user's content accounted for:

```text
Unable to save image.

The buffer text was saved. The image was not added.

Retry     Cancel
```

No raw stack traces in the UI. Technical detail goes to a log file that contains
no user content (invariant 10). **Never silently discard user content** — if a
write fails, say so and keep what is in memory.

---

## 15. Testing

The domain layer has no QtWidgets dependency, so all of this runs headless in CI.

| Area | Cases |
|---|---|
| Buffer | create, update, delete, pin, unpin, keep, release |
| Lifecycle | sweep skips kept; sweep moves to trash; trash purge; undo restores; confirmed delete of kept |
| Invariant | direct SQL `DELETE` of a kept buffer aborts (trigger) |
| Items | text item, image item, multiple items, ordering, removal |
| Persistence | save, reload, restart, migration forward, WAL recovery |
| Search | text, `source_name`, roll-up to buffer, kept and pinned included, ranking |
| Blobs | store, dedupe on identical paste, retrieve, unlink-when-last-reference, orphan GC, missing-blob handling |
| Drafts | draft with no content writes no row; draft with content writes exactly one |
| Durability | `kill -9` during typing; `kill -9` between blob write and row commit |

Clipboard paste gets an integration test driving `QClipboard` directly — it is
the only remaining platform-dependent surface, so it is the one that must not
regress silently.

---

## 16. Phases

Risk is front-loaded. v1 sequenced the two most platform-dependent features at
Phases 4 and 6, after five phases of UI had been built on the assumption they
would work — and put packaging at the very end, which is a cliff rather than a
phase.

**Phase 0 — Spike. ✅ COMPLETE.** Measured on Arch Linux, KDE Plasma /
Wayland, Qt 6.11.2, GCC 16.2.1, SQLite 3.53.4:

| Risk | Result |
|---|---|
| Clipboard image paste | **Pass.** `image/png` arrives direct and decodes; no transcode needed. |
| Clipboard text paste | **Pass.** `text/plain` alongside legacy `STRING`/`UTF8_STRING`. |
| Format preference order (§4) | **Pass.** `image/png` present ⇒ image wins, as specified. |
| Committed row survives `kill -9` | **Pass**, with WAL + `synchronous=NORMAL`. |
| `journal_mode=wal` persists across restart | **Pass.** |
| Data dir `0700`, DB `0600` | **Pass.** |
| Qt 6 + sqlite3 + CMake + Ninja toolchain | **Pass**, clean build, no warnings. |
| Wayland clipboard needs window focus | **Constraint found** — see §17. |

All three Phase 0 risks are retired. Phase 1 may proceed on this stack.

**Phase 1 — Foundation. ✅ COMPLETE.** Schema v1 with the `guard_kept_delete`
trigger, forward-only migrations on `user_version`, buffer and item
repositories, `BufferService` with draft semantics, single-instance guard, XDG
paths, and 39 test functions across 5 binaries — all passing.

Enforced structurally rather than by convention:

- `napkin_core` links `Qt6::Core` but **not** `Qt6::Widgets`, so the
  domain-has-no-GUI rule is a link error rather than a code-review note.
- The delete guard needs `PRAGMA recursive_triggers=ON`: without it,
  `INSERT OR REPLACE` deletes the replaced row **without firing the trigger**,
  which was a hole straight through invariant 1. The guard stops deletion, not
  an `UPDATE` that clears `kept` first — releasing a keep is a legitimate user
  action, so the trigger is an assertion against automatic *deletion*, not a
  capability model.
- `moveToTrash()` **returns false** for a kept buffer. The only way past it is
  `moveToTrashConfirmed()`, so no UI path can forget to ask (§6).
- Confirmed deletion of a kept buffer **releases the keep** as it trashes. No
  legitimate path therefore ever hard-deletes a `kept = 1` row, which leaves the
  trigger as a standing assertion against service-layer bugs.
- Blob liveness is `SELECT 1 FROM items WHERE blob_hash = ?`, not a refcount, so
  no drift is possible.

Verified against the real on-disk database, not just in-memory: an external
`sqlite3` process running `DELETE FROM buffers WHERE kept=1` is refused with
*"refusing to delete a kept buffer"* and the row survives — acceptance
criterion 17, end to end.

*Bug found and fixed during verification:* `napkin.db` and its `-wal`/`-shm`
sidecars were left at `0644` on a first run, because permissions were applied
before SQLite had created the files. `secureDatabaseFiles()` now runs after
`open()` and covers the sidecars, which carry uncommitted user content.
Regression test: `tests/test_paths.cpp`.

**Phase 2 — Text. ✅ COMPLETE.** Virtualized buffer stack with PINNED/RECENT
sections, draft creation, inline expansion editing, two-timer autosave,
relative timestamps, empty state. 19 new test functions (58 total, 8 binaries).

Measured against §12 on a 5000-buffer database, file-backed, not in-memory:

| §12 target | Measured |
|---|---|
| `listLive(5000)` metadata | **3 ms** |
| Previews for a screenful (12 cards) | **<1 ms** |
| Windowed query at offset 4980 | **2 ms** |
| Idle RSS | **78 MB** (target <120 MB) |

The list holds metadata only — roughly 48 bytes a row, so 5000 buffers is a
quarter of a megabyte. `sizeHint` does no text layout.

> **Corrected after review.** An earlier version of this paragraph claimed
> previews were fetched "lazily per visible row" and that `sizeHint` was O(1).
> Neither was true: `data()` computed the preview *before* the role switch, so
> every metadata read — including the `IsExpandedRole` that `sizeHint` reads for
> **every** row — issued two queries and loaded each buffer's text. A
> 5000-buffer startup executed ~10,000 statements. Metadata roles now return
> before any preview is touched, the preview cache is bounded, and preview text
> is truncated in SQL (`substr(text, 1, 2048)`) and again at 256 characters, so
> pasting a minified bundle no longer makes `elidedText` O(text length) on every
> repaint.

Behaviours that only exist once the UI is wired up, so they are covered by
headless GUI tests driving the real widget tree (`tests/test_editing.cpp`):

- `Ctrl+N` shows a card but writes **no row** until there is content (invariant 5).
- An abandoned empty draft evaporates — it never existed to clean up.
- `Esc` and window-close both flush *before* collapsing, so the debounce window
  is never a data-loss window.
- **The list does not re-sort while a card is expanded.** Autosave bumps
  `modified_at` continuously, so a naive reload would yank the card you are
  typing into to the top. Reloads are deferred and applied on collapse.

*Design change during implementation:* `Ctrl+N` is a `QAction`, not a bare
`QShortcut`. A headless window is never active, so key-chord delivery cannot be
relied on in tests — and the spec wants shortcuts discoverable anyway, which an
action carrying its own label and key hint gives for free.

**Phase 3 — Pin, Keep, Trash. ✅ COMPLETE.** Both flags with drawn indicators,
list-scope keys, context menu, soft delete, undo toast, trash view and the
confirmation flow. 12 new GUI test functions (70 total, 9 binaries).

- **Pin moves the card, Keep does not.** Pinning reloads the list — that is the
  point of it. Keeping refreshes one row in place, so the stack never reshuffles
  for a lifecycle change. Both are asserted.
- **The confirmation cannot be skipped.** `trashRow()` calls `service.trash()`,
  which *returns false* for a kept buffer; the dialog is the only route to
  `trashConfirmed()`. A UI path that forgot to ask would simply fail to delete.
- **Every delete is soft and undoable** for 8 seconds, from a toast, without
  hunting for the trash. A second delete replaces the standing offer — the most
  recent is the one the user most likely meant.
- Pin and Keep are inert in the trash view; `Delete` restores there instead.
- **Empty trash** is available while viewing the trash, behind a confirmation
  that names how many buffers it will destroy.
- Indicators are **drawn vector glyphs**, not an icon theme: a pushpin and a
  bookmark. Deliberately not a padlock — Keep is retention, not security, and
  the icon must not promise otherwise. Each state is also carried in the row's
  accessible label, so nothing is encoded in styling alone.

The header now carries the app name and the trash toggle, with the gap between
them reserved for the Phase 5 search field.

**Phase 4 — Images. ✅ COMPLETE.** Clipboard paste, content-addressed blob
store, thumbnailer, card thumbnails, lightbox, image picker and the
reconciliation sweep. 21 new test functions (91 total, 10 binaries).

- **The §4 preference order is a tested contract.** A `QMimeData` carrying both
  `image/png` and a URL — exactly what a browser's *Copy Image* produces —
  resolves to the image. Paste is intercepted in `QPlainTextEdit` itself, which
  would otherwise drop the image and paste the URL alongside it.
- **Invariant 6 end to end:** the blob is written to a temp file, fsynced, the
  containing directory fsynced, atomically renamed, and only then does the row
  commit. A crash leaves an orphan blob, never a dangling reference.
- **Invariant 7:** rows commit before any unlink, and `reconcileBlobs()` at
  startup reclaims orphans, deletes `.tmp` files from interrupted writes, and
  *reports* rows whose blob has vanished rather than rendering them silently
  blank — a missing image draws a visible placeholder.
- Thumbnails are scaled during decode, so a 4000×3000 photo never lands in
  memory whole, and are cached on disk plus in `QPixmapCache`.
- Migration v1 → v2 verified against a real v1 database: rows preserved, image
  rows backfilled to `image/png`.

Storing an image into an open empty draft promotes it in place rather than
creating a second buffer — the draft invariant survives contact with images.

**Phase 5 — Search. ✅ COMPLETE.** FTS5 with buffer-level roll-up, a persistent
header field, ranked results and a snippet showing why each one matched. 18 new
test functions.

- **Schema v5** adds `items_fts`, an *external-content* table: FTS5 keeps only
  the index and reads the columns back from `items`, so a pasted log is not
  stored twice. Triggers maintain it, which is the price of external content —
  such an index does not maintain itself. A just-pasted item is searchable
  immediately; there is no moment where new content is invisible.
- **`source_name` is indexed beside the text**, because looking for a filename is
  the same act as looking for a word and nobody remembers which column their
  memory lives in.
- **Results roll up to the buffer**, since that is what the list shows: a hit on
  item 3's filename surfaces the whole buffer.
- **What the user typed is not a query language.** Every token is quoted, so
  `AND`, `OR`, `NOT`, `NEAR`, an apostrophe or a stray quote are searched for
  rather than executed or rejected. The final token gets a prefix wildcard, so
  results narrow while a word is still being typed.
- **Trashed buffers are excluded**; pinned and kept ones are not. Search is for
  finding what you have.

**Search narrows both panes.** The list shows the buffers that matched; the
board then shows only the *items* that matched, with the term marked inside the
card text by a `QSyntaxHighlighter`. Finding which buffer matched and then having
to re-read it hunting for the word is half an answer.

A banner says what is hidden — *"2 of 4 items match ‘nginx’"* — with **Show all**
beside it, because the surrounding items are often the context you actually
wanted, and a board that silently hid two thirds of a buffer would look like the
buffer had lost them. Changing or clearing the query keeps you on the buffer you
are looking at rather than throwing you to the top of the restored list.

**5 ms across 2000 buffers**, so it runs on every keystroke behind a 120 ms
debounce that only exists to stop a fast typist re-querying mid-word.

> `MATERIALIZED` in the ranking query is load-bearing. FTS5's `bm25()` and
> `snippet()` only work when the index is the direct subject of the query, and
> SQLite flattens an ordinary CTE into the outer join — which puts them back
> somewhere they refuse with *"unable to use function bm25 in the requested
> context"*.

**Phase 6 — Sweep. ✅ COMPLETE.** The `OLDER` section, the nudge, the review
dialog and sweep-to-trash. 14 new test functions.

§6 has described this lifecycle since v2 and nothing implemented it:
`olderThanCutoff()` was dead code, there were only PINNED and RECENT sections,
and there was no sweep at all. The spec described an app that did not exist.

- **`OLDER`** is a third section below RECENT, drawn a touch quieter. Age changes
  where a buffer sits, never whether it exists — a year-old buffer is still
  there, still searchable, still one click away.
- **The nudge** is one quiet line above the list, and only when at least 12
  buffers are past the cutoff. Napkin tolerates accumulation; the offer should
  feel like a convenience, not a scolding. It can be waved away for the session.
- **The review dialog** shows exactly what it proposes to take, everything
  ticked, and says what it is leaving alone and why. Untick anything you want to
  keep.
- **A sweep trashes, never deletes.** It goes through the ordinary undo toast,
  and the whole sweep is undoable as one action.

> **Placement and lifecycle are different questions, and the code now says so.**
> `isOlder()` excludes pinned buffers — a pinned buffer is not in the recency
> order at all, so it cannot be "older" within it. `sweepableCount()` does *not*
> reuse that: only **Keep** exempts a buffer from a sweep. Conflating them would
> have made pinning a silent second Keep, which is exactly the confusion §3
> exists to prevent. A test asserts a pinned, un-kept, old buffer is offered.

**Phase 7 — Refinement.** Partly done: **menu bar and settings ✅**, link chips,
export and a final accessibility pass outstanding.

The menu bar is **File / Home / Trash / Settings** and carries every action the
application has, because it is the one place a user can go to find out what the
app can do — the header buttons and the key chords are shortcuts *to* these, not
a separate set. **Home → All buffers** is one gesture back to the ordinary view
from wherever you are: out of the trash, out of a search, back to the top.

Settings is deliberately small. Napkin's premise is that you do not configure it,
you throw things at it, so it holds only the choices that change how the app
behaves *over time* — appearance, when a buffer becomes "older", how long the
trash keeps things — and nothing that merely changes how it looks for its own
sake. The lifecycle values are read through `BufferService` from `QSettings`, so
the domain layer picks them up without depending on any dialog.

**Phase 8 — Linux delivery.** `.desktop`, icon, Flatpak and AppImage, optional
tray mode, and the global capture hotkey (see below).

**Phase 9 — Windows and macOS.** Only after Linux is genuinely good.

Packaging was claimed as Phase 0 work and was in fact never started. It is now
**partly** done, and the rest is honestly outstanding.

**Done:** the application has an identity. `resources/napkin-source.png` is the
artwork; the hicolor set (16 → 512) is generated from it and committed, so there
is no build-time image dependency. `resources/napkin.desktop` passes
`desktop-file-validate`. `install()` puts the binary, the desktop entry and the
icons where XDG expects them, verified by installing to a scratch prefix. The
icons are also compiled into the binary, so a build run straight out of the
source tree still has a window and taskbar icon — `setDesktopFileName("napkin")`
previously promised the compositor a file that existed nowhere.

**Still outstanding:** no CI, no Flatpak or AppImage, no `.desktop` MIME
association, and no release process.

---

## 17. Deferred, with intent

Explicitly *not* in v1, but the design does not foreclose them:

**Global capture hotkey.** The real friction is not `Ctrl+N` — it is
alt-tabbing to Napkin at all. A system-wide hotkey opening a small capture
window would be the highest-leverage single addition, and it is Phase 8 rather
than Phase 1 for two reasons: on Wayland it requires the
`org.freedesktop.portal.GlobalShortcuts` portal, whose support is uneven, and
because of the focus constraint measured in Phase 0 (below).

> **Phase 0 finding — Wayland serves the clipboard only to a focused client.**
> A Qt client with no activated window reads an *empty* format list; the same
> client with a shown, activated window reads `image/png` fine. This is a
> Wayland security property, not a Qt bug.
>
> Consequence: a background hotkey handler **cannot** silently read the
> clipboard. The capture window must actually appear and take focus before
> reading. That is acceptable — it is the interaction we want anyway — but it
> rules out a "capture invisibly on hotkey" design, so do not plan for one.

**Drag and drop.** The item model is type-tagged and position-ordered
specifically so a drop handler is additive: it constructs the same items paste
does. Re-add when Linux feels finished.

**A read mode.** Opening a buffer currently means editing it; there is no way to
simply look at a long one. Acceptable for a scratchpad, and surfaced only by
reviewing the master-detail mockup in §7, but worth revisiting once the content
types are richer.

**Generic files.** Deliberately closed. Reopening it means reopening large-file
policy, MIME handling, and the file-manager identity question. Do not.

---

## 18. Acceptance criteria

The happy path, from v1 and still correct:

1. Launch, `Ctrl+N`, type immediately.
2. Paste a screenshot into the same buffer.
3. Paste a URL; it renders as a link chip.
4. Close the application. Reopen. The buffer is there.
5. Pin it. Keep it. Create a dozen throwaway buffers.
6. Search across them.
7. Sweep. Verify the kept buffer is untouched.
8. Undo the sweep. Verify everything comes back.
9. Never once created a title, folder, category, tag, notebook, or account.

And the adversarial path, which v1 omitted entirely:

10. `kill -9` mid-typing → content survives to the last flush.
11. `kill -9` mid-image-paste → no dangling reference; at worst an orphan blob, GC'd on restart.
12. Disk full during a blob write → clear error, buffer intact, nothing half-written.
13. Read-only data directory at startup → refuses to start with an actionable message, never a stack trace.
14. Blob deleted out from under the app → broken-image placeholder, removable, no crash.
15. Second instance launched → existing window raises.
16. 5000 buffers → scroll and search stay inside §12 targets.
17. Direct `DELETE FROM buffers WHERE kept=1` via sqlite3 → **aborts.**

---

## 19. Two crashes from real use, and what they had in common

Both came from the same root: **`editingBuffer_` kept naming a row after that row
stopped being live.** A buffer trashed from the list, or purged by *Empty trash*,
left the canvas still pointing at it.

| Symptom | Cause |
|---|---|
| Pasting after deleting the buffer **crashed the application** | Appending to a purged row violates the foreign key. `DbError` unwound into Qt's event loop, which calls `std::terminate`. |
| A buffer stayed in the list with no items, still showing its old text | Milder form of the same thing: the paste landed *inside* the trashed buffer, so the text accumulated somewhere invisible. |

Two rules now hold, and both are tested:

1. **`currentBufferIsLive()` is checked before any write.** If the row has gone,
   the canvas lets go of it and the next capture starts a new buffer.
2. **No exception may reach the event loop.** `napkin::Application` overrides
   `QCoreApplication::notify()` and catches everything, because every event in a
   Qt program passes through there.

> **An earlier version of this section claimed "every database-writing slot runs
> through `guarded()`". That was false**, and a later audit reproduced two
> `SIGABRT`s to prove it — one of them on the *Delete key*. Wrapping individual
> write calls could never have been enough: a failing **read** during a row
> click, a query issued from inside `paint()`, and `MainWindow`'s own
> construction were all outside every wrapper. The boundary has to be under
> everything, not sprinkled over the paths someone remembered.

> The lesson generalises past these two: a `DbError` escaping a Qt slot is always
> a crash, never an error dialog. The type has existed since Phase 1 and the
> boundary that catches it did not.

---

## 20. Review findings and corrections

An independent adversarial review (security/performance and UI/UX, run as two
separate agents with an explicit brief to find what is wrong and to treat this
document as claims to verify rather than facts) produced the corrections above
and the fixes below. Recording it here because **several claims in earlier
versions of this spec were false, and one test had been written so that it could
not fail.**

| Finding | Severity | State |
|---|---|---|
| SVG could read local files via a bare `href` path | high, privacy | fixed — refused at import; §4 corrected |
| Text silently lost when a save fails, and on close | **critical** | fixed — collapse and close now refuse, and say so |
| Failed saves retried ~3×/second, for ever, in silence | medium | fixed — bounded retry, then a message |
| Undo of a confirmed delete silently dropped the Keep flag | high | fixed — undo restores `kept` and `modified_at` |
| Thumbnails were never deleted; `forget()` was dead code | high, privacy | fixed — the sweep now reclaims thumbnails too |
| `INSERT OR REPLACE` bypassed the kept-delete trigger | high | fixed — `PRAGMA recursive_triggers=ON` |
| Previews computed for every row, not per visible row | high, perf | fixed — metadata roles touch no preview |
| Preview cache unbounded; 253 MB at 5000 rich buffers | high, perf | fixed — bounded, and text truncated |
| `paint()` was O(text length): 153 ms for a 1 M-char line | high, perf | fixed — truncated in SQL and again at 256 chars |
| A 48 KB PNG declaring 20000×20000 was accepted | medium | fixed — 80-megapixel ceiling |
| Failed thumbnails re-decoded on every repaint | medium | fixed — negative results cached |
| Instance socket was world-connectable in `/tmp` | high | fixed — 0700 data dir, `UserAccessOption` |
| Empty-trash dialog counted rows it would not delete | low | fixed — counts what will actually go |
| `Ctrl+N` in the trash created a live buffer shown in the bin | medium | fixed — returns to the live list first |
| `Delete` in the trash *restored* instead of deleting | medium | fixed — Delete destroys (confirmed), `R` restores |
| Timestamps at 2.71:1 contrast; four styles failed WCAG AA | high, a11y | fixed — alphas raised to measured thresholds |
| Hover state was a 1.04:1 change, i.e. invisible | medium | fixed |
| Pin glyph read as a magnifying glass | medium | fixed — redrawn with crossbar and point |
| Three `QAction`s attached to no menu — bindings undiscoverable | high | fixed — ＋New button, ⋯ menu, shortcut sheet |
| Cards stretched to full window width | medium | fixed — 760px measure, centred |
| No accessible names; no initial selection for keyboard users | high, a11y | fixed |
| Test counts in this document were inflated | — | corrected (`PASS` lines counted init/cleanup) |

**Known and not yet fixed**, carried forward honestly:

- The lightbox does not page across a buffer's images with ←/→; each image is
  opened individually from the expanded card.
- No undo for Pin or Keep, and no confirmation that they happened beyond the
  glyph appearing.
- `reconcileBlobs()` runs synchronously on the UI thread, so a very large blob
  store will stall the window during *Empty trash*.
- The multi-instance guard is still best-effort; a real `flock` on the data
  directory would be the correct mutex.
- The undo toast still replaces rather than stacks, and does not name the buffer.

---

## 21. The test that governs every future feature

> Does this make it easier to **put something in**, **find it**, **use it**,
> **keep it**, or **get rid of it**?

If no, it does not go in Napkin. The power is low friction, not feature count.

> **It's just a napkin. Put it down and move on.**
