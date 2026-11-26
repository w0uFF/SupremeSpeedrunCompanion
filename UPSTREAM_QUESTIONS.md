# Upstream questions (not sent)

High priority, group for a concise maintainer consultation only after independent research.

- Does Display Config 1.8.0 have a supported native extension loading path outside DllMain? If not, would a narrow versioned C ABI be acceptable? What lifetime and game-thread constraints apply?
- Are there known native menu registration, selected button dispatch, actual track/rider/board/weather identity and race start/finish/reset/restart observations? Any existing research notes?
- Are native replay timing units, checkpoint semantics and coordinate layout already confirmed against results?
- Would optional stable-channel launcher integration be welcome once a tested companion exists? We propose independent release manifests, no mid-run update and safe rollback.

No upstream approval is assumed. Do not contact the maintainer automatically.

## T14 focused questions (not sent)

- Is there an existing post-loader-lock/game-thread extension entry point or preferred design? Source review found pinned WinMM proxy, lazy rd_config.txt resolver and optional Injector.exe path, but no verified native contract. Who owns Injector.exe and its lifetime?
- Menu: Get_Page_Names/Get_Button_Names enumerate legacy vectors; Button_Pressed and Time Attack sequence dispatch appear separate. Is there a known registration/dispatch extension approach that preserves controller navigation and avoids legacy STL ABI mismatch?
- Which event or result field reliably represents actual countdown release, finish and displayed IGT? Enter/Restart/Stop/Reset candidates are too coarse. Are there notes on active selection versus menu defaults?
- Replay: is terminal record excluded by design? What is the actual coordinate frame, and are header ticks sufficient for displayed IGT? Existing upstream geometry code handles Forest Medium/Hard ambiguity; we will not assume identity from positions alone.

Consult only if useful, in a concise grouped message via the owner. No upstream endorsement or permission assumed.

## T15 follow-up (not sent)

- Preferred extension boundary: do you already support optional native initialization after the loader lock? We found only file-overlay lazy resolution and an optional external Injector.exe helper. Would an explicit host lifecycle contract be acceptable? No need to answer details already documented elsewhere.
- Result presentation: do you know the original finish producer and final displayed time field/units? Get_Text_Results returns a borrowed object and the presentation consumer clears a flag also initialized before a race. We will not use that flag as finish.
- Menu registry: Button_List parsing and Get_Button_Names appear to access the same owner field; actual dispatch still separate. Any known safe registration mechanism or lifetime notes?
