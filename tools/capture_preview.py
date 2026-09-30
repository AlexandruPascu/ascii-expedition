"""Capture an actual game frame as an SVG for the README (Linux/macOS)."""
import argparse
import importlib.util
import re
import tempfile
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
COLORS = {
    0: '#e6edf3', 90: '#8b949e', 91: '#ff7b72', 92: '#7ee787',
    93: '#f2cc60', 94: '#79c0ff', 95: '#d2a8ff', 96: '#76e3ea',
}
ANSI = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')


def render_svg(data):
    rows = None
    # A read can end halfway through a frame; use the last complete one.
    for frame in reversed(data.split(b'\x1b[1;1H')[1:]):
        candidate = re.findall(
            rb'\x1b\[(\d+);1H(.*?)(?=\x1b\[\d+;1H|\Z)',
            b'\x1b[1;1H' + frame, re.S,
        )
        if len(candidate) == 37 and all(len(ANSI.sub(b'', row)) >= 80 for _, row in candidate):
            rows = candidate
            break
    if rows is None:
        raise RuntimeError('No complete game frame was captured')
    output = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="768" height="750" viewBox="0 0 768 750" role="img" aria-labelledby="title description">',
        '<title id="title">ASCII Expedition: a generated map</title>',
        '<desc id="description">Actual terminal capture of Normal difficulty, seed 42. Rooms contain stars, crates, guards, a scout, a sentry, and power-ups. Controls and run information appear below the arena.</desc>',
        '<rect width="768" height="750" rx="12" fill="#0d1117"/>',
        '<path d="M0 46H768" stroke="#30363d"/>',
        '<circle cx="24" cy="23" r="5" fill="#ff7b72"/>',
        '<circle cx="42" cy="23" r="5" fill="#f2cc60"/>',
        '<circle cx="60" cy="23" r="5" fill="#7ee787"/>',
        '<g font-family="DejaVu Sans Mono,Consolas,monospace" font-size="14" xml:space="preserve">',
        '<text x="92" y="28" fill="#8b949e">ASCII EXPEDITION</text>',
    ]
    color = COLORS[0]
    for row_number, raw in rows:
        x = 0
        for token in re.finditer(r'\x1b\[(\d+)m|([^\x1b]+)', raw.decode('ascii')):
            if token[1] is not None:
                color = COLORS.get(int(token[1]), COLORS[0])
                continue
            text = token[2][:80-x]
            # Position glyphs individually so SVG viewers preserve terminal cells.
            for offset, character in enumerate(text):
                if character != ' ':
                    output.append(
                        f'<text x="{24+(x+offset)*9}" y="{64+(int(row_number)-1)*18}" '
                        f'fill="{color}">{escape(character)}</text>'
                    )
            x += len(text)
            if x == 80:
                break
    output.extend(['</g>', '</svg>'])
    return '\n'.join(output) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        'terminal_tests', ROOT / 'My first Game' / 'tests' / 'terminal_tests.py',
    )
    terminal_tests = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(terminal_tests)
    with tempfile.TemporaryDirectory(prefix='ea-preview-') as temporary:
        app = terminal_tests.Terminal(args.executable.resolve(), [
            '--difficulty', 'normal', '--skip-tutorial', '--seed', '42',
            '--scores-file', str(Path(temporary) / 'scores'),
        ])
        try:
            app.wait_text('EXPEDITION 1')
            app.wait_text('.......') # Capture the sentry's visible warning.
            preview = render_svg(app.data)
            print(app.screen())
            app.quit()
        finally:
            app.cleanup()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(preview, encoding='utf-8')
    print(f'Wrote {args.output}')


if __name__ == '__main__':
    main()
