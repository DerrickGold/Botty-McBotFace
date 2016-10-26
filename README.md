# Botty-McBotFace
A simple IRC Bot/Bot library in C

## Building
A sample bot configuration has been included in `samplebot.c`. Once this bot
is configured via the IrcInfo and BotInfo structs at the top, simply run `build.sh`
to build both libbotty and the samplebot.

## Features
This bot has a few notable features. See `samplebot.c` for implementation details.

### Callbacks
First there are callback events that the bot will trigger which can be easily hooked into without having to parse text or muck around in the code implementation. These events include:

- When the bot connects to a server
- When the bot joins a channel
- When the bot receives a general message
- When another user joins
- When another user parts/quits
- When another user changes their nick
- When the bot receives a server message (such as API calls)

### Commands
Secondly, it is possible to register commands to the bot. These commands are up to MAX_CMD_LEN in length (currently 9 characters), and can have up to MAX_BOT_ARGS (currently 8) arguments passed to them. Commands are invoked by the first word of a message
written to the channel including CMD_CHAR ('~') as the first character of the word, with any arguments given separated by BOT_ARG_DELIM (a space).

The bot has a few built in commands with some basic info, and the functionality to shut the bot down:

- `help` :provides a list of the commands registered to the bot
- `info` :provides creator info, and build date and time
- `source` :provides the git command for cloning this repository
- `die` :shuts the bot down

The sample bot has some extra commands to demonstrate the command functionaility, these include:

- `say <text>` :Make the bot say <text> to the channel its in
- `roll <num dice>d<max dice roll>` :Get the bot to roll x number of dice with y max
- `roulette` :Start/participate in a game of roulette

### MultiBot
This library allows for multiple bots to be configured and run without blocking each other. Take a look at `multibot.c` for an example of the multibot feature in action.

