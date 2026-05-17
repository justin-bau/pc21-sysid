"""Maneuver registry. Add new maneuvers by importing and registering here."""

from .base import Maneuver, Axis, ManeuverDelta
from .doublet import Doublet
from .multistep_3211 import Multistep3211
from .sweep import Sweep


MANEUVERS: dict[str, type[Maneuver]] = {
    'doublet': Doublet,
    'multistep_3211': Multistep3211,
    'sweep': Sweep,
}


def build(name: str, axis: Axis, amplitude: float, params: dict) -> Maneuver:
    if name not in MANEUVERS:
        raise ValueError(f'Unknown maneuver {name!r}; available: {sorted(MANEUVERS)}')
    return MANEUVERS[name].from_params(axis, amplitude, params)
