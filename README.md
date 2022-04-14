# OpenMarketClient
Unofficial open-source cross-platform console client for [market.csgo.com](https://market.csgo.com) and [market.dota2.net](https://market.dota2.net)

![screenshot](screenshot.png)

# Features
* Keeps your market profile online
* Sends sold items
* Receives bought items
* Accepts Steam Guard confirmations
* Ability to import SteamDesktopAuthenticator's maFile (simply put it in a folder)
* Password encrypted config

# Usage
There are 2 ways to start using the program:
1. Launch the program and enter required details manually.
2. Put SDA's unencrypted maFile into the folder and the program will import most details automatically.

After that, you'll be asked to enter the encryption password and the details will be saved into the config.

## Required details
* Market's API-key ([you can get one here](https://market.csgo.com/docs-v2))
* Steam username
* Steam password

*and those Steam Guard Mobile Authenticator details*
* Shared secret
* Identity secret
* Device ID

# Build requirements
* C++17 supporting compiler
* libcurl
* wolfSSL
* RapidJSON
