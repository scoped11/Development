# Internal Cheats

## Overview
Internal cheats are loaded directly into the game's process. They share the same memory space as the game, allowing for deep integration and powerful features.

## How They Work
- **Process:** Injected as a module (usually a DLL) into the game process.
- **Data Access:** Have direct, "native" access to the game's memory, functions, and engine behavior. They can hook functions directly.
- **Visuals:** Drawn through the game's own renderer (e.g., DirectX or Vulkan hooks), making them feel like a seamless part of the game.

## Pros
- **Maximum Features:** Can perform complex tasks like silent aim, deep visual changes (chams), and fine-grained control.
- **High Performance:** Since they use the game's own data and rendering, there is very little overhead.
- **Smoothness:** Tracking is often smoother than external cheats because they read the live game state every frame.

## Cons
- **Higher Detection Risk:** Because they live inside the game process, anti-cheat systems have many more ways to detect them (e.g., module scanning, integrity checks, hook detection).
- **Complexity:** More difficult to develop and update because they are tightly coupled with the game's internal structures.

## Best For
Players who want the most features and the best "feel," and are willing to accept a higher risk of detection for a premium experience.
