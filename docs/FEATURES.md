# EMI Feature Inventory

EMI is a desk companion and co-worker. He is not intended to be a therapist, confidant, or substitute human friend.

Relationship rule: **the user can be EMI's best friend; EMI is not the user's best friend.** From EMI's character perspective, the user can be his favorite/closest person. That is one-way characterization and must never be used to imply that the user should treat EMI as a replacement for human friendship.

This is the canonical feature inventory for EMI. It should be updated as features are added, removed, redesigned, or explicitly rejected.


## Implemented in the current prototype

The following features are physically working now:

- ESP32-C3 Super Mini as the active controller
- SH1106 OLED face
- TTP223 touch/petting input
- verified I2S microphone hardware
- curiosity/contentment-driven face behavior
- recent-action anti-repetition
- natural blink/gaze behavior
- attention bids
- petting reactions
- deterministic `SHOW_TIME HH:MM`
- eye-aligned clock UI where HH and MM occupy the normal eye regions
- clean clock-to-eyes return animation
- 2.4 GHz Wi-Fi on the C3 with the verified 8.5 dBm TX-power workaround
- authenticated C3 polling of the Raspberry Pi hub
- deterministic local time-intent parsing on the Pi
- local `whisper.cpp` installation and benchmark on the Raspberry Pi
- physical Raspberry Pi -> C3 -> OLED time command proven end-to-end
- authenticated in-memory voice WAV endpoint on the Pi
- localhost-only whisper.cpp HTTP service for transient command transcription
- microphone voice path requires an explicit `Emi` wake address before executing an intent

The next active implementation target is the privacy-preserving microphone path: C3 RAM -> Wi-Fi -> Pi RAM -> local Whisper, with raw audio never written to disk.

## Personality and physical presence

- expressive OLED eyes
- quick gaze / saccades
- natural blinking with non-fixed timing
- recent-action memory to reduce repetitive idle behavior
- curiosity and contentment drives
- future bounded mood system rather than fixed canned animations
- occasional attention bids when appropriate
- attention bids should stop if ignored rather than guilt-trip the user
- EMI may regard the user as his best friend / favorite person from his side
- EMI should never claim to be the user's best friend or imply emotional exclusivity
- EMI should never pressure the user to prioritize him over real people
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

Power-off time should not accumulate unlimited sleep:

- rest credit reaches a cap after a modest powered-off period
- being off for 2 hours and 20 hours should both leave EMI reasonably rested
- long absences must never create absurd oversleep/groggy states
- calendar DND, notes mode, and active timers can suppress attention-seeking behavior

## Speech and languages

First-class languages:

- English: primary/default, fluent/native-like target
- Romanian: fluent/native-like target
- French: fluent/native-like target
- German: native-quality target for practice/tutoring
- Dutch: native-quality target for practice/tutoring

Planned language features:

- local speech-to-text (Pi backend proven; live C3 microphone streaming still in progress)
- local text-to-speech
- multilingual speech recognition
- code-switching inside one sentence
- canonical intent parsing independent of language
- native-language TTS voice per language
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
- readable timer overlay on the OLED
- timer state stored by the hub so ESP32 restarts do not destroy tasks

## Time and display UI

- readable timer in the top-right corner
- current time view **implemented**
- smooth eyes-to-clock transition with a large central clock **implemented**
- clock-to-eyes return animation **implemented**
- HH/MM groups aligned to EMI's normal left/right eye positions **implemented**
- small microphone/listening status icon
- minimal overlays that do not turn EMI into a dashboard
- face takes visual priority during direct interaction
- automatic time-of-day OLED dimming
- late-night brightness should reduce gradually rather than switch abruptly
- target behavior: somewhat dimmer after midnight and very dim around 02:00-07:00
- night mode should also suppress unnecessary attention-seeking behavior
- brightness returns gradually in the morning

## Calendar and classes

- calendar integration
- class recognition
- pre-class reminders
- reminders on phone when away from desk
- suppress movement/attention reminders during class
- suppress interruptions shortly before important events
- movement reminders only when calendar context says interruption is appropriate
- free/busy awareness without requiring storage of unnecessary calendar details
- class/meeting speech gate based on current audio output
- if headphones are the active output during a class/meeting, EMI may speak normally or quietly
- if speakers are the active output during a class/meeting, EMI should stay silent and use visual/text feedback instead
- if EMI cannot confidently determine whether the output is private, default to silence during class/meeting context
- manual override should be possible

## Notes and transcripts

Notes are an explicit mode, not a passive transcript of ordinary life.

- "EMI, start taking notes"
- "EMI, stop taking notes"
- persistent visible notes/listening state
- raw audio is never written to disk
- ordinary conversation and commands are not stored as transcripts
- only note-mode transcription is retained
- note transcripts are saved as Markdown
- notes can be written automatically into a configured Obsidian vault folder
- optional date/class-based filenames
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
- detect the active Ubuntu audio sink/output type where practical
- follow the system default output so switching between speakers and headphones also moves EMI's voice
- speech-permission policy can suppress TTS independently of other EMI actions
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

Music personalization is optional.

Preferred privacy-preserving design:

- read current Spotify/media metadata locally when useful
- prefer coarse labels such as often / sometimes / rarely over permanent exact play counts
- if temporary counts are needed to derive those labels, discard the detailed counters after aggregation
- avoid permanent timestamped listening history
- use decay so old listening patterns matter less over time
- do not infer sensitive personal traits from music taste

## Memory and learning about the user

Design principle: **functional memory, not intimate memory**.

EMI should remember what makes features work well, not try to build a human-like private biography.

Memory tiers:

1. ephemeral context - disappears after a short time/session
2. observations - low-confidence, automatically decay
3. derived preferences - repeated patterns, decay if not reinforced
4. explicit facts - durable only when directly supplied or confirmed

Planned capabilities:

- people when explicitly needed
- projects
- tasks
- routines relevant to features
- language progress
- preferences
- explicit memory write/delete commands only
- ordinary phrases containing words like "remember" or "forget" must never trigger persistent memory changes
- memory write/delete commands must use longer, unmistakable command phrases
- every persistent-memory write requires a second explicit "confirm save" step before anything is written
- every persistent-memory deletion requires a second explicit "confirm delete" step before anything is removed
- if confirmation is missing, ambiguous, or times out, no persistent-memory change occurs
- confidence levels
- source/provenance
- last-used / last-confirmed timestamps
- automatic forgetting for weak inferred memories
- memory budget so EMI does not keep everything forever

EMI should not intentionally pretend to forget. Forgetting should be predictable retention/decay for privacy and realism.

## Special dates

- birthday greeting only if explicitly enabled
- storing month/day is enough; birth year/age is unnecessary
- Christmas / New Year / other public holidays can come from a local holiday calendar
- no need to infer private celebrations

## Privacy/security

- no camera
- local-first speech recognition
- no cloud audio requirement
- raw audio is never written to disk
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

- GitHub project awareness for explicitly selected repositories only
- read-only project access by default
- no GitHub-profile crawling
- no web searching for information about the user
- CamTune project knowledge
- local project indexing/search
- selected external APIs only when explicitly enabled
- versioned firmware
- documentation updated alongside firmware changes
