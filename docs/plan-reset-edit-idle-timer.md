<!-- 2ce4f129-94b5-4c7a-a14c-da3e7e0042ff -->
---
todos:
  - id: "reset-on-rotate"
    content: "Reset edit idle timestamp in applyEncoderEdit when steps != 0"
    status: completed
isProject: false
---
# Reset edit idle timer on encoder rotation

## Status: done

In [`src/main.cpp`](../src/main.cpp), edit idle uses `gLastActivityMs`. Both button presses and encoder rotation refresh it:

- Button path sets `gLastActivityMs = millis()` when entering/cycling edit.
- `applyEncoderEdit()` sets `gLastActivityMs = millis()` when `steps != 0`.
- Settings encoder path does the same via `applySettingsEncoder()`.

`updateTimeouts()` exits EditMax/EditMin → Run only after **5 s with no button and no rotation**.

## Out of scope (unchanged)

No other control/UI changes required for this item.
