# External Cheats

## Overview
External cheats run as a separate program alongside the game. They do not inject themselves into the game's process, meaning they read game data from the "outside."

## How They Work
- **Process:** Run as a standalone process on the same PC as the game.
- **Data Access:** Read game memory using system APIs (e.g., `ReadProcessMemory` on Windows) or through a custom driver.
- **Visuals:** Typically use an **overlay**—a transparent window placed on top of the game window where visual information (like ESP boxes, health bars, and radar) is drawn.

## Pros
- **Lower Detection Risk:** Since they don't modify the game's internal code or functions, they have a smaller detection surface than internal cheats.
- **Easy Setup:** Usually just involve running a loader and a configuration file.
- **Stability:** If the external cheat crashes, the game process usually remains unaffected.
- **Streamproof:** Overlays can often be made "streamproof" so they are visible to the player but not captured by recording software.

## Cons
- **Fidelity:** Because they poll memory rather than reading the live game state directly, some features (like projectile prediction) might be slightly less precise.
- **Rendering:** Since the overlay is a separate window, it can sometimes have a slight performance cost compared to in-game rendering.

## Best For
Players looking for a balance between safety and features, especially those who want a "set it and forget it" experience without extra hardware.
