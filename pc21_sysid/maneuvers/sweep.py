"""Logarithmic frequency sweep: sin(2π · f(t) · t) with f swept log-linearly.

For frequency-domain sysid; covers a wide band in one maneuver.
"""

import math

from .base import Maneuver, Axis


class Sweep(Maneuver):
    def __init__(
        self,
        axis: Axis,
        amplitude: float,
        f_min_hz: float = 0.1,
        f_max_hz: float = 5.0,
        duration_s: float = 30.0,
    ):
        super().__init__(axis, amplitude)
        self.f_min = f_min_hz
        self.f_max = f_max_hz
        self.T = duration_s

    def duration(self) -> float:
        return self.T

    def signal(self, t_rel: float) -> float:
        # Logarithmic chirp: f(t) = f_min · (f_max/f_min)^(t/T)
        # Phase φ(t) = 2π · ∫f(t')dt' = 2π · f_min · T / ln(f_max/f_min) · ((f_max/f_min)^(t/T) - 1)
        if self.f_max <= self.f_min:
            f = self.f_min
            phase = 2 * math.pi * f * t_rel
        else:
            k = self.f_max / self.f_min
            phase = (
                2 * math.pi * self.f_min * self.T / math.log(k)
                * (k ** (t_rel / self.T) - 1)
            )
        return math.sin(phase)

    @classmethod
    def from_params(cls, axis: Axis, amplitude: float, params: dict) -> 'Sweep':
        return cls(
            axis,
            amplitude,
            f_min_hz=float(params.get('f_min_hz', 0.1)),
            f_max_hz=float(params.get('f_max_hz', 5.0)),
            duration_s=float(params.get('duration_s', 30.0)),
        )
