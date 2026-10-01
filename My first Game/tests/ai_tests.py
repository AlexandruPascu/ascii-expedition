"""Binding contract tests; only the standard library is required."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'ai'))
if len(sys.argv) > 1:
    os.environ['EXPEDITION_AI_LIBRARY'] = sys.argv.pop(1)
from native import Episode, FEATURES, TRAIN_SEEDS, VALIDATION_SEEDS, TEST_SEEDS, load_weights, rollout


class BindingTests(unittest.TestCase):
    def test_replay_and_scoring(self):
        for seed in (0, 42, 1001, 0xffffffff):
            a = rollout(seed, lambda _: 0)
            b = rollout(seed, lambda _: 0)
            self.assertEqual(a, b)
            self.assertEqual(a['success'], 1)
            self.assertEqual(a['stars'], 6)
            self.assertGreater(a['seconds'], 0)

    def test_frame_playback_parity(self):
        with Episode(17) as a, Episode(17) as b:
            for action in (6, 3, 4, 5, 0, 2):
                if a.info()['done']:
                    break
                a.step(action)
                list(b.frames(action))
                self.assertEqual(a.info(), b.info())
                self.assertEqual(a.observe(), b.observe())

    def test_closed_and_invalid(self):
        with self.assertRaises(ValueError):
            Episode(-1)
        e = Episode(42)
        with self.assertRaises(ValueError):
            e.step(9)
        e.close()
        e.close()
        with self.assertRaises(RuntimeError):
            e.observe()

    def test_data_split(self):
        self.assertFalse(set(TRAIN_SEEDS) & set(VALIDATION_SEEDS))
        self.assertFalse(set(TRAIN_SEEDS) & set(TEST_SEEDS))
        self.assertFalse(set(VALIDATION_SEEDS) & set(TEST_SEEDS))

    def test_weight_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'weights.json'
            path.write_text(json.dumps(dict(schema=1, features=FEATURES, weights=[0.] * 12)))
            self.assertEqual(load_weights(path), [0.] * 12)
            for weights in ([0.] * 11, [float('nan')] * 12, ['bad'] * 12):
                path.write_text(json.dumps(dict(schema=1, features=FEATURES, weights=weights)))
                with self.assertRaises(ValueError):
                    load_weights(path)


if __name__ == '__main__':
    unittest.main()
