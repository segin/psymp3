# Development Standards

## Code Organization

### Header Inclusion Policy
- **Master Header Rule**: All `.cpp` files should only include `psymp3.h` as the master header
- All other system and library includes must go in the master header (`psymp3.h`)
- This ensures consistent compilation environment and reduces header dependency issues

### Directory Structure
- **Source Code**: All implementation files (`.cpp`) go in `src/`
- **Resources**: Windows resource scripts, assets, fonts (like BitStream Vera Sans) go in `res/`
- **Tests**: All test files go in `tests/`
- **Headers**: Public headers go in `include/`

## Build System

### GNU Autotools
- Project uses GNU Autotools for build configuration
- **Rebuild toolchain**: Use `./generate-configure.sh` to regenerate build files
- `autogen.sh` exists as a symbolic link to `generate-configure.sh` for familiarity
- Always run `./generate-configure.sh` after modifying `configure.ac` or `Makefile.am` files

### Compilation Rules
- **NEVER invoke `gcc` or `g++` directly** when rebuilding changed files in the `src/` directory
- **Use Make**: Always use `make -C src File.o` to rebuild individual object files
- If already in the `src/` directory, use `make File.o` (omit the `-C src` portion)
- This ensures proper dependency tracking and build consistency

## Version Control

### Git Best Practices
- **Build Artifacts**: Always put build artifacts in `.gitignore`
- **CRITICAL**: NEVER USE `git add .` EVER! Always add files explicitly
- Use specific file paths when adding to git to avoid accidentally committing unwanted files

### Git Workflow for Tasks
- **Task Completion**: Always perform a `git commit` and `git push` when done with each task or subtask
- **Include Task Files**: Make sure to include the `tasks.md` file for whatever task list is being currently worked on
- This ensures progress is tracked and shared consistently

## Testing

### Current State
- **Known Issue**: No proper test harness currently exists
- **Priority**: Implementing a proper test framework should be addressed ASAP
- Individual test files exist in `tests/` but lack integration

### Test Organization
- Test files should follow naming convention: `test_<component>.cpp`
- Tests should be compilable and runnable independently where possible