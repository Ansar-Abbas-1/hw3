#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <chrono>
#include <limits>

#include <rclcpp/rclcpp.hpp>

#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/vehicle_attitude.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>

#include <Eigen/Dense>

using namespace std::chrono_literals;
using namespace px4_msgs::msg;


/*
 * Q3 Trajectory Planner
 *
 * Main idea:
 *
 *   Current position = P0
 *
 *   P0 ---- P1 ---- P2 ---- ... ---- P8
 *
 * The trajectory is a piecewise quintic polynomial.
 *
 * For every segment:
 *
 *   p(t) = c0 + c1*t + c2*t^2 + c3*t^3 + c4*t^4 + c5*t^5
 *
 * The coefficients are chosen so that each segment satisfies:
 *
 *   start position
 *   start velocity
 *   start acceleration
 *   end position
 *   end velocity
 *   end acceleration
 *
 * Intermediate waypoint velocities are NONZERO.
 * Final waypoint velocity is ZERO.
 *
 * Therefore the UAV passes through intermediate waypoints
 * without stopping and stops only at the final waypoint.
 *
 * The complete trajectory is computed once.
 * It is then evaluated every 20 ms.
 */


class TrajectoryPlanner : public rclcpp::Node
{
public:

    TrajectoryPlanner()
    : Node("trajectory_planner")
    {
        /*
         * QoS used for PX4 output topics.
         */
        rmw_qos_profile_t qos_profile = rmw_qos_profile_sensor_data;

        auto qos = rclcpp::QoS(
            rclcpp::QoSInitialization(qos_profile.history, 5),
            qos_profile);


        /*
         * ==========================
         * SUBSCRIBERS
         * ==========================
         */

        local_position_subscription_ =
            this->create_subscription<VehicleLocalPosition>(
                "/fmu/out/vehicle_local_position",
                qos,
                std::bind(
                    &TrajectoryPlanner::vehicle_local_position_callback,
                    this,
                    std::placeholders::_1));


        attitude_subscription_ =
            this->create_subscription<VehicleAttitude>(
                "/fmu/out/vehicle_attitude",
                qos,
                std::bind(
                    &TrajectoryPlanner::vehicle_attitude_callback,
                    this,
                    std::placeholders::_1));


        /*
         * ==========================
         * PUBLISHERS
         * ==========================
         */

        offboard_control_mode_publisher_ =
            this->create_publisher<OffboardControlMode>(
                "/fmu/in/offboard_control_mode", 10);


        trajectory_setpoint_publisher_ =
            this->create_publisher<TrajectorySetpoint>(
                "/fmu/in/trajectory_setpoint", 10);


        vehicle_command_publisher_ =
            this->create_publisher<VehicleCommand>(
                "/fmu/in/vehicle_command", 10);


        /*
         * ==========================
         * TIMERS
         * ==========================
         *
         * Offboard keepalive:
         *      every 100 ms
         *
         * Trajectory evaluation:
         *      every 20 ms = 50 Hz
         */

        timer_offboard_ =
            this->create_wall_timer(
                100ms,
                std::bind(
                    &TrajectoryPlanner::offboard_timer_callback,
                    this));


        timer_trajectory_publish_ =
            this->create_wall_timer(
                20ms,
                std::bind(
                    &TrajectoryPlanner::trajectory_timer_callback,
                    this));


        RCLCPP_INFO(
            this->get_logger(),
            "Q3 trajectory planner started.");

        RCLCPP_INFO(
            this->get_logger(),
            "Waiting for PX4 local position...");
    }


private:

    /*
     * ============================================================
     * ROS INTERFACES
     * ============================================================
     */

    rclcpp::Subscription<VehicleLocalPosition>::SharedPtr
        local_position_subscription_;

    rclcpp::Subscription<VehicleAttitude>::SharedPtr
        attitude_subscription_;

    rclcpp::Publisher<OffboardControlMode>::SharedPtr
        offboard_control_mode_publisher_;

    rclcpp::Publisher<VehicleCommand>::SharedPtr
        vehicle_command_publisher_;

    rclcpp::Publisher<TrajectorySetpoint>::SharedPtr
        trajectory_setpoint_publisher_;

    rclcpp::TimerBase::SharedPtr timer_offboard_;
    rclcpp::TimerBase::SharedPtr timer_trajectory_publish_;


    /*
     * ============================================================
     * PX4 FEEDBACK
     * ============================================================
     */

    VehicleLocalPosition current_position_{};
    VehicleAttitude current_attitude_{};

    bool position_received_{false};
    bool attitude_received_{false};


    /*
     * ============================================================
     * OFFBOARD STATE
     * ============================================================
     */

    int offboard_counter_{0};

    bool offboard_active_{false};
    bool trajectory_initialized_{false};
    bool trajectory_started_{false};
    bool trajectory_finished_{false};


    /*
     * ============================================================
     * TRAJECTORY SETTINGS
     * ============================================================
     */

    /*
     * Duration of EACH spline segment.
     *
     * We have:
     *
     * P0 -> P1
     * P1 -> P2
     * ...
     * P7 -> P8
     *
     * Therefore 8 segments.
     *
     * 3 seconds per segment gives:
     *
     * total trajectory duration = 8 * 3 = 24 seconds.
     */
    const double segment_duration_{3.0};


    /*
     * Constant yaw reference.
     *
     * yaw = 0 rad in PX4 NED.
     *
     * This satisfies the attitude/yaw planning requirement
     * using a fixed planned yaw.
     */
    const float desired_yaw_{0.0f};


    /*
     * Waypoints:
     *
     * waypoints_[0] = actual initial UAV position
     *
     * followed by 8 planned waypoints.
     */
    std::vector<Eigen::Vector3d> waypoints_;


    /*
     * Desired velocity at each waypoint.
     *
     * Intermediate waypoint velocities are nonzero.
     */
    std::vector<Eigen::Vector3d> waypoint_velocities_;


    /*
     * Desired acceleration at each waypoint.
     *
     * We use zero waypoint acceleration.
     *
     * Since the end acceleration of one segment and the
     * start acceleration of the next are identical,
     * acceleration is continuous at the waypoint.
     */
    std::vector<Eigen::Vector3d> waypoint_accelerations_;


    /*
     * ============================================================
     * QUINTIC SPLINE COEFFICIENTS
     * ============================================================
     *
     * For one segment:
     *
     * p(t) =
     * c0
     * + c1*t
     * + c2*t^2
     * + c3*t^3
     * + c4*t^4
     * + c5*t^5
     *
     * We need one polynomial for x,
     * one for y,
     * one for z.
     *
     * Matrix layout:
     *
     * row 0 -> c0 for x,y,z
     * row 1 -> c1 for x,y,z
     * ...
     * row 5 -> c5 for x,y,z
     */
    struct QuinticSegment
    {
        Eigen::Matrix<double, 6, 3> coefficients;
        double duration;
    };


    std::vector<QuinticSegment> segments_;


    /*
     * ============================================================
     * TRAJECTORY CLOCK
     * ============================================================
     */

    rclcpp::Time trajectory_start_time_{0, 0, RCL_ROS_TIME};


    /*
     * ============================================================
     * PX4 CALLBACKS
     * ============================================================
     */

    void vehicle_local_position_callback(
        const VehicleLocalPosition::SharedPtr msg)
    {
        current_position_ = *msg;

        /*
         * Do not initialize from invalid position data.
         */
        if (std::isfinite(msg->x) &&
            std::isfinite(msg->y) &&
            std::isfinite(msg->z))
        {
            position_received_ = true;
        }
    }


    void vehicle_attitude_callback(
        const VehicleAttitude::SharedPtr msg)
    {
        current_attitude_ = *msg;
        attitude_received_ = true;
    }


    /*
     * ============================================================
     * DEFINE WAYPOINTS
     * ============================================================
     */

    void create_waypoints()
    {
        waypoints_.clear();


        /*
         * P0 = actual current UAV position.
         */
        Eigen::Vector3d P0(
            current_position_.x,
            current_position_.y,
            current_position_.z);


        waypoints_.push_back(P0);


        /*
         * ========================================================
         * PLANNED WAYPOINTS
         * ========================================================
         *
         * All coordinates are RELATIVE to P0.
         *
         * PX4 local coordinates use NED:
         *
         * +x = North
         * +y = East
         * +z = Down
         *
         * Therefore:
         *
         * z = -3 means 3 m ABOVE the starting point.
         *
         * There are 8 planned waypoints after P0.
         * This clearly exceeds the minimum 7-waypoint requirement.
         */


        // P1: take off vertically to 3 m
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                0.0,
                0.0,
                -3.0));


        // P2
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                2.0,
                0.0,
                -4.0));


        // P3
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                4.0,
                2.0,
                -5.0));


        // P4
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                4.0,
                5.0,
                -5.5));


        // P5
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                2.0,
                7.0,
                -6.0));


        // P6
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                -1.0,
                6.0,
                -5.5));


        // P7
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                -2.0,
                3.0,
                -5.0));


        // P8: final waypoint
        waypoints_.push_back(
            P0 + Eigen::Vector3d(
                0.0,
                1.0,
                -4.0));


        RCLCPP_INFO(
            this->get_logger(),
            "Created %zu total trajectory points (%zu planned waypoints after P0).",
            waypoints_.size(),
            waypoints_.size() - 1);


        /*
         * Print all waypoints.
         */
        for (std::size_t i = 0; i < waypoints_.size(); ++i)
        {
            RCLCPP_INFO(
                this->get_logger(),
                "P%zu = [%.2f, %.2f, %.2f]",
                i,
                waypoints_[i](0),
                waypoints_[i](1),
                waypoints_[i](2));
        }
    }


    /*
     * ============================================================
     * CALCULATE WAYPOINT VELOCITIES
     * ============================================================
     */

    void calculate_waypoint_velocities()
    {
        waypoint_velocities_.resize(
            waypoints_.size(),
            Eigen::Vector3d::Zero());


        waypoint_accelerations_.resize(
            waypoints_.size(),
            Eigen::Vector3d::Zero());


        /*
         * Start velocity.
         *
         * The UAV begins from rest.
         */
        waypoint_velocities_.front().setZero();


        /*
         * Intermediate waypoint velocities.
         *
         * Central-difference estimate:
         *
         *             P_(i+1) - P_(i-1)
         *     v_i = -----------------------
         *                    2*T
         *
         * This gives a velocity approximately tangent to
         * the trajectory through waypoint Pi.
         *
         * Most importantly:
         *
         * intermediate waypoint velocities are NOT forced
         * to zero.
         */
        for (std::size_t i = 1;
             i < waypoints_.size() - 1;
             ++i)
        {
            waypoint_velocities_[i] =
                (waypoints_[i + 1] -
                 waypoints_[i - 1])
                /
                (2.0 * segment_duration_);
        }


        /*
         * Final velocity = zero.
         *
         * The UAV stops only at the final waypoint.
         */
        waypoint_velocities_.back().setZero();


        /*
         * Print waypoint velocities.
         */
        for (std::size_t i = 0;
             i < waypoint_velocities_.size();
             ++i)
        {
            const auto &v = waypoint_velocities_[i];

            RCLCPP_INFO(
                this->get_logger(),
                "Velocity at P%zu = [%.3f, %.3f, %.3f], |v| = %.3f m/s",
                i,
                v(0),
                v(1),
                v(2),
                v.norm());
        }
    }


    /*
     * ============================================================
     * SOLVE ONE QUINTIC POLYNOMIAL
     * ============================================================
     *
     * Boundary conditions:
     *
     * p(0) = p0
     * v(0) = v0
     * a(0) = a0
     *
     * p(T) = p1
     * v(T) = v1
     * a(T) = a1
     *
     * Polynomial:
     *
     * p(t) =
     * c0 + c1*t + c2*t^2
     * + c3*t^3 + c4*t^4 + c5*t^5
     */

    Eigen::Matrix<double, 6, 3>
    solve_quintic(
        const Eigen::Vector3d &p0,
        const Eigen::Vector3d &v0,
        const Eigen::Vector3d &a0,
        const Eigen::Vector3d &p1,
        const Eigen::Vector3d &v1,
        const Eigen::Vector3d &a1,
        double T)
    {
        Eigen::Matrix<double, 6, 3> C;


        /*
         * First three coefficients come directly from
         * the initial boundary conditions.
         */

        C.row(0) = p0.transpose();

        C.row(1) = v0.transpose();

        C.row(2) = (0.5 * a0).transpose();


        /*
         * Remaining unknown coefficients:
         *
         * c3, c4, c5
         *
         * are obtained from the final boundary conditions.
         */

        Eigen::Matrix3d A;

        A <<
            std::pow(T, 3),       std::pow(T, 4),        std::pow(T, 5),
            3.0 * std::pow(T, 2), 4.0 * std::pow(T, 3), 5.0 * std::pow(T, 4),
            6.0 * T,              12.0 * std::pow(T, 2), 20.0 * std::pow(T, 3);


        /*
         * Contributions already produced by c0,c1,c2.
         */

        Eigen::Vector3d delta_p =
            p1 -
            (
                p0
                + v0 * T
                + 0.5 * a0 * T * T
            );


        Eigen::Vector3d delta_v =
            v1 -
            (
                v0
                + a0 * T
            );


        Eigen::Vector3d delta_a =
            a1 - a0;


        /*
         * Right-hand side:
         *
         * Each column corresponds to x, y, z.
         */

        Eigen::Matrix<double, 3, 3> B;

        B.row(0) = delta_p.transpose();
        B.row(1) = delta_v.transpose();
        B.row(2) = delta_a.transpose();


        /*
         * Solve:
         *
         * A * [c3 c4 c5]^T = B
         */

        Eigen::Matrix<double, 3, 3> solution =
            A.colPivHouseholderQr().solve(B);


        C.row(3) = solution.row(0);
        C.row(4) = solution.row(1);
        C.row(5) = solution.row(2);


        return C;
    }


    /*
     * ============================================================
     * BUILD COMPLETE PIECEWISE TRAJECTORY
     * ============================================================
     */

    void build_trajectory()
    {
        segments_.clear();


        for (std::size_t i = 0;
             i < waypoints_.size() - 1;
             ++i)
        {
            QuinticSegment segment;


            segment.duration = segment_duration_;


            segment.coefficients =
                solve_quintic(
                    waypoints_[i],
                    waypoint_velocities_[i],
                    waypoint_accelerations_[i],

                    waypoints_[i + 1],
                    waypoint_velocities_[i + 1],
                    waypoint_accelerations_[i + 1],

                    segment.duration);


            segments_.push_back(segment);
        }


        trajectory_initialized_ = true;


        RCLCPP_INFO(
            this->get_logger(),
            "Piecewise quintic trajectory successfully generated.");


        RCLCPP_INFO(
            this->get_logger(),
            "Number of segments: %zu",
            segments_.size());


        RCLCPP_INFO(
            this->get_logger(),
            "Segment duration: %.2f s",
            segment_duration_);


        RCLCPP_INFO(
            this->get_logger(),
            "Total trajectory duration: %.2f s",
            total_trajectory_duration());
    }


    /*
     * ============================================================
     * TOTAL TRAJECTORY DURATION
     * ============================================================
     */

    double total_trajectory_duration() const
    {
        return
            static_cast<double>(segments_.size())
            * segment_duration_;
    }


    /*
     * ============================================================
     * EVALUATE ONE QUINTIC SEGMENT
     * ============================================================
     *
     * Input:
     *
     *      local time inside current segment
     *
     * Output:
     *
     *      desired position
     *      desired velocity
     *      desired acceleration
     */

    void evaluate_segment(
        const QuinticSegment &segment,
        double t,
        Eigen::Vector3d &position,
        Eigen::Vector3d &velocity,
        Eigen::Vector3d &acceleration)
    {
        /*
         * Clamp local time.
         */
        t = std::max(
            0.0,
            std::min(t, segment.duration));


        const auto &C = segment.coefficients;


        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t3 * t;
        double t5 = t4 * t;


        /*
         * Position:
         *
         * p =
         * c0 + c1*t + c2*t^2
         * + c3*t^3 + c4*t^4 + c5*t^5
         */

        position =
            C.row(0).transpose()
            + C.row(1).transpose() * t
            + C.row(2).transpose() * t2
            + C.row(3).transpose() * t3
            + C.row(4).transpose() * t4
            + C.row(5).transpose() * t5;


        /*
         * Velocity:
         *
         * v =
         * c1
         * + 2*c2*t
         * + 3*c3*t^2
         * + 4*c4*t^3
         * + 5*c5*t^4
         */

        velocity =
            C.row(1).transpose()
            + 2.0 * C.row(2).transpose() * t
            + 3.0 * C.row(3).transpose() * t2
            + 4.0 * C.row(4).transpose() * t3
            + 5.0 * C.row(5).transpose() * t4;


        /*
         * Acceleration:
         *
         * a =
         * 2*c2
         * + 6*c3*t
         * + 12*c4*t^2
         * + 20*c5*t^3
         */

        acceleration =
            2.0 * C.row(2).transpose()
            + 6.0 * C.row(3).transpose() * t
            + 12.0 * C.row(4).transpose() * t2
            + 20.0 * C.row(5).transpose() * t3;
    }


    /*
     * ============================================================
     * EVALUATE COMPLETE TRAJECTORY
     * ============================================================
     */

    void evaluate_trajectory(
        double trajectory_time,
        Eigen::Vector3d &position,
        Eigen::Vector3d &velocity,
        Eigen::Vector3d &acceleration)
    {
        /*
         * If the trajectory is finished,
         * hold the final point.
         */

        if (trajectory_time >= total_trajectory_duration())
        {
            position = waypoints_.back();

            velocity.setZero();

            acceleration.setZero();

            return;
        }


        /*
         * Determine which segment contains the current time.
         *
         * Example:
         *
         * segment_duration = 3 s
         *
         * t = 0 ... 3   -> segment 0
         * t = 3 ... 6   -> segment 1
         * t = 6 ... 9   -> segment 2
         */

        std::size_t segment_index =
            static_cast<std::size_t>(
                trajectory_time / segment_duration_);


        /*
         * Safety check.
         */

        if (segment_index >= segments_.size())
        {
            segment_index = segments_.size() - 1;
        }


        /*
         * Convert global trajectory time into
         * LOCAL time inside the current segment.
         */

        double local_time =
            trajectory_time
            -
            static_cast<double>(segment_index)
            * segment_duration_;


        /*
         * Evaluate the appropriate spline segment.
         */

        evaluate_segment(
            segments_[segment_index],
            local_time,
            position,
            velocity,
            acceleration);
    }


    /*
     * ============================================================
     * PUBLISH OFFBOARD CONTROL MODE
     * ============================================================
     */

    void publish_offboard_control_mode()
    {
        OffboardControlMode msg{};


        /*
         * We publish position, velocity and acceleration
         * trajectory references.
         */

        msg.position = true;
        msg.velocity = true;
        msg.acceleration = true;

        msg.attitude = false;
        msg.body_rate = false;


        msg.timestamp =
            this->get_clock()->now().nanoseconds() / 1000;


        offboard_control_mode_publisher_->publish(msg);
    }


    /*
     * ============================================================
     * SEND VEHICLE COMMAND
     * ============================================================
     */

    void publish_vehicle_command(
        uint16_t command,
        float param1 = 0.0f,
        float param2 = 0.0f)
    {
        VehicleCommand msg{};

        msg.param1 = param1;
        msg.param2 = param2;

        msg.command = command;

        msg.target_system = 1;
        msg.target_component = 1;

        msg.source_system = 1;
        msg.source_component = 1;

        msg.from_external = true;

        msg.timestamp =
            this->get_clock()->now().nanoseconds() / 1000;


        vehicle_command_publisher_->publish(msg);
    }


    /*
     * ============================================================
     * OFFBOARD TIMER
     * ============================================================
     *
     * Runs every 100 ms.
     */

    void offboard_timer_callback()
    {
        /*
         * We need valid position before doing anything.
         */

        if (!position_received_)
        {
            return;
        }


        /*
         * Initialize trajectory ONCE.
         *
         * This is the PLANNING stage.
         */

        if (!trajectory_initialized_)
        {
            create_waypoints();

            calculate_waypoint_velocities();

            build_trajectory();

            RCLCPP_INFO(
                this->get_logger(),
                "Trajectory planned. Sending Offboard keepalive...");
        }


        /*
         * Always send OffboardControlMode.
         */

        publish_offboard_control_mode();


        /*
         * PX4 requires Offboard messages before switching
         * into Offboard mode.
         *
         * At 100 ms:
         *
         * 10 messages ≈ 1 second.
         */

        if (!offboard_active_)
        {
            offboard_counter_++;


            if (offboard_counter_ >= 10)
            {
                /*
                 * Enter Offboard mode.
                 */

                publish_vehicle_command(
                    VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                    1.0f,
                    6.0f);


                /*
                 * Arm UAV.
                 */

                publish_vehicle_command(
                    VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM,
                    1.0f,
                    0.0f);


                offboard_active_ = true;


                /*
                 * Start trajectory clock NOW.
                 */

                trajectory_start_time_ =
                    this->get_clock()->now();

                trajectory_started_ = true;


                RCLCPP_INFO(
                    this->get_logger(),
                    "OFFBOARD requested and vehicle armed.");


                RCLCPP_INFO(
                    this->get_logger(),
                    "Trajectory execution started.");
            }
        }
    }


    /*
     * ============================================================
     * TRAJECTORY TIMER
     * ============================================================
     *
     * Runs every 20 ms = 50 Hz.
     *
     * IMPORTANT:
     *
     * We do NOT calculate a new trajectory here.
     *
     * The complete trajectory was already generated by
     * build_trajectory().
     *
     * Here we only EVALUATE that stored trajectory at
     * the current time.
     */

    void trajectory_timer_callback()
    {
        if (!trajectory_initialized_ ||
            !offboard_active_ ||
            !trajectory_started_)
        {
            return;
        }


        /*
         * Actual elapsed trajectory time.
         *
         * This is preferable to repeatedly doing:
         *
         * t += 0.02
         *
         * because timer callbacks can have small timing jitter.
         */

        double trajectory_time =
            (
                this->get_clock()->now()
                -
                trajectory_start_time_
            ).seconds();


        Eigen::Vector3d desired_position;
        Eigen::Vector3d desired_velocity;
        Eigen::Vector3d desired_acceleration;


        /*
         * Evaluate the stored trajectory.
         */

        evaluate_trajectory(
            trajectory_time,
            desired_position,
            desired_velocity,
            desired_acceleration);


        /*
         * Construct PX4 TrajectorySetpoint.
         */

        TrajectorySetpoint msg{};


        msg.position =
        {
            static_cast<float>(desired_position(0)),
            static_cast<float>(desired_position(1)),
            static_cast<float>(desired_position(2))
        };


        msg.velocity =
        {
            static_cast<float>(desired_velocity(0)),
            static_cast<float>(desired_velocity(1)),
            static_cast<float>(desired_velocity(2))
        };


        msg.acceleration =
        {
            static_cast<float>(desired_acceleration(0)),
            static_cast<float>(desired_acceleration(1)),
            static_cast<float>(desired_acceleration(2))
        };


        /*
         * Fixed planned yaw.
         */
        msg.yaw = desired_yaw_;


        msg.timestamp =
            this->get_clock()->now().nanoseconds() / 1000;


        /*
         * Send CURRENT desired trajectory state.
         */

        trajectory_setpoint_publisher_->publish(msg);


        /*
         * Detect trajectory completion once.
         */

        if (!trajectory_finished_ &&
            trajectory_time >= total_trajectory_duration())
        {
            trajectory_finished_ = true;


            RCLCPP_INFO(
                this->get_logger(),
                "Trajectory completed.");


            RCLCPP_INFO(
                this->get_logger(),
                "Holding final waypoint with zero velocity and acceleration.");
        }
    }
};


/*
 * ================================================================
 * MAIN
 * ================================================================
 */

int main(int argc, char *argv[])
{
    std::cout
        << "Starting trajectory planner node..."
        << std::endl;


    rclcpp::init(argc, argv);


    rclcpp::spin(
        std::make_shared<TrajectoryPlanner>());


    rclcpp::shutdown();


    return 0;
}
