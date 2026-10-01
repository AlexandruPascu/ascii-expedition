"""Test AI in the ordinary game executable, without Python ML dependencies."""
from pathlib import Path
import sys
import tempfile
from terminal_tests import Terminal


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='expedition-autopilot-') as temp:
        score = Path(temp) / 'scores'
        options = ['--seed', '42', '--scores-file', str(score)]
        app = Terminal(executable, [*options, '--difficulty', 'normal', '--ai', 'cem'])
        try:
            app.wait_text('AI: CEM')
            app.send('p'); app.wait_text('PAUSED')
            frozen = app.rows()
            app.read(.3)
            assert app.rows() == frozen, 'AI must stop with the game when paused'
            app.send('h'); app.expect('HOW TO PLAY'); app.expect('I: autopilot')
            app.send('iowasdf'); app.expect('HOW TO PLAY')
            app.send('h'); app.expect('PAUSED'); app.expect('AI: CEM')
            app.send('p')
            app.resize(20, 50); app.wait_text('WINDOW TOO SMALL')
            app.send('iowasdf'); app.read(.2)
            app.resize(40, 100); app.wait_text('PAUSED'); app.expect('AI: CEM')
            app.send('p')
            app.send('w'); app.wait_text('Control: MANUAL'); app.expect('assisted run')
            app.send('o'); app.expect('I: PPO autopilot')
            app.send('i'); app.expect('AI: PPO')
            app.send('o'); app.expect('AI: CEM')
            app.send('rp'); app.wait_text('PAUSED'); app.expect('Score: 0'); app.expect('Seed: 42')
            app.send('np'); app.wait_text('PAUSED'); app.expect('Score: 0')
            assert 'Seed: 42' not in app.screen(), 'New run must replace the seed'
            app.send('m'); app.expect('I: AI ON (CEM)')
            app.send('oi'); app.expect('I: AI OFF (PPO)')
            app.send('3'); app.wait_text('TUTORIAL 1/2'); app.expect('Hard'); app.expect('Control: MANUAL')
            app.quit()
            assert not score.exists(), 'AI and mixed-control runs must not create personal bests'
        finally:
            app.cleanup()
        print('PASS ordinary-game AI pause/help/resize, manual takeover, agent switch, restart/new run and menu controls')
        for agent, difficulty in (('cem', 'hard'), ('ppo', 'relaxed')):
            app = Terminal(executable, [*options, '--difficulty', difficulty, '--ai', agent], cwd=temp)
            try:
                app.wait_text('AI: ' + agent.upper())
                app.wait_text('STAGE CLEAR - RUN PROGRESS', timeout=45)
                app.send('p'); app.wait_text('PAUSED')
                app.read(1.4); app.expect('PAUSED'); app.expect('TUTORIAL 1/2')
                app.send('p'); app.wait_text('STAGE CLEAR - RUN PROGRESS')
                app.resize(20, 50); app.wait_text('WINDOW TOO SMALL')
                app.read(1.4)
                app.resize(40, 100); app.wait_text('PAUSED'); app.expect('TUTORIAL 1/2')
                app.send('p')
                app.wait_text('TUTORIAL 2/2', timeout=45)
                app.wait_text('EXPEDITION 1', timeout=45)
                app.wait_text('EXPEDITION 2', timeout=90)
                app.expect('AI: ' + agent.upper())
                result = app.quit()
                assert 'AI-assisted run: personal best not saved.' in result
                assert 'Expeditions cleared: 1' in result
                assert not score.exists(), 'Automatic progression must keep scores separate from human runs'
            finally:
                app.cleanup()
            print(f'PASS {agent.upper()} plays both tutorials and an expedition in the regular {difficulty} game')


if __name__ == '__main__':
    main()
