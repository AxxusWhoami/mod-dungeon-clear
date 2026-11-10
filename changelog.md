# Changelog — mod-dungeon-clear

Todos los cambios del upstream sincronizados, agrupados por categoria.

---

## Sincronizacion 2026-11-10 (commits 75c1c05 → 484c802)

### Nuevas mazmorras y bandas

- **Blackfathom Deeps**: anadido completo — puzzle de salto, pasaje de Sarevess, Fires of Aku'mai
- **Zul'Gurub**: Arlokk anadida al clear; se tira la basura alrededor del Gong of Bethekk antes de tocarlo; se fuerza a Jeklik fuera de su repisa
- **Ahn'Qiraj (templo)**: Ouro anadido al clear; el tanque se mantiene fuera del monticulo mientras esta sumergido
- **Onyxia**: anadida a `.dc test`, equipada como WotLK
- **Clasicos en test run**: ZG, AQ20, AQ40 y Onyxia disponibles en `.dc test`

### Mejoras de eventos de mazmorra

- **BRD — Grim Guzzler**: barriles de Hurley, reto de Ribbly, carrera de cerveza de Rocknot
- **BRD — Tomb of the Seven**: mustar, reto de Doom'rel, auto-reinicio al resetear
- **BRD — Lyceum**: braziers — cazar Flame Keepers, cargar antorchas, moverse en grupo
- **BRD — Iron Hall**: pelear alrededor del fuego de los Ironhand Guardians
- **BRD — salon del Emperador**: pre-limpiar en orden y nunca despertarlo antes de tiempo
- **UBRS — Dragonspine Door y Dragonspire Hall**: eventos completos
- **UBRS — Altar de Blackrock**: mustar, ritual, incarceradores, liberacion de Emberseer
- **UBRS — Rookery**: cruzar por un carril disenado, limpio de trampas de huevos
- **UBRS — Blackrock Stadium**: mustar, iniciar, aguantar Rend y Gyth
- **UBRS — The Beast**: campamento al sur de su linea de evade, pre-limpieza de la sala de Drakkisath
- **Eventos party-wide**: cadenas off-mesh (RawPath, volumenes de caida) y bendicion de respiracion acuatica
- **Panel de estado**: los eventos repetibles ya no leen Alive para toda la corrida; un boss saltado tambien cierra el evento repetible anterior; reporta eventos scriptados, natacion y planificacion de ruta

### Correcciones de juego

- **Deadmines**: Sneed se ancla al Shredder de Sneed para que aparezca en el roster
- **Maraudon**: se limpia la ala de dungeon-finder que el grupo encolo
- **Stratholme**: los critters trampa caidos se desparecen a 6yd en vez de 20; rescate de grupo que cae por el agujero del callejon de trampas Scarlet
- **Karazhan**: rescate de grupo arrastrado bajo la Guardian's Library
- **Loot**: siempre saquear cofres de encuentro; retener el fin de corrida hasta que el ultimo boss sea saqueado
- **Queue fill**: mantener el grupo rellenado hasta que el jugador abandone el grupo
- **Pull**: un boss forzado mantiene el camino que su engage inicio; seguir forzando un boss de pull-back cuyo pull falla
- **Combat**: atacantes no vistos ya no mantienen cerrado el hatch de combate fantasma
- **Nav**: un boss en podio sin ruta de stock se juzga por linea de vista, no como un piso abajo
- **Assist**: dejar un objetivo al que ninguna ruta o caminar puede alcanzar; frenar el ping-pong leader-assist / advance 5yd; el Blizzard de Aran no es un holder
- **Rez**: resetear los relojes NoRezzer cuando todos estan de pie; reconocer clases que conocen un resurreccion propio; nunca detener la corrida sobre el cadaver del lider
- **Stall**: no mas stalls falsos del fallback o del arbiter
- **Roster**: los bosses de pooled-spawn se re-anclan en el spawn donde la criatura viva esta; encontrar bosses de ICC / Ruby Sanctum en dificultad heroica
- **Raid muster**: las lineas de chat de mustar y wipe mostraban un guion corrupto
- **Status**: solo nombrar un evento que Drive sigue ejecutando
- **Molten Core**: limpiar Baron Geddon antes que Shazzrah
- **Raids estilo**: slots de boss-state como int32, como Molten Core los declara

### Mejoras de Test Run / Testdeck

- Reconstruir estrategias de miembros despues de agrupar para que todos tengan avoid aoe
- Acreditar un kill de mascara al boss, no a un objetivo que comparte su bit
- Registrar loot raro-o-mejor en el record de corrida
- Mostrar loot acumulado en la tarjeta de Live run
- CI de testdeck arreglado en Starlette 1.10 y ubuntu 26.04
- CI de gtest: parchear `<utility>` faltante del core upstream antes de compilar

### Mejoras de lider / run owner

- El tanque que mantiene la corrida conserva el lider
- Una sola corrida por grupo, indicado en los comentarios de eleccion
- Transferir el wing latch cuando `dc on` mueve la corrida
- Limpiar el wing latch del dueno anterior cuando `dc on` mueve la corrida

### Mejoras del Lab

- Deletrear `DcLab::Unit` en DcLabOracles.cpp

---

## Sincronizacion 2026-10-16 (commits 587b8ff → da0bdfb)

- **Compatibilidad de build**: nombres de exclusion pools bajo ambas convenciones de enumeradores (Dps/Attacker/Tank vs DpsTarget/DebuffTarget/TankTarget) via `DcPlayerbotsExclusionType.h`
- **Gate de comida/buff**: aplicar comida/worldbuff solo al entrar a la mazmorra, nunca quitar worldbuff
- **Herramientas**: `dungeon_model.py --no-rings` dibuja solo el layout de spawns; `gen_navmap.py` renderiza navmesh a PNG
- **Testdeck**: embedding por iframe opt-in via `allowed_frame_ancestors`

---

## Sincronizacion 2026-10-08 (commits 0b6dd16 → d224254)

- **Pull Lab**: sistema completo de laboratorio de trazas con grabador, oraculos, runner y museo de regresion
- **Correcciones de pull**: el tanque se une a peleas donde solo tiene flag de combate; el reaper pasivo ya no elimina el hold de scout-aggro; el DPS mantiene cada accion esperando al threat del tanque
- **Correcciones de Stratholme**: rompe tres deadlocks; deja de actuar sobre estado de un objetivo anterior
- **Correccion de Black Morass**: divide el salto de portal P4 a P2 a traves del vado
- **Correccion de build**: lee PlayerbotAIConfig bajo ambas convenciones de nombre

---

## Sincronizacion 2026-10-02 (commit 60f3d98)

- **Compatibilidad de movimiento**: soporte para mod-playerbots #2747 movement via `DcPbCompat.h`

---

## Sincronizacion 2026-09-29 (commits bbf3442 → b501c63)

- **BRD**: dividido en Detention Block y Upper City
- **BRS**: dividido en LBRS y UBRS
- **Testdeck**: resolucion de tokens de mazmorra retirados via aliases de catalogo; soak lock creado perezosamente para Python 3.9

---

## Mejoras propias (no del upstream)

- **Seguridad del addon hook**: 19 correcciones en `DungeonClearAddonHook.cpp` incluyendo validacion de dueno de corrida, validacion de entradas de boss, throttle de comandos, validacion de parametros, y sincronizacion de settings. Estas mejoras se preservan en cada sincronizacion.
