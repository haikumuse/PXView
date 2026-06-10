import os
import glob

lang_dir = r'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\lang'

for f in glob.glob(os.path.join(lang_dir, '*', 'dlg.json')):
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    if 'IDS_STYLE_FONT_DECODER' not in content:
        # Check what language it is based on the path
        if '\\cn\\' in f:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "str": "解码器注释字号"\n    }'
        elif '\\traditional\\' in f:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "str": "解碼器註釋字號"\n    }'
        else:
            new_entry = ',\n    {\n        "id": "IDS_STYLE_FONT_DECODER",\n        "str": "Decoder Font Size"\n    }'
        
        # Insert before the last closing bracket of the json array
        # This is a bit fragile if there are multiple arrays, let's just do a string replace on the last '}' that closes an object before the array close
        # Or better, replace the 'IDS_STYLE_FONT_CURSOR' block
        import re
        cursor_block = re.search(r'{\s*"id":\s*"IDS_STYLE_FONT_CURSOR",\s*"str":\s*"[^"]+"\s*}', content)
        if cursor_block:
            content = content[:cursor_block.end()] + new_entry + content[cursor_block.end():]
            with open(f, 'w', encoding='utf-8') as file:
                file.write(content)

print('Lang files updated')
