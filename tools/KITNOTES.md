# xplkit backlog — v3 candidates

Collected from the artist agents' final reports (Greek town cohort,
2026-08-31). The v2 absorption already took sweep/rod/catmull/polyhedra/
noise/blob/flat_ring from the first cohort; this is what the second
cohort hit. Their own workarounds live in assets/greenkit.py (vegetation)
and assets/gtownkit.py (architecture) — absorb from there, don't rewrite.

## Consistency bugs / traps
- `blob` keys its displacement on *direction*, not world position — two
  same-salt lumps twin visibly wherever placed. Everything else in the
  kit keys on position; make blob match (biggest gap reported).
- `Build.grid` winding is a silent trap: wrong corner order → normals
  point down → dark geometry that looks fine in source. Wants an
  `outward=<point>` guard or a mean-normal debug report. Bit three
  different agents (hedge, house2 roof, vine leaves).
- `ledge()` hardcodes n=(0,0,1): soffits (cornice undersides) shade as
  up-faces; it is also origin-centred only. See gtownkit `soffit()`.
- `catmull` has no arc-length reparametrisation — uneven control points
  give uneven station density vs per-station radius lists.

## Missing primitives (implementations exist in the two kits)
- `wall()` with real openings + `opening()/lintel()/shutters()` — unlit
  engines can't fake windows with dark quads (gtownkit).
- gradient `panel()/relief()` — non-linear vertical gradient on one wall
  quad as a cheap strip band (gtownkit).
- hard cast-shadow fields: `sun_shadow()` (cylinders/colonnades),
  `wall_shadow()` (planes) — ao_spheres is too soft for architecture
  (gtownkit).
- `flight()` — straight stair; existing steps() idea is concentric
  (gtownkit).
- `ground_flare()` — trunk/column ground-contact flare; used on 5 of 7
  vegetation pieces (greenkit).
- caps for `sweep`/`rod` (tube has them; swept tips are open holes).

## API asks
- position-taking colour callables for `sphere/tube/revolve/loft` (they
  build P internally and never expose it before shading; `at(P, f)`
  can't help). Third cohort-wide request for position-keyed colour.
- per-occluder self-shadow weight in `ao_spheres` (`self_k`).
- `Rig`: per-material fill tint (cool fill turns marble blue against
  warm masonry); `hsv()` exists now, keep.
- `preview(..., radius=)` override in xpl.py for metre-scale assets
  (auto-normalise renders an 8.6m cypress tiny).
- xpl.py preview is painter-sorted: large near-planar quads (soffits)
  paint over farther geometry — fine on the PS2 z-buffer. A z-buffer
  preview mode would remove the false alarms (two agents built scratch
  z-renderers).

## Engine note (not kit)
- clip-path bug: huge triangles at certain view angles render a
  mostly-black frame; repro in clipbug.scene + assets/ground_coarse.xpl
  (suspect clipVertLimitTS in src/vu1/im3d.dsm — see its TODO).
  Workaround everywhere else: tessellate so no triangle is huge.
