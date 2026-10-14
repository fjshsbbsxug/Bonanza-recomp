"""Manual hints for tools/disasm.py.

Add addresses here as code is understood better; rerun the disassembler
afterwards.
"""

# Extra code entry points the tracer cannot find on its own.
ENTRY_POINTS = [
    # sub_24536 (coffee breaks): tile loaders picked from tbl_246A6 by the stage number
    # (lea tbl(pc),a0 / movea.l (a0,d0.w),a0 / jsr (a0)); its first entry is 0
    0x246B6, 0x24720, 0x2478A,
]

# Extra jump tables: (address, kind) where kind is
#   "long"  - table of 32-bit code pointers
#   "rel16" - table of 16-bit offsets relative to the table start
TABLES = [
]

# Addresses that must never be treated as code.
DATA_START = set()

# (start, end) ranges that must never be treated as code.
FORCED_DATA = [
]
