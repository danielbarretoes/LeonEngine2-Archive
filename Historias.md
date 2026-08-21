# Historias de usuario — Leon Engine

Este documento describe **todo lo que un autor puede vivir hoy** en el editor Unreal-lite y, cuando abre un juego como Leon Tournament, en Play In Editor o empaquetado.

Cada historia es un recorrido concreto: qué hace la persona, qué ve, qué pulsa, y cuándo se considera hecha. Al final hay una lista de **lo que todavía no existe**, para no confundir deseo con producto.

Formato:

- **Como** — quién
- **Quiero** — la acción
- **Para** — el porqué
- **Recorrido** — pasos reales en la UI
- **Hecho cuando** — criterios
- **Límites** — qué no hace el editor (honesto)

---

## 1. Arranque y proyectos

### H1. Abrir el editor y ver el Project Hub

**Como** autor que arranca Leon Editor  
**Quiero** una pantalla de bienvenida con proyectos recientes  
**Para** no tener que buscar el `.lproject` a mano cada vez

**Recorrido**

1. Arranco el editor (`python Scripts/build_editor.py --run` o `LeonEditor.exe`).
2. Si no hay un proyecto ya abierto por entorno, veo el **Project Hub** a pantalla completa.
3. En la pestaña Recent aparecen nombre, ruta y versión de motor de los últimos proyectos.
4. Puedo hacer clic en uno para abrirlo, o **Browse** y elegir un `.lproject` (o una carpeta que lo contenga).
5. **Ctrl+P** o File → Project Browser… vuelve a abrir el hub en cualquier momento.

**Hecho cuando** el hub lista recientes, abre un proyecto válido y avisa si la ruta no existe.

**Límites** El hub no clona repos ni descarga samples de internet.

---

### H2. Crear un proyecto en blanco desde cero

**Como** autor que empieza un juego nuevo  
**Quiero** un proyecto vacío con mapa, GameMode de engine y primitivas  
**Para** blockoutear un nivel sin escribir C++ ni tener una DLL de juego

**Recorrido**

1. En el hub, pestaña **New Project**.
2. Pongo nombre y carpeta padre (Browse si hace falta).
3. Eligo plantilla **Blank Project (Empty Map)**.
4. Create & Open Project.
5. Se crea: `Nombre.lproject`, `Config/`, `Content/Maps/MainLevel.lmap`, carpetas Meshes / Textures / Audio / Raw.
6. El mapa abre con cielo procedural, una luz direccional y **sin** módulo de juego.
7. En Place Actors sigo teniendo Cube, Plane, Ramp, Sphere, Cylinder, luces y volúmenes (vienen del engine).
8. Play usa `AGameModeBase` + peón por defecto del engine (cámara fly).

**Hecho cuando** el proyecto abre, el mapa es un `.lmap` válido, Place Actors funciona y Play no exige una DLL.

**Límites** No se genera un módulo C++ compilable. No hay personajes FPS hasta que el proyecto registre clases propias.

---

### H3. Crear un proyecto Starter (luz + suelo)

**Como** autor que quiere un suelo bajo los pies al instante  
**Quiero** la plantilla **Starter Level (Light + Floor)**  
**Para** no empezar en el vacío absoluto

**Recorrido**

Igual que H2, pero la plantilla añade un suelo (cubo aplastado) y un **PlayerStart**. Play spawnea cerca del suelo.

**Hecho cuando** al dar Play veo el suelo iluminado y no caigo al infinito sin referencia.

---

### H4. Abrir un proyecto de juego existente (Leon Tournament)

**Como** autor del shooter  
**Quiero** abrir `LeonTournament.lproject` y que el editor cargue las clases del juego  
**Para** editar la arena y probar el personaje real

**Recorrido**

1. Abro el proyecto desde recientes o Browse.
2. El editor registra el módulo in-process (personaje, GameMode de partida, etc.).
3. Carga el mapa por defecto del `.lproject` (p. ej. menú o arena).
4. World Settings / Project Settings muestran GameModes del juego, no solo `AGameModeBase`.

**Hecho cuando** Place/Details/Play conocen las clases del juego y el mapa abre.

**Límites** Un `.lproject` con GameMode custom **sin** módulo carga un aviso; Play se niega si el GameMode no es del engine.

---

## 2. Mapas y no perder trabajo

### H5. Abrir un mapa desde el Content Browser

**Como** autor con varios `.lmap`  
**Quiero** hacer doble clic en un nivel  
**Para** cambiar de arena, menú o lab sin cerrar el proyecto

**Recorrido**

1. Content Browser → carpeta Maps.
2. Doble clic (o Open) en `TournamentArena.lmap`, `MainMenu.lmap`, etc.
3. El viewport muestra el nivel; la barra de título y el toolbar dicen el nombre del mapa.
4. Si el mapa actual tenía cambios sin guardar, aparece **Save / Don't Save / Cancel**.

**Hecho cuando** el mundo del editor es el del archivo elegido y no se pierde trabajo en silencio.

---

### H6. Ver que el mapa está sucio y guardarlo

**Como** autor que mueve un cubo  
**Quiero** un `*` en el título y Ctrl+S que siempre guarde  
**Para** no preguntarme si el disco está al día

**Recorrido**

1. Tras spawn, delete, duplicate, gizmo, Details o World Settings, el título pasa a `Leon Engine Editor - [Proyecto] - MapName *`.
2. **Ctrl+S**, File → Save Current Map, o el icono Save del toolbar.
3. Si el mapa es Untitled, se abre el diálogo nativo de guardar `.lmap`.
4. Tras un save correcto, desaparece el `*`.
5. Content Browser → **Save All** guarda el mapa y el `.lproject` si hay proyecto.

**Hecho cuando** el `*` refleja la realidad y Ctrl+S funciona también en Untitled.

---

### H7. No tirar el trabajo al cambiar de proyecto o al salir

**Como** autor con cambios sin guardar  
**Quiero** que Load / Open Project / Exit me pregunten  
**Para** elegir Save, Don't Save o Cancel

**Recorrido**

Cualquier carga de mapa, apertura de otro `.lproject` o Exit (Alt+F4) con mapa sucio abre el modal. Cancel deja el editor como estaba. Don't Save descarta. Save persiste y luego continúa.

**Hecho cuando** no hay cierre silencioso con cambios pendientes.

---

### H8. Crear un mapa nuevo desde Content

**Como** autor que quiere un nivel extra  
**Quiero** Add → Level / Map (`.lmap`)  
**Para** tener otro archivo en `/Game/Maps` y abrirlo después

**Hecho cuando** aparece un `.lmap` en la carpeta actual del browser y puedo abrirlo.

**Límites** El mapa nuevo es un stub de asset; no sustituye al flujo “guardar Untitled con diálogo”.

---

## 3. Viewport: ver, navegar, enfocar

### H9. Volar la cámara como en Unreal

**Como** autor blockouteando  
**Quiero** navegar en 3D sin que el ratón se pare en el borde del monitor  
**Para** orbitar una arena grande

**Recorrido**

- **RMB + WASD**: vuelo en perspectiva. **Q / E** bajan / suben. Rueda (con RMB) cambia la velocidad de vuelo (1–50).
- Mientras RMB está pulsado, el cursor se oculta (look continuo).
- **MMB** o **Alt+MMB**: pan.
- **Alt+LMB**: órbita alrededor del pivote.
- **Alt+RMB**: dolly.
- Rueda sin RMB: acercar / alejar.
- **F**: enfoca al actor primario (también Outliner → Focus in Viewport).

**Hecho cuando** puedo recorrer el nivel con look libre y F encuadra la selección.

---

### H10. Cambiar vista y sombreado

**Como** autor alineando geometría  
**Quiero** Perspective / Top / Front / Side y Lit / Unlit / Wireframe  
**Para** colocar suelos y paredes con precisión

**Recorrido** Combos en la barra del viewport. Ortho en Top/Front/Side. Lit es el render de juego; Unlit y Wireframe ayudan a leer siluetas.

**Hecho cuando** el modo se aplica al viewport embebido (no a una ventana extra).

---

### H11. Ver la rejilla del editor

**Como** autor de blockout  
**Quiero** una rejilla XZ de 1 m (mayor cada 10 m) alrededor del pivote  
**Para** estimar tamaños a ojo

**Recorrido** Toggle **Grid** en el viewport (por defecto encendida). Independiente de mostrar gizmos.

**Hecho cuando** las líneas se ven en el FBO del viewport y no sustituyen al suelo del juego.

---

## 4. Colocar el nivel (blockout)

### H12. Instanciar primitivas desde Place Actors

**Como** autor de un FPS/TPS  
**Quiero** Cube, Plane, Ramp, Sphere y Cylinder  
**Para** whiteboxear pasillos, cubiertas y desniveles

**Recorrido**

1. Place Actors → pestaña **Shapes**.
2. **Clic**: spawnea cerca del pivote / mundo.
3. **Arrastrar al viewport**: el fantasma sigue la superficie bajo el cursor (suelo, rampa, mesh con colisión). Si no hay hit, cae en el plano de la rejilla que pasa por el pivote (no siempre Y=0).
4. El actor queda seleccionado; Ctrl+Z deshace el spawn.

**Hecho cuando** las cinco formas aparecen con mesh de engine, se pueden transformar y el drop cae sobre rampas/suelos.

---

### H13. Colocar luces, cámara, PlayerStart y actor vacío

**Como** autor del nivel  
**Quiero** luces y puntos de spawn sin código  
**Para** iluminar y probar Play

**Recorrido** Place Actors:

- **Basic**: Empty Actor, Character, Pawn, Camera, PlayerStart.
- **Lights**: Directional, Point, Spot, **Sky Light / Skybox**.

Clic o drag al viewport. Sky Light: **solo uno por mapa**; si ya hay, no spawnea y avisa en el log (inglés).

**Hecho cuando** las luces se ven, PlayerStart existe para PIE, y el segundo Sky Light se rechaza.

---

### H14. Colocar volúmenes de bloqueo y de trigger

**Como** autor de una arena  
**Quiero** un Blocking Volume que pare al jugador y un Trigger Volume que no  
**Para** acotar el mapa y marcar zonas (daño, spawn, objetivos)

**Recorrido** Place Actors → **Volumes**. Trigger: overlap, no bloquea movimiento; en Details hay **Enabled**. Blocking: caja que sí bloquea.

**Hecho cuando** un trace de WorldStatic choca con el blocking y atraviesa el trigger; Details muestra el checkbox del trigger.

---

### H15. Spawn desde el Outliner

**Como** autor con el viewport lejos  
**Quiero** clic derecho en el Outliner → Empty / Cube / Sphere / luces / Camera  
**Para** añadir actores sin ir a Place Actors

**Recorrido** El spawn se registra en el historial (undo). El nuevo actor se selecciona.

---

### H16. Soltar un static mesh nativo desde Content al viewport

**Como** autor con un `.lmesh` importado  
**Quiero** arrastrarlo al viewport y que aparezca un actor con esa malla  
**Para** vestir el blockout con arte real

**Recorrido** Drag de un `.lmesh` (payload Content Browser). Spawn en la superficie bajo el cursor. **No** spawnea desde `.fbx` / `.obj` / `.gltf`.

**Hecho cuando** solo los nativos `.lmesh` crean geometría jugable.

**Límites** Drop de skeletal mesh (`.lskeletalmesh`) aún no instancia un personaje animado.

---

## 5. Transformar, seleccionar, deshacer

### H17. Trasladar, rotar y escalar en los 3 ejes

**Como** autor  
**Quiero** gizmos W / E / R y valores XYZ en Details  
**Para** colocar cualquier malla 3D

**Recorrido**

1. Selecciono el actor (clic en viewport o Outliner).
2. **Q** Select, **W** Translate, **E** Rotate, **R** Scale (no aplican con RMB de cámara).
3. Arrastro **un** eje del gizmo. Con Snap, solo ese eje se cuantiza (translate a lo largo del eje activo; rotate/scale el componente arrastrado).
4. Toggle **World / Local**. Snap on/off y menú de incrementos (1/5/10/50/100 u; 5/15/45/90°; 0.1–1.0 escala).
5. Details: Location / Rotation / Scale (o Relative si Local Transform Mode). Botones de reset por eje.
6. Al soltar el gizmo, el cambio entra en Undo. Ctrl+Z / Ctrl+Y (o menú Edit).

**Hecho cuando** los tres ejes funcionan, el snap no ensucia ejes inactivos, y Undo revierte el transform.

---

### H18. Selección simple, múltiple y marco

**Como** autor  
**Quiero** clic, Shift/Ctrl para añadir, y marquee  
**Para** mover un grupo de cubos

**Recorrido** Clic selecciona. Marquee en el viewport (no durante Play). Outliner: clic, rangos, multi-selección. Details en multi edita Location común si coincide.

**Hecho cuando** la selección vive solo en el contexto del editor (un solo dueño). Durante PIE no se edita el mundo de Play desde Outliner/Details/Place.

---

### H19. Duplicar y borrar actores

**Como** autor  
**Quiero** Ctrl+D y Delete  
**Para** repetir props y limpiar

**Recorrido** Duplicate desplaza en +X. Delete pide confirmación implícita vía comando (Undo restaura). Si el Content Browser está enfocado, Delete borra **assets**, no actores.

**Hecho cuando** Duplicate/Delete están en el historial y no pisan el delete de archivos.

---

## 6. Outliner: encontrar y organizar

### H20. Filtrar, buscar, ocultar y bloquear

**Como** autor en un mapa con 40+ actores  
**Quiero** filtros All / Meshes / Lights / Cameras / Characters / Volumes, búsqueda por nombre, ojo y candado  
**Para** no pinchar una luz al querer un mesh

**Recorrido** El filtro Volumes incluye blocking y triggers. Hide: el actor no se pinta ni se pickea. Lock: no se pickea en el viewport. Focus (F). Rename (F2). Folders: crear, renombrar, arrastrar actores, Move to Root.

**Hecho cuando** hide/lock se respetan en el viewport (no son cosmética) y el índice del filtro coincide con la categoría.

---

### H21. Jerarquía padre/hijo

**Como** autor  
**Quiero** reparentar arrastrando en el Outliner y Detach  
**Para** mover un grupo (p. ej. arma + mesh) juntos

**Recorrido** Drag and drop entre actores. Menú: Detach from Parent, Reset Location/Rotation/Scale.

---

## 7. Details: instanciar y vestir actores

### H22. Cambiar la malla de un actor y verla al momento

**Como** autor  
**Quiero** pegar o soltar un `.lmesh` en Mesh Asset  
**Para** sustituir el cubo por la malla importada

**Recorrido** Details → Static Mesh Component → campo Mesh Asset (texto o drop desde Content). El engine **carga** el mesh (`UAssetManager`), no solo guarda el string. Cast Shadows y Mobility (Static / Stationary / Movable).

**Hecho cuando** el viewport muestra la nueva geometría al validar el path.

---

### H23. Materiales, luces, cámara y colisión

**Como** autor  
**Quiero** asignar `.lmat`, ajustar luces, FOV de cámara y caja de colisión  
**Para** look de nivel sin recompilar

**Recorrido**

- Material: path o drop; se carga la instancia.
- Directional / Point / Spot: Enabled, color, intensity, radius (point/spot), mobility.
- Sky Light: Enabled, Exposure.
- Camera: FOV, near/far, primary.
- PlayerStart: Enabled y tags/team si aplica.
- Box Collision: Min/Max, Block Movement.
- **Add Component**: Static Mesh, Material, luces, Camera, Box Collision si el actor no los tiene.

Hay un filtro de propiedades en Details para buscar por nombre de sección.

**Límites** No hay Material Graph: el `.lmat` se edita como asset de texto / instancia, no como nodos.

---

### H24. Editar Trigger y Sky en Details

**Como** autor  
**Quiero** apagar un trigger o el cielo sin borrar el actor  
**Para** iterar gameplay y look

**Hecho cuando** Enabled del trigger y Enabled/Exposure del skybox sucian el mapa y se guardan en el `.lmap`.

---

## 8. Content Browser e importación

### H25. Navegar Content como carpeta de `/Game`

**Como** autor  
**Quiero** árbol, breadcrumbs, atrás/adelante, Grid/List, búsqueda, thumbnails  
**Para** encontrar texturas y meshes

**Recorrido** Settings del browser: tamaño de card, extensiones, Grid vs List. Tipos visibles: StaticMesh (`.lmesh`), SkeletalMesh, Skeleton, Animation, Texture (`.ltex` / `.lhdr`), Material, Level, Audio, Source (fbx/obj residuales), Folder.

**Hecho cuando** el browser refleja el disco del proyecto y no trata un FBX como mesh jugable.

---

### H26. Importar FBX / OBJ / texturas / HDR de verdad

**Como** autor con arte de Blender/Maya  
**Quiero** Import y obtener `.lmesh` / `.ltex` / `.lhdr`  
**Para** que el runtime no decodifique FBX en Play

**Recorrido**

1. Import → diálogo nativo (FBX, OBJ, PNG/JPG/TGA, HDR, y nativos Leon).
2. FBX/OBJ/textura/HDR lanza **AssetTool** en segundo plano (`import_assets.py`), como el bake: líneas en **Output Log**, toast si falla, status “Importing asset…”.
3. El resultado vive bajo `Content/Meshes`, `Content/Textures` o `Content/HDR`.
4. **No** se copia el `.fbx` a Content como si fuera el mesh del juego.
5. GLTF se rechaza con toast (usar FBX/OBJ o un `.lmesh` ya nativo).

**Hecho cuando** un FBX de prueba produce `.lmesh` (y materiales extraídos si el tool los genera) y el log está en inglés.

---

### H27. Traer audio y assets ya nativos

**Como** autor  
**Quiero** copiar `.wav` / `.ogg` / `.mp3` y también `.lmesh` / `.ltex` ya convertidos  
**Para** SFX y reutilizar packs nativos

**Recorrido** Import de esos formatos copia al directorio actual del browser (el runtime carga wav/ogg directo).

**Hecho cuando** el archivo aparece en Content y `USoundWave` puede resolver `/Game/...`.

---

### H28. Carpetas, duplicar, Explorer, borrar con referencias

**Como** autor  
**Quiero** Add Folder, Duplicate, F2 Rename, Show in Explorer, Delete  
**Para** ordenar el proyecto

**Recorrido** Delete lista actores del **mapa cargado** que referencian el path exacto (no el stem `Cube` vs `CubeRed`). Confirmar unlinks y borra. Rename de un asset **referenciado se bloquea** (log + toast): hay que desvincular o borrar refs primero.

**Hecho cuando** Cube y CubeRed no se confunden, y rename no deja paths rotos en silencio.

---

### H29. Crear material / script stub

**Como** autor  
**Quiero** Add → Material (`.lmat`) o Blueprint/Script (`.lua`)  
**Para** tener un archivo en disco que luego relleno

**Límites** No hay editor de nodos ni de Blueprints visuales. El `.lua` no es un Blueprint de Unreal.

---

## 9. Play In Editor (probar el juego)

### H30. Jugar el mapa en el viewport (FPS/TPS)

**Como** autor  
**Quiero** Play, poseer el peón del GameMode, mirar con el ratón capturado  
**Para** sentir aim, saltos y escala del nivel

**Recorrido**

1. Toolbar Play (o el flujo de Play Settings).
2. Si Auto-save está on, se guarda el mapa antes.
3. Se clona el mapa a un **PlayWorld**; Outliner/Details siguen el **EditorWorld** (no se edita la partida).
4. El cursor se oculta (GameOnly), ImGui no se come el input.
5. **Esc** pausa el juego (menú/pausa del GameMode), **no** mata la sesión.
6. **Stop** en toolbar o **Shift+Esc**: vuelve el editor, cámara y cursor.

**Hecho cuando** look no se corta en el borde de pantalla, Esc no cierra PIE, Stop restaura el editor.

**Límites** No hay Simulate, Possess/Eject ni frame step. Play es **Selected Viewport** (no hay ventana GLFW extra).

---

### H31. Ajustar Play Settings (jugadores y red)

**Como** autor de multiplayer  
**Quiero** 1–4 jugadores, Standalone / Listen / Client / Dedicated, puerto y auto-save  
**Para** probar listen-server en el mismo PC

**Recorrido** Icono engranaje junto a Play. Multi-instancia: host en el viewport; clientes extra son procesos `LeonEditor --pie-role=client`. Dedicated no hace Login local.

**Hecho cuando** los settings persisten en `Editor/Saved/PlaySettings.json` y el combo “New Editor Window” no miente (está deshabilitado).

---

### H32. Play en proyecto en blanco vs proyecto con módulo

**Como** autor  
**Quiero** reglas claras  
**Para** no pensar que Play está roto

- Blank / engine GameMode (`AGameModeBase` / `AGameMode`): Play **sí**, peón fly del engine.
- Proyecto con GameMode de juego y módulo cargado: Play usa ese GameMode (p. ej. personaje Tournament).
- GameMode de juego **sin** módulo: toast, no arranca.

---

## 10. Iluminación estática (Lightmass)

### H33. Hornear lightmaps Draft o Production

**Como** autor de una arena  
**Quiero** Bake Draft / Production en toolbar o Build  
**Para** ver GI/AO en Static lights sin GPU dedicada de GI en tiempo real

**Recorrido** Requiere mapa en disco. Corre el script en segundo plano; Output Log muestra progreso. Al terminar recarga el mapa para aplicar `.llightmap`. Si el hash del bake está stale, el log avisa y no aplica lightmaps viejos.

**Hecho cuando** Draft es rápido para iterar y Production es más caro; el viewport refleja el bake si el hash coincide.

**Límites** No es Lumen. Luces Movable siguen dinámicas.

---

## 11. World Settings y Project Settings

### H34. Elegir GameMode del mapa

**Como** autor  
**Quiero** World Settings → GameMode Override  
**Para** que esta arena use el GameMode de partida y el menú otro

**Recorrido** Combo de clases `*GameMode*` registradas. **None** = DefaultGameMode del `.lproject`. Marca el mapa sucio.

---

### H35. Resolución de lightmap del mundo

**Como** autor  
**Quiero** Enable Static Lighting y Default Lightmap Resolution  
**Para** controlar calidad vs tiempo de bake

---

### H36. Nombre, mapa de arranque y GameMode del proyecto

**Como** autor  
**Quiero** Project Settings: nombre, Editor Startup Map (`/Game/Maps/...`), Default GameMode, Save Settings  
**Para** que al reabrir el proyecto entre al mapa correcto

**Hecho cuando** Save escribe el `.lproject` y el siguiente Open Project respeta DefaultMap / DefaultGameMode.

---

## 12. Layout, logs y empaquetado

### H37. Enseñar, ocultar y guardar el layout de paneles

**Como** autor  
**Quiero** Window → Viewport, Place Actors, Outliner, Details, Content Browser, Output Log, World Settings, Project Settings  
**Para** trabajar en un portátil o en un 32"

**Recorrido** Docking ImGui. Reset to Default Layout. Save Layout As… y cargar layouts con nombre. El layout activo se recuerda.

---

### H38. Leer el Output Log y toasts

**Como** autor  
**Quiero** ver import, bake, PIE y errores en el log  
**Para** no adivinar por qué no apareció el mesh

**Recorrido** Window → Output Log. Toasts abajo al centro (éxito / error). Import y bake rellenan el log en inglés.

---

### H39. Lanzar el juego empaquetado

**Como** autor  
**Quiero** Build → Launch Packaged Game  
**Para** probar el exe como jugador, no el viewport de edición

**Límites** Eso no es PIE. El empaquetado sigue el pipeline de scripts del repo (`run_project` / cook), no una ventana extra del editor.

---

## 13. Experiencia de juego (Leon Tournament, cuando el proyecto está abierto)

### H40. Entrar en partida desde el menú

**Como** jugador / autor en PIE o empaquetado  
**Quiero** menú → mapa de arena, spawn, HUD  
**Para** validar el loop del producto

El GameInstance y el GameMode del módulo poseen menú, travel, match start/end, respawn. El editor no pinta un canvas UMG WYSIWYG: el HUD es el del juego.

---

### H41. Mover, mirar, disparar y pausar

**Como** jugador FPS  
**Quiero** WASD, ratón look (cursor capturado), disparo del arma del character, Esc = pausa  
**Para** sentir el TTK y el layout de la arena

**Hecho cuando** tras pausar y reanudar el look no da un spike salvaje, y Shift+Esc (en editor) sigue siendo Stop PIE.

---

### H42. Varios jugadores locales (listen)

**Como** autor de netcode  
**Quiero** Number of Players > 1 + Listen Server  
**Para** ver dos peones y un host en el mismo mapa

Clientes extra = procesos hijos del editor. Dedicated = sin peón local en el host.

---

## 14. Atajos (referencia rápida)

| Atajo | Dónde | Acción |
| :--- | :--- | :--- |
| Ctrl+S | Editor | Guardar mapa (diálogo si Untitled) |
| Ctrl+P | Editor | Project Hub |
| Ctrl+Z / Ctrl+Y | Editor | Undo / Redo (spawn, transform, delete, duplicate) |
| Ctrl+D | Actores | Duplicar |
| Delete | Actores / Content | Borrar selección (no actores si el browser tiene foco) |
| F | Viewport / Outliner | Enfocar actor |
| F2 | Outliner / Content | Renombrar |
| Q W E R | Viewport (sin RMB) | Select / Move / Rotate / Scale |
| RMB+WASD, Q/E | Viewport | Volar |
| Esc | PIE | Pausa del juego |
| Shift+Esc | PIE | Stop Play In Editor |
| Alt+F4 | Editor | Salir (con modal si hay cambios) |

---

## 15. Lo que todavía no puedo hacer (a propósito)

Estas cosas **no** son historias entregadas. Si aparecen en un diseño, son fases posteriores (full Unreal, no lite):

- **Grafo de materiales** (nodos, Shader Inspector).
- **UMG Canvas** drag-and-drop (`UButton`, `UTextBlock`, etc.).
- **Visualizador de Behavior Tree / Blackboard**.
- **Play en New Editor Window** (segunda ventana GLFW).
- **Simulate / Possess / Eject / frame step**.
- **Nanite, Lumen, Niagara, World Partition, Sequencer, Blueprints visuales**.
- Import **GLTF/GLB**.
- Soltar **skeletal mesh** al viewport como actor animado.
- **Rename** de un asset que sigue referenciado (hoy se bloquea; no reescribe paths en el mapa).
- Undo de **cada** float de Details (el historial cubre spawn, transform, delete, duplicate).

Cuando una de estas se implemente, se mueve a las secciones 1–13 con recorrido y criterios, igual que el resto.
)
