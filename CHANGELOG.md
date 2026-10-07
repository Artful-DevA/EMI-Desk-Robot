# Changelog

This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

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

## 2026-10-07 - Richer idle + contented pet strokes

### Changed

- idle behavior now has multiple action types instead of repeating the same random gaze cycle
- added tiny micro-saccades
- added focused glances with small follow-up corrections
- added occasional wide exploratory looks followed by a natural return toward center
- added deliberate long still periods
- increased normal blink interval to roughly 8.5-16.5 seconds
- reduced rare double blink probability
- petting attention now reaches the upper gaze position in about 95 ms
- every recognized pat now produces a smooth partial contented eye-close
- fast touch chatter under 260 ms is ignored as a separate stroke
- after several pats EMI can give one deeper relaxed blink
- petting drift is slower and only horizontal

### Removed / refined

- no per-pat random gaze jump
- no vertical petting bounce
- no rapid petting eye jiggle

## 2026-10-07 - Current breadboard personality firmware

### Added

- natural idle gaze with weighted gaze ranges
- quick saccade-style eye movement
- longer gaze holds
- infrequent normal blinking
- rare double blink
- debounced TTP223 input
- 2.2 second slow-pat grace period
- fast upward attention shift when petting begins
- slightly inward petting expression
- subtle per-pat inward acknowledgement
- tiny slow horizontal drift during petting
- one relaxed blink after several pats
- gentle return to neutral after petting

### Removed / refined

- removed aggressive petting squint
- removed fast vertical nuzzle bounce
- removed random per-pat gaze jumps
- reduced excessive blinking
- reduced constant idle motion
