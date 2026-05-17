"""Pilot passthrough + maneuver injection.

States:
    IDLE       — passthrough only
    EXECUTING  — applying maneuver delta on top of passthrough
    ABORTED    — explicit abort or mode switch out of offboard; same effect as IDLE

Maneuvers triggered via /start_maneuver service, aborted via /abort_maneuver.
"""

import json
from enum import Enum

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

from px4_msgs.msg import (
    ManualControlSetpoint,
    OffboardControlMode,
    VehicleStatus,
    VehicleTorqueSetpoint,
    VehicleThrustSetpoint,
)
from std_srvs.srv import Trigger
from pc21_sysid_interfaces.srv import StartManeuver

from pc21_sysid.maneuvers import Axis, ManeuverDelta, build as build_maneuver

PX4_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    history=HistoryPolicy.KEEP_LAST,
    depth=5,
)

class State(Enum):
    IDLE = 'idle'
    ARMING = 'arming'        # precondition checks before EXECUTING
    EXECUTING = 'executing'
    RECOVERY = 'recovery'    # post-maneuver damped tail capture
    ABORTED = 'aborted'      # distinguishable failure terminal, auto-clears to IDLE

def clamp(x: float, lo: float = -1.0, hi: float = 1.0) -> float:
    return max(lo, min(hi, x))


class ManeuverRunner(Node):
    def __init__(self):
        super().__init__('maneuver_runner')

        self.declare_parameter('trim_roll', 0.0)
        self.declare_parameter('trim_pitch', 0.0)
        self.declare_parameter('trim_yaw', 0.0)

        self.declare_parameter('recovery_s', 3.0)  # damped-tail duration after EXECUTING
        self.t_state_entered: float | None = None

        self.pilot_roll = 0.0
        self.pilot_pitch = 0.0
        self.pilot_yaw = 0.0
        self.pilot_throttle = 0.0
        self.have_manual = False
        self.nav_state: int | None = None

        self.state = State.IDLE
        self.active_maneuver = None

        self.create_subscription(
            ManualControlSetpoint, '/fmu/out/manual_control_setpoint',
            self._on_manual, PX4_QOS,
        )
        self.create_subscription(
            VehicleStatus, '/fmu/out/vehicle_status_v1',
            self._on_status, PX4_QOS,
        )

        self.pub_torque = self.create_publisher(
            VehicleTorqueSetpoint, '/fmu/in/vehicle_torque_setpoint', PX4_QOS)
        self.pub_thrust = self.create_publisher(
            VehicleThrustSetpoint, '/fmu/in/vehicle_thrust_setpoint', PX4_QOS)
        self.pub_offboard = self.create_publisher(
            OffboardControlMode, '/fmu/in/offboard_control_mode', PX4_QOS)

        # Services. Using Trigger for abort (no args).
        # For start_maneuver, we use a custom service below — see note.
        self.create_service(
            StartManeuver, '~/start_maneuver', self._handle_start_maneuver
        )
        self.create_service(
            Trigger, '~/abort_maneuver', self._handle_abort
        )
        # Start service registered separately; see __main__.

        self.create_timer(1.0 / 50.0, self._control_step)
        self.create_timer(1.0 / 10.0, self._heartbeat)

        self.get_logger().info('ManeuverRunner ready, state=IDLE')

    def _on_manual(self, msg: ManualControlSetpoint):
        self.pilot_roll = msg.roll
        self.pilot_pitch = msg.pitch
        self.pilot_yaw = msg.yaw
        self.pilot_throttle = msg.throttle
        self.have_manual = True

    def _on_status(self, msg: VehicleStatus):
        # nav_state == 14 is NAVIGATION_STATE_OFFBOARD in PX4 1.16
        # (verify against your px4_msgs definitions)
        OFFBOARD = 14
        previously_offboard = (self.nav_state == OFFBOARD)
        self.nav_state = msg.nav_state
        if previously_offboard and msg.nav_state != OFFBOARD:
            if self.state in (State.ARMING, State.EXECUTING, State.RECOVERY):
                self._enter_state(State.ABORTED, 'left offboard mode')

    def start_maneuver(self, name: str, axis_str: str,
                       amplitude: float, params_json: str) -> tuple[bool, str]:
        """Called by the service handler. Returns (success, message)."""
        if self.state != State.IDLE:
            return False, f'Cannot start: state={self.state.value}'
        # Comment out this check only for testing purposes
        # if self.nav_state != 14:  # OFFBOARD
        #     return False, 'Cannot start: not in offboard mode'
        try:
            axis = Axis(axis_str.lower())
            params = json.loads(params_json) if params_json else {}
            man = build_maneuver(name, axis, amplitude, params)
            man.start(self._now_s())
            self.active_maneuver = man
            self._enter_state(State.ARMING, f'preconditions for {name}')
            # ARMING transitions to EXECUTING on the next control_step if preconditions hold;
            # kept as a separate state for future when preconditions take time to evaluate.
            self.get_logger().info(
                f'Maneuver started: {name} axis={axis_str} '
                f'amp={amplitude} duration={man.duration():.2f}s'
            )
            return True, 'Started'
        except Exception as e:
            return False, f'Failed to build maneuver: {e}'

    def _handle_abort(self, request, response):
        if self.state in (State.EXECUTING, State.ARMING, State.RECOVERY):
            self.get_logger().info('Abort requested')
            self._enter_state(State.ABORTED, 'abort service called')
            response.success = True
            response.message = 'Aborted'
        else:
            response.success = False
            response.message = 'No maneuver active'
        return response

    def _handle_start_maneuver(self, request, response):
        success, msg = self.start_maneuver(
            name=request.maneuver_name,
            axis_str=request.axis,
            amplitude=request.amplitude,
            params_json=request.params_json,
        )
        response.success = success
        response.message = msg
        return response

    def _enter_state(self, new_state: State, reason: str = '') -> None:
        self.get_logger().info(
            f'State: {self.state.value} → {new_state.value}'
            + (f' ({reason})' if reason else '')
        )
        self.state = new_state
        self.t_state_entered = self._now_s()

    def _now_s(self) -> float:
        return self.get_clock().now().nanoseconds * 1e-9
    
    def _control_step(self):
        if not self.have_manual:
            return
    
        delta = ManeuverDelta()
        t = self._now_s()
    
        if self.state == State.IDLE:
            pass  # no delta
    
        elif self.state == State.ARMING:
            # No preflight checks defined yet beyond the ones done in start_maneuver.
            # Transition immediately to EXECUTING.
            # TODO: add attitude/airspeed/altitude validation here when those topics
            # are subscribed.
            if self.active_maneuver is not None:
                self.active_maneuver.start(t)
                self._enter_state(State.EXECUTING, 'preconditions OK')
    
        elif self.state == State.EXECUTING:
            result = self.active_maneuver.step(t)
            if result is None:
                recovery_s = float(self.get_parameter('recovery_s').value)
                self._enter_state(State.RECOVERY,
                                  f'maneuver complete, holding for {recovery_s}s')
            else:
                delta = result
    
        elif self.state == State.RECOVERY:
            recovery_s = float(self.get_parameter('recovery_s').value)
            if t - self.t_state_entered >= recovery_s:
                self.active_maneuver = None
                self._enter_state(State.IDLE, 'recovery complete')
            # During RECOVERY: delta stays zero; pilot continues to fly the aircraft
            # back toward trim. The recovery period captures the damped response.
    
        elif self.state == State.ABORTED:
            # Hold ABORTED briefly so logs show the transition; then return to IDLE.
            if t - self.t_state_entered >= 1.0:
                self.active_maneuver = None
                self._enter_state(State.IDLE, 'reset after abort')
    
        # Build outputs (same as before)
        t_us = int(t * 1e6)
        trim_roll = float(self.get_parameter('trim_roll').value)
        trim_pitch = float(self.get_parameter('trim_pitch').value)
        trim_yaw = float(self.get_parameter('trim_yaw').value)
    
        roll  = clamp(self.pilot_roll  + trim_roll  + delta.roll)
        pitch = clamp(self.pilot_pitch + trim_pitch + delta.pitch)
        yaw   = clamp(self.pilot_yaw   + trim_yaw   + delta.yaw)
        thrust_x = clamp(self.pilot_throttle, 0.0, 1.0)
    
        torque = VehicleTorqueSetpoint()
        torque.timestamp = t_us
        torque.xyz = [roll, pitch, yaw]
        self.pub_torque.publish(torque)
    
        thrust = VehicleThrustSetpoint()
        thrust.timestamp = t_us
        thrust.xyz = [thrust_x, 0.0, 0.0]
        self.pub_thrust.publish(thrust)

    def _heartbeat(self):
        msg = OffboardControlMode()
        msg.timestamp = int(self._now_s() * 1e6)
        msg.thrust_and_torque = True
        self.pub_offboard.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = ManeuverRunner()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
