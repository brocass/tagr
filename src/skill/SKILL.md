---
name: tagr
description: Read and edit audio metadata and cover art with Tagr's native command line. Use for MP3, FLAC, M4A, Ogg, Opus, WAV, AIFF, APE, WavPack, and Musepack tags.
---

# Tagr

Use `tagr` for native audio metadata operations. No Wine, GUI, or audio re-encoding is needed for these commands.

## Read tags

```sh
tagr read --json track.mp3
```

The command prints a JSON array. It exits 0 on success and 1 if any file fails; errors also go to stderr.

## Write tags

```sh
tagr write --json --title "Example" --artist "Artist" --album "Album" \
  --album-artist "Artist" --genre "Electronic" --year 2026 --track 1 --disc 1 track.mp3
```

Use `--comment`, `--cover cover.jpg`, or `--remove-cover` as needed. Empty strings and numeric zero clear their fields. Cover images must be JPEG or PNG and smaller than 20 MB.

Every write uses a temporary copy beside the source and an atomic rename. A failed write leaves the original untouched. Audio streams are copied without re-encoding.

## Install this skill

```sh
tagr skill install --json
```

For a test or custom location, add `--path /path/to/skill-directory`.
