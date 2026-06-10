import os
import glob

themes_dir = r'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\themes'

# For JSON files
for f in glob.glob(os.path.join(themes_dir, '*.json')):
    if f.endswith('theme-schema.json'):
        continue
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    if '"@decoder-font-size"' not in content:
        content = content.replace('"@ruler-font-size": "12px",', '"@ruler-font-size": "12px",\n    "@decoder-font-size": "12px",')
        with open(f, 'w', encoding='utf-8') as file:
            file.write(content)

# For QSS files
for f in glob.glob(os.path.join(themes_dir, '*.qss')):
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    if '@decoder-font-size' not in content:
        content = content.replace(' * @ruler-font-size: 12px', ' * @ruler-font-size: 12px\n * @decoder-font-size: 12px')
        with open(f, 'w', encoding='utf-8') as file:
            file.write(content)

print('Themes updated with @decoder-font-size')
