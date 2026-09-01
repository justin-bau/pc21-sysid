"""CSV logger for /fmu/out/vehicle_angular_velocity.

Logs PX4 timestamp (us since boot), ROS reception time, and body rates.
"""

import csv
from pathlib import Path
from datetime import datetime

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy

from px4_msgs.msg import VehicleAngularVelocity


class AngularVelocityLogger(Node):
    def __init__(self):
        super().__init__('angular_velocity_logger')

        # PX4 publishes BEST_EFFORT, depth 5. Subscriber must match.
        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=5,
        )

        log_dir = Path.home() / 'sysid_logs'
        log_dir.mkdir(exist_ok=True)
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.log_path = log_dir / f'angular_velocity_{ts}.csv'

        self.file = open(self.log_path, 'w', newline='')
        self.writer = csv.writer(self.file)
        self.writer.writerow(
            ['px4_timestamp_us', 'ros_recv_ns', 'p', 'q', 'r']
        )

        self.sub = self.create_subscription(
            VehicleAngularVelocity,
            '/fmu/out/vehicle_angular_velocity',
            self._callback,
            qos,
        )

        self.msg_count = 0
        self.create_timer(5.0, self._status)
        self.get_logger().info(f'Logging to {self.log_path}')

    def _callback(self, msg: VehicleAngularVelocity):
        ros_time = self.get_clock().now().nanoseconds
        self.writer.writerow(
            [msg.timestamp, ros_time, msg.xyz[0], msg.xyz[1], msg.xyz[2]]
        )
        self.msg_count += 1

    def _status(self):
        self.get_logger().info(
            f'Received {self.msg_count} messages so far'
        )

    def destroy_node(self):
        self.file.flush()
        self.file.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = AngularVelocityLogger()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
