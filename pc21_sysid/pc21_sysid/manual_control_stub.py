"""Stub publisher for ManualControlSetpoint.

Simulates pilot stick input for bench testing offboard passthrough nodes
without a paired RC receiver. Publishes a configurable constant pattern,
or a slow sinusoidal sweep across all axes for visual confirmation.
"""

import math

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

from px4_msgs.msg import ManualControlSetpoint


PX4_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    history=HistoryPolicy.KEEP_LAST,
    depth=5,
)


class ManualControlStub(Node):
    def __init__(self):
        super().__init__('manual_control_stub')

        # Mode: 'zero' (sticks centered), 'constant' (set values), 'sweep' (sinusoid).
        self.declare_parameter('mode', 'sweep')
        self.declare_parameter('const_roll', 0.0)
        self.declare_parameter('const_pitch', 0.0)
        self.declare_parameter('const_yaw', 0.0)
        self.declare_parameter('const_throttle', 0.5)
        self.declare_parameter('sweep_period_s', 8.0)
        self.declare_parameter('sweep_amplitude', 0.5)

        self.pub = self.create_publisher(
            ManualControlSetpoint,
            '/fmu/out/manual_control_setpoint',
            PX4_QOS,
        )

        self.t0 = self.get_clock().now().nanoseconds * 1e-9
        self.create_timer(1.0 / 50.0, self._publish)

        self.get_logger().info(
            f'manual_control_stub running in mode='
            f'{self.get_parameter("mode").value!r}'
        )

    def _publish(self):
        t_now = self.get_clock().now().nanoseconds * 1e-9
        t_us = int(t_now * 1e6)
        t_rel = t_now - self.t0

        mode = self.get_parameter('mode').value

        if mode == 'zero':
            roll = pitch = yaw = 0.0
            throttle = 0.0
        elif mode == 'constant':
            roll  = float(self.get_parameter('const_roll').value)
            pitch = float(self.get_parameter('const_pitch').value)
            yaw   = float(self.get_parameter('const_yaw').value)
            throttle = float(self.get_parameter('const_throttle').value)
        elif mode == 'sweep':
            T = float(self.get_parameter('sweep_period_s').value)
            A = float(self.get_parameter('sweep_amplitude').value)
            phase = 2 * math.pi * t_rel / T
            # Three axes 120° out of phase so each is independently visible.
            roll  = A * math.sin(phase)
            pitch = A * math.sin(phase + 2 * math.pi / 3)
            yaw   = A * math.sin(phase + 4 * math.pi / 3)
            throttle = 0.5 + 0.4 * math.sin(phase / 2)  # slower throttle wave
        else:
            self.get_logger().error(f'Unknown mode {mode!r}')
            return

        msg = ManualControlSetpoint()
        msg.timestamp = t_us
        msg.roll = roll
        msg.pitch = pitch
        msg.yaw = yaw
        msg.throttle = throttle
        self.pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = ManualControlStub()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
