"""Generate the read-only perspective reciprocal seeds offline."""
from pathlib import Path
values = [(1 << 63) // ((i + 257) << 23) for i in range(256)]
text = '// Offline: floor(2^63 / upper endpoint), normalized depth bins.\n'
text += ',\n'.join(','.join(str(v) + 'u' for v in values[i:i+8]) for i in range(0, 256, 8)) + '\n'
(Path(__file__).resolve().parents[1] / 'src/reciprocal_table.inc').write_text(text)
