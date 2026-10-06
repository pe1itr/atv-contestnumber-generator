"""Explicit opt-in lifecycle test of the real ffmix sink.
Requires ffmix to be absent. Starts two local UDP streams, plays a local test
signal into the newly created sink and leaves ffmix in place, as the UI does.
Never removes or modifies a pre-existing sink or sets default devices.
"""
import json
import subprocess
import check_audio as audio


def sinks():
    return json.loads(audio.run(['pactl','-f','json','list','sinks']).stdout)


def modules():
    # Some pactl versions omit IDs in JSON module output; short format has IDs.
    lines=audio.run(['pactl','list','short','modules']).stdout.decode().splitlines()
    return {int(line.split('\t')[0]) for line in lines
            if 'module-null-sink' in line and 'sink_name=ffmix' in line.split('\t')[2].split()}


def main():
    assert not any(s['name']=='ffmix' for s in sinks()), 'ffmix already exists; creation test skipped to preserve it'
    audio.OUT.mkdir(exist_ok=True)
    before=modules()
    default_sink=audio.run(['pactl','get-default-sink']).stdout
    default_source=audio.run(['pactl','get-default-source']).stdout
    noise=audio.OUT/'sink-stimulus.f32'
    audio.run(['ffmpeg','-v','error','-y','-f','lavfi','-i','sine=frequency=997:sample_rate=48000:duration=15',
               '-ac','2','-f','f32le',str(noise)])
    players=[]
    def play():
        players.append(subprocess.Popen(['paplay','--raw','--format=float32le','--rate=48000','--channels=2',
                                         '--device=ffmix',str(noise)]))
    try:
        audio.capture('ffmix.monitor',306882,96000,1000,5,'auto-ffmix-create',playback=play)
        created=[s for s in sinks() if s['name']=='ffmix']; assert len(created)==1
        created_modules=modules()-before
        assert len(created_modules)==1
        # Keep paplay alive across Stop/Start; stream destruction must not unload its sink.
        assert players[0].poll() is None
        audio.capture('ffmix.monitor',306882,96000,1000,5,'auto-ffmix-reuse')
        assert [s['index'] for s in sinks() if s['name']=='ffmix']==[created[0]['index']]
        assert modules()-before==created_modules
        assert audio.run(['pactl','get-default-sink']).stdout==default_sink
        assert audio.run(['pactl','get-default-source']).stdout==default_source
        print('Automatic ffmix creation, persistence, live player across Stop/Start, reuse without duplicate and unchanged defaults: OK',flush=True)
    finally:
        for player in players:
            if player.poll() is None: player.terminate()
            player.wait(timeout=3)


if __name__=='__main__': main()
