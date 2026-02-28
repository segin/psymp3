import subprocess

with open('remaining_branches.txt') as f:
    branches = [x.strip() for x in f.readlines() if x.strip() and x.strip() != '1-CURRENT']

for i, b in enumerate(branches):
    print(f"Attempting to merge {b}...")
    subprocess.run(['git', 'merge', '--abort'], capture_output=True)
    res = subprocess.run(['git', 'merge', '--no-edit', f'origin/{b}'])
    if res.returncode == 0:
        subprocess.run(['git', 'push'])
        subprocess.run(['git', 'push', 'origin', '--delete', b])
        print(f"Clean merge! Deleting {b} from list.")
        with open('remaining_branches.txt', 'w') as f:
            f.write('\n'.join(['1-CURRENT'] + branches[i+1:]) + '\n')
    else:
        print(f"Conflict! Stopped at {b}.")
        with open('remaining_branches.txt', 'w') as f:
            f.write('\n'.join(['1-CURRENT'] + branches[i:]) + '\n')
        break
