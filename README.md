# OpenMarketClient

An unofficial, cross-platform, console-based client for the following markets:
* [market.csgo.com](https://market.csgo.com)
* [market.dota2.net](https://market.dota2.net)
* [tf2.tm](https://tf2.tm)
* [rust.tm](https://rust.tm)
* [gifts.tm](https://gifts.tm)

![screenshot](screenshot.png)

# Features
* Multi-account support
* Proxy support
* Sets Steam inventory public
* Sets trade token and Steam web API key on the market
* Keeps your market profile online
* Sends sold items
* Receives bought items
* Accepts Steam Guard confirmations of sent offers
* Cancels offers that aren't accepted within 10 minutes (required since Steam removed CancelTradeOffer web API)
* Ability to import SteamDesktopAuthenticator's maFile
* Accounts are password encrypted

# Usage
You'll be asked to enter the encryption password which will be used to encrypt and decrypt saved accounts.

## Adding an Account Manually
If you run the client without any accounts added, you will be asked to add a new one. To add another account later, launch the program with the `--new` command-line option.

## Importing Steam Desktop Authenticator's maFile
To import an account from SDA, place the unencrypted `.maFile` into the `accounts` folder (create the folder if it doesn't exist). The program will automatically import most of the required details.

## Required Details
* Market API key ([you can get one here](https://market.csgo.com/docs-v2))
* Steam username
* Steam password
* Steam Guard Mobile Authenticator details:
  *   Two-factor authentication code
  *   Identity secret

You can find instructions on how to extract Steam Guard Mobile Authenticator details from your phone [here](https://github.com/JustArchiNET/ArchiSteamFarm/wiki/Two-factor-authentication#android-phone).

## Command-line Options
* `--new` - Enter new account manually
* `--proxy [scheme://][username:password@]host[:port]` - Set global proxy

# Build Requirements
* C++17 supporting compiler
* libcurl
* wolfSSL
* RapidJSON
