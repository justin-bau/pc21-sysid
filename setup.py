from setuptools import find_packages, setup

package_name = 'pc21_sysid'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='tud-rpi-msc-bauer',
    maintainer_email='tud-rpi-msc-bauer@todo.todo',
    description='TODO: Package description',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'angular_velocity_logger = pc21_sysid.angular_velocity_logger:main',
            'maneuver_runner = pc21_sysid.maneuver_runner:main',
            'manual_control_stub = pc21_sysid.manual_control_stub:main',
        ],
    },
)
