# Capstone: Smart Temperature Monitor

A simulated temperature-sensor kernel driver + a C++ userspace app that
reads it and runs a state machine (NORMAL -> WARNING -> CRITICAL).

## 1. Prerequisites

Use a real Ubuntu machine or VM (VirtualBox/VMware) — NOT WSL2's default
kernel, which cannot load custom kernel modules.

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) g++ make

# sanity check: this directory must exist, matching your running kernel
ls /usr/src/linux-headers-$(uname -r)
```

## 2. Build the driver

```bash
cd capstone-temp-monitor/driver
make
```

This produces `tempsensor.ko`. If `make` errors about a missing `build`
directory, your kernel headers package didn't install correctly — re-check
step 1.

## 3. Load the driver

```bash
sudo insmod tempsensor.ko
dmesg | tail -5          # should show "tempsensor: module loaded ..."
ls -l /dev/tempsensor    # device node should exist
```

If `/dev/tempsensor` doesn't appear, check `dmesg` for the exact error —
most commonly a class/device creation failure logged there.

## 4. Quick manual test (before running the app)

```bash
sudo cat /dev/tempsensor    # prints one temperature reading, e.g. 25.3
sudo cat /dev/tempsensor    # run again - value will have drifted slightly
```

## 5. Build the C++ app

```bash
cd ../app
g++ -Wall -O2 -o temp_monitor temp_monitor.cpp
```

## 6. Run the monitor

```bash
sudo ./temp_monitor
```

You'll see one line per second, and a highlighted `TRANSITION:` line
whenever the state changes. Let it run for a minute or two to show the
random walk naturally crossing 60°C and 80°C over time.

Press `Ctrl+C` to stop.

## 7. Demonstrate the ioctl calls (for the design-review discussion)

Reset the simulated sensor back to baseline:

```bash
sudo ./temp_monitor --reset
```

Speed up the drift so state transitions happen fast (great for a live
demo — you don't want to wait 10 minutes for CRITICAL to show up):

```bash
sudo ./temp_monitor --drift 40
```

(default drift is `10` = up to 1.0°C per reading; `40` = up to 4.0°C per
reading, so it reaches WARNING/CRITICAL within seconds)

## 8. Unload the driver when done

```bash
sudo rmmod tempsensor
dmesg | tail -5     # should show "tempsensor: module unloaded"
```

## Suggested live-demo script for students

1. `sudo insmod tempsensor.ko` — show `dmesg` confirming load.
2. `sudo cat /dev/tempsensor` twice — show the raw driver output changing.
3. `sudo ./temp_monitor --drift 40` — let it run, point out the
   `TRANSITION:` lines as they scroll past.
4. Open `tempsensor.c` and `temp_monitor.cpp` side by side — walk through
   how `read()` in the app maps to `temp_read()` in the driver, and how
   the `ioctl()` call in the app maps to `temp_ioctl()` in the driver.
5. Ask students to sketch the sequence diagram (App -> Driver -> back)
   for one `read()` call, and the state machine diagram for the
   NORMAL/WARNING/CRITICAL states, before they start extending the code
   themselves.

## Ideas for teams to extend (self-study / remaining capstone work)

- Add a `LOW_BATTERY` state fed by a second simulated ioctl.
- Replace the simulated driver with a real I2C/GPIO sensor on a Raspberry Pi.
- Add a class diagram covering `TempReader`, `StateMachine`, `Logger` as
  separate C++ classes instead of one flat `main()`.
- Add unit tests for the `classify()` state-transition logic.
