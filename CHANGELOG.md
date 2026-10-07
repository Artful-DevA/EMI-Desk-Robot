# Changelog

This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

## 2026-10-07 - Eye-scale clock morph

### Changed

- kept the timer at its current readable size
- replaced the font-based large clock with a custom 7-segment clock
- clock now occupies roughly the same visual footprint as EMI's eyes
- eyes close into thin bars before the clock appears
- the bars shrink while the clock reveals from the center
- reverse transition turns the clock back into eye-lines and then reopens EMI's eyes
- random valid HH:MM demo time still appears about 12 seconds after boot

### Documentation

- notes are now explicitly session-only transcription
- raw audio is never written to disk
- note-mode Markdown can be written directly to an Obsidian vault
- project-awareness boundaries are explicitly documented

## 2026-10-07 - Larger timer + random clock transition

### Changed

- enlarged the timer so it is readable at a glance
- kept the timer in the top-right corner
- removed the tiny-font rendering that could look like stray punctuation
- clock transition now always shows a clearly visible random HH:MM test time
- large clock draws its own colon dots
- clock reveal/hide expands symmetrically from the center
- feature inventory now uses the privacy principle "functional memory, not intimate memory"
- music-learning design now favors coarse preference categories instead of permanent exact counts

## 2026-10-07 - Attention bid + clock transition test

### Changed

- reduced pet-stroke partial eye closure again to avoid an over-squinted expression
- added a prototype attention bid using two deliberate blinks because blinking is visually noticeable
- attention bids are rate-limited and reset when EMI is petted
- added a one-shot eyes-to-clock-to-eyes animation approximately 12 seconds after boot
- clock animation uses a fixed 12:34 test value until real network time exists
- miniature timer hides during the clock animation
- direct touch interaction cancels the clock animation immediately

### Documentation

- added a canonical docs/FEATURES.md inventory
- documented bounded mood/rest design
- documented privacy boundaries and memory decay
- documented class reminders, phone notifications, multilingual goals, music preference learning, and special-date handling

## 2026-10-07 - Gentler petting + mini timer UI test

### Changed

- reduced per-stroke eye closure so petting looks contented rather than squinty
- pet-stroke closure now starts around 30% and caps around 42%
- shortened the pet-stroke animation slightly
- added a tiny 10-minute countdown in the top-right corner as a display-space test
- the miniature timer automatically starts at boot
- the timer hides while EMI is actively being petted so the face keeps visual priority
- the timer is right-aligned using the tiny 4x6 U8g2 font

## 2026-10-07 - Personality drives + less repetitive idle

### Changed

- added internal curiosity and contentment values that evolve over time
- idle behavior selection is now influenced by EMI's current internal state
- EMI remembers the previous two idle actions and strongly avoids repeating them
- added distinct still, micro-glance, focus, curious-peek, wide-glance, settle, and soft-attention behaviors
- curiosity rises while EMI is left alone and is reduced by exploration
- contentment increases through petting and fades slowly afterward
- high contentment makes calm/still idle behavior more likely after petting
- first pet attention movement is now about 85 ms after touch recognition
- every recognized stroke produces a contented partial eye-close
- repeated strokes gradually deepen the contented eye-close up to a safe limit
- deeper relaxed blink now requires several strokes
- normal blink timing widened to roughly 9-18 seconds
- double blink probability reduced again

### Removed / refined

- reduced purely random idle selection
- avoided immediate reuse of recent idle animations
- no vertical petting bounce
- no per-stroke random gaze jump
