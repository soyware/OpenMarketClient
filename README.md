# MarketsBot
Cross-platform console bot for market.csgo.com / market.dota2.net

![screenshot](screenshot.png)

# Features
* Keeps your market profile online
* Sends sold items
* Receives bought items
* Accepts Steam Guard confirmations
* Ability to import SDA's maFile (simply put it a folder)

# Usage
To work, this bot needs the following:
* Steam username
* Steam password
* Market API-key ([you can get one here](https://market.csgo.com/docs-v2))

*and those Steam Guard items*
* Shared secret
* Identity secret
* Device ID

All of those are encrypted and put into config after you enter them.

Simply compile / download latest release (rarely updated), start the program and follow instructions.

# Compile
You'll need a compiler with c++17 support, libcurl, wolfssl and rapidjson
