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

### The visual system

Tokenised in `src/ui/Tokens.h` rather than scattered as literals, so contrast is
a property of the system instead of a thing each call site gets right or wrong.
Every readable alpha is a measured threshold over `QPalette::Base` against
Breeze Light, the harsher theme: **255** body, **170** secondary (5.06:1),
**161** tertiary — timestamps, captions, section labels **and placeholders**,
which are text and get no exemption. **126** is the floor for a non-text
affordance carrying meaning; anything at or below 90 is decorative and never the
sole indicator of a state.

Three deliberate moves, from a full design pass (`docs/UX_TWO_PANE.md`):

- **Canvas body is two points larger than list text.** The list is chrome, the
  canvas is content, and size is the cheapest way to say so.
- **Text blocks have no border and no fill at rest.** A text item must not look
  like a form field, because it is not one — it is the content. What makes a
  borderless block read as an *object* is a 3px **gutter rail** to its left,
  empty at rest, solid when selected or being edited. Editing deliberately has
  no fill: a wash behind text you are actively reading degrades it, and the
  rail, border and caret are three signals already. Every state changes exactly
  two things, never three.
- **Both panes sit on `Base`; the gutters sit on `Window`.** The canvas is one
  large sheet of paper, the list cards are small sheets, both on the same desk.
  No tint difference, no gradient, no shadow anywhere.

Images draw at the pane width, **never upscaled** — a 200×140 favicon draws at
200×140, because stretching a small image to fill a column is the fastest way to
make a UI look cheap — and are capped at 560px tall so a phone screenshot cannot
own the whole canvas. One caption line beneath: filename or format, dimensions,
size, and *animated* when it moves. The filename-above-a-thumbnail row it
replaces was a file-manager row, not a picture.

### Items are selectable objects

The hard part is that a text block must be both a selectable object and an
editable field. Resolved by making a bare click mean the obvious thing for what
is under it:

| Gesture | Meaning |
|---|---|
| Click on text | place the caret (edit) |
| Click on an image | select the block — an image has no caret to mean instead |
| Ctrl / Shift + click | select the block, never place a caret |
| `Esc` while editing | leave the text, select its block |
| Click empty canvas | clear the selection |
| `Ctrl+A` | select every item (not the unwritten composer) |

`Ctrl+C`, `Ctrl+X` and `Delete` act on the selection when the canvas has focus,
and never while a caret is in a text block — there, they mean what they always
mean. A single selected image copies as an **image**, so it pastes into anything;
any other selection copies as text, joined in document order.

Deleting every item in a buffer trashes the buffer itself: an item-level delete
that leaves an empty husk behind is just litter. That goes through the ordinary
undo toast.

> **Item removal does not unlink blobs, deliberately.** An earlier version
> deleted the rows and reclaimed the files in one step, so undoing inside the
> 8-second window restored a buffer whose items no longer existed — measured at
> **0 items recovered out of 4**. Removal now captures the items, restores them
> at their original positions on undo, and leaves the files alone; the startup
> sweep reclaims them once undo is no longer on offer. The toast takes a closure
> rather than a `BufferId`, so buffer-level and item-level undo share one widget.

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

### Editing: inline expansion

v1 specified a card stack and separately specified "focus moves to an editor,"
and never connected them. **Decision: cards expand in place.**

```
click                →  selects only
double-click / Enter →  expands into an editable region, list position frozen
Esc                  →  collapses, list re-sorts
```

> **Selection and opening are separate gestures.** An earlier build opened a
> buffer on a single click, which made a card impossible to merely *select* —
> so pinning, keeping or deleting one with the mouse meant opening it first and
> closing it after. Opening is the deliberate second gesture.

### The expanded card shows every item

A buffer is a screenshot *and* a command *and* a URL kept together; an editor
that shows only a text box makes that concept a lie. Expanding renders the
buffer's items in order:

- **text items** as editable regions, each autosaved independently;
- **image items** as a thumbnail with name, dimensions, size and an *animated*
  marker, double-clicked for the lightbox, with a remove control;
- a **trailing composer** — always somewhere to type at the end, costing nothing
  until it has content (invariant 5).

A collapsed card shows up to three thumbnails and an overflow count, so a buffer
holding several images does not pretend to hold one.

No modal, no second pane, no navigation model to learn. This matches the "pieces
of paper on a desk" metaphor better than master-detail, and it means there is
exactly one screen in the entire application.

> **Reviewed against a master-detail mockup and upheld** (`docs/UI_suggestion.png`).
> A two-pane proposal
> (narrow list rail, large preview pane) was considered and rejected. The cost
> is not aesthetic: two panes add a navigation model — which pane has focus, two
> scroll positions, focus ping-pong on every interaction — to an application
> whose entire premise is zero friction.
>
> The specific losses in that layout were: every row elided to a single
> truncated line; **timestamps absent entirely**, on a surface organized by
> recency; no home for the PINNED/RECENT split, which makes pinning invisible;
> and a list that read as one row per *item* rather than per *buffer*, which
> would delete the concept that lets a screenshot, a command and a URL stay
> together as one thought.
>
> Three ideas from it were adopted: a **persistent search field** in the header
> rather than a hidden `Ctrl+K` (Phase 5); **thumbnails in the card** (Phase 4);
> and the legitimate problem it exposed — a 2560×1440 screenshot is cramped in a
> card — solved by the **lightbox** already planned for Phase 4, which buys the
> full-size view without making every text buffer pay for a permanent pane.

### Two latent bugs from v1, fixed here

**Sort thrash.** Recent buffers sort by `modified_at`, and autosave updates
`modified_at` continuously — so the buffer you are typing into would jump to the
top of the list as you type. *Fix: list position is frozen while a buffer is
expanded; re-sort happens on collapse.*

**Empty buffers.** v1 said abandoned empty buffers "can be discarded," without
defining abandonment. *Fix: `Ctrl+N` creates an in-memory **draft**. No row is
written until the first non-empty content exists (invariant 5).* An abandoned
draft evaporates because it never existed. No cleanup rule needed.

### Search

`Ctrl+K` filters the same list in place rather than opening a separate screen.
One mental model, one widget, matches are highlighted inline.

### Keyboard

v1's shortcuts collided with the editor and with OS conventions (`Ctrl+P` is
Print everywhere; `Ctrl+D` for Delete sat next to `Ctrl+N` with no undo). Scoped
model instead:

| Scope | Key | Action |
|---|---|---|
| Global | `Ctrl+N` | New draft |
| Global | `Ctrl+K` | Search |
| Global | `Ctrl+Shift+I` | Add image… |
| Global | `Esc` | Collapse / clear search |
| Editor | `Ctrl+V` | Paste |
| List focus | `Enter` | Expand |
| List focus | `p` | Pin / unpin |
| List focus | `k` | Keep / release |
| List focus | `Delete` | Move to trash |
| List focus | `/` | Search |

Bare letters only fire with list focus, so they can never conflict with typing.
`Delete` rather than `Ctrl+D` — standard, and much harder to hit by accident.

### Visual direction

The visual language should communicate: **minimal, quiet, tactile, temporary,
useful.** Think paper, desk, scratchpad — but avoid literal skeuomorphism. The
name Napkin does not require a napkin graphic anywhere. A restrained modern UI
carries the metaphor on its own.

Buffers may resemble cards, but must not look like modern dashboard cards.

| Avoid | Use |
|---|---|
| Heavy shadows | Whitespace |
| Large corner radii | Subtle separators |
| Gradients, colorful fills | Restrained borders |
| Oversized icons | Clear typography |
| Dense metadata | Quiet hover states |
| Decorative chrome | Small pin/keep indicators |

> **The buffer is content, not a UI widget.** The content must visually dominate.

Themes: Light, Dark, System. Nothing more elaborate. Avoid pure black and pure
white; prioritize text contrast, focus visibility, readable timestamps.

### Empty state

```text
+---------------------------------------------+
|                                             |
|                  Napkin                     |
|                                             |
|           Put something here.               |
|                                             |
|             Ctrl+N to begin                 |
|                                             |
+---------------------------------------------+
```

No onboarding sequence, no tour, no sample content. The empty state disappears
the moment the first buffer has content.

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

**Phase 5 — Search.** FTS5, buffer-level roll-up, inline filtering, highlighting,
and the persistent header search field adopted from the §7 mockup review.

**Phase 6 — Sweep.** Older section, nudge, review dialog, sweep-to-trash, trash purge.

**Phase 7 — Refinement.** Link chips, themes, accessibility pass, settings, error
handling, export.

**Phase 8 — Linux delivery.** `.desktop`, icon, Flatpak and AppImage, optional
tray mode, and the global capture hotkey (see below).

**Phase 9 — Windows and macOS.** Only after Linux is genuinely good.

Packaging is wired up in Phase 0 and stays green throughout. It is never a phase
you arrive at.

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

## 19. Review findings and corrections

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

## 20. The test that governs every future feature

> Does this make it easier to **put something in**, **find it**, **use it**,
> **keep it**, or **get rid of it**?

If no, it does not go in Napkin. The power is low friction, not feature count.

> **It's just a napkin. Put it down and move on.**
