# Privacy and Security

EMI is intended to be useful without becoming an unrestricted computer-control or surveillance device.

## Microphone

Design goals:

- no camera
- local speech recognition
- no cloud audio upload
- push-to-talk or another explicit listening mechanism preferred
- no intentional permanent audio archive
- visible listening state on EMI's display
- physical microphone disable control is a possible later addition

## Speech recognition

Planned speech-to-text runs locally on a Raspberry Pi or other local computer.

Candidate engines:

- Vosk for lightweight fixed-command recognition
- whisper.cpp for more natural local speech recognition

Network latency over the LAN is expected to be small compared with speech-recognition processing time.

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

- authentication between EMI, Raspberry Pi, and desktop
- firewall rules restricting which local hosts can reach each service
- minimal listening ports
- logging of important desktop actions
- no unnecessary internet exposure

## Memory

EMI's future local memory should be explicit rather than silently inferred.

Planned categories:

- people
- facts
- projects
- memories
- preferences

Commands such as "EMI, remember that..." and "forget that..." should control durable personal memory.
