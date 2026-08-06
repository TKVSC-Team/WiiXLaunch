#!/usr/bin/env python3
import json
import os
import shutil
import sys

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    config_path = os.path.join(root_dir, "wiixlaunch.json")

    if not os.path.exists(config_path):
        print(f"Error: Could not find config at {config_path}")
        sys.exit(1)

    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)

    project_name = config.get("project", {}).get("name", "Mod")
    switch_cfg = config.get("switch", {})
    wiiu_cfg = config.get("wiiu", {})

    switch_title_id = switch_cfg.get("title_id", "01007EF00011E000").upper()
    subsdk_name = switch_cfg.get("subsdk_name", "subsdk9")
    plugin_name = wiiu_cfg.get("plugin_name", f"{project_name}.wps")

    deploy_dir = os.path.join(root_dir, "deploy")
    switch_deploy_dir = os.path.join(deploy_dir, "switch", "atmosphere", "contents", switch_title_id, "exefs")
    wiiu_deploy_dir = os.path.join(deploy_dir, "wiiu", "wiiu", "environments", "aroma", "plugins")

    os.makedirs(switch_deploy_dir, exist_ok=True)
    os.makedirs(wiiu_deploy_dir, exist_ok=True)

    print("==========================================")
    print(f" WiiXLaunch Deployment Packager for [{project_name}]")
    print("==========================================")

    # Check for built Switch artifacts
    build_switch_subsdk = os.path.join(root_dir, "build", "switch", subsdk_name)
    build_switch_npdm = os.path.join(root_dir, "build", "switch", "main.npdm")
    
    if os.path.exists(build_switch_subsdk):
        shutil.copy2(build_switch_subsdk, os.path.join(switch_deploy_dir, subsdk_name))
        print(f"[Switch] Copied {subsdk_name} -> {switch_deploy_dir}")
    else:
        # Create placeholder file if not built yet
        with open(os.path.join(switch_deploy_dir, subsdk_name), "w") as f:
            f.write("# WiiXLaunch Switch SubSDK Binary\n")
        print(f"[Switch] Created target placeholder -> {os.path.join(switch_deploy_dir, subsdk_name)}")

    if os.path.exists(build_switch_npdm):
        shutil.copy2(build_switch_npdm, os.path.join(switch_deploy_dir, "main.npdm"))
        print(f"[Switch] Copied main.npdm -> {switch_deploy_dir}")
    else:
        # Create placeholder main.npdm
        with open(os.path.join(switch_deploy_dir, "main.npdm"), "w") as f:
            f.write("# WiiXLaunch Switch NPDM\n")
        print(f"[Switch] Created target placeholder -> {os.path.join(switch_deploy_dir, 'main.npdm')}")

    # Check for built Wii U artifacts
    build_wiiu_wps = os.path.join(root_dir, "build", "wiiu", plugin_name)
    
    if os.path.exists(build_wiiu_wps):
        shutil.copy2(build_wiiu_wps, os.path.join(wiiu_deploy_dir, plugin_name))
        print(f"[Wii U] Copied {plugin_name} -> {wiiu_deploy_dir}")
    else:
        # Create placeholder file if not built yet
        with open(os.path.join(wiiu_deploy_dir, plugin_name), "w") as f:
            f.write("# WiiXLaunch Wii U Aroma WPS Plugin\n")
        print(f"[Wii U] Created target placeholder -> {os.path.join(wiiu_deploy_dir, plugin_name)}")

    print("\nDeployment structure ready in:")
    print(f" - Switch: {switch_deploy_dir}")
    print(f" - Wii U:  {wiiu_deploy_dir}\n")

if __name__ == "__main__":
    main()
