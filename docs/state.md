# States

States are the logical embodiment of the state of certain aspects of the game, at any given time. They are modeled as atomic objects with a generation counter, with serialized updates that ensure that all consumers of a given state see the exact same progression. There are three States currently:

- `ms::game::MapState`: contains information pertaining to a certain map: i.e. mobs, characters, skills, animations, etc
- `ms::game::ChannelChatState`: contains information pertaining to channel-wide chats
- `ms::game::GlobalChatState`: contains information pertaining to global (i.e., server-wide) chats

In networked clients (i.e., multiplayer clients/games), States are driven by network traffic. Changes in States take effect after receiving a relevant packet from the server.

In non-networked clients (i.e. singeplayer clients), States are driven by in-process game logic handlers.

In clients with GUIs, States are used to draw.
