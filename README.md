# ThrottleControl for OMSI 2

Plugin for **OMSI 2** that makes keyboard throttle feel like a pedal: the value **holds** after you release the gas key, while dump and kickdown use the same binds as the game.

---

## Features

| Action | Behavior |
| --- | --- |
| **Throttle** (default `Num 8`) | Ramps up smoothly and **holds** after release |
| **Throttle + Num +** | Instant full throttle (kickdown) |
| **Num +** (hold) | Gradually lowers held throttle |
| **Num +** twice within **0.5 s** | Instant dump to `0` |
| **Brake** (default `Num 2`) | Instant throttle dump; brake stays stock OMSI |

Binds are read from `OMSI 2\Inputs\keyboard.cfg`:

- `throttle`
- `brake`
- `throttle_amplify`

Restart OMSI after changing key bindings in Options.

---

## Install

1. Build the plugin (see [Build](#build)) or download a Release.
2. Copy **both** files into `OMSI 2\plugins\`:

```
ThrottleControl.dll
ThrottleControl.opl
```

3. Start OMSI 2. In `logfile.txt` you should see:

```text
[ThrottleControl] Plugin ThrottleControl loaded.
```

The DLL must be **32-bit (Win32)** and sit next to the `.opl`.

---

## Controls

```
Num 8          -> ramp gas, hold after release
Num 8 + Num +  -> 100% throttle
Num +          -> smooth dump of held throttle
Num + x2       -> instant dump (0.5 s window)
Num 2          -> stock OMSI brake + instant throttle dump
```

Tune rates in `dllmain.cpp`:

```cpp
THROTTLE_ATTACK   // ramp up
THROTTLE_RELEASE  // smooth dump on Num+
```

---

## Build

**Requirements**

- Visual Studio 2022 (C++)
- Platform **Win32** (OMSI 2 is 32-bit)
- Configuration **Release**

**Steps**

1. Open `OMSI2Plugin-ThrottleControl.sln`.
2. Select `Release` | `x86` (Win32).
3. **Build -> Rebuild**.
4. Output: `Release\ThrottleControl.dll`.
5. Copy it with `ThrottleControl.opl` into `plugins\`.

Command line:

```powershell
msbuild OMSI2Plugin-ThrottleControl.vcxproj /p:Configuration=Release /p:Platform=Win32 /t:Rebuild
```

---

## Repository layout

```
├── dllmain.cpp                 # plugin logic
├── shared.h                    # logging / OMSI version check
├── exports.def                 # OMSI plugin exports
├── ThrottleControl.opl         # variable config
├── OMSI2Plugin-ThrottleControl.sln
├── OMSI2Plugin-ThrottleControl.vcxproj
├── LICENSE
└── README.md
```

---

## Compatibility

- OMSI 2 (Steam **2.3.004** and tram patch **2.2.032** for `logfile.txt` via the game logger)
- Keyboard only (binds from `keyboard.cfg`). Gamepad / wheel axes are not intercepted.
- Vehicle variable: `Throttle` via `[varlist]`. The plugin does **not** rewrite `Brake`.

Debug console: start OMSI with `-throttlecontrol_debug`.

---

## Known limitations

- Keys are polled via WinAPI; some setups may see rare micro-stutters on keypress (OMSI + DirectInput).
- Behavior can differ on unusual bus scripts.

---

## License

[MIT](LICENSE) — free to use, modify, and redistribute.

---

## Credits

Logfile integration and version detection inspired by [OMSIPresence](https://github.com/brokenphilip/OMSIPresence). Plugin API: [OMSI Plug-in interface](http://wiki.omnibussimulator.de/omsiwikineu.de/index.php?title=Plug-in-Schnittstelle).

---

## На русском

Плагин делает газ с клавиатуры похожим на педаль: значение **удерживается** после отпускания клавиши газа.

| Действие | Поведение |
| --- | --- |
| Газ (`Num 8`) | Плавно набирает и фиксирует уровень |
| Газ + `Num +` | Полный газ (kickdown) |
| `Num +` | Плавно снижает удержанный газ |
| `Num +` дважды за 0.5 с | Мгновенный сброс |
| Тормоз (`Num 2`) | Сброс газа; тормоз — штатный OMSI |

Бинды берутся из `Inputs\keyboard.cfg`. В `plugins\` нужны оба файла: `ThrottleControl.dll` + `ThrottleControl.opl` (DLL только Win32 / Release).
