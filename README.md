# [L4D/2] Max Players Override
This is a SourceMod Extension that allows server operators to override maxplayers by using the `maxplayers_override` command-line parameter. This extension is way more efficient simply because it doesn't rely on byte patches which are prone to breaking easily after game updates. It's also kept simple without the need to set up any additional CVars.

When all players have disconnected or the server hibernates, the `sv_allow_lobby_connect_only` CVar gets reverted back to its original value (`1` by default) if it was enabled before. The reservation cookie will also be cleared (unreserved) automatically if this CVar was disabled and players have connected through lobby/matchmaking.

# Command-Line Parameter
- `maxplayers_override` - to override max players

# Requirements
- [SourceMod 1.11+](https://www.sourcemod.net/downloads.php?branch=stable)
- [Matchmaking Extension Interface](https://github.com/shqke/imatchext)

# Supported Games
- Left 4 Dead 2
- Left 4 Dead

# Supported Platforms
- Windows
- Linux