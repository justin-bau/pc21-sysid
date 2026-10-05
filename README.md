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

## Co-Computer configuration

The project used a Raspberry Pi 3B+ running Ubuntu Server 22.04 LTS and ROS2 Humble Hawksbill
distribution.

## Building and deploying the ROS 2 workspace

### Workspace layout

This repository is the `src/` folder of a ROS 2 workspace. To set it up:

```bash
mkdir px4_ros_ws
git clone <repository url> px4_ros_ws/src
```

```text
px4_ros_ws/
├── src/                        # this repository
│   ├── pc21_sysid/             # the maneuver framework (node, maneuvers, registry)
│   ├── pc21_sysid_interfaces/  # custom ROS 2 interface definitions used by pc21_sysid
│   ├── px4_msgs/               # PX4 message definitions as a ROS 2 package (see below)
│   ├── deploy/systemd/         # systemd services for the Pi
│   └── docs/
├── build/                      # created by colcon, not versioned
├── install/                    # created by colcon, not versioned
└── log/                        # created by colcon, not versioned
```

### Building

All commands are run from the workspace root, `px4_ros_ws/`.

```bash
source /opt/ros/humble/setup.bash
rosdep update
rosdep install --from-paths src --ignore-src -y
colcon build --packages-up-to pc21_sysid --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

To run a node from the workspace:

```bash
source install/setup.bash
ros2 pkg executables pc21_sysid
ros2 run pc21_sysid <executable>
```

`install/setup.bash` adds the workspace on top of the ROS installation, so that `ros2` finds our
packages. `ros2 pkg executables` lists what the package provides.

### Deploying to the Pi

Code is compiled on the Pi itself and the development computer only sends the sources (maybe
cross-compiling could be possible?). The first build takes long because of `px4_msgs`. Avoid
recompiling that module when unchanged.

Assumes ROS 2 Humble on the Pi (see *Co-Computer configuration*), the workspace at `~/px4_ros_ws` on
the Pi, and an SSH host alias `pi` on the development computer:

**First build.** Copy the sources with the `rsync` command below, then log into the Pi and run the
commands of the *Building* section up there.

**Every update.** From the workspace root on the development computer:

```bash
rsync -az --exclude .git --filter=':- .gitignore' src/ pi:~/px4_ros_ws/src/
ssh pi 'source /opt/ros/humble/setup.bash && cd ~/px4_ros_ws && colcon build --packages-select pc21_sysid_interfaces pc21_sysid'
ssh -t pi sudo systemctl restart maneuver-runner
```

### Services on the Pi

Two systemd services, in `deploy/systemd/`, start everything at boot:

- `microxrce-agent.service` runs the uXRCE-DDS agent on the serial link to the Pixhawk
  (`/dev/ttyAMA0`, 921600 baud). The agent is built separately, in `~/px4_ros_uxrce_dds_ws`.
- `maneuver-runner.service` sources ROS 2 and `~/px4_ros_ws/install/setup.bash`, then runs
  `ros2 run pc21_sysid maneuver_runner`. It starts after the agent and restarts automatically if the
  node exits.

To install them on a fresh Pi, from the repository root:

```bash
sudo cp deploy/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now microxrce-agent maneuver-runner
```

Useful commands on the Pi:

```bash
systemctl status maneuver-runner
journalctl -u maneuver-runner -f
sudo systemctl restart maneuver-runner
sudo systemctl stop maneuver-runner
```

`journalctl -f` follows the node's output live. Stopping the service is needed before starting the
node by hand with `ros2 run`, otherwise two instances run at once.
