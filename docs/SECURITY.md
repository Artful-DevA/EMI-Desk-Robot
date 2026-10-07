# Privacy and Security

EMI is intended to be useful without becoming an unrestricted computer-control, surveillance, or personal-profiling device.

## Core rule

EMI should collect the **minimum data needed for a feature**.

A feature being technically possible is not enough reason to store the data behind it.

## Microphone

Design goals:

- no camera
- local speech recognition
- no cloud audio upload required
- microphone modes: off, push-to-talk, always-listen
- visible listening state on EMI's display
- raw audio discarded by default after transcription
- physical microphone disable control is a possible later addition

Always-listen is compatible with a privacy-first design only if audio stays under the user's control and the listening state is obvious.

## Speech recognition

Speech-to-text should run locally on hardware controlled by the user.

Whisper/whisper.cpp can run fully locally after the model is installed, but Whisper is still a machine-learning speech-recognition model. Vosk is also machine-learning based.

If the requirement is **no cloud**, local Whisper/Vosk are compatible with that goal.

If the requirement is literally **no machine learning at all**, modern multilingual free-form speech recognition and code-switching become much less practical; a restricted grammar recognizer would be needed instead.

## Personal data boundary

EMI should not store the following by default:

- passwords, API tokens, recovery codes
- banking/payment information
- private-message contents
- exact location history
- full browsing history
- raw microphone recordings
- permanent detailed music-listening history
- medical/health records
- highly sensitive identity/belief data
- birth year or age when only a birthday greeting is required

For birthday greetings, month/day is sufficient.

## Memory tiers and forgetting

Not every observation should become memory.

Recommended tiers:

1. **ephemeral context** - session-only
2. **observation** - low-confidence, short retention
3. **derived preference** - repeated pattern, confidence decays
4. **explicit fact** - durable only when supplied/confirmed

Derived memories should decay unless reinforced.

This gives EMI a limited, understandable form of forgetting without randomly deleting important explicit facts.

The user should be able to:

- inspect stored memory
- see why a derived preference exists
- delete individual memories
- delete whole categories
- export the database
- reset EMI's personal database

## Music learning

For Spotify on Ubuntu, prefer local playback metadata from the desktop media interface.

Store aggregates such as:

- artist play count
- track play count
- recency-weighted preference score

Avoid storing a permanent timestamped history of every song.

Do not infer sensitive personal traits from music choices.

## Calendar

Prefer minimum-data calendar access.

Possible strategy:

- read upcoming events
- derive free/busy windows
- keep only reminder-relevant class information
- avoid copying the user's complete long-term calendar into EMI's database unless explicitly requested

Calendar context may suppress:

- attention bids
- movement reminders
- casual interruptions

## Class reminders and phone notifications

Class reminders should be scheduled from explicit calendar events.

If the user is away from the desk, the phone app can display a local notification.

The design should not require storing continuous location history to decide whether the user is "away."

## Desktop integration

The robot should never send arbitrary shell commands for execution.

Preferred architecture:

1. EMI sends a structured action request.
2. A local desktop service authenticates the request.
3. The service checks the action against an explicit allow-list.
4. Only the predefined implementation for that action runs.

Example allowed actions may eventually include:

- media play/pause
- volume up/down
- read CPU temperature
- open CamTune
- create a timer
- lock the computer
- report the current song

## Privileged actions

Anything that needs root privileges should use a narrow, root-owned helper script with an exact sudoers rule.

Do not allow:

- arbitrary `sudo`
- arbitrary shell text supplied over the network
- command concatenation
- unrestricted file deletion

High-impact actions such as shutdown, reboot, software installation, or deletion should require explicit confirmation, ideally with a physical confirmation step.

## Network

Later stages should include:

- authentication between EMI, Raspberry Pi, phone, and computers
- firewall rules restricting which local hosts can reach each service
- minimal listening ports
- logging of important desktop actions
- no unnecessary internet exposure
- encrypted travel tunnel

If a third-party coordination service for a VPN is undesirable, direct WireGuard or a self-hosted coordination option can be considered later.

## Database protection

Planned protections:

- database stored only on trusted local hardware
- restrictive filesystem permissions
- encrypted storage where practical
- no secrets stored in the same general-purpose memory tables
- backups opt-in rather than automatic cloud backup
- clear retention policies
