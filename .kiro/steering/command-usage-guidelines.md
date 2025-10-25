# Command Usage Guidelines

## Head Command Usage

When using the `head` command to limit output, you are **forbidden** from using sizes as small as `10`. 

**Minimum Requirements:**
- Use `head -50` or larger for meaningful output analysis
- For debug output analysis, prefer `head -100` or more
- For log analysis, use `head -200` or more

**Rationale:**
Small head sizes like `head -10` provide insufficient context for debugging and analysis, leading to incomplete understanding of issues and requiring multiple iterations to gather adequate information.

**Examples:**
```bash
# FORBIDDEN
timeout 10s ./src/psymp3 --debug flac_codec "file.flac" 2>&1 | head -10

# CORRECT
timeout 10s ./src/psymp3 --debug flac_codec "file.flac" 2>&1 | head -50
```