#!/usr/bin/env python3
import json
import os
import shutil
import sys
import subprocess
import struct

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
    wiiu_tids = ", ".join(wiiu_cfg.get("target_title_ids", ["00050000101C9400", "00050000101C9500"]))

    deploy_dir = os.path.join(root_dir, "deploy")
    switch_deploy_dir = os.path.join(deploy_dir, "switch", "atmosphere", "contents", switch_title_id, "exefs")
    wiiu_deploy_dir = os.path.join(deploy_dir, "wiiu", "wiiu", "environments", "aroma", "plugins")
    cemu_deploy_dir = os.path.join(deploy_dir, "cemu", "graphicPacks", f"WiiXLaunch_{project_name}")

    os.makedirs(switch_deploy_dir, exist_ok=True)
    os.makedirs(wiiu_deploy_dir, exist_ok=True)
    os.makedirs(cemu_deploy_dir, exist_ok=True)

    print("==========================================")
    print(f" WiiXLaunch Deployment Packager for [{project_name}]")
    print("==========================================")

    build_switch_subsdk = os.path.join(root_dir, "build", "switch", subsdk_name)
    build_switch_npdm = os.path.join(root_dir, "build", "switch", "main.npdm")
    
    if os.path.exists(build_switch_subsdk):
        shutil.copy2(build_switch_subsdk, os.path.join(switch_deploy_dir, subsdk_name))
        print(f"[Switch] Copied {subsdk_name} -> {switch_deploy_dir}")
    else:
        with open(os.path.join(switch_deploy_dir, subsdk_name), "w") as f:
            f.write("# WiiXLaunch Switch SubSDK Binary\n")
        print(f"[Switch] Created target placeholder -> {os.path.join(switch_deploy_dir, subsdk_name)}")

    if os.path.exists(build_switch_npdm):
        shutil.copy2(build_switch_npdm, os.path.join(switch_deploy_dir, "main.npdm"))
        print(f"[Switch] Copied main.npdm -> {switch_deploy_dir}")
    else:
        with open(os.path.join(switch_deploy_dir, "main.npdm"), "w") as f:
            f.write("# WiiXLaunch Switch NPDM\n")
        print(f"[Switch] Created target placeholder -> {os.path.join(switch_deploy_dir, 'main.npdm')}")

    build_wiiu_wps = os.path.join(root_dir, "build", "wiiu", plugin_name)
    
    if os.path.exists(build_wiiu_wps):
        shutil.copy2(build_wiiu_wps, os.path.join(wiiu_deploy_dir, plugin_name))
        print(f"[Wii U] Copied {plugin_name} -> {wiiu_deploy_dir}")
    else:
        with open(os.path.join(wiiu_deploy_dir, plugin_name), "w") as f:
            f.write("# WiiXLaunch Wii U Aroma WPS Plugin\n")
        print(f"[Wii U] Created target placeholder -> {os.path.join(wiiu_deploy_dir, plugin_name)}")

    cemu_cfg = config.get("cemu", {})
    gp_path = cemu_cfg.get("graphic_pack_path", f"{project_name}/Mods/WiiXLaunch")
    gp_version = cemu_cfg.get("graphic_pack_version", "V100")
    mod_matches = cemu_cfg.get("module_matches", "0x00000000")

    cemu_rules_content = f"""[Definition]
titleIds = {wiiu_tids}
name = "{project_name} (WiiXLaunch Cemu Pack)"
path = "{gp_path}"
version = 7
"""
    cemu_rules_path = os.path.join(cemu_deploy_dir, "rules.txt")
    with open(cemu_rules_path, "w", encoding="utf-8") as f:
        f.write(cemu_rules_content)

    cemu_asm_content = f"[{project_name}_{gp_version}]\nmoduleMatches = {mod_matches}\n\n"
    
    elf_path = os.path.join(root_dir, "build", "wiixlaunch_cemu")
    if os.path.exists(elf_path):
        bin_path = os.path.join(root_dir, "build", "payload.bin")
        objcopy_cmd = "powerpc-eabi-objcopy"
        readelf_cmd = "powerpc-eabi-readelf"
        if os.name == "nt":
            objcopy_cmd = "C:\\devkitPro\\devkitPPC\\bin\\powerpc-eabi-objcopy.exe"
            readelf_cmd = "C:\\devkitPro\\devkitPPC\\bin\\powerpc-eabi-readelf.exe"
            
        subprocess.run([objcopy_cmd, "-O", "binary", "--set-section-flags", ".bss=alloc,load,contents", elf_path, bin_path], check=True)
        with open(bin_path, "rb") as f:
            payload_data = f.read()

        if len(payload_data) % 4 != 0:
            payload_data += b'\x00' * (4 - (len(payload_data) % 4))
            
        readelf_out = subprocess.check_output([readelf_cmd, "-r", elf_path], text=True)
        reloc_offsets = []
        for line in readelf_out.splitlines():
            if "R_PPC_ADDR32" in line or "R_PPC_RELATIVE" in line:
                parts = line.strip().split()
                if len(parts) >= 1:
                    offset = int(parts[0], 16)
                    reloc_offsets.append(offset)
                    
        num_relocs = len(reloc_offsets)
        binary_size = len(payload_data)
        entry_hook = int(cemu_cfg.get("entry_hook", "0x00000000"), 16)
        
        cemu_asm_content += f"# --- WiiXLaunch Self-Relocating C++ Payload ---\n"
        cemu_asm_content += ".origin = codecave\n"
        cemu_asm_content += "wiixlaunch_codecave_start:\n"
        cemu_asm_content += "  b wiixlaunch_reloc_stub\n"
        cemu_asm_content += "wiixlaunch_reloc_flag:\n"
        cemu_asm_content += "  .int 0\n\n"
        
        cemu_asm_content += "wiixlaunch_binary:\n"
        for i in range(0, binary_size, 4):
            word = struct.unpack(">I", payload_data[i:i+4])[0]
            cemu_asm_content += f"  .int 0x{word:08X}\n"
            
        cemu_asm_content += "\nwiixlaunch_reloc_stub:\n"
        cemu_asm_content += "  stwu r1, -0x30(r1)\n"
        cemu_asm_content += "  stw r0, 0x24(r1)\n"
        cemu_asm_content += "  mflr r0\n"
        cemu_asm_content += "  stw r0, 0x34(r1)\n"
        
        for i, reg in enumerate([11, 12, 10, 9, 8]):
            cemu_asm_content += f"  stw r{reg}, 0x{16 + i*4:02X}(r1)\n"
            
        cemu_asm_content += "  bl wiixlaunch_after_table\n"
        
        cemu_asm_content += "wiixlaunch_reloc_table:\n"
        for offset in reloc_offsets:
            cemu_asm_content += f"  .int 0x{offset:08X}\n"
            
        cemu_asm_content += "wiixlaunch_after_table:\n"
        cemu_asm_content += "  mflr r9\n"
        
        binary_offset_from_table = binary_size + 40
        binary_offset_h = binary_offset_from_table >> 16
        binary_offset_l = binary_offset_from_table & 0xFFFF
        
        cemu_asm_content += f"  lis r12, 0x{binary_offset_h:04X}\n"
        cemu_asm_content += f"  ori r12, r12, 0x{binary_offset_l:04X}\n"
        cemu_asm_content += f"  subf r8, r12, r9\n"
        
        cemu_asm_content += "  lwz r10, -4(r8)\n"
        cemu_asm_content += "  cmpwi r10, 0\n"
        cemu_asm_content += "  bne wiixlaunch_reloc_done\n"
        cemu_asm_content += "  li r10, 1\n"
        cemu_asm_content += "  stw r10, -4(r8)\n"
        
        if num_relocs > 0:
            num_relocs_h = num_relocs >> 16
            num_relocs_l = num_relocs & 0xFFFF
            cemu_asm_content += f"  lis r12, 0x{num_relocs_h:04X}\n"
            cemu_asm_content += f"  ori r12, r12, 0x{num_relocs_l:04X}\n"
            cemu_asm_content += "  mtctr r12\n"
            
            cemu_asm_content += "wiixlaunch_reloc_loop:\n"
            cemu_asm_content += "  lwz r10, 0(r9)\n"
            cemu_asm_content += "  lwzx r11, r8, r10\n"
            cemu_asm_content += "  add r11, r11, r8\n"
            cemu_asm_content += "  stwx r11, r8, r10\n"
            cemu_asm_content += "  addi r9, r9, 4\n"
            cemu_asm_content += "  bdnz wiixlaunch_reloc_loop\n"
            
        cemu_asm_content += "wiixlaunch_reloc_done:\n"
        cemu_asm_content += "  lwz r0, 0x34(r1)\n"
        cemu_asm_content += "  mtlr r0\n"
        cemu_asm_content += "  lwz r0, 0x24(r1)\n"
        for i, reg in enumerate([11, 12, 10, 9, 8]):
            cemu_asm_content += f"  lwz r{reg}, 0x{16 + i*4:02X}(r1)\n"
        cemu_asm_content += "  addi r1, r1, 0x30\n"
        
        cemu_asm_content += "  b wiixlaunch_binary\n\n"
        
        if entry_hook != 0:
            cemu_asm_content += f"# Entry Hook: redirect 0x{entry_hook:08X} -> wiixlaunch_codecave_start\n"
            cemu_asm_content += f".origin = 0x{entry_hook:08X}\n"
            cemu_asm_content += f"  b wiixlaunch_codecave_start\n\n"

    cemu_src_dir = os.path.join(root_dir, "src", "cemu")
    if os.path.exists(cemu_src_dir):
        for root, _, files in os.walk(cemu_src_dir):
            for file in sorted(files):
                if file.endswith(".asm"):
                    asm_file_path = os.path.join(root, file)
                    with open(asm_file_path, "r", encoding="utf-8") as f:
                        cemu_asm_content += f"# --- Included from src/cemu/{file} ---\n"
                        cemu_asm_content += f.read() + "\n\n"

    cemu_asm_path = os.path.join(cemu_deploy_dir, f"patch_{project_name}.asm")
    with open(cemu_asm_path, "w", encoding="utf-8") as f:
        f.write(cemu_asm_content)
    print(f"[Cemu] Generated Graphic Pack files -> {cemu_deploy_dir}")

    print("\nDeployment structures ready in:")
    print(f" - Switch (Console / Ryujinx / Yuzu): {switch_deploy_dir}")
    print(f" - Wii U  (Real Console / Aroma):    {wiiu_deploy_dir}")
    print(f" - Cemu   (PC Emulator Graphic Pack): {cemu_deploy_dir}\n")

if __name__ == "__main__":
    main()
