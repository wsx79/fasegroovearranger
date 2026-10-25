# Piano regeneration audit — 0.18.0

48 consecutive track regenerations in each of the 13 genres, with an eight-bar Main clip. Seed sequence: `mix(7919U * (take + 1))`, using the core's integer mix. Compare the right-hand attacks over the 32-step source motif; holds, ghost notes and accent/case differences are ignored. Fewer than six changed positions counts as a similar pair. This does not credit transposition, velocity, strum or humanisation as rhythmic variety.

The Arranger draws up to 32 candidates from the existing Piano genre generator. It accepts a candidate with at least six changed positions and 35% symmetric difference over the union of attacks, or retains the candidate with the most changes if the genre vocabulary is narrower. The chosen motif seed is saved and shared across the track's sections. Applying controls or loading a project does not draw again; locked grid segments are copied intact. No piano-engine source is changed.

| Genre index | Before | After |
|---|---|---|
| 0 | distinct 36/48, similar consecutive 7/47, mean changes 11.7234/32 | distinct 38/48, similar consecutive 0/47, mean changes 13.2128/32 |
| 1 | distinct 43/48, similar consecutive 22/47, mean changes 5.74468/32 | distinct 45/48, similar consecutive 0/47, mean changes 7.23404/32 |
| 2 | distinct 40/48, similar consecutive 26/47, mean changes 4.85106/32 | distinct 43/48, similar consecutive 0/47, mean changes 6.76596/32 |
| 3 | distinct 44/48, similar consecutive 5/47, mean changes 8.74468/32 | distinct 45/48, similar consecutive 0/47, mean changes 9.12766/32 |
| 4 | distinct 47/48, similar consecutive 1/47, mean changes 14.8085/32 | distinct 47/48, similar consecutive 0/47, mean changes 15.0638/32 |
| 5 | distinct 44/48, similar consecutive 4/47, mean changes 12.6596/32 | distinct 43/48, similar consecutive 0/47, mean changes 13.1702/32 |
| 6 | distinct 43/48, similar consecutive 17/47, mean changes 6.44681/32 | distinct 45/48, similar consecutive 0/47, mean changes 7.42553/32 |
| 7 | distinct 43/48, similar consecutive 8/47, mean changes 7.44681/32 | distinct 43/48, similar consecutive 0/47, mean changes 8.42553/32 |
| 8 | distinct 44/48, similar consecutive 0/47, mean changes 15.5745/32 | distinct 44/48, similar consecutive 0/47, mean changes 15.5745/32 |
| 9 | distinct 36/48, similar consecutive 7/47, mean changes 12.1064/32 | distinct 38/48, similar consecutive 0/47, mean changes 13.5957/32 |
| 10 | distinct 43/48, similar consecutive 8/47, mean changes 7.76596/32 | distinct 44/48, similar consecutive 0/47, mean changes 8.82979/32 |
| 11 | distinct 33/48, similar consecutive 43/47, mean changes 3.76596/32 | distinct 34/48, similar consecutive 1/47, mean changes 6.2766/32 |
| 12 | distinct 36/48, similar consecutive 7/47, mean changes 12.1064/32 | distinct 38/48, similar consecutive 0/47, mean changes 13.5957/32 |

The Chill vocabulary remains deliberately sparse; one pair out of 47 is closer than six attacks. Core regressions also compare consecutive take onsets for every genre and verify that zero-seed APPLY reproduces all stored notes, locks and untargeted clips.
