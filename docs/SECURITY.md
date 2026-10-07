# Privacy and Security

EMI is intended to be useful without becoming an unrestricted computer-control, surveillance, or personal-profiling device.

## Core rule

EMI should collect the **minimum data needed for a feature**.

A useful shorthand is:

**functional memory, not intimate memory**

EMI should remember enough to make timers, classes, language practice, device preferences, selected project knowledge, and chosen conveniences work well. EMI should not try to reconstruct the user's private life.

## Hard boundaries

EMI should not:

- search the web for information about the user
- enrich stored data with public profiles, social accounts, people-search results, or other external sources
- infer sensitive traits from music, browsing, speech, contacts, or projects
- act as a therapist, confidant, or substitute human relationship
- encourage disclosure of highly private information
- keep ordinary conversations as permanent memory by default

If a conversation starts becoming too personal, EMI should be able to say that it is better not to store or discuss that kind of information with him.

## Microphone

Design goals:

- no camera
- local speech recognition
- no cloud audio upload required
- microphone modes: off, push-to-talk, always-listen
- visible listening state on EMI's display
- raw audio discarded by default after transcription
- physical microphone disable control is a possible later addition

## Project awareness

GitHub/project awareness is allowed only as a selected work feature.

Recommended rules:

- repositories are explicitly selected by the user
- read-only access by default
- repository source/index data is stored separately from personal memory
- no crawling the user's GitHub profile
- no inferring habits from commit times
- no storing contributor emails or unrelated collaborator information
- no following links from repositories to build a personal profile
- the project index should be disposable and rebuildable

## Names and people

EMI does not need the user's legal name.

A nickname or first name can be stored only if the user wants it.

"Meet Bob" should default to temporary session context unless the user explicitly asks EMI to remember Bob.

If remembered, store only what is necessary, such as:

- a chosen name/nickname
- a simple relationship label if explicitly provided

Do not automatically collect surnames, profiles, contact data, addresses, faces, or other personal details.

## Speech recognition

Speech-to-text should run locally on hardware controlled by the user.

Whisper/whisper.cpp can run fully locally after the model is installed, but Whisper is still a machine-learning speech-recognition model. Vosk is also machine-learning based.

If the requirement is **no cloud**, local Whisper/Vosk fit that goal.

If the requirement is literally **no machine learning at all**, modern multilingual free-form speech recognition and code-switching become much less practical.

## Music learning

Music personalization should be optional.

A privacy-light approach is preferred:

- allow named playlist aliases, e.g. "favorite playlist" -> a playlist URI
- current song may be read transiently
- no permanent exact play counts required
- no timestamped listening history
- no personality inference from music
- any learned preference can be deleted completely from EMI's active database

## Memory and forgetting

Not every observation should become memory.

Recommended tiers:

1. **ephemeral context** - session-only
2. **observation** - low-confidence and short-lived
3. **derived preference** - repeated pattern, may decay
4. **explicit fact** - durable only when supplied or confirmed

Forgetting should mean deletion from active memory, not pretending to forget.

When the user asks EMI to forget something:

- remove the record from the active database
- remove derived indexes/caches referencing it
- remove search-index entries
- do not keep a hidden "deleted memories" table

On flash/SD media, perfect forensic erasure of an individual old record cannot always be guaranteed because of wear leveling. For stronger protection, EMI should use encrypted storage and avoid unnecessary backups.

## Database size

Personal memory should stay deliberately small.

Suggested design:

- personal memory database: small capped quota
- tasks/calendar cache: small and prunable
- transcripts: separate storage with explicit retention rules
- project/repository indexes: separate disposable cache
- raw audio: not retained by default
- no embedding/vector database unless a later feature truly needs it

SQLite text records are tiny; audio and duplicated repository data are what would grow storage quickly.

## Personal data boundary

EMI should not store by default:

- passwords, API tokens, recovery codes
- banking/payment information
- private-message contents
- exact location history
- full browsing history
- raw microphone recordings
- detailed music-listening history
- medical/health records
- highly sensitive identity/belief data
- birth year or exact age when only a birthday greeting is needed

## Calendar

Prefer minimum-data calendar access.

Possible strategy:

- read upcoming events
- derive free/busy windows
- keep only reminder-relevant class information
- avoid copying the full long-term calendar into personal memory

## Desktop integration

The robot should never send arbitrary shell commands for execution.

Use authenticated, allow-listed structured actions only.

## Database protection

Planned protections:

- database stored only on trusted local hardware
- restrictive filesystem permissions
- encrypted storage where practical
- backups opt-in
- clear retention policies
- easy inspect/export/delete controls
