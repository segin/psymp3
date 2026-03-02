import subprocess
import os

with open('conflicted_branches.txt') as f:
    branches = [x.strip() for x in f.readlines() if x.strip() and x.strip() != '1-CURRENT']

print(f"Found {len(branches)} branches to process.")

for i, b in enumerate(branches):
    print(f"[{i+1}/{len(branches)}] Processing {b}...")
    
    res = subprocess.run(['git', 'rev-parse', '--verify', f'origin/{b}'], capture_output=True)
    if res.returncode != 0:
        continue
        
    subprocess.run(['git', 'merge', '--abort'], capture_output=True)
    res = subprocess.run(['git', 'merge', '--no-edit', f'origin/{b}'], capture_output=True, text=True)
    
    if res.returncode == 0:
        subprocess.run(['git', 'push'], capture_output=True)    
        subprocess.run(['git', 'push', 'origin', '--delete', b], capture_output=True)
        print(f"  -> Merged cleanly!")
        continue

    status = subprocess.run(['git', 'status', '--porcelain'], capture_output=True, text=True).stdout
    unmerged = [line[3:] for line in status.splitlines() if line[:2] in ('UU', 'DU', 'UD', 'AA', 'AU', 'UA')]
    
    allowed = {'.github/workflows/claude-code-review.yml', 'src/mpris/MethodHandler.cpp'}
    if not set(unmerged).issubset(allowed):
        print(f"  -> Skipping, has other conflicts: {set(unmerged) - allowed}")
        subprocess.run(['git', 'merge', '--abort'], capture_output=True)
        continue

    if '.github/workflows/claude-code-review.yml' in unmerged:
        subprocess.run(['git', 'rm', '.github/workflows/claude-code-review.yml'], capture_output=True)
        
    if 'src/mpris/MethodHandler.cpp' in unmerged:
        with open('src/mpris/MethodHandler.cpp', 'r') as f:
            content = f.read()
            
        modified = False
        
        # We look for the common patterns.
        import re
        
        # Marker 1: DBusMessageIter args
        pattern1 = r'<<<<<<< HEAD\n\s*DBusMessageIter args, variant_iter;\n=======\n\s*DBusMessageIter args;\n\s*DBusMessageIter variant_iter;\n>>>>>>> origin/[^\n]+'
        if re.search(pattern1, content):
            content = re.sub(pattern1, '  DBusMessageIter args, variant_iter;', content)
            modified = True
            
        # Marker 2: catch blocks }
        pattern2 = r'<<<<<<< HEAD\n\s*\}\n=======\n\s*\}\n>>>>>>> origin/[^\n]+'
        if re.search(pattern2, content):
            # Just keep the HEAD one (one brace)
            # Find exact indentation of HEAD brace if possible, but let's just replace with the space sequence
            match = re.search(r'<<<<<<< HEAD\n(\s*\})\n', content)
            if match:
                content = re.sub(pattern2, match.group(1), content)
                modified = True
        
        if "<<<<<<< HEAD" in content:
            print("  -> Unrecognized marker remained, aborting.")
            subprocess.run(['git', 'merge', '--abort'], capture_output=True)
            subprocess.run(['git', 'checkout', 'src/mpris/MethodHandler.cpp'], capture_output=True)
            continue
            
        with open('src/mpris/MethodHandler.cpp', 'w') as f:
            f.write(content)
        
        subprocess.run(['git', 'add', 'src/mpris/MethodHandler.cpp'], capture_output=True)

    res_commit = subprocess.run(['git', 'commit', '--no-edit'], capture_output=True)
    if res_commit.returncode == 0:
        res_push = subprocess.run(['git', 'push'], capture_output=True)
        if res_push.returncode == 0:
            subprocess.run(['git', 'push', 'origin', '--delete', b], capture_output=True)
            print("  -> Auto-resolved MethodHandler/YML conflict!")
        else:
            print("  -> Auto-resolve failed to push.")
            subprocess.run(['git', 'reset', '--hard', 'origin/master'], capture_output=True)
    else:
        print("  -> Commit failed.")
        subprocess.run(['git', 'merge', '--abort'], capture_output=True)

print("Done processing advanced auto merges.")
