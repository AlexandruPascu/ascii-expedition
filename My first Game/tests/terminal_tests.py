"""Exercise the real executable in an isolated POSIX terminal. No third-party modules."""
import os, pty, re, select, signal, struct, subprocess, sys, tempfile, termios, time, fcntl
from pathlib import Path
ansi=re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')
class Terminal:
    def __init__(self,executable,args,size=(40,100)):
        self.master,self.slave=pty.openpty()
        fcntl.ioctl(self.slave,termios.TIOCSWINSZ,struct.pack('HHHH',*size,0,0))
        self.original=termios.tcgetattr(self.slave)
        self.proc=subprocess.Popen([str(executable),*args],stdin=self.slave,stdout=self.slave,stderr=self.slave)
        self.data=b''
        self.read(.22)
    def read(self,seconds=.13):
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            ready,_,_=select.select([self.master],[],[],max(0,until-time.monotonic()))
            if ready:
                try: part=os.read(self.master,65536)
                except OSError: break
                if not part: break
                self.data+=part
    def send(self,keys,seconds=.13):
        os.write(self.master,keys.encode() if isinstance(keys,str) else keys)
        self.read(seconds)
    def route(self,route):
        for i in range(0,len(route),32): self.send(route[i:i+32],.08)
        self.read(.08)
    def rows(self):
        rows={}
        data=self.data.rsplit(b'\x1b[2J',1)[-1]
        for match in re.finditer(rb'\x1b\[(\d+);1H(.*?)(?=\x1b\[\d+;1H|\Z)',data,re.S):
            raw=ansi.sub(b'',match[2])
            if match.end()==len(data):
                # PTYs may split any write, including an ANSI sequence. Keep
                # the previous complete row until all 80 columns have arrived.
                raw=raw.split(b'\x1b',1)[0]
                if len(raw)<80: continue
            text=raw.decode('utf-8',errors='replace')
            rows[int(match[1])]=text[:80].ljust(80)
        return [rows.get(i,'') for i in range(1,38)]
    def screen(self): return '\n'.join(self.rows())
    def expect(self,text):
        assert text in self.screen(), f'Missing {text!r}\n{self.screen()}'
    def wait_text(self,text,timeout=3.5):
        deadline=time.monotonic()+timeout
        while text not in self.screen() and time.monotonic()<deadline: self.read(.06)
        self.expect(text)
    def resize(self,rows,columns):
        fcntl.ioctl(self.slave,termios.TIOCSWINSZ,struct.pack('HHHH',rows,columns,0,0))
        self.read(.15)
    def player(self):
        for y,row in enumerate(self.rows()[:30]):
            match=re.search(r' O-|-O ',row)
            if match: return (match.start(),y)
        raise AssertionError('Player not visible')
    def quit(self):
        self.send('q',.15)
        return self.finish()
    def finish(self):
        self.proc.wait(timeout=5)
        self.read(.05)
        assert self.proc.returncode==0
        assert termios.tcgetattr(self.slave)==self.original, 'Terminal input mode not restored'
        assert b'\x1b[?25h\x1b[?1049l' in self.data, 'Cursor/alternate screen not restored'
        result=ansi.sub(b'',self.data).decode('utf-8',errors='replace')
        os.close(self.master); os.close(self.slave)
        return result
    def cleanup(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            self.read(.15)
            try: self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired: self.proc.kill(); self.proc.wait(timeout=3)
        for descriptor in (self.master,self.slave):
            try: os.close(descriptor)
            except OSError: pass

def polish_checks(executable):
    with tempfile.TemporaryDirectory(prefix='ea-polish-tests-') as temporary:
        options=['--difficulty','normal','--skip-tutorial','--seed','42','--scores-file',str(Path(temporary)/'scores')]
        app=Terminal(executable,options)
        try:
            app.wait_text('EXPEDITION 1')
            app.send('h'); app.wait_text('HOW TO PLAY')
            app.expect('F / Space'); app.expect('Sentries warn with yellow dots')
            app.send('wasdfnrmt123'); app.expect('HOW TO PLAY')
            app.read(1.2)
            app.send('?p'); app.wait_text('PAUSED')
            app.expect('Time: 120s')
            frozen=app.rows()[31]
            app.send('?'); app.expect('HOW TO PLAY')
            app.send('\r'); app.wait_text('PAUSED')
            assert app.rows()[31]==frozen, 'Closing help must preserve an existing pause'
            app.send('p'); app.read(1.1)
            assert 'Time: 120s' not in app.screen(), 'Play must resume after leaving help'
            app.send('m'); app.expect('Choose a difficulty')
            app.send('?'); app.expect('HOW TO PLAY')
            app.send('h'); app.expect('Choose a difficulty')
            app.quit()
        finally: app.cleanup()
        app=Terminal(executable,options)
        try:
            app.wait_text('Time: 120s')
            before=app.rows()[31]
            app.resize(20,50); app.wait_text('WINDOW TOO SMALL')
            app.send('wasdfpnrm123'); app.read(1.2)
            app.resize(40,100); app.wait_text('PAUSED')
            assert app.rows()[31]==before, 'Resizing must freeze time and ignore gameplay keys'
            app.send('p'); app.read(1.1)
            assert app.rows()[31]!=before, 'Resized games must resume only on request'
            app.send('h'); app.expect('HOW TO PLAY')
            app.resize(20,50); app.wait_text('WINDOW TOO SMALL')
            app.resize(40,100); app.wait_text('HOW TO PLAY')
            app.send('h'); app.wait_text('PAUSED')
            app.quit()
        finally: app.cleanup()
        app=Terminal(executable,options,size=(20,50))
        try:
            app.wait_text('WINDOW TOO SMALL')
            app.resize(40,100); app.wait_text('PAUSED'); app.expect('Time: 120s')
            app.quit()
        finally: app.cleanup()
        app=Terminal(executable,options)
        try:
            app.wait_text('EXPEDITION 1')
            app.proc.send_signal(signal.SIGINT)
            app.read(.15)
            output=app.finish()
            assert 'Run summary - Normal' in output, 'Interrupt must still print a run summary'
        finally: app.cleanup()
        print('PASS help from menus/play/pause, ignored guide inputs, and pause restoration')
        print('PASS shrinking/restoring windows, small startup, paused timers, and SIGINT restoration')


def reader_checks():
    app=Terminal.__new__(Terminal)
    old=b'Score: 550    Best: 550'.ljust(80)
    new=b'Score: 1200    Best: 1200'.ljust(80)
    app.data=b'\x1b[2J\x1b[11;1H'+old+b'\x1b[11;1H'+new[:12]+b'\x1b[9'
    assert app.rows()[10]==old.decode(), 'A partial row must not overwrite a complete row'
    app.data+=b'3m'+new[12:]+b'\x1b[0m'
    assert app.rows()[10]==new.decode(), 'A completed row must replace the previous row'
    print('PASS terminal reader handles fragmented rows and ANSI sequences')


def main():
    reader_checks()
    executable=Path(sys.argv[1]).resolve()
    plans=dict(line.split('\t',1) for line in subprocess.check_output([sys.argv[2]],text=True,timeout=10).splitlines())
    with tempfile.TemporaryDirectory(prefix='ea-feature-smoke-') as temp:
        score=str(Path(temp)/'scores')
        app=Terminal(executable,['--seed','42','--scores-file',score])
        try:
            app.expect('> 2. Normal')
            app.send('\x1b[A'); app.expect('> 1. Relaxed')
            app.send('\x1b'); app.send('[B'); app.expect('> 2. Normal')
            app.send('t3'); app.wait_text('EXPEDITION 1'); app.expect('Hard'); app.expect('Lives: 2'); app.expect('Q quit')
            app.expect('Time: 90s'); app.expect('/!\\'); app.expect('[+]')
            app.wait_text('.......')
            app.wait_text('!!!!!!!')
            app.send('p'); app.expect('PAUSED')
            hud=app.rows()[31]
            app.read(.3); assert app.rows()[31]==hud,'Pause must freeze timer'
            app.send('m'); app.expect('> 3. Hard')
            app.send('t2'); app.expect('TUTORIAL 1/2: COLLECT STARS'); app.expect('Next lesson: aim with arrows/WASD; F/Space fires')
            app.route(plans['tutorial0']); app.wait_text('STAGE CLEAR - RUN PROGRESS'); app.expect('Stars: 3'); app.expect('Score: 550    Best: 550')
            app.send('\r'); app.expect('TUTORIAL 2/2: PATROLS AND BLASTER'); app.expect('Shoot a red triangle guard')
            app.route(plans['tutorial1']); app.wait_text('STAGE CLEAR - RUN PROGRESS'); app.expect('Stars: 7')
            app.send('\r'); app.wait_text('EXPEDITION 1'); app.expect('Lives: 3')
            app.send('m'); app.send('t2'); app.wait_text('EXPEDITION 1'); app.expect('Score: 0')
            app.route(plans['crate']); app.send('f'); app.expect('Crate hit! One more shot')
            app.read(.25); app.send('f'); app.expect('Crate broken: +')
            app.send('r'); app.expect('Score: 0'); app.expect('Seed: 42')
            app.route(plans['boost']); app.expect('SPEED x2'); app.expect('Speed boost acquired')
            x,y,dx,dy=map(int,plans['step'].split())
            assert app.player()==(x,y)
            arrow={(1,0):'C',(-1,0):'D',(0,1):'B',(0,-1):'A'}[(dx,dy)]
            reverse={(1,0):'D',(-1,0):'C',(0,1):'A',(0,-1):'B'}[(dx,dy)]
            app.send('\x1b['+arrow); assert app.player()==(x+dx*2,y+dy*2),'Arrow must take two boosted steps'
            app.send('\x1b[1;2'+reverse); assert app.player()==(x+dx,y+dy),'Shift arrow must take one precise step'
            app.send('n'); app.expect('Score: 0'); assert 'Seed: 42' not in app.screen()
            app.send('m'); app.send('1'); app.expect('Relaxed'); app.expect('Lives: 5'); app.expect('Time: OFF')
            summary=app.quit()
            assert 'Run summary - Relaxed' in summary and 'Active time:' in summary
        finally: app.cleanup()
        app=Terminal(executable,['--seed','42','--scores-file',score])
        try:
            app.expect('2. Normal  | Best: 1200')
            app.send('t'); assert '2. Normal  | Best: 0' not in app.screen(),'Direct-run score did not persist'
            app.quit()
        finally: app.cleanup()
        app=Terminal(executable,['--difficulty','normal','--skip-tutorial','--seed','42','--scores-file',score])
        try:
            app.wait_text('EXPEDITION 1'); app.expect('Normal'); app.quit()
        finally: app.cleanup()
        print('PASS terminal menu, fragmented arrows, difficulty profiles, sentry warnings/pulses, pause')
        print('PASS two tutorial completions, run summary, crate feedback, replay, boost and Shift arrows')
        print('PASS persistent scores across restarts, direct CLI start, quit summary and terminal restoration')
    polish_checks(executable)


if __name__ == "__main__":
    main()
