"""Doublet maneuver: +A for T, -A for T, zero."""

from .base import Maneuver, Axis


class Doublet(Maneuver):
    def __init__(self, axis: Axis, amplitude: float, pulse_s: float = 0.5):
        super().__init__(axis, amplitude)
        self.pulse_s = pulse_s

    def duration(self) -> float:
        return 2 * self.pulse_s

    def signal(self, t_rel: float) -> float:
        if t_rel < self.pulse_s:
            return +1.0
        return -1.0

    @classmethod
    def from_params(cls, axis: Axis, amplitude: float, params: dict) -> 'Doublet':
        return cls(axis, amplitude, pulse_s=float(params.get('pulse_s', 0.5)))
