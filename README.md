# E#+ Studio 0.2.0

E#+ Studio is a native Windows IDE for the E#+ English-first programming language.

## Important

The standalone installer is not automatically trusted by Windows just because it is hosted on GitHub. A trusted public code-signing certificate is still needed to avoid publisher/security warnings outside the Microsoft Store.

## New 3D + Image Import Syntax

E#+ games can now create real 3D objects and click them:

```text
game "My 3D Game"
window width 1280
window height 720

camera first person
player create "Player"
player position 0 0 8

object create "Cube" "cube"
object position "Cube" 0 1 0
object scale "Cube" 2
object color "Cube" 80 140 220
object clickable "Cube"

set number score to 0

when object "Cube" clicked
    add number 1 to score
end
```

### Image imports

Use the new import syntax:

```text
import "file.png" as "accountName"
```

Then attach the imported image to a 3D object:

```text
object texture "Cube" "accountName"
```

E#+ Studio now includes **Import Image** and **Add Import** toolbar actions.

### Timers

A repeating one-second block is available for simple passive game logic:

```text
every second
    add number 1 to cookies
```

See `examples/3D-Cookie-Clicker.eplus` for a complete example.
