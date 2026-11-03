# Job registry support

The embedded registry contains 17 jobs. Holzfäller and Bauarbeiter use the same
earnings and absolute Job-Skill formats as the existing jobs and appear in the
existing job overview and detail views.

Holzfäller task and progress messages were verified against a local SA-MP
chatlog from 22 October 2026:

- NPC speaker: `Vince Redwood`.
- `Fälle 5 Bäume, zersäge sie und liefere das Holz ab.`
- `Pflanze Setzlinge. Ziel: 5 (max. 5 pro Auftrag).` (also observed with target 3).
- `Du hast zusätzlich 2 Holz gefunden.` (also observed with amount 1).
- Earnings of `526$` and an absolute Job-Skill value of `146` use the shared formats.

The server also writes job earnings with German thousand separators, for example
`2.940$`. All job earnings rules accept both grouped and ungrouped amounts.

Both reward trees use the thresholds supplied by the user on 24 October 2026.
They appear in the existing skill-tree cards and use the saved Job-Skill value.

| Job | Points | Reward |
| --- | ---: | --- |
| Holzfäller | 50 | 5 Bäume fällen |
| Holzfäller | 100 | Holzfäller Skin freischalten |
| Holzfäller | 250 | Privatfahrzeug: Bobcat |
| Holzfäller | 500 | +10% Lohn |
| Holzfäller | 1000 | +25% Lohn |
| Holzfäller | 1500 | Job-Achievement freigeschaltet |
| Bauarbeiter | 25 | Großbaustellen werden freigeschaltet |
| Bauarbeiter | 100 | Bauarbeiter Skin freischalten |
| Bauarbeiter | 500 | +10% Lohn |
| Bauarbeiter | 750 | Business: Blueberry Construction |
| Bauarbeiter | 1000 | +25% Lohn |
| Bauarbeiter | 1500 | Job-Achievement freigeschaltet |

No Bauarbeiter-specific NPC/task messages were available in the inspected
evidence. Add verified messages to `res/jobs_registry.json` when they are available.
