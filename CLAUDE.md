# CLAUDE.md — notes for future sessions on this repo

A KRunner plugin that answers `ai <query>` via any OpenAI-compatible
chat completions endpoint (defaults to OpenRouter). Two shared objects:
the runner itself and a Qt Widgets KCM for settings.

## Target

KF6 / Plasma 6 (`KRunner::` namespace, `find_package(KF6 …)`, install
namespace `kf6/krunner`). The docs under
[resources/develop-kde-org/](resources/develop-kde-org/) are the **KF5
era** tutorial — useful for shape, wrong for namespaces and cmake.
Translate `Plasma::` → `KRunner::`, `KF5::` → `KF6::`, `kf5/krunner`
→ `kf6/krunner` when consulting them.

## Build / install loop

System-wide:

```sh
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install

# pick up code changes:
kquitapp6 krunner   # next Alt+Space respawns it
```

For user-local installs (or immutable hosts like Bazzite / Silverblue /
Kinoite where `/usr` is read-only): build inside a toolbox / distrobox
since you can't `dnf install` the dev deps on the host, then install to
`~/.local` and add `QT_PLUGIN_PATH=$HOME/.local/lib64/plugins` via
`~/.config/environment.d/`. See [README.md](README.md) for the full
recipe.

Build deps (Fedora 41+ package names):
`cmake extra-cmake-modules kf6-krunner-devel kf6-kconfig-devel
kf6-ki18n-devel kf6-kcoreaddons-devel kf6-kcmutils-devel
kf6-kconfigwidgets-devel qt6-qtbase-devel gcc-c++ make`.

## How the plugin gets discovered

Plasma 6 launches krunner with no `QT_PLUGIN_PATH`, so user-local
plugins are invisible unless the path is added via
`~/.config/environment.d/`. `systemd --user` reads that directory at
session start, so a logout + login is required after editing it. For
one-off testing without logout:

```sh
QT_PLUGIN_PATH=$HOME/.local/lib64/plugins krunner --replace &
QT_PLUGIN_PATH=$HOME/.local/lib64/plugins kcmshell6 kcm_krunner_llm
```

Final install paths (user-local):
- `~/.local/lib64/plugins/kf6/krunner/krunner_llm.so`
- `~/.local/lib64/plugins/kf6/krunner/kcms/kcm_krunner_llm.so`

## KF6 API quirks worth remembering

- `KRunner::AbstractRunner` constructor takes `(QObject *parent, const KPluginMetaData &)` — **no `QVariantList args`** like the KF5 docs show.
- `QueryMatch::Type` and `setType()` are **gone in KF6**. No more
  `InformationalMatch`. Use `setMultiLine(true)` for long answers and
  copy to clipboard yourself in `run()`.
- `KCModule` ctor in KF6 is `(QObject *parent, const KPluginMetaData &)`;
  build child widgets parented to `widget()`, not `this`.
- The KCM install namespace is `kf6/krunner/kcms` (KF5 was `kf5/krunner/kcms`).

## Markdown rendering

LLM answers come back as markdown. milou's
[ResultDelegate.qml](file:///usr/lib64/qt6/qml/org/kde/milou/ResultDelegate.qml)
renders multi-line text as Qt's QML `Text.StyledText` — a *simplified*
HTML4 subset that ignores inline CSS, so you cannot drop
`QTextDocument::toHtml()` output in (its `<span style="font-weight:700">`
spans render unstyled). The path that works:

1. Parse the markdown with `QTextDocument::setMarkdown()` (Qt uses md4c
   internally, CommonMark-conformant).
2. Walk `QTextBlock` / `QTextFragment` and emit only the documented
   StyledText subset: `<b> <i> <code> <pre> <a> <s> <u> <br>`.
3. Map markdown blocks: fenced code → `<pre>`, list items → `• ` or
   `1. ` prefix, headings → `<b>` (the `<hN>` tags would balloon the
   row height).
4. Set `setMultiLine(true)` so the delegate switches to StyledText
   rendering.

`setData()` still holds the raw markdown so pressing Enter copies the
unstyled source to the clipboard.

## Threading model

- `match()` runs on a krunner worker thread, **not** the GUI thread.
  Synchronous HTTP via `QNetworkAccessManager` + a local `QEventLoop`
  inside `match()` is safe and is how this plugin works.
- Debounce is the natural krunner pattern: at the top of `match()`,
  sleep in small chunks polling `context.isValid()`. When the user
  types another letter, the prior context invalidates and we bail.
  No external debouncer.
- `run()` is on the GUI thread — clipboard access is safe there.

## Configuration

All settings live in `~/.config/krunnerrc`, group `[Runners][krunner_llm]`:

| Key          | Default                          | Notes                              |
|--------------|----------------------------------|------------------------------------|
| `apiKey`     | *(empty → "no key" message)*     | Plain string; no env-var fallback. |
| `apiUrl`     | `https://openrouter.ai/api/v1`   | Base of an OpenAI-compatible chat API. `buildEndpoint()` appends `/chat/completions` and tolerates trailing slashes or full-URL input. |
| `model`      | `deepseek/deepseek-v4-flash`     | Provider-specific slug.            |
| `debounceMs` | `600`                            | Polled in 25 ms chunks.            |
| `trigger`    | `ai`                             | The metadata regex assumes `^ai\s` — change it in [llmrunner.json](llmrunner.json) if you rename the trigger. |

Only `Authorization: Bearer <apiKey>` and `Content-Type: application/json`
are sent — no OpenRouter app-attribution headers, no `HTTP-Referer`. If
you ever want to opt back in to OpenRouter's leaderboard, the place to
add `HTTP-Referer` / `X-OpenRouter-Title` is inside `askLlm()` in
[llmrunner.cpp](llmrunner.cpp); they were removed deliberately for
privacy.

The KCM reads/writes the same group via `KSharedConfig::openConfig("krunnerrc")`.
Saving from the KCM triggers krunner's `reloadConfiguration()` automatically.

## Don't

- Don't put the API key in source, env, or `environment.d` — KCM only.
  (Past incident: it leaked into a session transcript.)
- Don't call `reply->readAll()` after `reply->abort()` — emits
  `QIODevice::read: device not open`. Check `error() == OperationCanceledError`
  first.
- Don't trust the cloned docs for cmake/install paths — they're KF5.
- **Don't pass `QTextDocument::toHtml()` to `setText` on a multi-line
  match.** Its output is HTML4 with inline-CSS `<span style="…">` for
  bold/italic/etc. Qt QML `Text.StyledText` ignores CSS styling, so the
  result renders as plain prose. Walk the QTextDocument and emit
  explicit `<b>` / `<i>` / `<code>` / etc. tags instead — see the
  "Markdown rendering" section above.
- **Don't try to update an already-shown match by re-adding with the same
  id.** We tested this with `X-Plasma-Runner-Unique-Results: true` to add
  a "Thinking…" placeholder that would be replaced by the real answer.
  Both `addMatch` calls return `true`, `context.isValid()` stays `true`,
  and the answer arrives in full — but KRunner's results model dedupes
  at display time and keeps the **first** id it saw. There is no
  public signal to mutate an existing match. Streaming and progress
  indicators are therefore not feasible without UI-side work in KRunner
  itself. Just block in `match()` and let the answer appear when ready.

## File map

- [CMakeLists.txt](CMakeLists.txt) — both targets in one file.
- [defaults.h](defaults.h) — shared default values + config-group constants, included by both the runner and the KCM. Bump versions / change defaults here so the runner and KCM can't drift.
- [llmrunner.h](llmrunner.h) / [llmrunner.cpp](llmrunner.cpp) — the runner.
- [llmrunner.json](llmrunner.json) — runner metadata, declares the trigger regex and points at the KCM.
- [kcm/kcm_krunner_llm.{h,cpp,json}](kcm/) — settings panel.
- [resources/](resources/) — gitignored container for vendored reference material. Currently holds [resources/develop-kde-org/](resources/develop-kde-org/), the KF5-era KDE krunner tutorial.
