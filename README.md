# PC21 "Baron" sys_id

Modular framework to inject automated maneuvers on the baron's PX4 flight controller using a
companion computer.

## To document

- Description of the hardware / software stack (with versions (don't forget the cherrypicked
  commit))
- Instructions to obtain the software stack
- How it works, how to build it and run it
- How to modify or add maneuvers
- What to upgrade in the future: json, noise, multi-axis, parameterized control (not always aux5)
- I compiled directly on the raspberry which is slow. Document how to crosscompile it properly

## To add to the repo

- px4_msgs
- deployment config (systemd services, .sh, ...)

## PX4 changes

This needs a modified PX4 builds: defines new pti_params messages + adds a pwm_center for AUX
channels Also changed the list of publications and their rate in:
PX4-Autopilot/src/modules/uxrce_dds_client/dds_topics.yaml

### Clone from my fork (easier, but will break if fork becomes unavailable)

```bash
git clone https://github.com/justin-bau/PX4-Autopilot.git
cd PX4-Autopilot
git checkout dc2722f429          
git submodule update --init --recursive
```

### Patch the original source code

```bash
git clone --branch v1.17.0 https://github.com/PX4/PX4-Autopilot.git PX4-Autopilot
cd PX4-Autopilot
git am path/to/pti-sysid/docs/px4-patches/*.patch
git submodule update --init --recursive
```
