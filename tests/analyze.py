import numpy as np, scipy.io.wavfile as w, glob, sys
for f in sorted(glob.glob('out/note_*.wav')):
    sr,x=w.read(f); x=x[:,0].astype(float)
    note=int(f.split('_')[1]); f0=440*2**((note-69)/12)
    pk=np.max(np.abs(x))
    # harmonic levels in window 50-250ms after onset
    s=int(0.1*sr); seg=x[s:s+int(0.2*sr)]*np.hanning(int(0.2*sr))
    S=np.abs(np.fft.rfft(seg,8*len(seg))); fr=np.fft.rfftfreq(8*len(seg),1/sr)
    def lv(fq):
        i=np.argmin(abs(fr-fq)); return 20*np.log10(S[max(0,i-20):i+20].max()+1e-12)
    h1=lv(f0); hs=[lv(k*f0)-h1 for k in (2,3,4,5)]
    bell=lv(6.27*f0)-h1 if 6.27*f0<sr/2 else -999
    # decay: rms 0.5s vs 2.5s
    def rms(a,b): return 20*np.log10(np.sqrt(np.mean(x[int(a*sr):int(b*sr)]**2))+1e-12)
    print(f"{f[4:]:22s} pk {20*np.log10(pk):6.1f}dB  H2..5 {' '.join(f'{v:6.1f}' for v in hs)}  bell {bell:6.1f}  rms@.3 {rms(.2,.4):6.1f} @2.5 {rms(2.4,2.6):6.1f} rel {rms(3.2,3.3):6.1f}")
