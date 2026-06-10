import os
import glob
themes_dir = r'c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\themes'
for f in glob.glob(os.path.join(themes_dir, '*.json')) + glob.glob(os.path.join(themes_dir, '*.qss')):
    with open(f, 'r', encoding='utf-8') as file:
        content = file.read()
    content = content.replace('"@trace-label-font-size": "10px"', '"@trace-label-font-size": "12px"')
    content = content.replace('"@ruler-font-size": "10px"', '"@ruler-font-size": "12px"')
    content = content.replace('@trace-label-font-size: 10px', '@trace-label-font-size: 12px')
    content = content.replace('@ruler-font-size: 10px', '@ruler-font-size: 12px')
    with open(f, 'w', encoding='utf-8') as file:
        file.write(content)
print('Theme files updated')
