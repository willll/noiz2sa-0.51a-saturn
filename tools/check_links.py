import os
import re

files_to_check = [
    "README.md",
    "tools/README.md",
    "Tests/BULLETML_PARITY_TEST_README.md"
]

# Add doc/*.md
if os.path.exists("doc"):
    for f in os.listdir("doc"):
        if f.endswith(".md"):
            files_to_check.append(os.path.join("doc", f))

link_pattern = re.compile(r'\[.*?\]\((.*?)\)')

for filepath in files_to_check:
    if not os.path.exists(filepath):
        continue
    with open(filepath, 'r', encoding='utf-8') as f:
        for i, line in enumerate(f, 1):
            for link in link_pattern.findall(line):
                # Ignore http/https/mailto/anchors-only
                if link.startswith(('http://', 'https://', 'mailto:', '#')):
                    continue
                
                # Strip anchor
                target = link.split('#')[0]
                if not target:
                    continue
                
                # Resolve relative to file directory
                resolved = os.path.normpath(os.path.join(os.path.dirname(filepath), target))
                
                if not os.path.exists(resolved):
                    print(f"{filepath}:{i} -> {link} -> {resolved}")
