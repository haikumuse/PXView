import os

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        return

    original_content = content

    content = content.replace('DSVIEW', 'PXVIEW')
    content = content.replace('DSView', 'PXView')
    content = content.replace('dsview', 'pxview')

    # Revert the specific copyright header
    content = content.replace('PXView is based on PXView.', 'PXView is based on DSView.')

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Updated {filepath}")

for root, dirs, files in os.walk("."):
    # skip .git, build directories, etc
    if ".git" in root or "build" in root or "install" in root or ".gemini" in root:
        continue
        
    for file in files:
        if file.endswith((".md", ".log", ".pyc", ".exe", ".dll", ".zip", ".bin", ".fw", ".qm", ".qrc.depends", ".out")):
            continue
            
        filepath = os.path.join(root, file)
        
        # Don't modify the script itself
        if file in ["replace_dsview.py", "replace_log.py"]:
            continue
            
        process_file(filepath)
