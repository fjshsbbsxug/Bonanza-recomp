"""Manual hints for tools/disasm.py.

Add addresses here as code is understood better; rerun the disassembler
afterwards.
"""

# Extra code entry points the tracer cannot find on its own.
ENTRY_POINTS = [
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
