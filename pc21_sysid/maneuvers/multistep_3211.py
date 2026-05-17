"""3-2-1-1 multistep: standard fixed-wing sysid input.

±A for 3Δt, ∓A for 2Δt, ±A for Δt, ∓A for Δt.
"""

from .base import Maneuver, Axis


class Multistep3211(Maneuver):
    def __init__(self, axis: Axis, amplitude: float, dt_s: float = 0.5):
        super().__init__(axis, amplitude)
        self.dt_s = dt_s

    def duration(self) -> float:
        return 7 * self.dt_s  # 3+2+1+1

    def signal(self, t_rel: float) -> float:
        t = t_rel / self.dt_s
        if t < 3.0:
            return +1.0
        if t < 5.0:  # 3 + 2
            return -1.0
        if t < 6.0:  # 5 + 1
            return +1.0
        return -1.0   # 6 + 1

    @classmethod
    def from_params(cls, axis: Axis, amplitude: float, params: dict) -> 'Multistep3211':
        return cls(axis, amplitude, dt_s=float(params.get('dt_s', 0.5)))
