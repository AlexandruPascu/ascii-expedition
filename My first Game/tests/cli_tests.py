"""Check user-facing command-line failures without requiring a terminal."""
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    executable = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="ea-cli-tests-") as temporary:
        score_file = Path(temporary) / "scores"

        def run(*arguments):
            return subprocess.run(
                [executable, "--scores-file", str(score_file), *arguments],
                input="", capture_output=True, text=True, timeout=5,
            )

        result = run("--help")
        assert result.returncode == 0 and "--difficulty" in result.stdout
        assert "H/? guide" in result.stdout and "\x1b" not in result.stdout
        for seed in ("0", "4294967295"):
            assert run("--seed", seed, "--help").returncode == 0
        for seed in ("-1", "4294967296", "abc", "1.5", ""):
            result = run("--seed", seed)
            assert result.returncode != 0 and "Seed must be an integer" in result.stderr, seed
        for mode in ("relaxed", "normal", "hard"):
            assert run("--difficulty", mode, "--help").returncode == 0
        result = run("--difficulty", "impossible")
        assert result.returncode != 0 and "Difficulty must be" in result.stderr
        for argument in ("--seed", "--difficulty", "--scores-file", "--unknown"):
            result = run(argument)
            assert result.returncode != 0 and "Unknown or incomplete option" in result.stderr, argument
        result = run("--scores-file", "")
        assert result.returncode != 0 and "Score file path cannot be empty" in result.stderr
        result = run("--difficulty", "normal", "--skip-tutorial", "--seed", "42")
        assert result.returncode != 0 and "interactive terminal" in result.stderr
        assert "\x1b" not in result.stdout and not score_file.exists()
    print("PASS help, valid options, invalid options, noninteractive launch, and isolated scores")


if __name__ == "__main__":
    main()
