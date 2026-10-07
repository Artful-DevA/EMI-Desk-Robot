# EMI Feature Inventory

This is the canonical feature inventory for EMI. It should be updated as features are added, removed, redesigned, or explicitly rejected.

## Personality and physical presence

- expressive OLED eyes
- quick gaze / saccades
- natural blinking with non-fixed timing
- recent-action memory to reduce repetitive idle behavior
- curiosity and contentment drives
- future bounded mood system rather than fixed canned animations
- occasional attention bids when appropriate
- attention bids should stop if ignored rather than guilt-trip the user
- petting detection through the TTP223
- slow strokes treated as one petting session
- small contented eye-close on individual strokes
- deeper relaxed blink after sustained petting
- SG90 head movement planned
- future movement language tied to mood/state
- time-of-day behavior planned
- sleep/rest model planned with bounded effects

## Mood and rest model

Planned internal dimensions:

- contentment
- curiosity
- energy
- social/attention need
- focus
- sleepiness/restedness

These values should influence probabilities and timing, not turn EMI into a melodramatic virtual pet.

Power-off time should **not** accumulate unlimited sleep. Proposed rule:

- after a modest amount of powered-off time, rest credit reaches a cap
- being off for 2 hours and 20 hours should both leave EMI reasonably rested
- long absences must never produce absurd states such as "12 hours overslept and groggy"
- calendar DND, notes mode, and active timers can suppress attention-seeking behavior

## Speech and languages

First-class languages:

- English: primary/default, fluent/native-like target
- Romanian: fluent/native-like target
- French: fluent/native-like target
- German: native-quality target for practice/tutoring
- Dutch: native-quality target for practice/tutoring

Planned language features:

- local speech-to-text
- local text-to-speech
- multilingual speech recognition
- code-switching inside one sentence
- canonical intent parsing independent of language
- native-language TTS voice per language rather than an English voice reading foreign text
- German learning/practice
- Dutch learning/practice
- vocabulary progress
- pronunciation practice
- Romanian/French maintenance practice
- off / push-to-talk / always-listen microphone modes
- voice-activated transcript/note mode
- visible always-listening indicator

## Productivity

- todo list
- task completion
- subtasks
- timers
- multiple timers
- task-linked timers
- stopwatch
- counters
- focus sessions
- reminders
- miniature timer overlay on the OLED
- timer state stored by the hub so ESP32 restarts do not destroy tasks

## Time and display UI

- miniature timer in corner
- current time view
- smooth eyes-to-clock transition
- clock-to-eyes return animation
- small microphone/listening status icon
- minimal overlays that do not turn EMI into a dashboard
- face takes visual priority during direct interaction

## Calendar and classes

- calendar integration
- class recognition
- pre-class reminders
- reminders on phone when away from desk
- suppress movement/attention reminders during class
- suppress interruptions shortly before important events
- movement reminders only when calendar context says interruption is appropriate
- free/busy awareness without requiring storage of unnecessary calendar details

## Notes and transcripts

- "EMI, start taking notes"
- "EMI, stop notes"
- continuous transcription session
- persistent visible notes/listening state
- transcript storage
- raw audio disabled by default
- optional transcript search
- optional summarization later
- class note mode

## Desktop and laptop integration

- Ubuntu desktop agent
- Ubuntu laptop agent
- both connected simultaneously
- default target priority: desktop > laptop > phone
- explicit manual target switching
- automatic fallback if selected device disconnects
- separate control target and speaker/output target
- media play/pause
- volume control
- current song
- CPU/system status
- approved app launch
- CamTune integration
- timers
- lock screen
- safe allow-listed Linux actions
- no arbitrary remote shell execution

## Phone

- companion app
- BLE pairing
- BLE provisioning/setup
- Wi-Fi credential provisioning
- multiple remembered Wi-Fi profiles
- connection/status screen
- microphone-mode control
- target selection
- phone as EMI speaker
- phone as travel gateway
- phone hotspot as preferred travel network
- class reminder notifications
- local notification rather than SMS by default

## Travel

- phone hotspot
- easy new-Wi-Fi provisioning
- Tailscale from phone to home Raspberry Pi
- phone relay for EMI microphone traffic
- home Raspberry Pi remains speech backend
- phone audio output when no computer is present
- touch/eyes/personality remain functional if internet disappears

## Music awareness

Preferred privacy-preserving design on Ubuntu:

- read current Spotify playback metadata locally through the desktop media interface where possible
- learn aggregate play counts rather than keeping a permanent timestamped listening history
- optional top artists / top tracks model
- use decay so old listening patterns matter less over time
- do not infer or store sensitive traits from music taste

## Memory and learning about the user

Memory should use tiers:

1. **ephemeral context** - disappears after a short time/session
2. **observations** - low-confidence, automatically decay
3. **derived preferences** - based on repeated behavior, decay if not reinforced
4. **explicit facts** - only durable when directly supplied or confirmed

Planned capabilities:

- people
- projects
- tasks
- routines
- language progress
- preferences
- explicit remember / forget
- confidence levels
- source/provenance
- last-used / last-confirmed timestamps
- automatic forgetting for weak inferred memories
- memory budget so EMI does not keep everything forever

EMI should not intentionally "pretend to forget." Forgetting should be predictable retention/decay for privacy and realism.

## Special dates

- birthday greeting only if explicitly enabled
- storing month/day is enough; birth year/age is unnecessary
- Christmas / New Year / other public holidays can come from a local holiday calendar
- no need to infer private celebrations

## Privacy/security

- no camera
- local-first speech recognition
- no cloud audio requirement
- no permanent raw audio archive by default
- visible listening state
- microphone-off mode
- future physical mic kill switch possible
- authenticated local devices
- narrow action allow-list
- no arbitrary shell commands
- high-impact actions require explicit confirmation
- encrypted travel tunnel
- minimize retained personal information
- local database should be encrypted at rest where practical
- user should be able to inspect/export/delete EMI's database

## Data EMI should not store by default

- passwords, tokens, or recovery codes
- banking/payment data
- private-message contents
- full browsing history
- exact location history
- raw microphone recordings
- full permanent music listening history
- birth year / age when only birthday greeting is needed
- medical/health information
- highly sensitive identity or belief information
- anything collected merely because it is technically possible

## Developer/project features

- GitHub project awareness
- CamTune project knowledge
- local project indexing/search
- selected external APIs only when explicitly enabled
- versioned firmware
- documentation updated alongside firmware changes
