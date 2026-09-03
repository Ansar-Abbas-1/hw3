## 📌 Homework Branches

The different parts of the homework are organized in separate Git branches.

**Question 1 (Q1) is available in the `main` branch.**

To test or verify another part of the homework, first switch to the corresponding branch before building or running the simulation. This is important because each branch may contain different PX4 models, airframe configurations, ROS 2 packages, or source-code modifications.

For example:


# Question 1
```bash
git switch main
```

# Question 2
```bash
git switch q2
```

# Question 3
```bash
git switch q3
```

# Homework 3 — Question 1: Custom x100 Quadrotor

This branch contains the files required to reproduce **Question 1 (Q1)** using PX4 SITL, Gazebo Harmonic, ROS 2 Humble, and QGroundControl.

Q1 is on the `main` branch. :contentReference[oaicite:0]{index=0}

---

## 1. Clone the Repository

On the **HOST**:

```bash
git clone --branch main --recurse-submodules https://github.com/Ansar-Abbas-1/hw3.git
cd hw3
```

The PX4 repository is included as a Git submodule and is pinned to:

```text
PX4 version: v1.16.0
PX4 commit: 6ea3539157ca358c70a515878b77077af7d4611d
```

If the repository was cloned without `--recurse-submodules`, this is also fine because the setup script initializes the required submodules automatically.

---

## 2. Apply the Q1 PX4 Modifications

Before building PX4, run the setup script on the **HOST**:

```bash
./setup_q1.sh
```

The script initializes the PX4 submodules and copies the Q1-specific files from:

```text
q1_custom/
├── x100/
├── x100_base/
├── 22000_gz_x100
├── airframes_CMakeLists.txt
└── dds_topics.yaml
```

into the required locations inside:

```text
src/PX4-Autopilot/
```

The script must be run after a fresh clone before launching the custom `x100` vehicle.

---

## 3. Start the Docker Container

On the **HOST**:

```bash
cd ros2_docker_scripts
./docker_run_container.sh
```

When the script asks which directory to mount, select the **repository root** (`hw3`), not the `src` directory.

The repository should therefore be available inside Docker as:

```text
/home/user/ros2_ws
```

and PX4 should be located at:

```text
/home/user/ros2_ws/src/PX4-Autopilot
```

---

## 4. Build the ROS 2 Workspace

Inside **Docker Terminal 1**:

```bash
cd /home/user/ros2_ws

source /opt/ros/humble/setup.bash

colcon build

source install/setup.bash
```

The ROS 2 workspace only needs to be rebuilt when its packages are changed.

---

## 5. Start PX4 SITL and Gazebo

Inside **Docker Terminal 1**:

```bash
cd /home/user/ros2_ws/src/PX4-Autopilot

make px4_sitl gz_x100
```

Keep this terminal running.

This launches the custom `x100` vehicle in Gazebo.

---

## 6. Start the DDS Agent

Open another **HOST terminal**:

```bash
cd hw3/ros2_docker_scripts
./docker_connect.sh
```

Inside **Docker Terminal 2**:

```bash
cd /home/user/ros2_ws
./DDS_run.sh
```

Keep this terminal running.

---

## 7. Start QGroundControl

Run QGroundControl on the **HOST**.

QGroundControl should connect automatically to PX4 SITL.

For manual testing:

1. Select **Position** mode.
2. Arm the vehicle.
3. Take off.
4. Fly the vehicle using roll, pitch, yaw, and altitude commands.
5. Land and disarm.

---

## 8. Check the ROS 2 Actuator Output Topic

Open another **HOST terminal**:

```bash
cd hw3/ros2_docker_scripts
./docker_connect.sh
```

Inside **Docker Terminal 3**:

```bash
cd /home/user/ros2_ws

source /opt/ros/humble/setup.bash
source install/setup.bash
```

Check that the actuator-output topic is available:

```bash
ros2 topic list | grep actuator
```

The Q1 topic should include:

```text
/fmu/out/actuator_outputs
```

Check its message type:

```bash
ros2 topic info /fmu/out/actuator_outputs
```

The message type should be:

```text
px4_msgs/msg/ActuatorOutputs
```

Display the actuator outputs:

```bash
ros2 topic echo /fmu/out/actuator_outputs
```

For the `x100`, the first four output channels correspond to the four motors:

```text
output[0] -> Motor 1 / rotor_0 / CCW
output[1] -> Motor 2 / rotor_1 / CCW
output[2] -> Motor 3 / rotor_2 / CW
output[3] -> Motor 4 / rotor_3 / CW
```

---

## 9. Plot the Motor Outputs with PlotJuggler

With PX4, Gazebo, and the DDS Agent running, connect another Docker terminal:

```bash
cd hw3/ros2_docker_scripts
./docker_connect.sh
```

Inside Docker:

```bash
cd /home/user/ros2_ws

source /opt/ros/humble/setup.bash
source install/setup.bash

ros2 run plotjuggler plotjuggler
```

In PlotJuggler:

1. Select **ROS2 Topic Subscriber**.
2. Subscribe to:

```text
/fmu/out/actuator_outputs
```

3. Plot:

```text
/fmu/out/actuator_outputs/output[0]
/fmu/out/actuator_outputs/output[1]
/fmu/out/actuator_outputs/output[2]
/fmu/out/actuator_outputs/output[3]
```

4. Start streaming.
5. Fly the vehicle from QGroundControl.

---

## Quick Reproduction

### HOST

```bash
git clone --branch main --recurse-submodules https://github.com/Ansar-Abbas-1/hw3.git
cd hw3

./setup_q1.sh

cd ros2_docker_scripts
./docker_run_container.sh
```

Select the `hw3` repository root as the workspace when requested.

### Docker Terminal 1 — Build ROS 2

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash
colcon build
source install/setup.bash
```

### Docker Terminal 1 — PX4 + Gazebo

```bash
cd /home/user/ros2_ws/src/PX4-Autopilot
make px4_sitl gz_x100
```

### HOST Terminal 2

```bash
cd hw3/ros2_docker_scripts
./docker_connect.sh
```

### Docker Terminal 2 — DDS Agent

```bash
cd /home/user/ros2_ws
./DDS_run.sh
```

### HOST

Start QGroundControl.

### HOST Terminal 3

```bash
cd hw3/ros2_docker_scripts
./docker_connect.sh
```

### Docker Terminal 3 — ROS 2

```bash
cd /home/user/ros2_ws

source /opt/ros/humble/setup.bash
source install/setup.bash

ros2 topic echo /fmu/out/actuator_outputs
```

### Optional — PlotJuggler

```bash
ros2 run plotjuggler plotjuggler
```

Subscribe to:

```text
/fmu/out/actuator_outputs
```

and plot:

```text
output[0]
output[1]
output[2]
output[3]
```

# 🛬 Homework 3 – Question 2: Force-Landing Trigger

This branch contains **Question 2 (Q2)** of Homework 3.

The `force_land` ROS 2 node sends a LAND command when the vehicle exceeds **20 m**. After the first trigger, LAND is not triggered again until PX4 reports that the vehicle has actually landed using `VehicleLandDetected`.

The tested PX4 version is:

```text
PX4-Autopilot: v1.16.0
Commit: 6ea3539157ca358c70a515878b77077af7d4611d
```

---

## 📁 Relevant Files

```text
src/force_land/
src/force_land/src/force_land_node.cpp
src/PX4-Autopilot/        # PX4 v1.16.0 Git submodule
DDS_run.sh
ros2_docker_scripts/
videos_plots/q2/
```

---

# 🔄 Reproduce Q2

## 1. Clone the Q2 Branch

PX4 is included as a Git submodule.

```bash
git clone --branch q2 --recurse-submodules https://github.com/Ansar-Abbas-1/hw3.git
cd hw3
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

Verify the PX4 revision:

```bash
cd src/PX4-Autopilot
git describe --tags --exact-match HEAD
git rev-parse HEAD
cd ../..
```

Expected:

```text
v1.16.0
6ea3539157ca358c70a515878b77077af7d4611d
```

---

## 2. Start Docker

### HOST

From the repository root:

```bash
cd ros2_docker_scripts
./docker_run_container.sh
```

When prompted for the workspace, select the **repository root**, not the `src/` directory.

The repository will be mounted inside Docker at:

```text
/home/user/ros2_ws
```

---

## 3. Build the ROS 2 Workspace

### DOCKER – Terminal 1

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash

colcon build

source install/setup.bash
```

Verify the Q2 node:

```bash
ros2 pkg executables force_land
```

---

## 4. Start PX4 SITL and Gazebo

### DOCKER – Terminal 1

```bash
cd /home/user/ros2_ws/src/PX4-Autopilot
make px4_sitl gz_x500
```

This starts PX4 SITL and the Gazebo Harmonic `x500` simulation.

Keep this terminal running.

---

## 5. Start the DDS Agent

Open another HOST terminal and connect to the running container.

### HOST

From the repository root:

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

### DOCKER – Terminal 2

```bash
cd /home/user/ros2_ws
./DDS_run.sh
```

Keep the DDS Agent running.

---

## 6. Run the `force_land` Node

Open another HOST terminal and connect to Docker:

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

### DOCKER – Terminal 3

```bash
cd /home/user/ros2_ws

source /opt/ros/humble/setup.bash
source install/setup.bash

ros2 run force_land force_land
```

The node uses:

```text
/fmu/out/vehicle_local_position
/fmu/out/vehicle_land_detected
/fmu/in/vehicle_command
```

---

## 7. Run QGroundControl and Verify Q2

Run **QGroundControl on the HOST** and connect to the PX4 SITL vehicle.

Verify the following sequence:

1. Take off and climb above **20 m** → LAND is triggered.
2. Interrupt the landing before touching the ground.
3. Climb above **20 m again** → LAND is **not** triggered again.
4. Land completely.
5. PX4 reports `landed: true`.
6. Take off again and climb above **20 m** → LAND can trigger again.

The landing state can be checked with:

```bash
ros2 topic echo /fmu/out/vehicle_land_detected
```

---

# 💾 Record the Q2 Experiment

Record the required ROS 2 topics:

```bash
ros2 bag record \
/fmu/out/vehicle_local_position \
/fmu/out/manual_control_setpoint \
/fmu/out/vehicle_land_detected
```

The recorded Q2 bag is stored under:

```text
videos_plots/q2/
```

---

# 📊 Reproduce the Plot

Start PlotJuggler inside Docker:

```bash
ros2 run plotjuggler plotjuggler
```

Load the recorded Q2 ROS 2 bag.

Plot the manual altitude-control input:

```text
/fmu/out/manual_control_setpoint/throttle
```

PX4 local position uses NED coordinates. For positive-up ENU altitude, use:

```text
/fmu/out/vehicle_local_position/z
```

and create a PlotJuggler Custom Series named:

```text
Altitude_ENU
```

with:

```lua
return -value
```

Plot:

```text
Altitude_ENU
/fmu/out/manual_control_setpoint/throttle
```

---

## Notes

- Q2 uses the standard PX4 `x500` model.
- `src/PX4-Autopilot` is a Git submodule pinned to PX4 `v1.16.0`.
- QGroundControl runs on the HOST.
- PX4, Gazebo, DDS, ROS 2, and PlotJuggler run inside Docker.
- Always select the repository root as the Docker workspace.
