import os
import re
Import("env")


def read_fw_version(config_path):
    if not os.path.isfile(config_path):
        return None
    with open(config_path, "r", encoding="utf-8") as f:
        match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
    return match.group(1).strip() if match else None


def parse_env_file(path):
    values = {}
    if not os.path.isfile(path):
        return values
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            key = key.strip()
            val = val.strip()
            if len(val) >= 2 and ((val[0] == val[-1] == '"') or (val[0] == val[-1] == "'")):
                val = val[1:-1]
            values[key] = val
    return values


project_dir = env.subst("$PROJECT_DIR")

fw_version = read_fw_version(os.path.join(project_dir, "include", "config.h"))
if fw_version:
    safe_ver = re.sub(r"[^0-9A-Za-z._-]", "_", fw_version)
    env.Replace(PROGNAME=f"firmware_v{safe_ver}")

env_vars = {}
for candidate in (
    os.path.join(project_dir, ".env"),
    os.path.join(project_dir, "include", ".env"),
):
    env_vars.update(parse_env_file(candidate))

string_keys = {
    "WIFI_SSID": "DEFAULT_WIFI_SSID",
    "WIFI_PASS": "DEFAULT_WIFI_PASS",
    "TIMEZONE": "DEFAULT_TIMEZONE",
    "ADSB_LOL_API_KEY": "ADSB_LOL_API_KEY",
    "OTA_PASSWORD": "OTA_PASSWORD",
}

for src_key, macro_name in string_keys.items():
    if env_vars.get(src_key):
        env.Append(CPPDEFINES=[(macro_name, env.StringifyMacro(env_vars[src_key]))])

for src_key, macro_name in (
    ("LATITUDE", "DEFAULT_LATITUDE"),
    ("LONGITUDE", "DEFAULT_LONGITUDE"),
):
    if env_vars.get(src_key):
        try:
            float(env_vars[src_key])
            env.Append(CPPDEFINES=[(macro_name, env_vars[src_key])])
        except ValueError:
            pass

if env.GetProjectOption("upload_protocol", "") == "espota":
    if env_vars.get("OTA_HOST"):
        env.Replace(UPLOAD_PORT=env_vars["OTA_HOST"])
    if env_vars.get("OTA_PASSWORD"):
        env.Append(UPLOAD_FLAGS=["--auth=" + env_vars["OTA_PASSWORD"]])
