# Fooyin MCP Plugin

Dieses Plugin macht den **Fooyin Audio Player** steuerbar über das **Model Context Protocol (MCP)**. Es erlaubt KI-Assistenten wie Claude oder Cursor, Musik zu suchen, Playlists zu erstellen und die Wiedergabe zu steuern.

## Features
- **Playback Control:** Play, Pause, Stop, Next, Previous, Seek.
- **Library Search:** Durchsuche die Musikbibliothek nach Artist oder Titel.
- **Now Playing:** Erhalte Details zum aktuell laufenden Song.
- **Playlist Management:** Liste Playlists auf und erstelle neue.
- **Named Pipe Interface:** Nutzt `\\.\pipe\fooyin_mcp` für schnelle, lokale Kommunikation.

## Build Anweisungen

### Voraussetzungen
1. **Qt 6.x** (Core, Network, Concurrent, Widgets).
2. **Fooyin SDK** (Die Header und Libs aus deinem Fooyin-Build-Ordner).
3. **CMake 3.19+** und ein C++20 Compiler (MSVC unter Windows).

### Kompilieren
1. Öffne die Developer PowerShell für VS.
2. Navigiere in diesen Ordner.
3. Führe folgende Befehle aus:

```powershell
mkdir build
cd build
# Ersetze die Pfade durch deine tatsächlichen Qt und Fooyin Installationspfade
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.x/msvc2019_64;C:/Path/To/Fooyin/Build"
cmake --build . --config Release
```

4. Kopiere die entstandene `fooyin_mcp_plugin.dll` in den `plugins`-Ordner deiner Fooyin-Installation.

## Sicherheit & Authentifizierung
Das Plugin implementiert mehrere Sicherheitsstufen:
- **Localhost Binding:** Der WebSocket-Server bindet ausschließlich an `127.0.0.1`.
- **Origin Check:** Browser-basierte Clients werden blockiert, sofern sie nicht von `localhost` kommen.
- **Token-Authentifizierung:** Alle Verbindungen (WebSocket & Pipe) müssen sich mit einem Token authentifizieren, sofern einer beim Start konfiguriert wurde.
- **DoS Schutz:** Maximale Nachrichtengröße (1MB) und maximale Verbindungsanzahl (20) werden erzwungen.

### Authentifizierung (JSON-RPC)
Nach dem Verbindungsaufbau muss als erster Request `authenticate` gesendet werden:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "authenticate",
  "params": { "token": "DEIN_SICHERER_TOKEN" }
}
```

## MCP Konfiguration

Um das Plugin in **Claude Desktop** oder **Cursor** zu nutzen, musst du einen "Staging-Client" (Bridge) verwenden, der von JSON-RPC auf die Named Pipe übersetzt. Da das Plugin direkt eine Named Pipe öffnet, kannst du ein einfaches Python-Skript oder ein Node.js-Tool als Bridge nutzen.

### Claude Desktop Konfiguration
Füge dies zu deiner `claude_desktop_config.json` hinzu:

```json
{
  "mcpServers": {
    "fooyin": {
      "command": "python",
      "args": ["C:/Path/To/fooyin_bridge.py"]
    }
  }
}
```

*(Hinweis: Ein Beispiel für `fooyin_bridge.py`, das die Named Pipe `\\.\pipe\fooyin_mcp` anspricht, kann auf Anfrage bereitgestellt werden.)*

## Architektur
- `mcp_plugin.cpp/.h`: Einstiegspunkt (Plugin-Lifecycle).
- `fooyin_wrapper.cpp/.h`: Direkte Interaktion mit den Fooyin-Engines.
- `mcp_server.cpp/.h`: Named Pipe Server & JSON-RPC Dispatcher.
- `mcp_tools.cpp/.h`: Definition der MCP-Tools und Schemata.
