"""Maneuver base class.

A maneuver is a stateful object that produces a per-axis torque delta as a
function of time relative to its start. The framework manages lifecycle
(start, step, completion, abort) and the maneuver itself only defines its
shape and duration.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum


class Axis(Enum):
    ROLL = 'roll'
    PITCH = 'pitch'
    YAW = 'yaw'


@dataclass
class ManeuverDelta:
    """Per-axis torque additions in [-1, +1] (caller clamps total result)."""
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0


class Maneuver(ABC):
    """Base class for all sysid maneuvers.

    Subclasses define maneuver shape via step(); the framework calls start(),
    step() repeatedly, and stops when step() returns None.
    """

    def __init__(self, axis: Axis, amplitude: float, **kwargs):
        self.axis = axis
        self.amplitude = amplitude  # in [-1, +1], applied to chosen axis
        self.t_start: float | None = None  # seconds, set by start()

    def start(self, t_now: float) -> None:
        self.t_start = t_now

    @abstractmethod
    def duration(self) -> float:
        """Total maneuver duration in seconds (for safety / scheduling)."""
        ...

    @abstractmethod
    def signal(self, t_rel: float) -> float:
        """Maneuver signal in [-1, +1] at relative time t_rel ≥ 0.

        Caller multiplies by amplitude and applies to the chosen axis.
        """
        ...

    def step(self, t_now: float) -> ManeuverDelta | None:
        """Return delta for current time, or None if maneuver complete."""
        if self.t_start is None:
            return None
        t_rel = t_now - self.t_start
        if t_rel >= self.duration():
            return None
        value = self.amplitude * self.signal(t_rel)
        return self._apply_to_axis(value)

    def _apply_to_axis(self, value: float) -> ManeuverDelta:
        if self.axis == Axis.ROLL:
            return ManeuverDelta(roll=value)
        if self.axis == Axis.PITCH:
            return ManeuverDelta(pitch=value)
        if self.axis == Axis.YAW:
            return ManeuverDelta(yaw=value)
        return ManeuverDelta()

    @classmethod
    @abstractmethod
    def from_params(cls, axis: Axis, amplitude: float, params: dict) -> 'Maneuver':
        """Construct from generic parameter dict (for JSON-driven dispatch)."""
        ...
