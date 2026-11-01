using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Globalization;

// Native 2221 Y records only. No WaveStar packets, templates or COM dependencies.
public sealed class Waveform
{
    public float[] Samples;
    public long IntervalFs, TriggerPhaseFs;
    public double Range;
    public string Coupling;
    public int Bits;
    static string Field(string text, string key)
    {
        Match m = Regex.Match(text, @"(?:^|,)\s*" + Regex.Escape(key) + @"\s*:\s*([^,;]+)");
        if(!m.Success) throw new InvalidDataException("Missing " + key);
        return m.Groups[1].Value.Trim();
    }
    static double Number(string value)
    {
        double n = double.Parse(value, CultureInfo.InvariantCulture);
        if(double.IsNaN(n) || double.IsInfinity(n)) throw new InvalidDataException("Nonfinite scaling");
        return n;
    }
    public static Waveform Load(string folder)
    {
        string text = File.ReadAllText(Path.Combine(folder, "preamble.txt")).Trim();
        byte[] data = Tek2221Capture.ParseCurve(File.ReadAllBytes(Path.Combine(folder, "curve-response.bin")));
        if(Field(text,"PT.FMT") != "Y") throw new NotSupportedException("Prototype requires PT.FMT:Y; ENV and XY are not implemented yet.");
        if(Field(text,"BN.FMT") != "RP" || Field(text,"ENCDG") != "BINARY" || Field(text,"XUNIT") != "S" || Field(text,"YUNIT") != "V")
            throw new NotSupportedException("Requires calibrated unsigned binary data in seconds and volts.");
        int bytes = int.Parse(Field(text,"BYT/NR"));
        int bits = int.Parse(Field(text,"BIT/NR"));
        int count = int.Parse(Field(text,"NR.PT"));
        if(!((bytes == 1 && bits == 8) || (bytes == 2 && bits == 16)) || count < 1 || count > 4096 || data.Length != count * bytes)
            throw new InvalidDataException("Invalid point count or sample width");
        Match id = Regex.Match(text, "WFID:\"([^\"]+)\"");
        string[] f = id.Groups[1].Value.Split(',');
        if(!id.Success || f.Length < 5 || f[0].Trim() != "ACQ" || f[1].Trim() != "CH1")
            throw new NotSupportedException("Prototype requires ACQ CH1");
        Match scale = Regex.Match(f[2].Trim(), @"^([0-9.+Ee-]+)([mun]?)V$");
        if(!scale.Success) throw new InvalidDataException("Invalid V/div in WFID");
        double volts = Number(scale.Groups[1].Value);
        switch(scale.Groups[2].Value) { case "m": volts *= 1e-3; break; case "u": volts *= 1e-6; break; case "n": volts *= 1e-9; break; }
        double dt = Number(Field(text,"XINCR")), mult = Number(Field(text,"YMULT"));
        double off = Number(Field(text,"YOFF")), point = Number(Field(text,"PT.OFF"));
        if(dt <= 0 || dt > 1e3 || mult <= 0 || volts <= 0 || off == -10000 || point == -10000)
            throw new InvalidDataException("Unknown/invalid scaling or trigger reference");
        var w = new Waveform { Samples = new float[count], Bits = bits, Range = 8 * volts, Coupling = f[3].Trim(),
            IntervalFs = checked((long)Math.Round(dt * 1e15)), TriggerPhaseFs = checked((long)Math.Round(-point * dt * 1e15)) };
        if(w.IntervalFs <= 0 || (w.Coupling != "AC" && w.Coupling != "DC" && w.Coupling != "GND"))
            throw new InvalidDataException("Invalid interval or coupling");
        int center = bytes == 1 ? 128 : 32768;
        for(int i=0; i<count; i++)
        {
            int code = bytes == 1 ? data[i] : (data[2*i] << 8) | data[2*i+1];
            w.Samples[i] = (float)((code - center - off) * mult);
            if(float.IsInfinity(w.Samples[i])) throw new InvalidDataException("Sample overflow");
        }
        return w;
    }
    public string Header()
    {
        return string.Format(CultureInfo.InvariantCulture,"T2221 1 {0} {1} {2} {3:R} {4} {5}",
            Samples.Length, IntervalFs, TriggerPhaseFs, Range, Coupling, Bits);
    }
}
