"""Reproduce the previous quantized sin/cos tangent offline, including poles."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
text = (root/'src/fixed_tables.inc').read_text()
sine = list(map(int, re.search(r'sineTable\[360\]=\{(.*?)\};', text, re.S).group(1).replace('\n','').strip(',').split(',')))
values=[]
for i, value in enumerate(sine):
    denominator=sine[(i+90)%360]
    quotient=(abs(value)*65536//abs(denominator)) if denominator else 0
    if value*denominator<0: quotient=-quotient
    values.append(max(-2147483648,min(2147483647,quotient)))
output='// Offline quantized sin/cos, truncation toward zero; poles preserve zero.\nstatic const int tangentTable[360]={\n'
output+=',\n'.join(','.join(str(v) for v in values[i:i+12]) for i in range(0,360,12))+'\n};\n'
(root/'src/tan_table.inc').write_text(output)
