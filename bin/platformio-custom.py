#!/usr/bin/env python3
# trunk-ignore-all(ruff/F821)
# trunk-ignore-all(flake8/F821): For SConstruct imports
import sys
import os
from os.path import join
import subprocess
import json
import re
from datetime import datetime
from typing import Dict

from readprops import readProps

Import("env")
platform = env.PioPlatform()
progname = env.get("PROGNAME")
lfsbin = f"{progname.replace('firmware-', 'littlefs-')}.bin"
manifest_ran = False

def infer_architecture(board_cfg):
    try:
        mcu = board_cfg.get("build.mcu") if board_cfg else None
    except KeyError:
        mcu = None
    except Exception:
        mcu = None
    if not mcu:
        return None
    mcu_l = str(mcu).lower()
    if "esp32s3" in mcu_l:
        return "esp32-s3"
    if "esp32c6" in mcu_l:
        return "esp32-c6"
    if "esp32c3" in mcu_l:
        return "esp32-c3"
    if "esp32" in mcu_l:
        return "esp32"
    if "rp2040" in mcu_l:
        return "rp2040"
    if "rp2350" in mcu_l:
        return "rp2350"
    if "nrf52" in mcu_l or "nrf52840" in mcu_l:
        return "nrf52840"
    if "stm32" in mcu_l:
        return "stm32"
    return None

def manifest_gather(source, target, env):
    global manifest_ran
    if manifest_ran:
        return
    # Skip manifest generation if we cannot determine architecture (host/native builds)
    board_arch = infer_architecture(env.BoardConfig())
    if not board_arch:
        print(f"Skipping mtjson generation for unknown architecture (env={env.get('PIOENV')})")
        manifest_ran = True
        return
    manifest_ran = True
    out = []
    board_platform = env.BoardConfig().get("platform")
    board_mcu = env.BoardConfig().get("build.mcu").lower()
    needs_ota_suffix = board_platform == "nordicnrf52"
    
    # Mapping of bin files to their target partition names
    # Maps the filename pattern to the partition name where it should be flashed
    partition_map = {
        f"{progname}.bin": "app0",              # primary application slot (app0 / OTA_0)
        lfsbin: "spiffs",                        # filesystem image flashed to spiffs
    }
    
    check_paths = [
        progname,
        f"{progname}.elf",
        f"{progname}.bin",
        f"{progname}.factory.bin",
        f"{progname}.hex",
        f"{progname}.merged.hex",
        f"{progname}.uf2",
        f"{progname}.factory.uf2",
        f"{progname}.zip",
        lfsbin,
        f"mt-{board_mcu}-ota.bin",
        "bleota-c3.bin"
    ]
    for p in check_paths:
        f = env.File(env.subst(f"$BUILD_DIR/{p}"))
        if f.exists():
            manifest_name = p
            if needs_ota_suffix and p == f"{progname}.zip":
                manifest_name = f"{progname}-ota.zip"
            d = {
                "name": manifest_name,
                "md5": f.get_content_hash(), # Returns MD5 hash
                "bytes": f.get_size() # Returns file size in bytes
            }
            # Add part_name if this file represents a partition that should be flashed
            if p in partition_map:
                d["part_name"] = partition_map[p]
            out.append(d)
            print(d)
    manifest_write(out, env)

def manifest_write(files, env):
    # Defensive: also skip manifest writing if we cannot determine architecture
    def get_project_option(name):
        try:
            return env.GetProjectOption(name)
        except Exception:
            return None

    def get_project_option_any(names):
        for name in names:
            val = get_project_option(name)
            if val is not None:
                return val
        return None

    def as_bool(val):
        return str(val).strip().lower() in ("1", "true", "yes", "on")

    def as_int(val):
        try:
            return int(str(val), 10)
        except (TypeError, ValueError):
            return None

    def as_list(val):
        return [item.strip() for item in str(val).split(",") if item.strip()]

    manifest = {
        "version": verObj["long"],
        "build_epoch": build_epoch,
        "platformioTarget": env.get("PIOENV"),
        "mcu": env.get("BOARD_MCU"),
        "repo": repo_owner,
        "files": files,
        "has_mui": False,
        "has_inkhud": False,
    }
    # Get partition table (generated in esp32_pre.py) if it exists
    if env.get("custom_mtjson_part"):
        # custom_mtjson_part is a JSON string, convert it back to a dict
        pj = json.loads(env.get("custom_mtjson_part"))
        manifest["part"] = pj
    # Enable has_mui for TFT builds
    if ("HAS_TFT", 1) in env.get("CPPDEFINES", []):
        manifest["has_mui"] = True
    if "MESHTASTIC_INCLUDE_INKHUD" in env.get("CPPDEFINES", []):
        manifest["has_inkhud"] = True

    pioenv = env.get("PIOENV")
    device_meta = {}
    device_meta_fields = [
        ("hwModel", ["custom_meshtastic_hw_model"], as_int),
        ("hwModelSlug", ["custom_meshtastic_hw_model_slug"], str),
        ("architecture", ["custom_meshtastic_architecture"], str),
        ("activelySupported", ["custom_meshtastic_actively_supported"], as_bool),
        ("displayName", ["custom_meshtastic_display_name"], str),
        ("supportLevel", ["custom_meshtastic_support_level"], as_int),
        ("images", ["custom_meshtastic_images"], as_list),
        ("tags", ["custom_meshtastic_tags"], as_list),
        ("requiresDfu", ["custom_meshtastic_requires_dfu"], as_bool),
        ("partitionScheme", ["custom_meshtastic_partition_scheme"], str),
        ("url", ["custom_meshtastic_url"], str),
        ("key", ["custom_meshtastic_key"], str),
        ("variant", ["custom_meshtastic_variant"], str),
    ]


    for manifest_key, option_keys, caster in device_meta_fields:
        raw_val = get_project_option_any(option_keys)
        if raw_val is None:
            continue
        parsed = caster(raw_val) if callable(caster) else raw_val
        if parsed is not None and parsed != "":
            device_meta[manifest_key] = parsed

    # Determine architecture once; if we can't infer it, skip manifest generation
    board_arch = device_meta.get("architecture") or infer_architecture(env.BoardConfig())
    if not board_arch:
        print(f"Skipping mtjson write for unknown architecture (env={env.get('PIOENV')})")
        return

    device_meta["architecture"] = board_arch

    # Always set requiresDfu: true for nrf52840 targets
    if board_arch == "nrf52840":
        device_meta["requiresDfu"] = True

    device_meta.setdefault("displayName", pioenv)
    device_meta.setdefault("activelySupported", False)

    if device_meta:
        manifest.update(device_meta)

    # Write the manifest to the build directory
    with open(env.subst("$BUILD_DIR/${PROGNAME}.mt.json"), "w") as f:
        json.dump(manifest, f, indent=2)

Import("projenv")

prefsLoc = projenv["PROJECT_DIR"] + "/version.properties"
verObj = readProps(prefsLoc)
# NAVARICO: override opcional de la version embebida (variable NAVARICO_APP_VERSION) para
# reproducir byte a byte builds anteriores (p. ej. "2.7.26.54e0d8d"). Sin la variable, la
# version es la normal del repo (2.7.26.<sha de git>). Lo usa verificar_paridad.ps1.
navarico_ver = os.environ.get("NAVARICO_APP_VERSION")
if navarico_ver:
    verObj["long"] = navarico_ver
print(f"Using meshtastic platformio-custom.py, firmware version {verObj['long']} on {env.get('PIOENV')}")

# get repository owner if git is installed
try:
    r_owner = (
        subprocess.check_output(["git", "config", "--get", "remote.origin.url"])
        .decode("utf-8")
        .strip().split("/")
    )
    repo_owner = r_owner[-2] + "/" + r_owner[-1].replace(".git", "")
except subprocess.CalledProcessError:
    repo_owner = "unknown"

jsonLoc = env["PROJECT_DIR"] + "/userPrefs.jsonc"
# NAVARICO: cada env puede apuntar a su perfil de claves/canales (custom_meshtastic_prefs en navarrico.ini).
# Si no se define, se usa el userPrefs.jsonc de la raiz (comportamiento original de Meshtastic).
custom_prefs = env.GetProjectOption("custom_meshtastic_prefs", "")
if custom_prefs:
    jsonLoc = custom_prefs if os.path.isabs(custom_prefs) else join(env["PROJECT_DIR"], custom_prefs)
with open(jsonLoc) as f:
    jsonStr = re.sub("//.*","", f.read(), flags=re.MULTILINE)
    userPrefs = json.loads(jsonStr)

pref_flags = []
# NAVARICO: builds Propia (R2IP/R1IP) — las claves del operador y el PIN BT se piden por
# variables de entorno y NUNCA se almacenan en el repo. Los envs Propia extienden los de
# General y activan la opcion custom_meshtastic_propia_keys; aqui sus macros sobrescriben
# las del perfil General. Sin las variables el build aborta con instrucciones.
nav_propia_raw = env.GetProjectOption("custom_meshtastic_propia_keys", "")
nav_propia_on = str(nav_propia_raw).strip().lower() in ("1", "true", "yes", "on")
if nav_propia_on:
    def nav_propia_required(var):
        val = os.environ.get(var, "").strip()
        if not val:
            print("NAVARICO ERROR: build Propia requiere la variable de entorno " + var)
            print("  Define NAVARICO_PROPIA_KEY_0 y NAVARICO_PROPIA_KEY_1 (hex entre llaves,")
            print("  p. ej. '{ 0xaa, 0xbb, ... }') y NAVARICO_PROPIA_BT (PIN de 6 digitos).")
            print("  O usa build_propia.ps1, que las pide de forma interactiva SIN guardarlas.")
            sys.exit(1)
        return val
    userPrefs["USERPREFS_USE_ADMIN_KEY_0"] = nav_propia_required("NAVARICO_PROPIA_KEY_0")
    userPrefs["USERPREFS_USE_ADMIN_KEY_1"] = nav_propia_required("NAVARICO_PROPIA_KEY_1")
    userPrefs["USERPREFS_FIXED_BLUETOOTH"] = nav_propia_required("NAVARICO_PROPIA_BT")
    print("NAVARICO: build Propia — claves y PIN inyectados desde variables de entorno (no se almacenan)")
# Pre-process the userPrefs
for pref in userPrefs:
    if userPrefs[pref].startswith("{"):
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref].lstrip("-").replace(".", "").isdigit():
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref] == "true" or userPrefs[pref] == "false":
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    elif userPrefs[pref].startswith("meshtastic_"):
        pref_flags.append("-D" + pref + "=" + userPrefs[pref])
    # If the value is a string, we need to wrap it in quotes
    else:
        pref_flags.append("-D" + pref + "=" + env.StringifyMacro(userPrefs[pref]) + "")

# General options that are passed to the C and C++ compilers
# Calculate unix epoch for current day (midnight)
# NAVARICO: BUILD_EPOCH por defecto = medianoche de hoy (comportamiento original de Meshtastic).
# Para reproducciones byte-idénticas de un build anterior, definir la variable de entorno
# NAVARICO_BUILD_EPOCH con el epoch Unix del día original (lo hace verificar_paridad.ps1).
navarico_epoch = os.environ.get("NAVARICO_BUILD_EPOCH")
if navarico_epoch:
    build_epoch = int(navarico_epoch)
else:
    current_date = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    build_epoch = int(current_date.timestamp())

# NAVARICO: los envs navarrico_* reportan el APP_ENV canónico de la placa (custom_meshtastic_app_env)
# para que el binario sea idéntico al build original (p. ej. Faketec reporta nrf52_promicro_diy_tcxo).
app_env = env.GetProjectOption("custom_meshtastic_app_env", "") or env.get("PIOENV")

flags = [
        "-DAPP_VERSION=" + verObj["long"],
        "-DAPP_VERSION_SHORT=" + verObj["short"],
        "-DAPP_ENV=" + app_env,
        "-DAPP_REPO=" + repo_owner,
        "-DBUILD_EPOCH=" + str(build_epoch),
    ] + pref_flags

print("Using flags:")
for flag in flags:
    print(flag)

projenv.Append(
    CCFLAGS=flags,
)

# NAVARICO: mapeo de ruta de libdeps para paridad byte-a-byte.
# El parser de build_flags de PlatformIO elimina los backslash, asi que el
# -ffile-prefix-map se inyecta aqui (Python) con la ruta completa real.
# El binario embebe rutas ".pio\libdeps\<PIOENV>\..." en bibliotecas que usan
# __FILE__ (p. ej. arduino-fsm). Con este mapa, todos los envs embeben la ruta
# canonica (custom_meshtastic_libdeps_map) del build original, y el MD5 deja de
# depender del nombre del env. Inerte si la opcion no existe.
nav_libdeps_map = env.GetProjectOption("custom_meshtastic_libdeps_map", "")
if nav_libdeps_map:
    src_prefix = ".pio\\libdeps\\" + env.get("PIOENV")
    canonical = nav_libdeps_map.replace("/", "\\")
    map_flag = "-ffile-prefix-map=" + src_prefix + "=" + canonical
    # NOTA: las librerias se compilan en envs propios (lib builders), NO en projenv ni en env.
    # Se inyecta el flag en cada una para que su __FILE__ (ruta de libdeps con el nombre del env)
    # quede remapeado a la ruta canonica del build original (paridad byte-a-byte).
    for lb in env.GetLibBuilders():
        lb.env.Append(CCFLAGS=[map_flag])
    env.Append(CCFLAGS=[map_flag])
    print(f"NAVARICO: -ffile-prefix-map {src_prefix} -> {canonical}")

# NAVARICO: override de la marca temporal embebida (Crypto/RNG y RadioLib/Module usan __TIME__/__DATE__).
# Sin las variables de entorno, comportamiento original (marca de hoy). Con ellas
# (verificar_paridad.ps1) se reproduce la marca del build de referencia byte a byte.
navarico_btime = os.environ.get("NAVARICO_BUILD_TIME", "")
navarico_bdate = os.environ.get("NAVARICO_BUILD_DATE", "")
if navarico_btime or navarico_bdate:
    stamp_flags = []
    if navarico_btime:
        # Comillas escapadas (\\"): el parser de linea de comandos de Windows (CommandLineToArgvW)
        # eliminaria las comillas simples, dejando __TIME__ sin comillas (error de compilacion).
        stamp_flags.append('-D__TIME__=\\"' + navarico_btime + '\\"')
    if navarico_bdate:
        stamp_flags.append('-D__DATE__=\\"' + navarico_bdate + '\\"')
    for lb in env.GetLibBuilders():
        lb.env.Append(CCFLAGS=stamp_flags)
    env.Append(CCFLAGS=stamp_flags)
    projenv.Append(CCFLAGS=stamp_flags)
    print(f"NAVARICO: marca temporal override {navarico_btime} {navarico_bdate}")

for lb in env.GetLibBuilders():
    if lb.name == "meshtastic-device-ui":
        lb.env.Append(CPPDEFINES=[("APP_VERSION", verObj["long"])])
        break

# Get the display resolution from macros
def get_display_resolution(build_flags):
    # Check "DISPLAY_SIZE" to determine the screen resolution
    for flag in build_flags:
        if isinstance(flag, tuple) and flag[0] == "DISPLAY_SIZE":
            screen_width, screen_height = map(int, flag[1].split("x"))
            return screen_width, screen_height
    print("No screen resolution defined in build_flags. Please define DISPLAY_SIZE.")
    exit(1)

def load_boot_logo(source, target, env):
    build_flags = env.get("CPPDEFINES", [])
    logo_w, logo_h = get_display_resolution(build_flags)
    print(f"TFT build with {logo_w}x{logo_h} resolution detected")

    # Load the boot logo from `branding/logo_<width>x<height>.png` if it exists
    source_path = join(env["PROJECT_DIR"], "branding", f"logo_{logo_w}x{logo_h}.png")
    dest_dir = join(env["PROJECT_DIR"], "data", "boot")
    dest_path = join(dest_dir, "logo.png")
    if env.File(source_path).exists():
        print(f"Loading boot logo from {source_path}")
        # Prepare the destination
        env.Execute(f"mkdir -p {dest_dir} && rm -f {dest_path}")
        # Copy the logo to the `data/boot` directory
        env.Execute(f"cp {source_path} {dest_path}")

# Load the boot logo on TFT builds
if ("HAS_TFT", 1) in env.get("CPPDEFINES", []):
    env.AddPreAction(f"$BUILD_DIR/{lfsbin}", load_boot_logo)

board_arch = infer_architecture(env.BoardConfig())
should_skip_manifest = board_arch is None

# For host/native envs, avoid depending on 'buildprog' (some targets don't define it)
mtjson_deps = [] if should_skip_manifest else ["buildprog"]
if not should_skip_manifest and platform.name == "espressif32":
    # Build littlefs image as part of mtjson target
    # Equivalent to `pio run -t buildfs`
    target_lfs = env.DataToBin(
        join("$BUILD_DIR", "${ESP32_FS_IMAGE_NAME}"), "$PROJECT_DATA_DIR"
    )
    mtjson_deps.append(target_lfs)

if should_skip_manifest:
    def skip_manifest(source, target, env):
        print(f"mtjson: skipped for native environment: {env.get('PIOENV')}")

    env.AddCustomTarget(
        name="mtjson",
        dependencies=mtjson_deps,
        actions=[skip_manifest],
        title="Meshtastic Manifest (skipped)",
        description="mtjson generation is skipped for native environments",
        always_build=True,
    )
else:
    env.AddCustomTarget(
        name="mtjson",
        dependencies=mtjson_deps,
        actions=[manifest_gather],
        title="Meshtastic Manifest",
        description="Generating Meshtastic manifest JSON + Checksums",
        always_build=True,
    )

    # Run manifest generation as part of the default build pipeline for non-native builds.
    env.Default("mtjson")
