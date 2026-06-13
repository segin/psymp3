import subprocess
import time

with open('conflicted_branches.txt') as f:
    branches = [x.strip() for x in f.readlines() if x.strip() and x.strip() != '1-CURRENT']

print(f"Found {len(branches)} branches to process.")

for i, b in enumerate(branches):
    print(f"[{i+1}/{len(branches)}] Processing {b}...")
    
    # Check if branch exists
    res = subprocess.run(['git', 'rev-parse', '--verify', f'origin/{b}'], capture_output=True)
    if res.returncode != 0:
        print(f"  -> Branch does not exist, skipping.")
        continue
        
    subprocess.run(['git', 'merge', '--abort'], capture_output=True)
    res = subprocess.run(['git', 'merge', '--no-edit', f'origin/{b}'], capture_output=True, text=True)
    
    if res.returncode == 0:
        subprocess.run(['git', 'push'], capture_output=True)
        subprocess.run(['git', 'push', 'origin', '--delete', b], capture_output=True)
        print(f"  -> Merged cleanly!")
        continue

    status = subprocess.run(['git', 'status', '--porcelain'], capture_output=True, text=True).stdout
    lines = status.splitlines()
    unmerged = [line[3:] for line in lines if line[:2] in ('UU', 'DU', 'UD', 'AA', 'AU', 'UA')]
    
    if len(unmerged) == 1 and unmerged[0] == '.github/workflows/claude-code-review.yml':
        subprocess.run(['git', 'rm', '.github/workflows/claude-code-review.yml'], capture_output=True)
        subprocess.run(['git', 'commit', '--no-edit'], capture_output=True)
        res_push = subprocess.run(['git', 'push'], capture_output=True)
        if res_push.returncode == 0:
            subprocess.run(['git', 'push', 'origin', '--delete', b], capture_output=True)
            print(f"  -> Auto-resolved yml conflict!")
        else:
            print(f"  -> Auto-resolve failed to push.")
            subprocess.run(['git', 'reset', '--hard', 'origin/master'], capture_output=True)
        continue
        
    print(f"  -> Needs manual resolution. Unmerged files: {unmerged}")
    subprocess.run(['git', 'merge', '--abort'], capture_output=True)

print("Done processing branches.")
