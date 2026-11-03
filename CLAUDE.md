# Enclave: instructions for AI agents

Enclave is a UE 5.8 C++ multiplayer FPS (module `Source/Enclave`). It's a labor
of love and a portfolio piece that could become a real game, so the right design
matters more than speed.

## Start here

- [Documents/ProjectStatus.md](Documents/ProjectStatus.md): what's done, what's
  in progress, what's next. Then read the plan of the milestone in progress.
- [Documents/DevNotes.md](Documents/DevNotes.md): environment, build and test,
  conventions, design rules, gotchas.

## How we work

- **The user sketches; you explain and review.** For new systems, explain the
  next step (what it does, where it goes, the engine facts, the decisions to
  make), then let the user write it. When they say "check my code", review it
  and report issues, most serious first; don't rewrite it. When they say
  "implement" (usually mechanical refactors, or a design already worked out
  together), do it and they review the diff.
- **To compare their sketch with your fixes**, commit their sketch first as a
  `wip(...)` commit, then make your edits on top.
- **Commit only when asked.** Use `type(scope): summary` with a bullet body.
- **Tangents:** game dev rabbit holes are easy. When one comes up, ask "now or
  later?" instead of diving in. Don't add "parking lot" sections to milestone
  plans.
- **Explain with sources.** SSR and networking are areas where the user relies
  on your knowledge: check engine facts in the UE 5.8 source
  (`D:\UnrealEngine\Versions\UE_5.8`) and cite file and line, or point to
  references, rather than asking for trust.
- **Never kill the user's editor.** If it's open, Live Coding blocks the build:
  ask them to close it.
- **Keep the docs current.** When a milestone item or moment lands, update
  `ProjectStatus.md` and the milestone plan in the same commit or the next one.

## Design rules the user cares about

- **No concept in a class that doesn't apply to every class below it.** A team
  field in a free-for-all mode, or a winning player in a team mode, is a design
  leak. Check this before proposing a "convenient" shape; the user will catch it.
- Recommend the better long-term design even when it costs more, and say plainly
  when something is a shortcut.
- Past stage docs and gates are reference points, not requirements.
- More rules (hostility, server clock, ordering) are in DevNotes.md.
