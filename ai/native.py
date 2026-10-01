"""Standard-library binding to the real C++ game; no separate Python simulator."""
import ctypes as C
import json
import math
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FEATURES = ('distance', 'star', 'crate', 'heart', 'time', 'speed', 'shield',
            'exit', 'nearby_stars', 'exit_distance', 'deadline_fraction', 'remaining_stars')
ACTIONS = ('nearest star', 'clustered stars', 'farthest star', 'crate', 'heart',
           'time bonus', 'speed boost', 'shield', 'exit')
INFO = ('score', 'success', 'seconds', 'hits', 'crates', 'stars', 'lives', 'done', 'truncated', 'decisions')
TRAIN_SEEDS = tuple(range(1000, 1256))
VALIDATION_SEEDS = tuple(range(20000, 20064))
TEST_SEEDS = tuple(range(30000, 30128))
_libraries = {}


def library(path=None):
    path = path or os.environ.get('EXPEDITION_AI_LIBRARY')
    if path is None:
        candidates = [ROOT / 'build' / sub / name
                      for sub in ('', 'Release', 'Debug')
                      for name in ('libexpedition_ai.so', 'libexpedition_ai.dylib', 'expedition_ai.dll')]
        path = next((p for p in candidates if p.is_file()), None)
    if path is None:
        raise FileNotFoundError('Build the game first, or set EXPEDITION_AI_LIBRARY to the native library.')
    path = str(Path(path).resolve())
    if path in _libraries:
        return _libraries[path]
    lib = C.CDLL(path)
    signatures = {
        'ae_version': ([], C.c_int), 'ae_error': ([], C.c_char_p),
        'ae_create': ([C.c_uint], C.c_void_p), 'ae_destroy': ([C.c_void_p], None),
        'ae_observe': ([C.c_void_p, C.POINTER(C.c_float)], C.c_int),
        'ae_step': ([C.c_void_p, C.c_int], C.c_int),
        'ae_begin': ([C.c_void_p, C.c_int], C.c_int), 'ae_tick': ([C.c_void_p], C.c_int),
        'ae_info': ([C.c_void_p, C.POINTER(C.c_double)], None),
        'ae_render': ([C.c_void_p], C.c_char_p),
    }
    for name, (args, result) in signatures.items():
        function = getattr(lib, name)
        function.argtypes, function.restype = args, result
    if lib.ae_version() != 1:
        raise RuntimeError('Incompatible AI library; rebuild this checkout.')
    _libraries[path] = lib
    return lib


class Episode:
    def __init__(self, seed, path=None):
        if not isinstance(seed, int) or not 0 <= seed <= 0xffffffff:
            raise ValueError('Map seed must be an unsigned 32-bit integer')
        self.lib = library(path)
        self.handle = self.lib.ae_create(seed)
        if not self.handle:
            raise RuntimeError(self.lib.ae_error().decode())
        self._observation = (C.c_float * (len(ACTIONS) * len(FEATURES)))()
        self._info = (C.c_double * len(INFO))()

    def _check(self, result):
        if result < 0:
            raise RuntimeError(self.lib.ae_error().decode())
        return result

    def _open(self):
        if not self.handle:
            raise RuntimeError('Episode is closed')

    def observe(self):
        self._open()
        self._check(self.lib.ae_observe(self.handle, self._observation))
        return list(self._observation)

    def info(self):
        self._open()
        self.lib.ae_info(self.handle, self._info)
        return dict(zip(INFO, self._info))

    def step(self, action):
        self._open()
        if not 0 <= int(action) < len(ACTIONS):
            raise ValueError('Action must be between 0 and 8')
        self._check(self.lib.ae_step(self.handle, int(action)))
        return self.info()

    def frames(self, action):
        """The same transition as step(), yielding after every real input tick."""
        self._open()
        self._check(self.lib.ae_begin(self.handle, int(action)))
        while True:
            active = self._check(self.lib.ae_tick(self.handle))
            yield self.render()
            if not active:
                break

    def render(self):
        self._open()
        frame = self.lib.ae_render(self.handle)
        if frame is None:
            raise RuntimeError(self.lib.ae_error().decode())
        return frame.decode()

    def close(self):
        if getattr(self, 'handle', None):
            self.lib.ae_destroy(self.handle)
            self.handle = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        self.close()


def linear_action(observation, weights):
    return max(range(len(ACTIONS)), key=lambda a: sum(
        observation[a * len(FEATURES) + f] * w for f, w in enumerate(weights)))


def load_weights(path):
    data = json.loads(Path(path).read_text())
    if data.get('schema') != 1 or tuple(data.get('features', ())) != FEATURES:
        raise ValueError('Weight file has an incompatible feature schema')
    weights = data['weights']
    if len(weights) != len(FEATURES) or not all(isinstance(w, (float, int)) and math.isfinite(w) for w in weights):
        raise ValueError('Expected 12 finite weights')
    return weights


def rollout(seed, policy):
    with Episode(seed) as episode:
        while not episode.info()['done']:
            episode.step(policy(episode.observe()))
        return dict(seed=seed, **episode.info())
