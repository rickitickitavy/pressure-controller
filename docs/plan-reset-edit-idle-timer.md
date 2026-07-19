<!-- 2ce4f129-94b5-4c7a-a14c-da3e7e0042ff -->
---
todos:
  - id: "reset-on-rotate"
    content: "Reset edit idle timestamp in applyEncoderEdit when steps != 0"
    status: pending
isProject: false
---
# Reset edit idle timer on encoder rotation

## Change

In [`src/main.cpp`](src/main.cpp), when edit mode applies encoder steps, refresh the idle timestamp the same way a button press does.

Today only the button path resets the timer:

```75:75:src/main.cpp
  gLastButtonMs = millis();
```

Update `applyEncoderEdit()` so that when `steps != 0`, it also sets `gLastButtonMs = millis()` (or rename to `gLastEditActivityMs` for clarity). Then `updateEditTimeout()` still exits to Run only after **5 s with no button and no rotation**.

Also update the behavior note in the project plan: encoder rotation **does** reset the 5 s idle window.

## Out of scope

No other control/UI changes. Leave display debug rotation instrumentation alone unless you ask to clean it up separately.
