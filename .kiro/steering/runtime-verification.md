# Runtime Verification Requirements

## CRITICAL RULE: Never Assume Success Without User Confirmation

**DO NOT ASSUME THE PROGRAM IS WORKING JUST BECAUSE LOGS DON'T SHOW ERRORS.**

## The Problem

Silent failures and behavioral issues often don't manifest as errors in logs or debug output. A program can:
- Compile cleanly
- Run without crashes
- Produce no error messages
- Still be completely broken from a user perspective

## Mandatory Verification Pattern

After implementing fixes or changes, you MUST:

1. **Ask the user to test** - Don't assume it works
2. **Request specific feedback** - Ask about actual behavior, not just "does it work?"
3. **Verify user experience** - Logs don't capture everything

### Example Verification Questions

```
✅ CORRECT - Soliciting actual feedback:
"Can you test the FLAC playback now? Does the audio play correctly, or do you hear any issues like stuttering, silence, or incorrect playback speed?"

"Please try playing the file and let me know:
- Does it start playing?
- Does the audio sound correct?
- Does it play all the way through?
- Are there any freezes or hangs?"

❌ INCORRECT - Assuming success:
"The logs look clean now, so the issue should be fixed."
"I don't see any errors in the output, so playback should work."
"The code compiles without warnings, so it should be working."
```

## Types of Silent Failures

### Audio Issues
- Playback produces silence (no error, just no sound)
- Audio plays at wrong speed (no error, just wrong behavior)
- Stuttering or glitches (no error, just poor quality)
- Premature stopping (no error, just incomplete playback)

### UI Issues
- Controls don't respond (no error, just no action)
- Display shows wrong information (no error, just incorrect data)
- Freezes or hangs (no error, just unresponsive)

### Data Processing Issues
- Incorrect calculations (no error, just wrong results)
- Missing data (no error, just incomplete output)
- Corrupted output (no error, just garbled data)

## When to Verify

### Always Verify After:
- Implementing a fix for a reported bug
- Making changes to audio processing code
- Modifying codec or demuxer logic
- Changing threading or synchronization
- Updating I/O handling
- Refactoring core functionality

### Verification Checklist
- [ ] Ask user to test the specific functionality
- [ ] Request feedback on actual behavior
- [ ] Inquire about edge cases or corner scenarios
- [ ] Confirm the original issue is resolved
- [ ] Check for any new issues introduced

## Communication Pattern

### After Making Changes:
1. **Summarize what was changed** - Brief technical summary
2. **Request testing** - Ask user to verify behavior
3. **Provide test guidance** - Suggest what to look for
4. **Wait for feedback** - Don't proceed without confirmation

### Example:
```
I've updated the FLAC frame parsing to handle variable block sizes correctly. 
The code compiles cleanly and the logs show proper frame detection.

However, I need you to test the actual playback to confirm it's working:
- Does the FLAC file play without issues?
- Is the audio quality correct?
- Does it play all the way through?

Please let me know what you observe when you try playing the file.
```

## Why This Matters

1. **Logs are incomplete** - They don't capture user experience
2. **Silent failures exist** - Many bugs don't produce error messages
3. **Behavior matters** - Correct code doesn't guarantee correct behavior
4. **User feedback is essential** - Only the user knows if it actually works

## Enforcement

This is a mandatory requirement for all development work:

- ✅ Always ask for user verification after fixes
- ✅ Request specific behavioral feedback
- ✅ Wait for confirmation before assuming success
- ❌ Never assume success based solely on logs
- ❌ Never proceed without user feedback
- ❌ Never trust "clean logs" as proof of correctness

## Summary

**Clean logs ≠ Working software**

Always verify runtime behavior with the user. Logs show what the code reports, not what the user experiences. Silent failures are real, common, and invisible to logging systems.

When in doubt, ask the user to test and provide feedback on actual behavior.
