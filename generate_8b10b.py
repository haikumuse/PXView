import json

with open('doc/private_protocols/8b10b_table.json', 'r') as f:
    data = json.load(f)

c_code = 'struct sym_8b10b {\n    uint32_t val1;\n    const char* name;\n    uint8_t val8;\n    uint32_t val3;\n};\n\nstatic const struct sym_8b10b table_8b10b[] = {\n'

for d in data:
    c_code += f'    {{ {d["val1"]}, "{d["name"]}", {d["val2"]}, {d["val3"]} }},\n'
c_code += '};\n'

with open('libsigrokdecode/c_decoders_private/8b10b_table.h', 'w') as f:
    f.write(c_code)
