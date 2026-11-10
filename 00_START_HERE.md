# FaseGroove Arranger — Codex execution pack
Baseline checked: v0.43.2, repository `wsx79/fasegroovearranger` (always verify current HEAD before work).

## Product decisions (non-negotiable)
- **Sung Vocal Studio FIRST, then existing Vocal Chops**. Voice generation/editing must precede new mixing/mastering features.
- Harmony, musicality, arrangement and 15 genre engines are already satisfactory with appropriate VST instruments and user samples: do not alter without a reproduced defect.
- Built-in guide sounds are lightweight previews, **not** targets for production-grade sound design.
- Keep two separate workflows: Vocal Studio (lyrics+MIDI singing) and Vocal Chops (sample slicing/triggering). Reuse existing piano roll, vocal chops, audio tracks/takes, mixer, EQ, export, plugin host, automation and project persistence. Inspect actual current implementation first.
- Preserve backward compatibility of `.fga` projects and deterministic existing MIDI generation.
- No fabricated test results, quality claims, model capabilities, or licensing assumptions.

## Execution
1. Read `00_START_HERE.md`, `01_ENGINEERING_RULES.md`, `02_STATUS.md` and **only the current phase file** under `phases/`.
2. Inspect `git status`, branch, HEAD and relevant code. If repo version differs, adapt plan and note differences.
3. Work on one phase only. Within phases, use small independently testable substeps.
4. At the end update `02_STATUS.md` and only relevant changelog sections. Report <=200 words.
5. Commit only after relevant tests pass; **do not push, tag, release or deploy without explicit authorization**. If tests fail, do not create a completion commit.
6. Advance to the next phase only when user authorizes it. Never automatically execute the entire roadmap.

## Phases
1. `phases/01_VOCAL_STUDIO.md` — original singing from lyrics + MIDI; first priority.
2. `phases/02_VOCAL_CHOPS.md` — existing sample-based chops; targeted audit and improvements.
3. `phases/03_AUDIO_ENGINE.md` — realtime and plugin reliability.
4. `phases/04_EXPORT_TRUE_PEAK.md` — export, metering and limiting verification.
5. `phases/05_DYNAMICS.md` — native dynamics if missing.
6. `phases/06_SIDECHAIN.md` — extend existing sidechain safely.
7. `phases/07_MIX_ASSISTANT.md` — local non-destructive guidance.
8. `phases/08_MASTERING_ASSISTANT.md` — mastering workflow after voice and mix.
9. `phases/09_RELEASE_GATE.md` — regression and release readiness.

**IMPORTANT:** Proposed milestone names are organizational, not automatic version bumps. Preserve current release numbering unless authorized.
