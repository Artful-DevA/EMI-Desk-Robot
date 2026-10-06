# Changelog

This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

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
- after several strokes, EMI can give one deeper relaxed blink
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
