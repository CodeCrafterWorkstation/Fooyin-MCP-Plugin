# Fooyin MCP Plugin

<span style="color:red">**DISCLAIMER: This project was developed with the assistance of Large Language Models (LLMs). It is currently in Early Alpha. Use at your own risk. The authors assume no liability for any damages to your system or music library.**</span>

## Overview
Integrates the **Fooyin Audio Player** with the **Model Context Protocol (MCP)**, allowing AI assistants (Claude, Cursor, etc.) to control playback and manage your library.

## Features
- **Playback:** Play, Pause, Stop, Next, Prev, Seek.
- **Library:** Advanced search using Fooyin query syntax.
- **Playlists:** List, create, remove, and sort tracks.
- **Metadata:** Edit tags (Title, Artist, Rating) and write to disk.
- **Real-time:** WebSocket notifications for track changes.

## Build Requirements
- **Visual Studio 2026** (MSVC 14.51+)
- **Qt 6.12.0** (located at `X:/Qt/6.12.0/msvc2022_64`)
- **Fooyin Source:** Expected at `X:/fooyin`

```powershell
mkdir build && cd build
cmake -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="X:/Qt/6.12.0/msvc2022_64" ..
cmake --build . --config Release
```

## Authentication
WebSockets require a security token found here:
`%APPDATA%\fooyin\mcp_token`

## Testing
Use the included `TestUI/index.html` to validate the WebSocket API:
`ws://127.0.0.1:8080/?token=YOUR_TOKEN`

## License
GPL-3.0-or-later
