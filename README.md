## 📌 Homework Branches

The different parts of the homework are organized in separate Git branches.

**Question 1 (Q1) is available in the `main` branch.**

To test or verify another part of the homework, first switch to the corresponding branch before building or running the simulation. This is important because each branch may contain different PX4 models, airframe configurations, ROS 2 packages, or source-code modifications.

For example:

```bash
# Question 1
git switch main

# Question 2
git switch q2

# Question 3
git switch q3

# Question 1: x100 Custom Quadrotor

## About Q1

This main contains the implementation for **Question 1 (Q1)** of Homework 3.

For Q1, a custom quadrotor UAV named `x100` was developed and tested using **PX4 SITL**, **Gazebo Harmonic**, **ROS 2 Humble**, and **QGroundControl**.

The Q1 implementation includes:

- `x100_base` – custom base model containing the vehicle mass, inertia, geometry, rotor positions, sensors, and propellers.
- `x100` – Gazebo simulation model containing the motor plugins.
- `22000_gz_x100` – custom PX4 airframe configuration for the x100 vehicle.
- ROS 2 support for `ActuatorOutputs.msg` to monitor the four motor actuator outputs.
- Flight testing of the custom vehicle in **Position mode** using QGroundControl.
- Visualization of the four motor actuator outputs using **PlotJuggler**.

The custom `x100` vehicle can be launched directly in PX4 SITL with Gazebo Harmonic and flown in Position mode without manually configuring the vehicle parameters in QGroundControl.

> **Note:** The `main` branch contains the solution for **Question 1 (Q1)**. Other homework questions are maintained in their corresponding Git branches.

---

## 📝 Before You Start

The simulation is intended to run inside the provided Docker environment.

The required Docker scripts are provided in:

```text
ros2_docker_scripts/
```

The host workspace that is mounted into Docker must have a structure similar to:

```text
<workspace>/
├── DDS_run.sh
└── src/
    ├── PX4-Autopilot/
    ├── px4_msgs/
    ├── px4_ros_com/
    ├── force_land/
    ├── offboard_rl/
    └── read_rpy/
```

The name and location of `<workspace>` on the host machine are not important.

The important requirement is that its `src/` directory contains the required ROS 2 packages and the modified `PX4-Autopilot` source.

---

## 🐳 Start the Docker Environment

On the **HOST**, enter the provided Docker scripts directory:

```bash
cd ros2_docker_scripts
```

Start the container:

```bash
./docker_run_container.sh
```

The script will ask which host folder should be mounted into the Docker container.

You may either:

- press **Enter** to use the default folder proposed by the script, or
- enter the path of the workspace that contains the `src/` directory shown above.

For example, the selected host directory should conceptually look like:

```text
<selected-host-folder>/
├── DDS_run.sh
└── src/
    ├── PX4-Autopilot/
    ├── px4_msgs/
    ├── px4_ros_com/
    ├── force_land/
    ├── offboard_rl/
    └── read_rpy/
```

This host workspace is mounted inside the Docker container as:

```text
/home/user/ros2_ws
```

Therefore, after entering the container, the packages should be accessible as:

```text
/home/user/ros2_ws/
├── DDS_run.sh
└── src/
    ├── PX4-Autopilot/
    ├── px4_msgs/
    ├── px4_ros_com/
    ├── force_land/
    ├── offboard_rl/
    └── read_rpy/
```

All commands below use the Docker path `/home/user/ros2_ws`, so no personal host path is required.

---

## 🔨 Build the ROS 2 Workspace

If the ROS 2 workspace has not already been built, enter the Docker container and run:

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash
colcon build
```

After building, source the workspace:

```bash
source install/setup.bash
```

The workspace only needs to be rebuilt when the ROS 2 packages are changed.

---

## 🚁 Start PX4 and Gazebo

### Docker Terminal 1 – PX4 SITL + Gazebo

The first terminal is created by:

```bash
./docker_run_container.sh
```

Inside the Docker container, go to the PX4 source directory:

```bash
cd /home/user/ros2_ws/src/PX4-Autopilot
```

Launch the custom `x100` model:

```bash
make px4_sitl gz_x100
```

This starts:

- PX4 SITL
- Gazebo Harmonic
- the custom `x100` quadrotor model

Keep this terminal running.

---

## 🔗 Start the DDS Agent

Open a second **HOST terminal**.

Enter the Docker scripts directory:

```bash
cd ros2_docker_scripts
```

Connect to the already running container:

```bash
./docker_connect.sh
```

### Docker Terminal 2 – DDS Agent

Inside Docker:

```bash
cd /home/user/ros2_ws
./DDS_run.sh
```

Keep this terminal running.

`DDS_run.sh` starts the Micro XRCE-DDS Agent, which provides communication between PX4 and ROS 2.

---

## 🎮 Start QGroundControl

Run **QGroundControl on the HOST machine**.

QGroundControl should automatically connect to the running PX4 SITL vehicle.

To test the x100:

1. Select **Position** flight mode.
2. Arm the vehicle.
3. Take off to approximately 2–3 m.
4. Check that the vehicle maintains a stable hover.
5. Apply small roll, pitch, and yaw commands.
6. Return to a stable hover.
7. Land the vehicle.
8. Disarm.

The custom x100 should take off without flipping and remain controllable during the flight.

---

## 📡 Check the PX4 ROS 2 Topics

Open a third **HOST terminal** and connect to the running Docker container:

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

### Docker Terminal 3 – ROS 2

Inside Docker, source ROS 2 and the workspace:

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
```

Check the available ROS 2 topics:

```bash
ros2 topic list
```

PX4 topics should appear under `/fmu/`.

To check only actuator-related topics:

```bash
ros2 topic list | grep -i actuator
```

The following topics should be available:

```text
/fmu/in/actuator_motors
/fmu/in/actuator_servos
/fmu/out/actuator_outputs
```

If only `/parameter_events` and `/rosout` are visible, make sure that `DDS_run.sh` is running in Docker Terminal 2.

---

## ⚙️ Check the Actuator Outputs

Check the actuator-output topic:

```bash
ros2 topic info /fmu/out/actuator_outputs
```

The expected message type is:

```text
px4_msgs/msg/ActuatorOutputs
```

To display one message:

```bash
ros2 topic echo /fmu/out/actuator_outputs --once
```

When the vehicle is disarmed, the motor outputs should normally be zero.

Run the same command while the vehicle is flying:

```bash
ros2 topic echo /fmu/out/actuator_outputs --once
```

The first four outputs should now contain the active motor commands.

For the x100 quadrotor:

```text
output[0] -> Motor 1 / rotor_0 / CCW
output[1] -> Motor 2 / rotor_1 / CCW
output[2] -> Motor 3 / rotor_2 / CW
output[3] -> Motor 4 / rotor_3 / CW
```

`ActuatorOutputs.msg` contains 16 output slots, but the x100 is a quadrotor. Therefore, only `output[0]` to `output[3]` are used for the four motors. The remaining channels are unused.

---

## 📊 Plot the Motor Outputs with PlotJuggler

Make sure:

- PX4 and Gazebo are running.
- `DDS_run.sh` is running.
- QGroundControl is connected.

Open another Docker terminal using:

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

Inside Docker:

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
```

Start PlotJuggler:

```bash
ros2 run plotjuggler plotjuggler
```

In PlotJuggler:

1. Select **ROS2 Topic Subscriber**.
2. Set the streaming buffer to approximately `120 sec`.
3. Subscribe to:

```text
/fmu/out/actuator_outputs
```

4. Add the following four signals to the same plot:

```text
/fmu/out/actuator_outputs/output[0]
/fmu/out/actuator_outputs/output[1]
/fmu/out/actuator_outputs/output[2]
/fmu/out/actuator_outputs/output[3]
```

5. Start streaming.
6. Arm and take off in **Position mode**.
7. Hover for a few seconds.
8. Apply small roll, pitch, and yaw commands.
9. Return to hover.
10. Land and disarm.
11. Stop PlotJuggler streaming.

During steady hover, the four motor outputs should have similar values.

During roll, pitch, and yaw maneuvers, PX4 changes the individual motor commands and the four curves will separate.

After landing and disarming, the actuator outputs should return to zero.

To save the final figure, right-click on the graph and select:

```text
Save plot to file
```

---

## 📁 Important Custom PX4 Files

The custom Gazebo model is located at:

```text
PX4-Autopilot/Tools/simulation/gz/models/x100/
```

The custom base model is located at:

```text
PX4-Autopilot/Tools/simulation/gz/models/x100_base/
```

The custom PX4 airframe is:

```text
PX4-Autopilot/ROMFS/px4fmu_common/init.d-posix/airframes/22000_gz_x100
```

The DDS topic configuration is:

```text
PX4-Autopilot/src/modules/uxrce_dds_client/dds_topics.yaml
```

The actuator output topic is included in the `publications` section as:

```yaml
- topic: /fmu/out/actuator_outputs
  type: px4_msgs::msg::ActuatorOutputs
```

---

## ✅ Quick Start

The complete simulation uses **three Docker terminals** and **QGroundControl on the host**.

### 1. HOST – Start Docker

```bash
cd ros2_docker_scripts
./docker_run_container.sh
```

When requested, select the host workspace containing:

```text
DDS_run.sh
src/
```

The selected workspace will be mounted as:

```text
/home/user/ros2_ws
```

### 2. Docker Terminal 1 – PX4 + Gazebo

```bash
cd /home/user/ros2_ws/src/PX4-Autopilot
make px4_sitl gz_x100
```

### 3. HOST – Open Terminal 2

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

Inside Docker:

```bash
cd /home/user/ros2_ws
./DDS_run.sh
```

### 4. HOST – Start QGroundControl

Start QGroundControl and wait for the PX4 SITL vehicle to connect.

Select **Position mode** for the manual flight test.

### 5. HOST – Open Terminal 3

```bash
cd ros2_docker_scripts
./docker_connect.sh
```

Inside Docker:

```bash
cd /home/user/ros2_ws
source /opt/ros/humble/setup.bash
source install/setup.bash

ros2 topic list | grep -i actuator
```

To monitor the motor outputs:

```bash
ros2 topic echo /fmu/out/actuator_outputs
```

The setup is working correctly if:

- Gazebo launches the `x100` model.
- QGroundControl connects to PX4 SITL.
- The vehicle can fly in Position mode.
- `/fmu/out/actuator_outputs` is visible in ROS 2.
- `output[0]` to `output[3]` change while the four motors are operating.
