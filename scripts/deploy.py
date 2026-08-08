#!/usr/bin/env python3
import json
import os
import re
import shutil
import sys
import subprocess
import struct

def find_devkitppc_tool(name):
    """Resolve a devkitPPC binary without hardcoding the install location.

    Order: PATH, then $DEVKITPRO_WIN (Windows-path override used by the .bat
    scripts too), then the platform-default install roots. The DEVKITPRO env
    var itself is usually the msys2-style /opt/devkitpro, which only resolves
    inside devkitPro's msys2 - it is still tried last in case this script runs
    in such a shell.
    """
    found = shutil.which(name)
    if found:
        return found

    exe = name + (".exe" if os.name == "nt" else "")
    roots = [os.environ.get("DEVKITPRO_WIN"),
             "C:\\devkitPro" if os.name == "nt" else None,
             "/opt/devkitpro",
             os.environ.get("DEVKITPRO")]
    for root in roots:
        if not root:
            continue
        candidate = os.path.join(root, "devkitPPC", "bin", exe)
        if os.path.exists(candidate):
            return candidate

    raise RuntimeError(
        f"Cannot find {name}. Install devkitPPC (devkitPro) or set "
        f"DEVKITPRO_WIN to your devkitPro install directory.")


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
    
    # Placeholders are only ever written where no deployed file exists yet -
    # a previously deployed real binary must never be clobbered by running
    # deploy.py after building just one of the platforms.
    def deploy_or_placeholder(build_path, deploy_path, placeholder_text, label):
        if os.path.exists(build_path):
            shutil.copy2(build_path, deploy_path)
            print(f"[{label}] Copied {os.path.basename(deploy_path)} -> {os.path.dirname(deploy_path)}")
        elif not os.path.exists(deploy_path):
            with open(deploy_path, "w") as f:
                f.write(placeholder_text)
            print(f"[{label}] Created target placeholder -> {deploy_path}")
        else:
            print(f"[{label}] No fresh build for {os.path.basename(deploy_path)}; keeping existing deployed file")

    deploy_or_placeholder(build_switch_subsdk, os.path.join(switch_deploy_dir, subsdk_name),
                          "# WiiXLaunch Switch SubSDK Binary\n", "Switch")
    deploy_or_placeholder(build_switch_npdm, os.path.join(switch_deploy_dir, "main.npdm"),
                          "# WiiXLaunch Switch NPDM\n", "Switch")

    build_wiiu_wps = os.path.join(root_dir, "build", "wiiu", plugin_name)

    deploy_or_placeholder(build_wiiu_wps, os.path.join(wiiu_deploy_dir, plugin_name),
                          "# WiiXLaunch Wii U Aroma WPS Plugin\n", "Wii U")

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
        objcopy_cmd = find_devkitppc_tool("powerpc-eabi-objcopy")
        readelf_cmd = find_devkitppc_tool("powerpc-eabi-readelf")
            
        subprocess.run([objcopy_cmd, "-O", "binary", "--set-section-flags", ".bss=alloc,load,contents", elf_path, bin_path], check=True)
        with open(bin_path, "rb") as f:
            payload_data = f.read()

        if len(payload_data) % 4 != 0:
            payload_data += b'\x00' * (4 - (len(payload_data) % 4))

        # WiiXLaunch_Cemu_Init hand-relocates its own g_CodeCaveBase computation
        # in place (see the @h/@l immediates in src/main.cpp and the matching
        # comment in scripts/cemu.ld) - those absolute refs must NOT also be
        # "fixed" below, or the delta would get applied twice and zero out
        # g_CodeCaveBase. cemu.ld brackets that exact instruction range with
        # these two symbols so we can exclude it purely by address, without
        # guessing at offsets.
        syms_out = subprocess.check_output([readelf_cmd, "-s", "-W", elf_path], text=True)
        bootstrap_start, bootstrap_end = None, None
        for line in syms_out.splitlines():
            parts = line.split()
            if len(parts) < 8:
                continue
            name = parts[7]
            if name == "__wiixl_bootstrap_start":
                bootstrap_start = int(parts[1], 16)
            elif name == "__wiixl_bootstrap_end":
                bootstrap_end = int(parts[1], 16)
        if bootstrap_start is None or bootstrap_end is None:
            raise RuntimeError("__wiixl_bootstrap_start/end symbols not found - check scripts/cemu.ld")

        # Full 32-bit absolute words: reading the already-linked (base-0) value
        # and adding the runtime delta directly is exact, no extra info needed.
        readelf_out = subprocess.check_output([readelf_cmd, "-r", elf_path], text=True)
        reloc_offsets = []
        # Split 16-bit absolute-address halves (from `lis`/`addis`+`ori`/`addi`
        # pairs materializing an absolute address in code, e.g. libm's rodata
        # constant loads - see the sqrtf crash this was added to fix). Unlike
        # ADDR32, these can't be fixed by adding delta to the bits already
        # baked into the instruction (that's only half the real address, and
        # HA additionally bakes in a sign-extension rounding adjustment) - so
        # each entry instead carries the relocation's own fully-resolved
        # S+Addend, and the target half is recomputed from scratch at deploy
        # time against (S+Addend+delta).
        lo_entries, ha_entries, hi_entries = [], [], []
        # Only relocations for sections that actually end up in the flat
        # binary may be turned into runtime fixups. Debug sections (.rela.
        # debug_info etc., present whenever the payload is compiled with -g)
        # also carry R_PPC_ADDR32 relocs, but their offsets are relative to
        # the debug sections - applying them would corrupt arbitrary words of
        # the payload at those offsets. (This happened: the resulting garbage
        # jump crashed Cemu's recompiler at boot.)
        current_section = ""
        for line in readelf_out.splitlines():
            parts = line.strip().split()
            if line.startswith("Relocation section"):
                m = re.search(r"'([^']+)'", line)
                current_section = m.group(1) if m else ""
                continue
            if ".debug" in current_section:
                continue
            if "R_PPC_ADDR32" in line or "R_PPC_RELATIVE" in line:
                if len(parts) >= 1:
                    reloc_offsets.append(int(parts[0], 16))
                continue
            for rtype, bucket in (("R_PPC_ADDR16_LO", lo_entries),
                                   ("R_PPC_ADDR16_HA", ha_entries),
                                   ("R_PPC_ADDR16_HI", hi_entries)):
                if rtype in line and len(parts) >= 5:
                    offset = int(parts[0], 16)
                    if bootstrap_start <= offset < bootstrap_end:
                        break
                    sym_value = int(parts[3], 16)
                    # "Sym.Name + Addend" (or "- Addend") is everything from
                    # parts[4] onward; addend is always the last token.
                    addend_tok = parts[-1]
                    sign = -1 if (len(parts) >= 6 and parts[-2] == "-") else 1
                    addend = sign * int(addend_tok, 16)
                    s_plus_a = (sym_value + addend) & 0xFFFFFFFF
                    bucket.append((offset, s_plus_a))
                    break

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

        # Split 16-bit absolute halves (ADDR16_LO/HA/HI): each entry is
        # {offset, S+Addend} (8 bytes) rather than just an offset, since the
        # target half can't be recovered by adding delta to what's already
        # baked into the instruction - see the comment above where these are
        # collected. Each table reuses the same bl-then-mflr trick as the
        # ADDR32 table above to find its own runtime address, so this is
        # exactly the proven mechanism repeated, not new addressing logic.
        for name, entries, needs_rounding in (
            ("lo", lo_entries, False),
            ("ha", ha_entries, True),
            ("hi", hi_entries, False),
        ):
            if not entries:
                continue

            cemu_asm_content += f"  bl wiixlaunch_after_{name}_table\n"
            cemu_asm_content += f"wiixlaunch_{name}_table:\n"
            for offset, value in entries:
                cemu_asm_content += f"  .int 0x{offset:08X}\n"
                cemu_asm_content += f"  .int 0x{value:08X}\n"
            cemu_asm_content += f"wiixlaunch_after_{name}_table:\n"
            cemu_asm_content += "  mflr r9\n"

            count = len(entries)
            cemu_asm_content += f"  lis r12, 0x{count >> 16:04X}\n"
            cemu_asm_content += f"  ori r12, r12, 0x{count & 0xFFFF:04X}\n"
            cemu_asm_content += "  mtctr r12\n"

            cemu_asm_content += f"wiixlaunch_{name}_loop:\n"
            cemu_asm_content += "  lwz r10, 0(r9)\n"   # offset
            cemu_asm_content += "  lwz r11, 4(r9)\n"   # S+Addend
            cemu_asm_content += "  add r11, r11, r8\n" # target = S+Addend+delta
            if needs_rounding:
                # HA = (target + 0x8000) >> 16 (rounds so the sign-extending
                # ADDI/LFS/etc. paired with it reconstructs the low half correctly)
                cemu_asm_content += "  lis r12, 0x0000\n"
                cemu_asm_content += "  ori r12, r12, 0x8000\n"
                cemu_asm_content += "  add r11, r11, r12\n"
                cemu_asm_content += "  rlwinm r11, r11, 16, 16, 31\n"
            else:
                # LO = target & 0xFFFF (sthx below truncates for us);
                # HI = target >> 16 (no rounding, paired with ORI not ADDI)
                if name == "hi":
                    cemu_asm_content += "  rlwinm r11, r11, 16, 16, 31\n"
            cemu_asm_content += "  sthx r11, r8, r10\n"
            cemu_asm_content += "  addi r9, r9, 8\n"
            cemu_asm_content += f"  bdnz wiixlaunch_{name}_loop\n"

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
    if not os.path.exists(elf_path) and os.path.exists(cemu_asm_path):
        # Same rule as deploy_or_placeholder: without a fresh build, never
        # replace a previously deployed patch (which contains the compiled
        # payload) with a payload-less shell.
        print(f"[Cemu] No fresh build ({elf_path} missing); keeping existing patch file")
    else:
        with open(cemu_asm_path, "w", encoding="utf-8") as f:
            f.write(cemu_asm_content)
        print(f"[Cemu] Generated Graphic Pack files -> {cemu_deploy_dir}")

    print("\nDeployment structures ready in:")
    print(f" - Switch (Console / Ryujinx / Yuzu): {switch_deploy_dir}")
    print(f" - Wii U  (Real Console / Aroma):    {wiiu_deploy_dir}")
    print(f" - Cemu   (PC Emulator Graphic Pack): {cemu_deploy_dir}\n")

if __name__ == "__main__":
    main()
