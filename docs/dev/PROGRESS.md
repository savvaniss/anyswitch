# Progress reporting

[`tools/site/progress.py`](../../tools/site/progress.py) computes the progress numbers; [`tools/site/build_site.py`](../../tools/site/build_site.py) turns them into the Pages site. The [Pages workflow](../../.github/workflows/pages.yml) deploys the site after pushes to `main` that change the counted files or the site. The [Progress report](../../.github/workflows/progress-report.yml) and [Progress comment](../../.github/workflows/progress-comment.yml) workflows compare the base and head of a pull request and comment the difference.

## What is counted

Everything is read from the repository without building it.

| Number | Source |
|---|---|
| SDK exports per module | `NX_GUEST_FN` and `NX_GUEST_VAR` definitions in `runtime/src/<module>/` |
| Open gaps per module | bullets starting with `- **` in `docs/dev/debt/<module>.md` |
| Corpus cases | entries in `tests/corpus/cases.json` |
| Patch classes | enumerators of `PatchKind` in `core/relinker/include/nx/relinker/patcher.hpp` |
| Roadmap | milestones and statuses in `docs/roadmap.json` |

## What the numbers do not say

There is no public list of every SDK function, so there is no denominator: an export count measures what exists, not a share of what titles need. Per-title needs come from `tools/phase0/symbol_inventory.py`, run by people on titles they own. Exports are counted whether or not they are complete; partial behaviour is in the debt files. None of the numbers say a title is playable.

## Outputs

`progress.py OUT` writes `progress.json`, `badge-exports.svg`, `badge-corpus.svg`, `badge-roadmap.svg` and `progress.svg` (the roadmap map). `progress.json` holds no timestamps, so two runs on the same tree are identical.

## Comparisons

`progress.py --compare BASE.json HEAD.json` prints a Markdown table of the numbers that changed, roadmap status changes, and added or removed exports (readable names for nested C++ symbols and operators). It prints nothing when nothing changed, and the comment workflow then removes its comment.

## Verification

```bash
python3 -I tools/site/progress.py
python3 -I tools/site/progress.py --json
python3 -I tools/site/build_site.py build/site
```
