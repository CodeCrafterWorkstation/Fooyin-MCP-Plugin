# Fooyin MCP Plugin

Dieses Plugin integriert den **Fooyin Audio Player** in das **Model Context Protocol (MCP)**. Es ermöglicht KI-Assistenten (wie Claude, Cursor oder IDEs), deine Musikbibliothek zu steuern, Songs zu suchen und Metadaten zu verwalten.

## Features

- **Wiedergabesteuerung:** Play, Pause, Stop, Next, Previous, Seek.
- **Bibliothekssuche:** Umfangreiche Suche mit Fooyin-Query-Syntax (z. B. `artist HAS beatles`).
- **Playlist-Management:** Playlisten auflisten, erstellen, löschen und sortieren.
- **Metadaten-Editor:** Bearbeiten von Tags (Titel, Artist, Album, Rating etc.) direkt in der Bibliothek oder in den Dateien.
- **Echtzeit-Notifications:** WebSocket-Unterstützung für Live-Updates bei Track-Wechseln oder Statusänderungen.
- **Named Pipe & WebSocket:** Unterstützung für lokale Pipes (`\\.\pipe\fooyin_mcp`) und WebSockets (Port 8080).

## Installation & Build

### Voraussetzungen
- **Visual Studio 2026** (MSVC 14.51+)
- **Qt 6.12.0** (installiert unter `X:/Qt/6.12.0/msvc2022_64`)
- **Fooyin Source/Build:** Erwartet unter `X:/fooyin`

### Build-Schritte
Das Plugin wird mit CMake gebaut. Ein vorkonfiguriertes Build-Skript (PowerShell) sieht so aus:

```powershell
mkdir build
cd build
cmake -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="X:/Qt/6.12.0/msvc2022_64" ..
cmake --build . --config Release
```

Die fertige `fooyin_mcp_plugin.dll` befindet sich danach im Ordner `build/Release/`. Kopiere diese in das Plugin-Verzeichnis von Fooyin.

## Sicherheit & Authentifizierung

Der WebSocket-Server ist durch ein Token geschützt.

### Token finden
Das Plugin generiert bei der ersten Ausführung ein kryptografisch sicheres Token. Du findest es hier:
- Pfad: `%APPDATA%\fooyin\mcp_token`
- Beispiel: `C:\Users\<DeinName>\AppData\Roaming\fooyin\mcp_token`

### Test UI
Im Ordner `TestUI/` befindet sich eine `index.html`. Öffne diese im Browser, um alle Funktionen des Plugins über eine grafische Oberfläche zu testen. Verbinde dich mit:
`ws://127.0.0.1:8080/?token=DEIN_TOKEN`

## MCP Konfiguration

Um das Plugin in **Claude Desktop** zu nutzen, füge folgendes zu deiner `claude_desktop_config.json` hinzu:

```json
{
  "mcpServers": {
    "fooyin": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-fooyin", "--pipe", "\\\\.\\pipe\\fooyin_mcp"]
    }
  }
}
```
*(Hinweis: Ein entsprechender Wrapper-Client, der die Named Pipe anspricht, ist erforderlich.)*

## Projektstruktur
- `PluginCODE/`: C++ Quellcode des Plugins.
- `TestUI/`: Einfache Weboberfläche zum Testen der WebSocket-API.
- `fooyin_wrapper.cpp`: Kernlogik zur Kommunikation mit der Fooyin-API.
- `mcp_tools.cpp`: Definition der MCP-Tools und deren JSON-Schemas.
