# --- WiiXLaunch coreinit time import shims (Cemu code-cave only) ---
#
# Same mechanism as cemu_logging.asm / gx2_imports.asm: the payload is a raw
# codecave blob, never a real RPL module, so nothing it calls goes through OS
# import resolution. Cemu's own patch assembler DOES resolve
# `import.<lib>.<Name>`, so each entry below is a one-instruction tail call
# that Cemu points at the real coreinit export.
#
# scripts/deploy.py includes this file into the same codecave as the compiled
# payload and patches g_CemuTimeShimTableOffset (see
# include/wiixlaunch/time.hpp) with THIS table's own offset - the line below
# tells deploy.py which C++ global to patch. Keep
# wiixlaunch_cemu_time_shim_table in EXACTLY the same order as the
# CemuTimeImport enum in that header.
#
# WIIXL_OFFSET_SYMBOL: g_CemuTimeShimTableOffset

wiixlaunch_cemu_time_shim_table:
  .int wiixlaunch_cemu_time_shim_OSGetTime
  .int wiixlaunch_cemu_time_shim_OSTicksToCalendarTime

# Neither import is variadic, so unlike the OSReport shim these need no
# CR-bit-6 setup. A plain tail call passes the arguments through untouched:
# OSTicksToCalendarTime takes the 64-bit OSTime in r3:r4 (PowerPC EABI splits
# a 64-bit argument across an even/odd register pair) and the OSCalendarTime
# out-pointer in r5.
wiixlaunch_cemu_time_shim_OSGetTime:
  b import.coreinit.OSGetTime

wiixlaunch_cemu_time_shim_OSTicksToCalendarTime:
  b import.coreinit.OSTicksToCalendarTime
