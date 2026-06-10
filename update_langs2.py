import os
import glob
import re

lang_dir = r'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\lang'

for f in glob.glob(os.path.join(lang_dir, '*', 'dlg.json')):
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    if 'IDS_STYLE_FONT_DECODER' not in content:
        if '\\cn\\' in f:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "text": "解码器注释字号"\n    }'
        elif '\\traditional\\' in f:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "text": "解碼器註釋字號"\n    }'
        else:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "text": "Decoder Font Size"\n    }'
        
        cursor_block = re.search(r'\{\s*"id"\s*:\s*"IDS_STYLE_FONT_CURSOR"\s*,\s*"text"\s*:\s*"[^"]+"\s*\}', content)
        if cursor_block:
            content = content[:cursor_block.end()] + new_entry + content[cursor_block.end():]
            with open(f, 'w', encoding='utf-8') as file:
                file.write(content)
            print(f"Updated: {f}")
        else:
            print(f"Failed to find cursor block in {f}")

print('Lang files updated')
